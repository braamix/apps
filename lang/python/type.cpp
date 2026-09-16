// Type objects, instances, the MRO and the descriptors.
#include "type.h"

#include "call.h"
#include "complex.h"
#include "exc.h"
#include "frame.h"
#include "gc.h"
#include "intern.h"
#include "iter.h"
#include "kernel/fmt.h"
#include "method.h"
#include "ops.h"
#include "vm.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// Every TypeObj made for a static descriptor, so `int` is one object.
struct Home {
    Vec<Value> wraps;
    Value object; // the base of every MRO
};

Home *home;

void home_mark()
{
    if (!home)
        return;
    for (usize i = 0; i < home->wraps.size(); i++)
        gc_mark(home->wraps[i]);
    gc_mark(home->object);
}

Home *here()
{
    if (!home) {
        home = heap_new<Home>();
        if (home)
            gc_root_hook(home_mark);
    }
    return home;
}

DictObj *dict_at(Value v)
{
    return static_cast<DictObj *>(v.obj());
}

// ------------------------------------------------------------------ tracing

void type_trace(Obj *o)
{
    TypeObj *t = static_cast<TypeObj *>(o);
    gc_mark(t->name);
    gc_mark(t->dict);
    gc_mark(t->bases);
    gc_mark(t->mro);
    gc_mark(t->native);
}

R type_repr(Value v, String &out)
{
    Buf<96> b;
    b.put("<class '").put(type_obj(v)->slots.name).put("'>");
    return out.append(b.str()) ? R::Ok : oom();
}

R type_getattr(Value v, StrObj *name, Value &out)
{
    Str n = name->str();
    if (n == "__name__") {
        out = type_obj(v)->name;
        return R::Ok;
    }
    if (n == "__bases__") {
        out = type_obj(v)->bases;
        return out.is_nil() ? R::NotImpl : R::Ok;
    }
    if (n == "__dict__") {
        out = type_obj(v)->dict;
        return out.is_nil() ? R::NotImpl : R::Ok;
    }
    Root found, owner;
    R r = type_lookup(v, name, found.v, &owner.v);
    if (r != R::Ok)
        return r;
    // Reached through the class: nothing to bind a function to.
    return type_bind(found.v, Value(), v, out);
}

void inst_trace(Obj *o)
{
    InstObj *i = static_cast<InstObj *>(o);
    gc_mark(i->cls);
    gc_mark(i->dict);
    gc_mark(i->native);
}

R inst_repr(Value v, String &out)
{
    char tmp[24];
    Buf<96> b;
    b.put("<").put(type_name(v)).put(" object at 0x");
    b.put(int_text(tmp, sizeof tmp, i64(usize(v.obj()))));
    b.put('>');
    return out.append(b.str()) ? R::Ok : oom();
}

void method_trace(Obj *o)
{
    MethodObj *m = static_cast<MethodObj *>(o);
    gc_mark(m->fn);
    gc_mark(m->self);
}

// A bound method answers for what it is bound to: `A().f.__name__` is `f`.
R method_getattr(Value v, StrObj *name, Value &out)
{
    Str n = name->str();
    if (n != "__name__" && n != "__qualname__" && n != "__doc__" && n != "__module__" &&
        n != "__func__" && n != "__self__")
        return R::NotImpl;
    MethodObj *m = static_cast<MethodObj *>(v.obj());
    if (n == "__func__")
        return out = m->fn, R::Ok;
    if (n == "__self__")
        return out = m->self, R::Ok;
    return py_getattr(m->fn, name, out) == R::Ok ? R::Ok : (err_clear(), R::NotImpl);
}

R method_repr(Value v, String &out)
{
    MethodObj *m = static_cast<MethodObj *>(v.obj());
    if (!out.append("<bound method "))
        return oom();
    if (py_repr(m->fn, out) != R::Ok)
        return R::Err;
    return out.push('>') ? R::Ok : oom();
}

void prop_trace(Obj *o)
{
    PropObj *p = static_cast<PropObj *>(o);
    gc_mark(p->get);
    gc_mark(p->set);
    gc_mark(p->del);
}

void wrap_trace(Obj *o)
{
    gc_mark(static_cast<WrapObj *>(o)->fn);
}

void super_trace(Obj *o)
{
    SuperObj *s = static_cast<SuperObj *>(o);
    gc_mark(s->cls);
    gc_mark(s->self);
}

R prop_getattr(Value v, StrObj *name, Value &out);

R plain_repr(Value v, String &out)
{
    Buf<64> b;
    b.put('<').put(type_name(v)).put(" object>");
    return out.append(b.str()) ? R::Ok : oom();
}

R super_repr(Value v, String &out)
{
    SuperObj *s = static_cast<SuperObj *>(v.obj());
    Buf<96> b;
    b.put("<super: <class '").put(type_obj(s->cls)->slots.name).put("'>, ");
    b.put(s->self.is_nil() ? Str("NULL") : Str("<...>")).put('>');
    return out.append(b.str()) ? R::Ok : oom();
}

