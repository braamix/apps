// Argument binding, and the continuation a suspending builtin parks in.
#include "call.h"

#include "exc.h"
#include "gc.h"
#include "gen.h"
#include "intern.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "method.h"
#include "ops.h"
#include "type.h"

extern const Type seqiter_type;

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// CPython's `iterator`: a class with __getitem__ and no __iter__ is walked
// by index, and IndexError or StopIteration is the end.
struct SeqIterObj : Obj {
    Value seq; // Nil once it is over
    u32 index;
};

void seqiter_trace(Obj *o)
{
    gc_mark(static_cast<SeqIterObj *>(o)->seq);
}

bool is_seqiter(Value v)
{
    return v.is_obj() && v.obj()->type == &seqiter_type;
}

R stop()
{
    Value e = exc_new(exc_find("StopIteration"), Value());
    return e.is_nil() ? R::Err : err_set_value(e);
}

// s[0] the iterator, s[1] the bound __getitem__.
R seqiter_step(ContObj *k, Value in)
{
    SeqIterObj *it = static_cast<SeqIterObj *>(k->s[0].obj());
    if (k->i++ == 0)
        return cont_call(k, k->s[1], Value::of_int(i32(it->index)));
    if (in.is_nil()) {
        it->seq = Value();
        return stop();
    }
    it->index++;
    return cont_done(k, in);
}

