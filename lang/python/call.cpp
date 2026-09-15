// Argument binding, and the continuation a suspending builtin parks in.
#include "call.h"

#include "gc.h"
#include "kernel/fmt.h"
#include "ops.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

Str fn_name(CodeObj *co)
{
    return is_str(co->name) ? str_of(co->name)->str() : Str("?");
}

bool same_name(Value a, Value b)
{
    if (a == b)
        return true;
    bool eq = false;
    return is_str(a) && is_str(b) && py_eq(a, b, eq) == R::Ok && eq;
}

R too_many(CodeObj *co, u32 given)
{
    char tmp[24];
    Buf<128> m;
    m.put(fn_name(co)).put("() takes ").put(int_text(tmp, sizeof tmp, i64(co->argcount)));
    m.put(" positional arguments but ").put(int_text(tmp, sizeof tmp, i64(given)));
    m.put(" were given");
    return err_set("TypeError", m.str());
}

R missing(CodeObj *co, Value name, Str what)
{
    Buf<128> m;
    m.put(fn_name(co)).put("() missing a required ").put(what).put(" argument: '");
    m.put(is_str(name) ? str_of(name)->str() : Str("?")).put("'");
    return err_set("TypeError", m.str());
}

void cont_trace(Obj *o)
{
    ContObj *k = static_cast<ContObj *>(o);
    for (Value &v : k->s)
        gc_mark(v);
    gc_mark(k->fn);
    for (Value &v : k->a)
        gc_mark(v);
    gc_mark(k->out);
    gc_mark(k->next);
}

R cont_repr(Value v, String &out)
{
    (void)v;
    return out.append("<continuation>") ? R::Ok : oom();
}

} // namespace

constexpr Type cont_type{ .name = "continuation", .trace = cont_trace, .repr = cont_repr };

Value cont_new(ContStep step)
{
    ContObj *k = static_cast<ContObj *>(obj_alloc(&cont_type, sizeof(ContObj)));
    if (!k)
        return oom(), Value();
    k->step = step;
    for (Value &v : k->s)
        v = Value();
    k->fn = Value();
    for (Value &v : k->a)
        v = Value();
    k->out   = Value();
    k->next  = Value();
    k->nargs = 0;
    k->i = k->j = 0;
    return obj_value(k);
}

R bind_args(FuncObj *fn, CodeObj *co, FrameObj *nf, const CallArgs &a)
{
    Value *lo   = nf->slots();
    u32 argc    = co->argcount;
    u32 named   = argc + co->kwonly;
    u32 at_star = named;
    u32 at_kw   = named + ((co->flags & CO_VARARGS) ? 1 : 0);

    if (a.nargs > argc && !(co->flags & CO_VARARGS))
        return too_many(co, a.nargs);

    u32 direct = a.nargs < argc ? a.nargs : argc;
    for (u32 i = 0; i < direct; i++)
        lo[i] = a.args[i];

    if (co->flags & CO_VARARGS) {
        u32 extra   = a.nargs > argc ? a.nargs - argc : 0;
        TupleObj *t = tuple_new(extra);
        if (!t)
            return oom();
        for (u32 i = 0; i < extra; i++)
            t->items()[i] = a.args[argc + i];
        lo[at_star] = obj_value(t);
    }

    DictObj *rest = nullptr;
    if (co->flags & CO_VARKW) {
        rest = dict_new();
        if (!rest)
            return oom();
        lo[at_kw] = obj_value(rest);
    }

    for (u32 k = 0; k < a.nkw; k++) {
        Value key = a.kwnames[k], val = a.kwvals[k];
        u32 slot = named;
        for (u32 i = co->posonly; i < named; i++)
            if (same_name(co->varnames[i], key)) {
                slot = i;
                break;
            }
        if (slot < named) {
            if (!lo[slot].is_nil()) {
                Buf<128> m;
                m.put(fn_name(co)).put("() got multiple values for argument '");
                m.put(is_str(key) ? str_of(key)->str() : Str("?")).put("'");
                return err_set("TypeError", m.str());
            }
            lo[slot] = val;
            continue;
        }
        if (!rest) {
            Buf<128> m;
            m.put(fn_name(co)).put("() got an unexpected keyword argument '");
            m.put(is_str(key) ? str_of(key)->str() : Str("?")).put("'");
            return err_set("TypeError", m.str());
        }
        if (dict_set(rest, key, val) != R::Ok)
            return R::Err;
    }

    // Defaults line up with the last positional parameters.
    if (!fn->defaults.is_nil()) {
        TupleObj *d = static_cast<TupleObj *>(fn->defaults.obj());
        u32 first   = argc - (d->len < argc ? u32(d->len) : argc);
        for (u32 i = first; i < argc; i++)
            if (lo[i].is_nil())
                lo[i] = d->items()[i - first];
    }
    for (u32 i = 0; i < argc; i++)
        if (lo[i].is_nil())
            return missing(co, co->varnames[i], "positional");

    if (!fn->kwdefaults.is_nil())
        for (u32 i = argc; i < named; i++) {
            if (!lo[i].is_nil())
                continue;
            Value got;
            R r = dict_get(static_cast<DictObj *>(fn->kwdefaults.obj()), co->varnames[i], got);
            if (r == R::Err)
                return R::Err;
            if (r == R::Ok)
                lo[i] = got;
        }
    for (u32 i = argc; i < named; i++)
        if (lo[i].is_nil())
            return missing(co, co->varnames[i], "keyword-only");

    // A parameter some nested scope captures is copied into its cell by the
    // first instructions of the body; nothing to do here.
    return R::Ok;
}