// ---------------------------------------------------------------------- mro

usize tuple_len(Value v)
{
    return is_tuple(v) ? static_cast<TupleObj *>(v.obj())->len : 0;
}

Value tuple_at(Value v, usize i)
{
    return static_cast<TupleObj *>(v.obj())->items()[i];
}

bool holds(const Vec<Value> &v, Value x)
{
    for (usize i = 0; i < v.size(); i++)
        if (v[i] == x)
            return true;
    return false;
}

// In the tail of some list still to be taken from: C3's "not in any tail".
bool in_a_tail(const Vec<Vec<Value>> &lists, const Vec<usize> &at, Value x)
{
    for (usize i = 0; i < lists.size(); i++)
        for (usize k = at[i] + 1; k < lists[i].size(); k++)
            if (lists[i][k] == x)
                return true;
    return false;
}

// C3 over the bases' own linearizations and the base list itself.
Value mro_of(Value cls, Value bases)
{
    Vec<Vec<Value>> lists;
    Vec<usize> at;
    for (usize i = 0; i < tuple_len(bases); i++) {
        Vec<Value> one;
        Value m = type_obj(tuple_at(bases, i))->mro;
        for (usize k = 0; k < tuple_len(m); k++)
            if (!one.push(tuple_at(m, k)))
                return oom(), Value();
        if (!lists.push(static_cast<Vec<Value> &&>(one)) || !at.push(0))
            return oom(), Value();
    }
    {
        Vec<Value> tail;
        for (usize i = 0; i < tuple_len(bases); i++)
            if (!tail.push(tuple_at(bases, i)))
                return oom(), Value();
        if (tail.size() && (!lists.push(static_cast<Vec<Value> &&>(tail)) || !at.push(0)))
            return oom(), Value();
    }

    Vec<Value> out;
    if (!out.push(cls))
        return oom(), Value();
    for (;;) {
        bool done = true;
        for (usize i = 0; i < lists.size(); i++)
            if (at[i] < lists[i].size())
                done = false;
        if (done)
            break;

        Value head;
        for (usize i = 0; i < lists.size() && head.is_nil(); i++) {
            if (at[i] >= lists[i].size())
                continue;
            Value c = lists[i][at[i]];
            if (!in_a_tail(lists, at, c))
                head = c;
        }
        if (head.is_nil())
            return err_set("TypeError", "cannot create a consistent method resolution order"),
                   Value();
        if (!holds(out, head) && !out.push(head))
            return oom(), Value();
        for (usize i = 0; i < lists.size(); i++)
            if (at[i] < lists[i].size() && lists[i][at[i]] == head)
                at[i]++;
    }

    TupleObj *t = tuple_new(out.size());
    if (!t)
        return oom(), Value();
    for (usize i = 0; i < out.size(); i++)
        t->items()[i] = out[i];
    return obj_value(t);
}

// ---------------------------------------------------------- native delegates

Value delegate(Value v)
{
    return is_inst(v) ? inst_of(v)->native : v;
}

bool dg_truth(Value v)
{
    return py_truth(delegate(v));
}

R dg_hash(Value v, u32 &out)
{
    return py_hash(delegate(v), out);
}

R dg_eq(Value a, Value b, bool &out)
{
    return py_eq(delegate(a), delegate(b), out) == R::Ok ? R::Ok : R::Err;
}

R dg_order(Value a, Value b, Cmp op, bool &out)
{
    return py_cmp(delegate(a), delegate(b), op, out);
}

R dg_repr(Value v, String &out)
{
    return py_repr(delegate(v), out);
}

R dg_str(Value v, String &out)
{
    return py_str(delegate(v), out);
}

R dg_len(Value v, usize &out)
{
    return py_len(delegate(v), out);
}

R dg_getitem(Value v, Value k, Value &out)
{
    return py_getitem(delegate(v), k, out);
}

R dg_setitem(Value v, Value k, Value x)
{
    return py_setitem(delegate(v), k, x);
}

R dg_delitem(Value v, Value k)
{
    return py_delitem(delegate(v), k);
}

R dg_contains(Value v, Value x, bool &out)
{
    return py_contains(delegate(v), x, out);
}

R dg_binop(Value a, Value b, Op op, Value &out)
{
    return py_binop(delegate(a), delegate(b), op, out);
}

Value dg_iter(Value v)
{
    return py_iter(delegate(v));
}

R dg_next(Value v, Value &out)
{
    return py_next(delegate(v), out);
}

R dg_getattr(Value v, StrObj *name, Value &out)
{
    return py_getattr(delegate(v), name, out);
}