R si_next(const CallArgs &a, Value &out)
{
    if (a.nargs != 1 || a.nkw || !is_seqiter(a.args[0]))
        return err_set("TypeError", "__next__() takes no arguments");
    Root self{ a.args[0] };
    SeqIterObj *it = static_cast<SeqIterObj *>(self.v.obj());
    if (it->seq.is_nil())
        return stop();
    Root get{ type_special(it->seq, "__getitem__") };
    if (get.v.is_nil())
        return err_pending() ? R::Err : not_iterable(it->seq);
    Root kv{ cont_new(seqiter_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k  = cont_of(kv.v);
    k->s[0]     = self.v;
    k->s[1]     = get.v;
    k->catching = CATCH_SEQEND;
    out         = kv.v;
    return R::Ok;
}

R si_iter(const CallArgs &a, Value &out)
{
    out = a.args[0];
    return R::Ok;
}

constexpr Method SEQITER_METHODS[] = {
    { "__next__", si_next },
    { "__iter__", si_iter },
};

// iter(seq) for a class with only __getitem__.
R si_make(const CallArgs &a, Value &out)
{
    Root seq{ method_self(a.args[0]) };
    SeqIterObj *it = static_cast<SeqIterObj *>(obj_alloc(&seqiter_type, sizeof(SeqIterObj)));
    if (!it)
        return oom();
    it->seq   = a.args[0];
    it->index = 0;
    out       = obj_value(it);
    return R::Ok;
}

// Something do_call can call with no arguments to get the next item. Nil when
// the native protocol answers instead.
Value stepper(Value it)
{
    return next_special(it);
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
        Value it = iter_special(k->s[5]);
        if (it.is_nil())
            return not_iterable(k->s[5]);
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

constexpr Type seqiter_type{ .name = "iterator", .trace = seqiter_trace };

Value iter_special(Value v)
{
    if (is_seqiter(v)) {
        Root rv{ v };
        Root fn{ native_new("__iter__", si_iter) };
        return fn.v.is_nil() ? Value() : method_new(fn.v, rv.v);
    }
    Value m = type_special(v, "__iter__");
    if (!m.is_nil() || err_pending())
        return m;
    if (!type_has_py_special(v, "__getitem__") || type_has_special(v, "__iter__"))
        return Value();
    Root rv{ v };
    Root fn{ native_new("__iter__", si_make) };
    return fn.v.is_nil() ? Value() : method_new(fn.v, rv.v);
}

Value next_special(Value v)
{
    if (is_resumable(v))
        return genrun_new(v, GR_NEXT);
    if (v.is_obj() && v.obj()->type->vmnext) {
        StrObj *n = str_intern("__next__");
        Value m;
        if (!n)
            return oom(), Value();
        return method_find(v, n, m) == R::Ok ? m : Value();
    }
    if (is_seqiter(v)) {
        Root rv{ v };
        Root fn{ native_new("__next__", si_next) };
        return fn.v.is_nil() ? Value() : method_new(fn.v, rv.v);
    }
    return type_special(v, "__next__");
}

bool seqiter_methods()
{
    return method_install(&seqiter_type, SEQITER_METHODS);
}

constexpr Type cont_type{ .name = "continuation", .trace = cont_trace, .repr = cont_repr };

Value cont_new(ContStep step)
{
    ContObj *k = static_cast<ContObj *>(obj_alloc(&cont_type, sizeof(ContObj)));
    if (!k)
        return oom(), Value();
    k->step = step;
    k->fail = nullptr;
    for (i64 &n : k->x)
        n = 0;
    k->redo = nullptr;
    for (Value &v : k->s)
        v = Value();
    k->fn = Value();
    for (Value &v : k->a)
        v = Value();
    k->argv    = Value();
    k->kwnames = Value();
    k->kwvals  = Value();
    k->out     = Value();
    k->next    = Value();
    k->locals  = Value();
    k->caught  = Value();
    k->nargs   = 0;
    k->i = k->j = 0;
    k->catching = CATCH_NONE;
    k->drop     = false;
    k->reading  = false;
    return obj_value(k);
}

bool iter_needs_vm(Value v)
{
    if (is_resumable(v) || is_seqiter(v))
        return true;
    if (v.is_obj() && v.obj()->type->vmnext)
        return true;
    if (type_has_py_special(v, "__iter__") || type_has_py_special(v, "__next__"))
        return true;
    return type_has_py_special(v, "__getitem__") && !type_has_special(v, "__iter__");
}

R iter_park(const CallArgs &a, u32 at, R (*again)(const CallArgs &, Value &out), Value &out)
{
    Root src{ a.args[at] };
    // Nil for a class instance: the drain calls its __iter__ first.
    bool own = is_resumable(src.v) || (src.v.is_obj() && src.v.obj()->type->vmnext);
    Root m{ own ? next_special(src.v) : Value() };
    if (own && m.v.is_nil())
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

namespace {

// s[0] the call to make first, s[1] and s[3] its arguments; s[2] the builtin's
// own, copied; j where the answer goes.
R redo_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call(k, k->s[0], k->s[1], k->nargs, k->s[3]);
    TupleObj *pos      = static_cast<TupleObj *>(k->s[2].obj());
    TupleObj *kwv      = static_cast<TupleObj *>(k->s[4].obj());
    TupleObj *kwn      = static_cast<TupleObj *>(k->s[5].obj());
    pos->items()[k->j] = in;
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

} // namespace

R redo_with(const CallArgs &a, u32 at, Value fn, Value a0, Value a1, u32 n,
            R (*again)(const CallArgs &, Value &out), Value &out)
{
    Root rf{ fn }, r0{ a0 }, r1{ a1 };
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
    Root kv{ cont_new(redo_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = rf.v;
    k->s[1]    = r0.v;
    k->s[2]    = rp.v;
    k->s[3]    = r1.v;
    k->s[4]    = rv.v;
    k->s[5]    = rn.v;
    k->nargs   = n;
    k->j       = at;
    k->redo    = again;
    out        = kv.v;
    return R::Ok;
}

namespace {

// s[0] the bound method, s[1] its argument or Nil.
R answer_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return k->s[1].is_nil() ? cont_call(k, k->s[0], Value(), 0)
                                : cont_call(k, k->s[0], k->s[1]);
    return cont_done(k, in);
}

} // namespace

bool answer_special(Value v, Str name, Value a0, Value &out, R &r)
{
    if (!type_has_py_special(v, name))
        return false;
    Root ra{ a0 };
    Root m{ type_special(v, name) };
    Root kv{ m.v.is_nil() ? Value() : cont_new(answer_step) };
    if (kv.v.is_nil()) {
        r = R::Err;
        return true;
    }
    cont_of(kv.v)->s[0] = m.v;
    cont_of(kv.v)->s[1] = ra.v;
    out                 = kv.v;
    r                   = R::Ok;
    return true;
}

bool redo_converted(const CallArgs &a, u32 at, Str name, R (*again)(const CallArgs &, Value &out),
                    Value &out, R &r)
{
    if (at >= a.nargs || !type_has_py_special(a.args[at], name))
        return false;
    Root m{ type_special(a.args[at], name) };
    r = m.v.is_nil() ? R::Err : redo_with(a, at, m.v, Value(), Value(), 0, again, out);
    return true;
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

namespace {

R n_pass(const CallArgs &a, Value &out)
{
    out = a.args[0];
    return R::Ok;
}

struct PassHome {
    Value pass;
};

PassHome *pass_home;

void pass_mark()
{
    if (pass_home)
        gc_mark(pass_home->pass);
}

Value pass_native()
{
    if (!pass_home) {
        pass_home = heap_new<PassHome>();
        if (!pass_home)
            return err_set("MemoryError", "out of memory"), Value();
        gc_root_hook(pass_mark);
    }
    if (pass_home->pass.is_nil())
        pass_home->pass = native_new("_await", n_pass);
    return pass_home->pass;
}

// s[0] the object, s[1] the name, s[2] and s[3] the arguments; j how many.
R method_step(ContObj *k, Value in)
{
    switch (k->i) {
    case 0: {
        Got g = py_attr(k->s[0], str_of(k->s[1]), in);
        if (g == Got::Error)
            return R::Err;
        if (g == Got::Missing) {
            Buf<128> m;
            m.put("'").put(type_name(k->s[0])).put("' object has no attribute '");
            m.put(str_of(k->s[1])->str()).put("'");
            return err_set("AttributeError", m.str());
        }
        k->i = 2;
        if (g == Got::Call) {
            k->i = 1;
            return cont_await(k, in);
        }
        break;
    }
    case 1:
        k->i = 2;
        break;
    default:
        return cont_done(k, in);
    }
    return k->j == 0 ? cont_call(k, in, Value(), 0) : cont_call(k, in, k->s[2], k->j, k->s[3]);
}

} // namespace

R cont_await(ContObj *k, Value cont)
{
    Root rc{ cont };
    Value p = pass_native();
    if (p.is_nil())
        return R::Err;
    return cont_call(k, p, rc.v);
}

R cont_attr(ContObj *k, Value obj, Str name)
{
    Root ro{ obj };
    StrObj *nm = str_intern(name);
    if (!nm)
        return err_set("MemoryError", "out of memory");
    Value got;
    Got g = py_attr(ro.v, nm, got);
    if (g == Got::Error)
        return R::Err;
    if (g == Got::Missing) {
        Buf<128> m;
        m.put("'").put(type_name(ro.v)).put("' object has no attribute '").put(name).put("'");
        return err_set("AttributeError", m.str());
    }
    return cont_await(k, got);
}

R cont_method(ContObj *k, Value obj, Str name, u32 n, Value a0, Value a1)
{
    Root ro{ obj }, r0{ a0 }, r1{ a1 };
    StrObj *nm = str_intern(name);
    if (!nm)
        return err_set("MemoryError", "out of memory");
    Root rn{ obj_value(nm) };
    // The common case, a plain method, costs no second continuation.
    Value got;
    Got g = py_attr(ro.v, nm, got);
    if (g == Got::Ok)
        return n == 0 ? cont_call(k, got, Value(), 0) : cont_call(k, got, r0.v, n, r1.v);
    if (g == Got::Error)
        return R::Err;
    Root kv{ cont_new(method_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *m = cont_of(kv.v);
    m->s[0]    = ro.v;
    m->s[1]    = rn.v;
    m->s[2]    = r0.v;
    m->s[3]    = r1.v;
    m->j       = n;
    return cont_await(k, kv.v);
}
