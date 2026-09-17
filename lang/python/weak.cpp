// Weak references, and the callbacks the collector owes for them.
//
// The point of a weak reference is a pointer the collector does not follow, so
// `target` is a raw Obj* and weak_trace does not mark it. What makes that safe
// is the sweep hook: between marking and sweeping, everything is still whole,
// and a reference whose target is about to go is cleared there -- before any
// finalizer can see a dangling one, which is the order CPython keeps too.
//
// This is `_weakref`, the floor CPython's weakref.py stands on; the library
// module itself waits for the phase that borrows the library.
#include "weak.h"

#include "builtin.h"
#include "call.h"
#include "gc.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "type.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

using WeakObj = WeakRefObj;

// Every live reference, so the sweep can find the ones whose target is going.
// A Vec cannot be a namespace-scope object here; built on first use.
Vec<WeakObj *> *all;

WeakObj *weak_of(Value v)
{
    return static_cast<WeakObj *>(v.obj());
}

void weak_trace(Obj *o)
{
    gc_mark(static_cast<WeakObj *>(o)->callback);
}

R weak_repr(Value v, String &out)
{
    WeakObj *w = weak_of(v);
    Buf<96> b;
    b.put("<weakref at ");
    char tmp[24];
    b.put(addr_text(tmp, sizeof tmp, v.obj()));
    b.put("; ").put(w->target ? Str("to an object") : Str("dead")).put('>');
    return out.append(b.str()) ? R::Ok : oom();
}

R weak_hash(Value v, u32 &out)
{
    out = weak_of(v)->hash;
    return R::Ok;
}

// Two references are equal while both are alive and name the same object;
// once either is dead only identity is left, which is what CPython says.
R weak_eq(Value a, Value b, bool &out)
{
    if (!is_weakref(b))
        return R::NotImpl;
    WeakObj *x = weak_of(a), *y = weak_of(b);
    out = x->target && y->target ? x->target == y->target : a == b;
    return R::Ok;
}

// The reference is called to get the object back: ref() is None once it has
// gone, which is the whole interface.
R weak_call(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__call__", 0, 0))
        return R::Err;
    Obj *t = weak_of(a.args[0])->target;
    out    = t ? Value::of_obj(t) : value_none();
    return R::Ok;
}

constexpr Method WEAK_METHODS[] = { { "__call__", weak_call } };

// A weak reference cannot point at just anything: the target has to be an
// object the collector owns and nothing else has a stake in.
bool referenceable(Value v)
{
    return is_inst(v) || is_type(v) || is_func(v);
}

// ref(o) and proxy(o) with no callback are one object per target, as in
// CPython; one with a callback is always new.
R make_weak(const CallArgs &a, Str who, const Type *t, Value &out)
{
    if (!args_only(a, who, 1, 2))
        return R::Err;
    if (!referenceable(a.args[0])) {
        Buf<128> m;
        m.put("cannot create weak reference to '").put(type_name(a.args[0])).put("' object");
        return err_set("TypeError", m.str());
    }
    if (!all) {
        all = heap_new<Vec<WeakObj *>>();
        if (!all)
            return oom();
    }
    Root rt{ a.args[0] }, rc{ a.nargs > 1 && !is_none(a.args[1]) ? a.args[1] : Value() };
    if (rc.v.is_nil())
        for (usize i = 0; i < all->size(); i++) {
            WeakObj *w = (*all)[i];
            if (w->type == t && w->target == rt.v.obj() && w->callback.is_nil()) {
                out = obj_value(w);
                return R::Ok;
            }
        }
    WeakObj *w = static_cast<WeakObj *>(obj_alloc(t, sizeof(WeakObj)));
    if (!w)
        return oom();
    w->target   = rt.v.obj();
    w->callback = rc.v;
    w->hash     = u32(usize(rt.v.obj())) >> 4;
    Root rw{ obj_value(w) };
    if (!all->push(weak_of(rw.v)))
        return oom();
    out = rw.v;
    return R::Ok;
}

R b_ref(const CallArgs &a, Value &out)
{
    return make_weak(a, "ref", &weakref_type, out);
}

R b_proxy(const CallArgs &a, Value &out)
{
    bool callable = a.nargs && py_callable(a.args[0]);
    return make_weak(a, "proxy", callable ? &callable_proxy_type : &proxy_type, out);
}

