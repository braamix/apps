// `atexit`: functions to call once the program is done.
//
// The VM asks for them where the main module returns or an exception ends it,
// and runs them last registered first. One that raises is reported and the
// rest still run, as CPython does.
#include "atexit.h"
#include "builtin.h"
#include "call.h"
#include "exc.h"
#include "gc.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "module.h"
#include "ops.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// Each entry is a tuple (func, args, kwnames, kwvals).
struct Home {
    Value calls; // ListObj
};

Home *home;

void home_mark()
{
    gc_mark(home->calls);
}

ListObj *calls()
{
    if (!home) {
        home = heap_new<Home>();
        if (!home)
            return oom(), nullptr;
        gc_root_hook(home_mark);
    }
    if (home->calls.is_nil()) {
        ListObj *l = list_new();
        if (!l)
            return oom(), nullptr;
        home->calls = obj_value(l);
    }
    return list_of(home->calls);
}

Value tuple_of(const Value *xs, u32 n)
{
    TupleObj *t = tuple_new(n);
    if (!t)
        return oom(), Value();
    for (u32 i = 0; i < n; i++)
        t->items()[i] = xs[i];
    return obj_value(t);
}

R b_register(const CallArgs &a, Value &out)
{
    if (!a.nargs)
        return err_set("TypeError", "register() takes at least 1 argument (0 given)");
    if (!py_callable(a.args[0]))
        return err_set("TypeError", "the first argument must be callable");
    Root args{ tuple_of(a.args + 1, a.nargs - 1) };
    Root names{ tuple_of(a.kwnames, a.nkw) };
    Root vals{ tuple_of(a.kwvals, a.nkw) };
    if (args.v.is_nil() || names.v.is_nil() || vals.v.is_nil())
        return R::Err;
    Value four[4] = { a.args[0], args.v, names.v, vals.v };
    Root entry{ tuple_of(four, 4) };
    ListObj *l = calls();
    if (entry.v.is_nil() || !l || !list_push(l, entry.v))
        return err_pending() ? R::Err : oom();
    out = a.args[0];
    return R::Ok;
}

R b_unregister(const CallArgs &a, Value &out)
{
    if (!args_only(a, "unregister", 1, 1))
        return R::Err;
    ListObj *l = calls();
    if (!l)
        return R::Err;
    Root rf{ a.args[0] };
    usize keep = 0;
    for (usize i = 0; i < list_of(home->calls)->items.size(); i++) {
        Value e   = list_of(home->calls)->items[i];
        Value fn  = static_cast<TupleObj *>(e.obj())->items()[0];
        bool same = fn == rf.v;
        if (!same && py_eq(fn, rf.v, same) != R::Ok)
            return R::Err;
        if (!same)
            list_of(home->calls)->items[keep++] = e;
    }
    while (list_of(home->calls)->items.size() > keep)
        list_of(home->calls)->items.pop();
    out = value_none();
    return R::Ok;
}

R b_clear(const CallArgs &a, Value &out)
{
    if (!args_only(a, "_clear", 0, 0))
        return R::Err;
    ListObj *l = calls();
    if (!l)
        return R::Err;
    l->items.clear();
    out = value_none();
    return R::Ok;
}

R b_ncallbacks(const CallArgs &a, Value &out)
{
    if (!args_only(a, "_ncallbacks", 0, 0))
        return R::Err;
    ListObj *l = calls();
    if (!l)
        return R::Err;
    out = Value::of_int(i32(l->items.size()));
    return R::Ok;
}

// Every registered call, newest first. i counts the calls made; a caught
// exception is reported before the next.
R run_step(ContObj *k, Value in)
{
    (void)in;
    if (!k->caught.is_nil()) {
        Root c{ k->caught };
        k->caught = Value();
        String text;
        if (!text.append("Exception ignored in atexit callback ") ||
            py_repr(k->s[1], text) != R::Ok || !text.append(":\n"))
            err_clear();
        exc_line(c.v, text);
        text.push('\n');
        atexit_report(text.str());
    }
    ListObj *l = calls();
    if (!l)
        return R::Err;
    if (l->items.empty())
        return cont_done(k, value_none());
    Value e = l->items[l->items.size() - 1];
    l->items.pop();
    TupleObj *t = static_cast<TupleObj *>(e.obj());
    k->s[1]     = t->items()[0];
    k->catching = CATCH_ANY;
    return cont_call_kw(k, t->items()[0], t->items()[1], t->items()[2], t->items()[3]);
}

R b_run(const CallArgs &a, Value &out)
{
    if (!args_only(a, "_run_exitfuncs", 0, 0))
        return R::Err;
    out = cont_new(run_step);
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr ModDef DEFS[] = {
    { "register", b_register },      { "unregister", b_unregister }, { "_clear", b_clear },
    { "_ncallbacks", b_ncallbacks }, { "_run_exitfuncs", b_run },
};

} // namespace

bool atexit_pending()
{
    return home && !home->calls.is_nil() && !list_of(home->calls)->items.empty();
}

Value atexit_runner()
{
    return cont_new(run_step);
}

bool atexit_install(DictObj *into)
{
    return mod_defs(into, DEFS);
}
