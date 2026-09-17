// The generator objects and the bound resume. What a resume does is in vm.cpp,
// beside the dispatch loop whose frame chain it pushes onto.
#include "gen.h"

#include "gc.h"
#include "kernel/fmt.h"
#include "method.h"
#include "module.h"
#include "ops.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

void gen_trace(Obj *o)
{
    GenObj *g = static_cast<GenObj *>(o);
    gc_mark(g->frame);
    gc_mark(g->name);
    gc_mark(g->qualname);
    gc_mark(g->code);
    gc_mark(g->handling);
}

// "<coroutine object f at 0x...>", or without a name for the helper objects.
R addr_repr(Value v, Str kind, Value name, String &out)
{
    char tmp[24];
    Buf<128> b;
    b.put('<').put(kind).put(" object ");
    if (is_str(name))
        b.put(str_of(name)->str()).put(' ');
    b.put("at ").put(addr_text(tmp, sizeof tmp, v.obj())).put('>');
    return out.append(b.str()) ? R::Ok : oom();
}

R gen_repr(Value v, String &out)
{
    return addr_repr(v, type_of(v)->name, gen_of(v)->qualname, out);
}

// A generator is its own iterator. There is no `next` slot, because stepping
// one pushes a frame; ForIter and the drain in call.cpp use a GenRunObj.
Value self_iter(Value v)
{
    return v;
}

} // namespace

Value gen_awaiting(Value v)
{
    GenObj *g = gen_of(v);
    if (g->state != GEN_SUSPENDED || g->frame.is_nil())
        return value_none();
    FrameObj *f       = frame_of(g->frame);
    const CodeObj *co = code_of(f->code);
    if (f->pc >= co->code.size() || co->code[f->pc].op != Bc::YieldFrom || !f->sp)
        return value_none();
    return f->stack()[f->sp - 1];
}