// ------------------------------------------------------------- the proxies

// Everything a proxy is asked is asked of its referent. What reaches Python
// is unwrapped where the lookup is made -- attr.cpp and type_special -- and
// what stays native is forwarded here.
R proxy_repr(Value v, String &out)
{
    WeakObj *w = weak_of(v);
    char tmp[24];
    Buf<160> b;
    b.put("<weakproxy at ").put(addr_text(tmp, sizeof tmp, v.obj()));
    if (!w->target) {
        b.put("; dead>");
    } else {
        b.put("; to '").put(type_name(Value::of_obj(w->target))).put("' at ");
        b.put(addr_text(tmp, sizeof tmp, w->target)).put('>');
    }
    return out.append(b.str()) ? R::Ok : oom();
}

R proxy_str(Value v, String &out)
{
    Value t = proxy_target(v);
    return t.is_nil() ? R::Err : py_str(t, out);
}

R proxy_hash(Value v, u32 &)
{
    return err_unhashable(v);
}

bool proxy_truth(Value v)
{
    Value t = proxy_target(v);
    return !t.is_nil() && py_truth(t);
}

R proxy_eq(Value a, Value b, bool &out)
{
    Root x{ proxy_target(a) }, y{ unproxy(b) };
    if (x.v.is_nil() || y.v.is_nil())
        return R::Err;
    return py_eq(x.v, y.v, out);
}

R proxy_order(Value a, Value b, Cmp op, bool &out)
{
    Root x{ proxy_target(a) }, y{ unproxy(b) };
    if (x.v.is_nil() || y.v.is_nil())
        return R::Err;
    return py_cmp(x.v, y.v, op, out);
}

R proxy_len(Value v, usize &out)
{
    Value t = proxy_target(v);
    return t.is_nil() ? R::Err : py_len(t, out);
}

R proxy_getitem(Value v, Value key, Value &out)
{
    Root k{ key }, t{ proxy_target(v) };
    return t.v.is_nil() ? R::Err : py_getitem(t.v, k.v, out);
}

R proxy_setitem(Value v, Value key, Value x)
{
    Root k{ key }, rx{ x }, t{ proxy_target(v) };
    return t.v.is_nil() ? R::Err : py_setitem(t.v, k.v, rx.v);
}

R proxy_delitem(Value v, Value key)
{
    Root k{ key }, t{ proxy_target(v) };
    return t.v.is_nil() ? R::Err : py_delitem(t.v, k.v);
}

R proxy_contains(Value v, Value item, bool &out)
{
    Root i{ item }, t{ proxy_target(v) };
    return t.v.is_nil() ? R::Err : py_contains(t.v, i.v, out);
}

R proxy_binop(Value a, Value b, Op op, Value &out)
{
    Root x{ unproxy(a) }, y{ unproxy(b) };
    if (x.v.is_nil() || y.v.is_nil())
        return R::Err;
    return py_binop(x.v, y.v, op, out);
}

Value proxy_iter(Value v)
{
    Value t = proxy_target(v);
    return t.is_nil() ? Value() : py_iter(t);
}

R proxy_next(Value v, Value &out)
{
    Value t = proxy_target(v);
    return t.is_nil() ? R::Err : py_next(t, out);
}

R b_getweakrefcount(const CallArgs &a, Value &out)
{
    if (!args_only(a, "getweakrefcount", 1, 1))
        return R::Err;
    i64 n = 0;
    if (all && a.args[0].is_obj())
        for (usize i = 0; i < all->size(); i++)
            n += (*all)[i]->target == a.args[0].obj();
    out = Value::of_int(i32(n));
    return R::Ok;
}

// _remove_dead_weakref(dict, key): drop the entry if its weakref is dead. A key
// already gone is not an error: WeakValueDictionary races its own callbacks.
R b_remove_dead_weakref(const CallArgs &a, Value &out)
{
    if (!args_only(a, "_remove_dead_weakref", 2, 2))
        return R::Err;
    if (!is_dict(a.args[0]))
        return err_set2("TypeError", "_remove_dead_weakref() argument 1 must be dict",
                        type_name(a.args[0]));
    DictObj *d = static_cast<DictObj *>(a.args[0].obj());
    Value got;
    R r = dict_get(d, a.args[1], got);
    if (r == R::Err)
        return R::Err;
    if (r == R::Ok) {
        if (!is_weakref(got))
            return err_set("TypeError", "not a weakref");
        if (!weak_of(got)->target && dict_del(d, a.args[1]) == R::Err)
            return R::Err;
    }
    out = value_none();
    return R::Ok;
}