// A class gets the slots its native base answers, redirected at the delegate.
// Only what the built-in has: an absent slot stays absent, so the generic
// operation still says what it says.
void copy_slots(Type &s, const Type *n)
{
    if (n->truth)
        s.truth = dg_truth;
    if (n->hash)
        s.hash = dg_hash;
    if (n->eq)
        s.eq = dg_eq;
    if (n->order)
        s.order = dg_order;
    if (n->repr)
        s.repr = dg_repr;
    if (n->str)
        s.str = dg_str;
    if (n->len)
        s.len = dg_len;
    if (n->getitem)
        s.getitem = dg_getitem;
    if (n->setitem)
        s.setitem = dg_setitem;
    if (n->delitem)
        s.delitem = dg_delitem;
    if (n->contains)
        s.contains = dg_contains;
    if (n->binop)
        s.binop = dg_binop;
    if (n->iter)
        s.iter = dg_iter;
    if (n->next)
        s.next = dg_next;
    if (n->getattr)
        s.getattr = dg_getattr;
}

TypeObj *type_alloc()
{
    TypeObj *t = static_cast<TypeObj *>(obj_alloc(&type_type, sizeof(TypeObj)));
    if (!t)
        return oom(), nullptr;
    t->slots  = Type{};
    t->name   = Value();
    t->dict   = Value();
    t->bases  = Value();
    t->mro    = Value();
    t->native = Value();
    t->desc   = nullptr;
    t->exc    = nullptr;
    t->heap   = false;
    return t;
}

} // namespace

constexpr Type type_type{ .name    = "type",
                          .trace   = type_trace,
                          .repr    = type_repr,
                          .getattr = type_getattr };

constexpr Type method_type{ .name    = "method",
                            .trace   = method_trace,
                            .repr    = method_repr,
                            .getattr = method_getattr };

constexpr Type property_type{ .name    = "property",
                              .trace   = prop_trace,
                              .repr    = plain_repr,
                              .getattr = prop_getattr };

constexpr Type staticmethod_type{ .name = "staticmethod", .trace = wrap_trace, .repr = plain_repr };

constexpr Type classmethod_type{ .name = "classmethod", .trace = wrap_trace, .repr = plain_repr };

constexpr Type super_type{ .name    = "super",
                           .trace   = super_trace,
                           .repr    = super_repr,
                           .getattr = super_getattr };

// The descriptor `object`'s TypeObj wraps; instances of a plain class point at
// a copy of it inside their own type.
constexpr Type object_type{ .name = "object", .trace = inst_trace, .repr = inst_repr };

Value method_new(Value fn, Value self)
{
    Root rf{ fn }, rs{ self };
    MethodObj *m = static_cast<MethodObj *>(obj_alloc(&method_type, sizeof(MethodObj)));
    if (!m)
        return oom(), Value();
    m->fn   = rf.v;
    m->self = rs.v;
    return obj_value(m);
}

Value type_wrap(const Type *t)
{
    Home *h = here();
    if (!h)
        return oom(), Value();
    for (usize i = 0; i < h->wraps.size(); i++)
        if (type_obj(h->wraps[i])->desc == t)
            return h->wraps[i];

    Root name{ str_new(t->name) };
    if (name.v.is_nil())
        return Value();
    DictObj *d = dict_new();
    if (!d)
        return oom(), Value();
    Root rd{ obj_value(d) };
    TypeObj *o = type_alloc();
    if (!o)
        return Value();
    Root ro{ obj_value(o) };
    o->slots = *t;
    o->name  = name.v;
    o->dict  = rd.v;
    o->desc  = t;
    // An instance made from the wrapper itself -- object() -- points at this
    // copy, so it has to lead back here.
    o->slots.owner = o;

    TupleObj *bases = tuple_new(t == &object_type ? 0 : 1);
    if (!bases)
        return oom(), Value();
    Root rb{ obj_value(bases) };
    if (t != &object_type) {
        // bool is an int here the way it is in CPython; everything else sits
        // straight under object.
        Value ob = t == &bool_type ? type_wrap(&int_type) : type_object();
        if (ob.is_nil())
            return Value();
        static_cast<TupleObj *>(rb.v.obj())->items()[0] = ob;
    }
    type_obj(ro.v)->bases = rb.v;
    Value m               = mro_of(ro.v, rb.v);
    if (m.is_nil())
        return Value();
    type_obj(ro.v)->mro = m;
    if (!h->wraps.push(ro.v))
        return oom(), Value();
    return ro.v;
}

Value type_object()
{
    Home *h = here();
    if (!h)
        return oom(), Value();
    if (h->object.is_nil())
        h->object = type_wrap(&object_type);
    return h->object;
}

Value type_of_value(Value v)
{
    const Type *t = type_of(v);
    if (!t)
        return err_set("SystemError", "no type"), Value();
    if (t->owner)
        return obj_value(t->owner);
    return type_wrap(t);
}