namespace {

// The attributes all three share, under their own prefix: gi_, cr_ or ag_.
R gen_common(Value v, Str n, Str pre, Value &out)
{
    GenObj *g = gen_of(v);
    if (n == "__name__")
        out = g->name;
    else if (n == "__qualname__")
        out = g->qualname;
    else if (!n.starts_with(pre))
        return R::NotImpl;
    else if (n.substr(pre.size()) == "frame")
        out = g->frame.is_nil() ? value_none() : g->frame;
    else if (n.substr(pre.size()) == "code")
        out = g->code;
    else if (n.substr(pre.size()) == "suspended")
        out = value_bool(g->state == GEN_SUSPENDED);
    else if (n.substr(pre.size()) == "running")
        out = value_bool(is_agen(v) ? g->running : g->state == GEN_RUNNING);
    else if (n.substr(pre.size()) == (is_gen(v) ? Str("yieldfrom") : Str("await")))
        out = gen_awaiting(v);
    else if (n == "cr_origin" && is_coro(v))
        out = value_none();
    else
        return R::NotImpl;
    return R::Ok;
}

R bound(Value v, u8 how, Value &out)
{
    out = genrun_new(v, how);
    return out.is_nil() ? R::Err : R::Ok;
}

R gen_getattr(Value v, StrObj *name, Value &out)
{
    Str n = name->str();
    if (n == "__next__")
        return bound(v, GR_NEXT, out);
    if (n == "send")
        return bound(v, GR_SEND, out);
    if (n == "throw")
        return bound(v, GR_THROW, out);
    if (n == "close")
        return bound(v, GR_CLOSE, out);
    if (n == "__iter__")
        return bound(v, GR_ITER, out);
    return gen_common(v, n, "gi_", out);
}

// A coroutine is not an iterator: `__await__` is how a plain caller gets one.
R coro_getattr(Value v, StrObj *name, Value &out)
{
    Str n = name->str();
    if (n == "send")
        return bound(v, GR_SEND, out);
    if (n == "throw")
        return bound(v, GR_THROW, out);
    if (n == "close")
        return bound(v, GR_CLOSE, out);
    if (n == "__await__")
        return bound(v, GR_AWAIT, out);
    return gen_common(v, n, "cr_", out);
}

// The methods an async generator has are natives in its table; these are its
// data.
R agen_getattr(Value v, StrObj *name, Value &out)
{
    return gen_common(v, name->str(), "ag_", out);
}

void corowrap_trace(Obj *o)
{
    gc_mark(static_cast<CoroWrapObj *>(o)->coro);
}

R corowrap_repr(Value v, String &out)
{
    return addr_repr(v, "coroutine_wrapper", Value(), out);
}

// The iterator protocol, and send, throw and close, all by resuming. An
// awaitable is also its own __await__; a coroutine's wrapper is not.
R iter_getattr(Value v, StrObj *name, Value &out)
{
    Str n = name->str();
    if (n == "__next__")
        return bound(v, GR_NEXT, out);
    if (n == "send")
        return bound(v, GR_SEND, out);
    if (n == "throw")
        return bound(v, GR_THROW, out);
    if (n == "close")
        return bound(v, GR_CLOSE, out);
    if (n == "__iter__")
        return bound(v, GR_ITER, out);
    if (n == "__await__" && is_awaitobj(v))
        return bound(v, GR_AWAIT, out);
    return R::NotImpl;
}

void await_trace(Obj *o)
{
    AwaitObj *a = static_cast<AwaitObj *>(o);
    gc_mark(a->target);
    gc_mark(a->arg);
    gc_mark(a->dflt);
    gc_mark(a->iter);
}

R await_repr(Value v, String &out)
{
    return addr_repr(v, type_of(v)->name, Value(), out);
}

void genrun_trace(Obj *o)
{
    gc_mark(static_cast<GenRunObj *>(o)->gen);
}

Str genrun_name(u8 how)
{
    switch (how) {
    case GR_NEXT:
        return "__next__";
    case GR_SEND:
        return "send";
    case GR_THROW:
        return "throw";
    case GR_CLOSE:
        return "close";
    case GR_AWAIT:
        return "__await__";
    default:
        return "__iter__";
    }
}

R genrun_repr(Value v, String &out)
{
    Buf<96> b;
    b.put("<method '").put(genrun_name(genrun_of(v)->how)).put("' of '");
    b.put(type_name(genrun_of(v)->gen)).put("' objects>");
    return out.append(b.str()) ? R::Ok : oom();
}

void wrapval_trace(Obj *o)
{
    gc_mark(static_cast<WrapValObj *>(o)->v);
}

// ------------------------------------------------- an async generator's methods

GenObj *self_agen(const CallArgs &a, Str who)
{
    if (a.nargs && is_agen(a.args[0]))
        return gen_of(a.args[0]);
    err_set2("TypeError", "descriptor requires an 'async_generator' object", who);
    return nullptr;
}

// `collections.abc.Awaitable` looks for __await__ in the type's namespace, so
// answering it from the getattr slot alone is not enough: inspect.isawaitable
// would say no and asyncio would refuse `await agen.aclose()`.
R a_await(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__await__", 0, 0))
        return R::Err;
    out = method_self(a.args[0]);
    return R::Ok;
}

constexpr Method AWAITABLE_METHODS[] = { { "__await__", a_await } };

R m_aiter(const CallArgs &a, Value &out)
{
    if (!self_agen(a, "__aiter__") || !meth_args(a, "__aiter__", 0, 0))
        return R::Err;
    out = a.args[0];
    return R::Ok;
}

// s[0] the generator, s[1] the hook, s[2] the awaitable to answer with.
R firstiter_step(ContObj *k, Value in)
{
    (void)in;
    if (k->i++ == 0)
        return cont_call(k, k->s[1], k->s[0]);
    return cont_done(k, k->s[2]);
}

// PEP 525: the loop is told the first time an async generator is stepped, so
// that it can close the ones a program abandons. The hook is Python, so the
// awaitable is handed over behind a continuation that calls it first.
} // namespace

