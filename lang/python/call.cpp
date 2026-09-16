// Argument binding, and the continuation a suspending builtin parks in.
#include "call.h"

#include "gc.h"
#include "gen.h"
#include "kernel/fmt.h"
#include "ops.h"
#include "type.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// Something do_call can call with no arguments to get the next item. Nil when
// the native protocol answers instead.
Value stepper(Value it)
{
    return is_gen(it) ? genrun_new(it, GR_NEXT) : type_special(it, "__next__");
}

// One turn of the drain iter_park sets up. s[0] is the bound __next__, s[1]
// the list being filled, s[2] the positional arguments, s[3] the keyword
// values, s[4] their names, s[5] the iterable. j is the argument the list
// replaces.
R drain_step(ContObj *k, Value in)
{
    // s[0] is Nil for a class instance, whose __iter__ has to run first.
    if (k->i++ == 0) {
        if (!k->s[0].is_nil())
            return cont_call(k, k->s[0], Value(), 0);
        Value it = type_special(k->s[5], "__iter__");
        if (it.is_nil())
            return err_set2("TypeError", "object is not iterable", type_name(k->s[5]));
        return cont_call(k, it, Value(), 0);
    }
    if (k->s[0].is_nil()) {
        Root rit{ in };
        k->s[0] = stepper(rit.v);
        if (k->s[0].is_nil()) {
            // __iter__ answered with a built-in iterator, which walks itself.
            ListObj *xs = py_list_of(rit.v);
            if (!xs)
                return R::Err;
            k->s[1] = obj_value(xs);
            in      = Value();
        } else {
            return cont_call(k, k->s[0], Value(), 0);
        }
    }
    if (!in.is_nil()) {
        if (!list_push(list_of(k->s[1]), in))
            return oom();
        return cont_call(k, k->s[0], Value(), 0);
    }
    // Nil is the StopIteration CATCH_STOP swallowed. The drain is over, so
    // the builtin runs again over the list.
    TupleObj *pos      = static_cast<TupleObj *>(k->s[2].obj());
    TupleObj *kwv      = static_cast<TupleObj *>(k->s[3].obj());
    TupleObj *kwn      = static_cast<TupleObj *>(k->s[4].obj());
    pos->items()[k->j] = k->s[1];
    CallArgs a;
    a.args    = pos->items();
    a.nargs   = u32(pos->len);
    a.kwvals  = kwv->items();
    a.kwnames = kwn->items();
    a.nkw     = u32(kwn->len);
    Value got;
    R r = k->redo(a, got);
    return r == R::Ok ? cont_done(k, got) : r;
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
    gc_mark(k->argv);
    gc_mark(k->kwnames);
    gc_mark(k->kwvals);
    gc_mark(k->out);
    gc_mark(k->next);
    gc_mark(k->locals);
    gc_mark(k->caught);
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
    k->fail = nullptr;
    k->redo = nullptr;
    for (Value &v : k->s)
        v = Value();
    k->fn = Value();
    for (Value &v : k->a)
        v = Value();
    k->argv    = Value();
    k->kwnames = Value();
    k->kwvals  = Value();
    k->out    = Value();
    k->next   = Value();
    k->locals = Value();
    k->caught = Value();
    k->nargs  = 0;
    k->i = k->j = 0;
    k->catching = CATCH_NONE;
    k->drop     = false;
    k->reading  = false;
    return obj_value(k);
}

bool iter_needs_vm(Value v)
{
    if (is_gen(v))
        return true;
    return type_has_special(v, "__iter__") || type_has_special(v, "__next__");
}

R iter_park(const CallArgs &a, u32 at, R (*again)(const CallArgs &, Value &out), Value &out)
{
    Root src{ a.args[at] };
    // Nil for a class instance: the drain calls its __iter__ first.
    Root m{ is_gen(src.v) ? genrun_new(src.v, GR_NEXT) : Value() };
    if (is_gen(src.v) && m.v.is_nil())
        return R::Err;
    ListObj *xs = list_new();
    if (!xs)
        return oom();
    Root rl{ obj_value(xs) };

    // The call is copied whole. It is made again once the list is full.
    TupleObj *pos = tuple_new(a.nargs);
    if (!pos)
        return oom();
    for (u32 i = 0; i < a.nargs; i++)
        pos->items()[i] = a.args[i];
    Root rp{ obj_value(pos) };
    TupleObj *kwv = tuple_new(a.nkw);
    if (!kwv)
        return oom();
    for (u32 i = 0; i < a.nkw; i++)
        kwv->items()[i] = a.kwvals[i];
    Root rv{ obj_value(kwv) };
    TupleObj *kwn = tuple_new(a.nkw);
    if (!kwn)
        return oom();
    for (u32 i = 0; i < a.nkw; i++)
        kwn->items()[i] = a.kwnames[i];
    Root rn{ obj_value(kwn) };

    Root kv{ cont_new(drain_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k  = cont_of(kv.v);
    k->s[0]     = m.v;
    k->s[1]     = rl.v;
    k->s[2]     = rp.v;
    k->s[3]     = rv.v;
    k->s[4]     = rn.v;
    k->s[5]     = src.v;
    k->j        = at;
    k->redo     = again;
    k->catching = CATCH_STOP;
    out         = kv.v;
    return R::Ok;
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