R type_lookup(Value t, StrObj *name, Value &out, Value *owner)
{
    Value m = type_obj(t)->mro;
    for (usize i = 0; i < tuple_len(m); i++) {
        Value c = tuple_at(m, i);
        R r     = dict_get(dict_at(type_obj(c)->dict), obj_value(name), out);
        if (r == R::Err)
            return R::Err;
        if (r == R::Ok) {
            if (owner)
                *owner = c;
            return R::Ok;
        }
    }
    return R::NotImpl;
}

bool type_issub(Value t, Value base)
{
    if (!is_type(t) || !is_type(base))
        return false;
    Value m = type_obj(t)->mro;
    for (usize i = 0; i < tuple_len(m); i++)
        if (tuple_at(m, i) == base)
            return true;
    return false;
}

bool type_isinstance(Value v, Value t)
{
    Value vt = type_of_value(v);
    if (vt.is_nil())
        return err_clear(), false;
    if (vt == t)
        return true;
    // An int is a bool's base here the way it is in CPython.
    return type_issub(vt, t);
}

Value inst_new(Value cls)
{
    return obj_value(type_alloc_inst(cls, sizeof(InstObj)));
}

Value type_new(Value name, Value bases, Value dict)
{
    Root rn{ name }, rb{ bases }, rd{ dict };
    if (rb.v.is_nil() || !tuple_len(rb.v)) {
        Value ob = type_object();
        if (ob.is_nil())
            return Value();
        TupleObj *t = tuple_new(1);
        if (!t)
            return oom(), Value();
        t->items()[0] = ob;
        rb            = obj_value(t);
    }
    for (usize i = 0; i < tuple_len(rb.v); i++)
        if (!is_type(tuple_at(rb.v, i)))
            return err_set2("TypeError", "a base is not a class", type_name(tuple_at(rb.v, i))),
                   Value();

    TypeObj *t = type_alloc();
    if (!t)
        return Value();
    Root rt{ obj_value(t) };
    t->name        = rn.v;
    t->dict        = rd.v;
    t->bases       = rb.v;
    t->heap        = true;
    t->slots       = Type{};
    t->slots.name  = is_str(rn.v) ? str_of(rn.v)->str() : Str("?");
    t->slots.trace = inst_trace;
    t->slots.repr  = inst_repr;
    t->slots.owner = t;

    Value m = mro_of(rt.v, rb.v);
    if (m.is_nil())
        return Value();
    type_obj(rt.v)->mro = m;

    // The nearest built-in in the MRO decides the instance layout and lends
    // its slots; a plain class finds only `object` and keeps its own.
    for (usize i = 1; i < tuple_len(m); i++) {
        Value c = tuple_at(m, i);
        if (type_obj(c)->heap)
            continue;
        if (type_obj(c)->exc) {
            // An exception is not delegated to: the instance is one.
            type_obj(rt.v)->exc = type_obj(c)->exc;
            exc_slots(type_obj(rt.v)->slots);
            break;
        }
        const Type *n = type_obj(c)->desc;
        if (!n || n == &object_type)
            continue;
        type_obj(rt.v)->native = c;
        copy_slots(type_obj(rt.v)->slots, n);
        break;
    }
    return rt.v;
}

// ------------------------------------------------------------------ attributes

R type_bind(Value found, Value self, Value cls, Value &out)
{
    // A built-in written in C++ binds like a function: BaseException.__init__
    // reached through super() is one.
    if ((is_func(found) || is_native(found)) && !self.is_nil()) {
        out = method_new(found, self);
        return out.is_nil() ? R::Err : R::Ok;
    }
    const Type *t = found.is_obj() ? found.obj()->type : nullptr;
    if (t == &staticmethod_type) {
        out = static_cast<WrapObj *>(found.obj())->fn;
        return R::Ok;
    }
    if (t == &classmethod_type) {
        out = method_new(static_cast<WrapObj *>(found.obj())->fn, cls);
        return out.is_nil() ? R::Err : R::Ok;
    }
    out = found;
    return R::Ok;
}