R b_getweakrefs(const CallArgs &a, Value &out)
{
    if (!args_only(a, "getweakrefs", 1, 1))
        return R::Err;
    ListObj *l = list_new();
    if (!l)
        return oom();
    Root rl{ obj_value(l) };
    if (all && a.args[0].is_obj())
        for (usize i = 0; i < all->size(); i++)
            if ((*all)[i]->target == a.args[0].obj() &&
                !list_push(list_of(rl.v), obj_value((*all)[i])))
                return oom();
    out = rl.v;
    return R::Ok;
}

// Between marking and sweeping: what is about to go is still whole, so a
// reference to it is cleared here and its callback owed. The registry loses
// the references that are themselves going, which is the only tidying it needs.
void weak_sweep()
{
    if (!all)
        return;
    usize keep = 0;
    for (usize i = 0; i < all->size(); i++) {
        WeakObj *w = (*all)[i];
        if (gc_is_dying(w))
            continue;
        if (w->target && gc_is_dying(w->target)) {
            w->target = nullptr;
            if (!w->callback.is_nil()) {
                gc_defer(obj_value(w), w->callback);
                w->callback = Value();
            }
        }
        (*all)[keep++] = w;
    }
    while (all->size() > keep)
        all->pop();
}

} // namespace

constexpr Type weakref_type{ .name  = "weakref",
                             .trace = weak_trace,
                             .hash  = weak_hash,
                             .eq    = weak_eq,
                             .repr  = weak_repr };

#define PROXY_SLOTS                                                                \
    .trace = weak_trace, .truth = proxy_truth, .hash = proxy_hash, .eq = proxy_eq, \
    .order = proxy_order, .repr = proxy_repr, .str = proxy_str, .len = proxy_len,  \
    .getitem = proxy_getitem, .setitem = proxy_setitem, .delitem = proxy_delitem,  \
    .contains = proxy_contains, .binop = proxy_binop, .iter = proxy_iter, .next = proxy_next

constexpr Type proxy_type{ .name = "weakref.ProxyType", PROXY_SLOTS };
constexpr Type callable_proxy_type{ .name = "weakref.CallableProxyType", PROXY_SLOTS };

#undef PROXY_SLOTS

Value proxy_target(Value v)
{
    Obj *t = static_cast<WeakObj *>(v.obj())->target;
    if (!t)
        return err_set("ReferenceError", "weakly-referenced object no longer exists"), Value();
    return Value::of_obj(t);
}

bool weak_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    gc_sweep_hook(weak_sweep);
    if (!method_install(&weakref_type, WEAK_METHODS))
        return false;
    struct Named {
        Str name;
        R (*fn)(const CallArgs &, Value &out);
    };
    static constexpr Named NAMES[] = {
        { "ref", b_ref },
        { "proxy", b_proxy },
        { "getweakrefcount", b_getweakrefcount },
        { "getweakrefs", b_getweakrefs },
        { "_remove_dead_weakref", b_remove_dead_weakref },
    };
    for (const Named &e : NAMES) {
        Root fn{ native_new(e.name, e.fn) };
        StrObj *n = str_intern(e.name);
        if (fn.v.is_nil() || !n ||
            dict_set(static_cast<DictObj *>(rd.v.obj()), obj_value(n), fn.v) != R::Ok)
            return false;
    }
    Root t{ type_wrap(&weakref_type) };
    StrObj *n = str_intern("ReferenceType");
    if (t.v.is_nil() || !n ||
        dict_set(static_cast<DictObj *>(rd.v.obj()), obj_value(n), t.v) != R::Ok)
        return false;
    return mod_type(static_cast<DictObj *>(rd.v.obj()), &proxy_type) &&
           mod_type(static_cast<DictObj *>(rd.v.obj()), &callable_proxy_type);
}
