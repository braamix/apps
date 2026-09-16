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

#include "call.h"
#include "gc.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "method.h"
#include "type.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

struct WeakObj : Obj {
    Obj *target;    // raw on purpose: the collector must not follow it
    Value callback; // called with this reference once the target has gone
    u32 hash;       // the target's identity, kept so a dead ref still hashes
};

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
    b.put("<weakref at 0x");
    char tmp[24];
    b.put(int_text(tmp, sizeof tmp, i64(usize(v.obj()))));
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

R b_ref(const CallArgs &a, Value &out)
{
    if (!args_only(a, "ref", 1, 2))
        return R::Err;
    if (!referenceable(a.args[0]))
        return err_set2("TypeError", "cannot create weak reference to", type_name(a.args[0]));
    if (!all) {
        all = heap_new<Vec<WeakObj *>>();
        if (!all)
            return oom();
    }
    Root rt{ a.args[0] }, rc{ a.nargs > 1 && !is_none(a.args[1]) ? a.args[1] : Value() };
    WeakObj *w = static_cast<WeakObj *>(obj_alloc(&weakref_type, sizeof(WeakObj)));
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
        { "getweakrefcount", b_getweakrefcount },
        { "getweakrefs", b_getweakrefs },
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
    return !t.v.is_nil() && n &&
           dict_set(static_cast<DictObj *>(rd.v.obj()), obj_value(n), t.v) == R::Ok;
}