Got py_attr(Value v, StrObj *name, Value &out)
{
    if (is_super(v)) {
        R r = super_getattr(v, name, out);
        return r == R::Ok ? Got::Ok : r == R::Err ? Got::Error : Got::Missing;
    }
    if (!is_inst(v)) {
        const Type *t = type_of(v);
        if (t && t->getattr) {
            R r = t->getattr(v, name, out);
            if (r == R::Ok)
                return Got::Ok;
            if (r == R::Err)
                return Got::Error;
        }
        // A method in the built-in type's own namespace: `"".split`.
        R m = method_find(v, name, out);
        if (m == R::Ok)
            return Got::Ok;
        if (m == R::Err)
            return Got::Error;
        out = Value();
        // A module may answer for itself (PEP 562). Its __getattr__ takes the
        // name and nothing else, so it is not bound.
        if (is_module(v)) {
            StrObj *ga = str_intern("__getattr__");
            Value fn;
            if (!ga)
                return oom(), Got::Error;
            if (dict_get(module_dict(v), obj_value(ga), fn) == R::Ok)
                out = fn;
        }
        return Got::Missing;
    }

    InstObj *o = inst_of(v);
    if (!o->dict.is_nil()) {
        R r = dict_get(dict_at(o->dict), obj_value(name), out);
        if (r == R::Err)
            return Got::Error;
        if (r == R::Ok)
            return Got::Ok;
    }
    Root found, owner;
    R r = type_lookup(o->cls, name, found.v, &owner.v);
    if (r == R::Err)
        return Got::Error;
    if (r == R::Ok) {
        // __new__ is implicitly a staticmethod.
        if (Str("__new__") == name->str()) {
            out = found.v;
            return Got::Ok;
        }
        if (is_property(found.v)) {
            PropObj *p = static_cast<PropObj *>(found.v.obj());
            if (p->get.is_nil()) {
                err_set2("AttributeError", "unreadable attribute", name->str());
                return Got::Error;
            }
            out = method_new(p->get, v);
            return out.is_nil() ? Got::Error : Got::Call;
        }
        if (type_bind(found.v, v, o->cls, out) != R::Ok)
            return Got::Error;
        return Got::Ok;
    }
    // What the class's own slots answer -- an exception's `args`, a native
    // base's attributes -- comes after the namespace and before __getattr__.
    const Type *t = type_of(v);
    if (t->getattr) {
        R g = t->getattr(v, name, out);
        if (g == R::Ok)
            return Got::Ok;
        if (g == R::Err)
            return Got::Error;
    }
    if (Str("__dict__") == name->str()) {
        if (o->dict.is_nil()) {
            DictObj *d = dict_new();
            if (!d)
                return oom(), Got::Error;
            o->dict = obj_value(d);
        }
        out = o->dict;
        return Got::Ok;
    }

    out        = Value();
    StrObj *ga = str_intern("__getattr__");
    if (ga && type_lookup(o->cls, ga, found.v) == R::Ok) {
        out = method_new(found.v, v);
        if (out.is_nil())
            return Got::Error;
    }
    return Got::Missing;
}

R inst_setattr(Value v, StrObj *name, Value val)
{
    if (is_type(v)) {
        if (!type_obj(v)->heap)
            return err_set2("TypeError", "cannot set an attribute on a built-in type",
                            type_obj(v)->slots.name);
        return dict_set(dict_at(type_obj(v)->dict), obj_value(name), val);
    }
    if (!is_inst(v)) {
        const Type *t = type_of(v);
        if (t && t->setattr)
            return t->setattr(v, name, val);
        return err_set2("AttributeError", "object has no attribute", name->str());
    }

    InstObj *o = inst_of(v);
    Root rv{ v }, rn{ obj_value(name) }, rx{ val };
    Value found;
    if (type_lookup(o->cls, name, found) == R::Ok && is_property(found))
        return err_set2("AttributeError", "this attribute needs the interpreter", name->str());
    if (o->dict.is_nil()) {
        DictObj *d = dict_new();
        if (!d)
            return oom();
        inst_of(rv.v)->dict = obj_value(d);
    }
    return dict_set(dict_at(inst_of(rv.v)->dict), rn.v, rx.v);
}

R inst_delattr(Value v, StrObj *name)
{
    if (is_type(v) && type_obj(v)->heap) {
        R r = dict_del(dict_at(type_obj(v)->dict), obj_value(name));
        return r == R::NotImpl ? err_set2("AttributeError", "no attribute", name->str()) : r;
    }
    if (!is_inst(v) || inst_of(v)->dict.is_nil())
        return err_set2("AttributeError", "object has no attribute", name->str());
    R r = dict_del(dict_at(inst_of(v)->dict), obj_value(name));
    return r == R::NotImpl ? err_set2("AttributeError", "no attribute", name->str()) : r;
}

R super_getattr(Value v, StrObj *name, Value &out)
{
    SuperObj *s = static_cast<SuperObj *>(v.obj());
    Value mro;
    if (is_type(s->self))
        mro = type_obj(s->self)->mro;
    else if (!s->self.is_nil())
        mro = type_obj(type_of_value(s->self))->mro;
    else
        mro = type_obj(s->cls)->mro;

    // Everything after the class super() was written in.
    usize at = 0;
    while (at < tuple_len(mro) && tuple_at(mro, at) != s->cls)
        at++;
    for (usize i = at + 1; i < tuple_len(mro); i++) {
        Value c = tuple_at(mro, i);
        Value found;
        R r = dict_get(dict_at(type_obj(c)->dict), obj_value(name), found);
        if (r == R::Err)
            return R::Err;
        if (r == R::Ok)
            return type_bind(found, is_type(s->self) ? Value() : s->self, c, out);
    }
    return R::NotImpl;
}