Value agen_firstiter(Value agen, Value awaitable)
{
    GenObj *g = gen_of(agen);
    if (g->hooks)
        return awaitable;
    g->hooks   = true;
    Value hook = sys_asyncgen_firstiter();
    if (hook.is_nil())
        return awaitable;
    Root ra{ agen }, rw{ awaitable }, rh{ hook };
    Root kv{ cont_new(firstiter_step) };
    if (kv.v.is_nil())
        return Value();
    cont_of(kv.v)->s[0] = ra.v;
    cont_of(kv.v)->s[1] = rh.v;
    cont_of(kv.v)->s[2] = rw.v;
    return kv.v;
}

namespace {

R m_anext(const CallArgs &a, Value &out)
{
    if (!self_agen(a, "__anext__") || !meth_args(a, "__anext__", 0, 0))
        return R::Err;
    Root self{ a.args[0] };
    out = await_new(self.v, AK_ASEND, value_none());
    if (out.is_nil())
        return R::Err;
    out = agen_firstiter(self.v, out);
    return out.is_nil() ? R::Err : R::Ok;
}

R m_asend(const CallArgs &a, Value &out)
{
    if (!self_agen(a, "asend") || !meth_args(a, "asend", 1, 1))
        return R::Err;
    Root self{ a.args[0] };
    out = await_new(self.v, AK_ASEND, a.args[1]);
    if (out.is_nil())
        return R::Err;
    out = agen_firstiter(self.v, out);
    return out.is_nil() ? R::Err : R::Ok;
}

// The arguments are kept as a tuple and made an exception when thrown, which
// is when CPython checks them too.
R m_athrow(const CallArgs &a, Value &out)
{
    if (!self_agen(a, "athrow") || !meth_args(a, "athrow", 1, 3))
        return R::Err;
    TupleObj *t = tuple_new(a.nargs - 1);
    if (!t)
        return oom();
    for (u32 i = 1; i < a.nargs; i++)
        t->items()[i - 1] = a.args[i];
    Root rt{ obj_value(t) };
    out = await_new(a.args[0], AK_ATHROW, rt.v);
    return out.is_nil() ? R::Err : R::Ok;
}

R m_aclose(const CallArgs &a, Value &out)
{
    if (!self_agen(a, "aclose") || !meth_args(a, "aclose", 0, 0))
        return R::Err;
    out = await_new(a.args[0], AK_ACLOSE, Value());
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr Method AGEN_METHODS[] = {
    { "__aiter__", m_aiter }, { "__anext__", m_anext }, { "asend", m_asend },
    { "athrow", m_athrow },   { "aclose", m_aclose },
};

} // namespace

constexpr Type gen_type{ .name    = "generator",
                         .trace   = gen_trace,
                         .repr    = gen_repr,
                         .iter    = self_iter,
                         .getattr = gen_getattr };

constexpr Type coro_type{ .name    = "coroutine",
                          .trace   = gen_trace,
                          .repr    = gen_repr,
                          .getattr = coro_getattr };

constexpr Type agen_type{ .name    = "async_generator",
                          .trace   = gen_trace,
                          .repr    = gen_repr,
                          .getattr = agen_getattr };

constexpr Type corowrap_type{ .name    = "coroutine_wrapper",
                              .trace   = corowrap_trace,
                              .repr    = corowrap_repr,
                              .iter    = self_iter,
                              .getattr = iter_getattr };

constexpr Type asend_type{ .name    = "async_generator_asend",
                           .trace   = await_trace,
                           .repr    = await_repr,
                           .iter    = self_iter,
                           .getattr = iter_getattr };

constexpr Type athrow_type{ .name    = "async_generator_athrow",
                            .trace   = await_trace,
                            .repr    = await_repr,
                            .iter    = self_iter,
                            .getattr = iter_getattr };

constexpr Type anext_type{ .name    = "anext_awaitable",
                           .trace   = await_trace,
                           .repr    = await_repr,
                           .iter    = self_iter,
                           .getattr = iter_getattr };

constexpr Type genrun_type{ .name = "method", .trace = genrun_trace, .repr = genrun_repr };

constexpr Type wrapval_type{ .name = "async_generator_wrapped_value", .trace = wrapval_trace };

Value gen_new(Value frame)
{
    Root rf{ frame };
    const CodeObj *co = code_of(frame_of(rf.v)->code);
    const Type *t     = (co->flags & CO_COROUTINE)         ? &coro_type
                        : (co->flags & CO_ASYNC_GENERATOR) ? &agen_type
                                                           : &gen_type;
    GenObj *g         = static_cast<GenObj *>(obj_alloc(t, sizeof(GenObj)));
    if (!g)
        return oom(), Value();
    co          = code_of(frame_of(rf.v)->code);
    g->frame    = rf.v;
    g->name     = co->name;
    g->qualname = co->qualname;
    g->code     = frame_of(rf.v)->code;
    g->handling = Value();
    g->state    = GEN_CREATED;
    g->running  = false;
    g->closed   = false;
    g->hooks    = false;
    // A generator dropped at a yield owes its `finally` a run, and that is
    // what a finalizer is: the collector owes it one close.
    g->flags |= OBJ_FINAL;
    return obj_value(g);
}

Str gen_kind(Value v)
{
    return is_coro(v) ? Str("coroutine") : is_agen(v) ? Str("async generator") : Str("generator");
}

bool gen_awaitable(Value v)
{
    return is_gen(v) && (code_of(gen_of(v)->code)->flags & CO_ITERABLE_COROUTINE);
}

Value genrun_new(Value gen, u8 how)
{
    Root rg{ gen };
    GenRunObj *m = static_cast<GenRunObj *>(obj_alloc(&genrun_type, sizeof(GenRunObj)));
    if (!m)
        return oom(), Value();
    m->gen = rg.v;
    m->how = how;
    return obj_value(m);
}

Value resumer(Value v, u8 how)
{
    if (!is_resumable(v) && !is_coro(v) && !is_agen(v))
        return Value();
    return genrun_new(v, how);
}

Value await_new(Value target, u8 kind, Value arg)
{
    Root rt{ target }, ra{ arg };
    const Type *t = kind == AK_ASEND     ? &asend_type
                    : kind == AK_DEFAULT ? &anext_type
                                         : &athrow_type;
    AwaitObj *a   = static_cast<AwaitObj *>(obj_alloc(t, sizeof(AwaitObj)));
    if (!a)
        return oom(), Value();
    a->target = rt.v;
    a->arg    = ra.v;
    a->dflt   = Value();
    a->iter   = Value();
    a->kind   = kind;
    a->state  = AS_INIT;
    return obj_value(a);
}

Value anext_default(Value awaitable, Value dflt)
{
    Root rd{ dflt };
    Value v = await_new(awaitable, AK_DEFAULT, Value());
    if (!v.is_nil())
        await_of(v)->dflt = rd.v;
    return v;
}

Value corowrap_new(Value coro)
{
    Root rc{ coro };
    CoroWrapObj *w = static_cast<CoroWrapObj *>(obj_alloc(&corowrap_type, sizeof(CoroWrapObj)));
    if (!w)
        return oom(), Value();
    w->coro = rc.v;
    return obj_value(w);
}

Value wrapval_new(Value v)
{
    Root rv{ v };
    WrapValObj *w = static_cast<WrapValObj *>(obj_alloc(&wrapval_type, sizeof(WrapValObj)));
    if (!w)
        return oom(), Value();
    w->v = rv.v;
    return obj_value(w);
}

bool gen_methods()
{
    return method_install(&agen_type, AGEN_METHODS) &&
           method_install(&asend_type, AWAITABLE_METHODS) &&
           method_install(&athrow_type, AWAITABLE_METHODS) &&
           method_install(&anext_type, AWAITABLE_METHODS);
}