// ------------------------------------------------------------------ builtins

namespace {

// The class body has run; its namespace is the class.
R build_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call(k, k->s[0], Value(), 0);
    (void)in;
    Value t = type_new(k->s[1], k->s[2], k->s[3]);
    return t.is_nil() ? R::Err : cont_done(k, t);
}

// __build_class__(body, name, *bases): the shape the compiler emits.
R b_build_class(const CallArgs &a, Value &out)
{
    if (a.nargs < 2 || !is_func(a.args[0]))
        return err_set("TypeError", "__build_class__() takes a function and a name");
    if (a.nkw)
        return err_set("TypeError", "class keywords are not supported");

    TupleObj *bases = tuple_new(a.nargs - 2);
    if (!bases)
        return oom();
    for (u32 i = 2; i < a.nargs; i++)
        bases->items()[i - 2] = a.args[i];
    Root rb{ obj_value(bases) };
    DictObj *ns = dict_new();
    if (!ns)
        return oom();
    Root rn{ obj_value(ns) };
    Root kv{ cont_new(build_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = a.args[0];
    k->s[1]    = a.args[1];
    k->s[2]    = rb.v;
    k->s[3]    = rn.v;
    k->locals  = rn.v;
    out        = kv.v;
    return R::Ok;
}

R b_type(const CallArgs &a, Value &out)
{
    if (a.nargs == 3 && !a.nkw) {
        out = type_new(a.args[0], a.args[1], a.args[2]);
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (!args_only(a, "type", 1, 1))
        return R::Err;
    out = type_of_value(a.args[0]);
    return out.is_nil() ? R::Err : R::Ok;
}

// isinstance and issubclass take a type or a tuple of them.
R any_of(Value t, Value v, bool cls, bool &out)
{
    if (is_tuple(t)) {
        for (usize i = 0; i < tuple_len(t); i++) {
            R r = any_of(tuple_at(t, i), v, cls, out);
            if (r != R::Ok || out)
                return r;
        }
        out = false;
        return R::Ok;
    }
    if (!is_type(t))
        return err_set("TypeError", "the second argument must be a type or a tuple of types");
    out = cls ? type_issub(v, t) : type_isinstance(v, t);
    return R::Ok;
}

R b_isinstance(const CallArgs &a, Value &out)
{
    if (!args_only(a, "isinstance", 2, 2))
        return R::Err;
    bool yes = false;
    if (any_of(a.args[1], a.args[0], false, yes) != R::Ok)
        return R::Err;
    out = value_bool(yes);
    return R::Ok;
}

R b_issubclass(const CallArgs &a, Value &out)
{
    if (!args_only(a, "issubclass", 2, 2))
        return R::Err;
    if (!is_type(a.args[0]))
        return err_set2("TypeError", "issubclass() argument 1 must be a class",
                        type_name(a.args[0]));
    bool yes = false;
    if (any_of(a.args[1], a.args[0], true, yes) != R::Ok)
        return R::Err;
    out = value_bool(yes);
    return R::Ok;
}

// object.__new__(cls), which is where every class's instance comes from.
R b_object(const CallArgs &a, Value &out)
{
    if (!args_only(a, "object", 0, 1))
        return R::Err;
    Value cls = a.nargs ? a.args[0] : type_object();
    if (!is_type(cls))
        return err_set2("TypeError", "object.__new__() argument must be a type", type_name(cls));
    out = inst_new(cls);
    return out.is_nil() ? R::Err : R::Ok;
}

} // namespace

namespace {

Value prop_new(Value get, Value set, Value del)
{
    Root a{ get }, b{ set }, c{ del };
    PropObj *p = static_cast<PropObj *>(obj_alloc(&property_type, sizeof(PropObj)));
    if (!p)
        return oom(), Value();
    p->get = a.v;
    p->set = b.v;
    p->del = c.v;
    return obj_value(p);
}

// property.getter/setter/deleter: a copy with one of the three replaced.
R prop_with(const CallArgs &a, u32 which, Value &out)
{
    if (a.nargs != 2 || a.nkw)
        return err_set("TypeError", "property.setter() takes one argument");
    PropObj *p = static_cast<PropObj *>(a.args[0].obj());
    Value g = p->get, s = p->set, d = p->del;
    (which == 0 ? g : which == 1 ? s : d) = a.args[1];
    out                                   = prop_new(g, s, d);
    return out.is_nil() ? R::Err : R::Ok;
}

R b_prop_getter(const CallArgs &a, Value &out)
{
    return prop_with(a, 0, out);
}

R b_prop_setter(const CallArgs &a, Value &out)
{
    return prop_with(a, 1, out);
}

R b_prop_deleter(const CallArgs &a, Value &out)
{
    return prop_with(a, 2, out);
}

R prop_getattr(Value v, StrObj *name, Value &out)
{
    Str n                              = name->str();
    R (*fn)(const CallArgs &, Value &) = n == "getter"    ? b_prop_getter
                                         : n == "setter"  ? b_prop_setter
                                         : n == "deleter" ? b_prop_deleter
                                                          : nullptr;
    if (!fn)
        return R::NotImpl;
    Root f{ native_new("property", fn) };
    if (f.v.is_nil())
        return R::Err;
    out = method_new(f.v, v);
    return out.is_nil() ? R::Err : R::Ok;
}

R b_property(const CallArgs &a, Value &out)
{
    Root get, set, del;
    if (a.nargs > 0)
        get = a.args[0];
    if (a.nargs > 1)
        set = a.args[1];
    if (a.nargs > 2)
        del = a.args[2];
    for (u32 i = 0; i < a.nkw; i++) {
        Str n = is_str(a.kwnames[i]) ? str_of(a.kwnames[i])->str() : Str();
        if (n == "fget")
            get = a.kwvals[i];
        else if (n == "fset")
            set = a.kwvals[i];
        else if (n == "fdel")
            del = a.kwvals[i];
        else if (n != "doc")
            return err_set2("TypeError", "property() got an unexpected keyword argument", n);
    }
    out = prop_new(get.v, set.v, del.v);
    return out.is_nil() ? R::Err : R::Ok;
}

R wrap_one(const CallArgs &a, const Type *t, Str who, Value &out)
{
    if (!args_only(a, who, 1, 1))
        return R::Err;
    Root rf{ a.args[0] };
    WrapObj *w = static_cast<WrapObj *>(obj_alloc(t, sizeof(WrapObj)));
    if (!w)
        return oom();
    w->fn = rf.v;
    out   = obj_value(w);
    return R::Ok;
}

R b_staticmethod(const CallArgs &a, Value &out)
{
    return wrap_one(a, &staticmethod_type, "staticmethod", out);
}

R b_classmethod(const CallArgs &a, Value &out)
{
    return wrap_one(a, &classmethod_type, "classmethod", out);
}

// Zero-argument super(): the class is the one in self's MRO whose namespace
// holds the function now running. CPython uses a __class__ cell for this; the
// search finds the same class and costs the compiler nothing.
R super_here(Value &self, Value &cls)
{
    Value fr = vm_frame();
    if (fr.is_nil() || !frame_of(fr)->nlocals)
        return err_set("RuntimeError", "super(): no arguments");
    self       = frame_of(fr)->slots()[0];
    Value code = frame_of(fr)->code;
    Value t    = type_of_value(self);
    if (t.is_nil())
        return R::Err;
    Value m = type_obj(t)->mro;
    for (usize i = 0; i < tuple_len(m); i++) {
        Value c  = tuple_at(m, i);
        usize at = 0;
        Value k, v;
        while (table_next(dict_at(type_obj(c)->dict)->t, at, k, v))
            if (is_func(v) && func_of(v)->code == code) {
                cls = c;
                return R::Ok;
            }
    }
    return err_set("RuntimeError", "super(): no class found");
}

R b_super(const CallArgs &a, Value &out)
{
    if (!a.nargs && !a.nkw) {
        Root self, cls;
        if (super_here(self.v, cls.v) != R::Ok)
            return R::Err;
        SuperObj *s = static_cast<SuperObj *>(obj_alloc(&super_type, sizeof(SuperObj)));
        if (!s)
            return oom();
        s->cls  = cls.v;
        s->self = self.v;
        out     = obj_value(s);
        return R::Ok;
    }
    if (!args_only(a, "super", 2, 2))
        return R::Err;
    if (!is_type(a.args[0]))
        return err_set2("TypeError", "super() argument 1 must be a type", type_name(a.args[0]));
    bool ok = is_type(a.args[1]) ? type_issub(a.args[1], a.args[0])
                                 : type_isinstance(a.args[1], a.args[0]);
    if (!ok)
        return err_set("TypeError", "super(type, obj): obj must be an instance or subtype");
    Root rc{ a.args[0] }, rs{ a.args[1] };
    SuperObj *s = static_cast<SuperObj *>(obj_alloc(&super_type, sizeof(SuperObj)));
    if (!s)
        return oom();
    s->cls  = rc.v;
    s->self = rs.v;
    out     = obj_value(s);
    return R::Ok;
}

struct Named {
    Str name;
    R (*fn)(const CallArgs &, Value &out);
};

constexpr Named CLASS_BUILTINS[] = {
    { "__build_class__", b_build_class },
    { "isinstance", b_isinstance },
    { "issubclass", b_issubclass },
    { "property", b_property },
    { "staticmethod", b_staticmethod },
    { "classmethod", b_classmethod },
    { "super", b_super },
};

// The built-in types a program can name, subclass or test against.
const Type *const NAMED[] = { &int_type,       &float_type,     &bool_type,    &str_type,
                              &bytes_type,     &bytearray_type, &tuple_type,   &list_type,
                              &dict_type,      &set_type,       &range_type,   &type_type,
                              &frozenset_type, &slice_type,     &memview_type, &complex_type };

// `v` is pinned first: interning the name allocates, and a fresh native with
// nothing pointing at it is exactly what a collection there would take.
bool put(Value d, Str name, Value v)
{
    Root rd{ d }, rv{ v };
    if (rv.v.is_nil())
        return false;
    StrObj *n = str_intern(name);
    return n && dict_set(dict_at(rd.v), obj_value(n), rv.v) == R::Ok;
}

} // namespace

bool type_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    for (const Named &e : CLASS_BUILTINS)
        if (!put(rd.v, e.name, native_new(e.name, e.fn)))
            return false;
    Root ob{ type_object() };
    if (ob.v.is_nil() || !put(rd.v, "object", ob.v))
        return false;
    // `object` answers a call itself; the rest are constructors already.
    if (!put(type_obj(ob.v)->dict, "__new__", native_new("object", b_object)))
        return false;
    for (const Type *t : NAMED) {
        Value w = type_wrap(t);
        if (w.is_nil() || !put(rd.v, t->name, w))
            return false;
    }
    // type(x) and type(name, bases, dict) are both calls of the type `type`.
    Root mk{ native_new("type", b_type) };
    return type_set_ctor(&type_type, mk.v);
}

bool type_set_ctor(const Type *t, Value fn)
{
    Root rf{ fn };
    Value w = type_wrap(t);
    return !w.is_nil() && put(type_obj(w)->dict, "__new__", rf.v);
}

Value type_special(Value v, Str name)
{
    if (!is_inst(v))
        return Value();
    StrObj *n = str_intern(name);
    Root found;
    if (!n || type_lookup(inst_of(v)->cls, n, found.v) != R::Ok)
        return Value();
    // A staticmethod or a classmethod here too: the wrapper is not callable.
    Value out;
    return type_bind(found.v, v, inst_of(v)->cls, out) == R::Ok ? out : Value();
}

bool type_has_special(Value v, Str name)
{
    if (!is_inst(v))
        return false;
    StrObj *n = str_intern(name);
    Value found;
    return n && type_lookup(inst_of(v)->cls, n, found) == R::Ok;
}

// The `__new__` a class wrote itself, rather than the one object lends it.
Value type_own_new(Value cls)
{
    StrObj *n = str_intern("__new__");
    Value found;
    if (!n || type_lookup(cls, n, found) != R::Ok || is_native(found))
        return Value();
    return found;
}

bool type_native_takes_args(Value cls)
{
    Value n = type_obj(cls)->native;
    if (n.is_nil())
        return false;
    const Type *d = type_obj(n)->desc;
    return d == &tuple_type || d == &str_type || d == &bytes_type || d == &int_type ||
           d == &float_type;
}

Value type_make_native(Str name, Value base, const Type *desc, const ExcType *exc)
{
    Root rb{ base };
    Root nm{ str_new(name) };
    if (nm.v.is_nil())
        return Value();
    DictObj *d = dict_new();
    if (!d)
        return oom(), Value();
    Root rd{ obj_value(d) };
    TypeObj *o = type_alloc();
    if (!o)
        return Value();
    Root ro{ obj_value(o) };
    o->slots       = *desc;
    o->slots.name  = str_of(nm.v)->str();
    o->slots.owner = o;
    o->name        = nm.v;
    o->dict        = rd.v;
    o->desc        = desc;
    o->exc         = exc;

    if (rb.v.is_nil()) {
        rb = type_object();
        if (rb.v.is_nil())
            return Value();
    }
    TupleObj *bases = tuple_new(1);
    if (!bases)
        return oom(), Value();
    bases->items()[0] = rb.v;
    Root rbt{ obj_value(bases) };
    type_obj(ro.v)->bases = rbt.v;
    Value m               = mro_of(ro.v, rbt.v);
    if (m.is_nil())
        return Value();
    type_obj(ro.v)->mro = m;
    return ro.v;
}

Obj *type_alloc_inst(Value cls, usize bytes)
{
    Root rc{ cls };
    Obj *o = obj_alloc(&type_obj(rc.v)->slots, bytes);
    if (!o)
        return oom(), nullptr;
    InstObj *i = static_cast<InstObj *>(o);
    i->cls     = rc.v;
    i->dict    = Value();
    i->native  = Value();
    return o;
}

Value type_property(Value v, StrObj *name, u32 which)
{
    if (!is_inst(v))
        return Value();
    Value found;
    if (type_lookup(inst_of(v)->cls, name, found) != R::Ok || !is_property(found))
        return Value();
    PropObj *p = static_cast<PropObj *>(found.obj());
    Value fn   = which == 0 ? p->get : which == 1 ? p->set : p->del;
    return fn.is_nil() ? Value() : method_new(fn, v);
}
