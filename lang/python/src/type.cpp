// Type objects, instances, the MRO and the descriptors.
#include "type.h"

#include "builtin.h"
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
#include "union.h"
#include "vm.h"
#include "weak.h"

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
    gc_mark(t->qualname);
    gc_mark(t->dict);
    gc_mark(t->bases);
    gc_mark(t->mro);
    gc_mark(t->native);
    gc_mark(t->origbases);
    gc_mark(t->subs);
}

// <class 'module.qualname'>, and no module for a builtin.
R type_repr(Value v, String &out)
{
    TypeObj *t = type_obj(v);
    Value mod, qual;
    if (t->heap) {
        StrObj *k = str_intern("__module__");
        if (!k || t->dict.is_nil() ||
            dict_get(static_cast<DictObj *>(t->dict.obj()), obj_value(k), mod) == R::Err)
            err_clear();
    } else {
        StrObj *k = str_intern("__module__");
        if (!k || t->dict.is_nil() ||
            dict_get(static_cast<DictObj *>(t->dict.obj()), obj_value(k), mod) == R::Err)
            err_clear();
    }
    qual = t->qualname.is_nil() ? t->name : t->qualname;
    if (!out.append("<class '"))
        return oom();
    if (is_str(mod) && str_of(mod)->str() != "builtins" &&
        (!out.append(str_of(mod)->str()) || !out.push('.')))
        return oom();
    Str q = is_str(qual) ? str_of(qual)->str() : t->slots.name;
    return out.append(q) && out.append("'>") ? R::Ok : oom();
}

void inst_trace(Obj *o)
{
    InstObj *i = static_cast<InstObj *>(o);
    gc_mark(i->cls);
    gc_mark(i->dict);
    gc_mark(i->native);
    u32 n    = 0;
    Value *s = inst_slots(o, n);
    for (u32 k = 0; k < n; k++)
        gc_mark(s[k]);
}

R inst_repr(Value v, String &out)
{
    char tmp[24];
    Buf<96> b;
    b.put("<").put(type_name(v)).put(" object at ");
    b.put(addr_text(tmp, sizeof tmp, v.obj()));
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
// A bound method answers for the function it wraps, so an attribute a
// decorator left on the function is found through it too.
R method_getattr(Value v, StrObj *name, Value &out)
{
    Str n        = name->str();
    MethodObj *m = static_cast<MethodObj *>(v.obj());
    if (n == "__func__")
        return out = m->fn, R::Ok;
    if (n == "__self__")
        return out = m->self, R::Ok;
    if (n == "__class__" || n == "__dict__" || n == "__get__")
        return R::NotImpl;
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
    gc_mark(p->doc);
    gc_mark(p->pname);
}

void wrap_trace(Obj *o)
{
    gc_mark(static_cast<WrapObj *>(o)->fn);
    gc_mark(static_cast<WrapObj *>(o)->dict);
}

WrapObj *wrap_of(Value v)
{
    return static_cast<WrapObj *>(v.obj());
}

// The wrapper's own dict, with what it copied off the function when made.
DictObj *wrap_dict(Value v)
{
    if (!wrap_of(v)->dict.is_nil())
        return static_cast<DictObj *>(wrap_of(v)->dict.obj());
    Root rv{ v };
    DictObj *d = dict_new();
    if (!d)
        return oom(), nullptr;
    wrap_of(rv.v)->dict    = obj_value(d);
    constexpr Str COPIED[] = { "__module__", "__name__", "__qualname__", "__doc__" };
    for (Str name : COPIED) {
        StrObj *n = str_intern(name);
        if (!n)
            return oom(), nullptr;
        Value got, args;
        Got g = attr_plain(wrap_of(rv.v)->fn, n, got, args);
        if (g == Got::Error)
            err_clear();
        if (g != Got::Ok)
            continue;
        if (dict_set(static_cast<DictObj *>(wrap_of(rv.v)->dict.obj()), obj_value(n), got) != R::Ok)
            return nullptr;
    }
    return static_cast<DictObj *>(wrap_of(rv.v)->dict.obj());
}

R wrap_getattr(Value v, StrObj *name, Value &out)
{
    Str n = name->str();
    if (n == "__func__" || n == "__wrapped__") {
        out = wrap_of(v)->fn;
        return R::Ok;
    }
    Root rv{ v };
    DictObj *d = wrap_dict(rv.v);
    if (!d)
        return R::Err;
    if (n == "__dict__") {
        out = wrap_of(rv.v)->dict;
        return R::Ok;
    }
    R r = dict_get(d, obj_value(name), out);
    if (r != R::NotImpl)
        return r;
    if (n == "__isabstractmethod__") {
        Value got, args;
        Got g = attr_plain(wrap_of(rv.v)->fn, name, got, args);
        if (g == Got::Error)
            err_clear();
        out = g == Got::Ok ? value_bool(py_truth(got)) : value_bool(false);
        return R::Ok;
    }
    return R::NotImpl;
}

R wrap_setattr(Value v, StrObj *name, Value val)
{
    Root rv{ v }, rx{ val };
    DictObj *d = wrap_dict(rv.v);
    if (!d)
        return R::Err;
    if (rx.v.is_nil()) {
        R r = dict_del(d, obj_value(name));
        return r == R::NotImpl ? err_set2("AttributeError", "no such attribute", name->str()) : r;
    }
    return dict_set(d, obj_value(name), rx.v);
}

R wrap_repr(Value v, String &out)
{
    Root rv{ v };
    if (!out.push('<') || !out.append(type_name(rv.v)) || !out.push('('))
        return oom();
    if (py_repr(wrap_of(rv.v)->fn, out) != R::Ok)
        return R::Err;
    return out.append(")>") ? R::Ok : oom();
}

void super_trace(Obj *o)
{
    SuperObj *s = static_cast<SuperObj *>(o);
    gc_mark(s->cls);
    gc_mark(s->self);
}

R prop_getattr(Value v, StrObj *name, Value &out);
R prop_setattr(Value v, StrObj *name, Value val);

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

// A frozendict subclass prints under its own name, as frozendict does.
R dg_repr(Value v, String &out)
{
    Value d = delegate(v);
    if (!is_frozendict(d) || !static_cast<DictObj *>(d.obj())->t.live)
        return is_frozendict(d) ? (out.append(type_name(v)) && out.append("()") ? R::Ok : oom())
                                : py_repr(d, out);
    if (!out.append(type_name(v)) || !out.push('('))
        return oom();
    if (dict_repr(d, out) != R::Ok)
        return R::Err;
    return out.push(')') ? R::Ok : oom();
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

// `d` is the descriptor the object points at: `type` itself, or a metaclass's
// slots where one made this class.
TypeObj *type_alloc_at(const Type *d)
{
    TypeObj *t = static_cast<TypeObj *>(obj_alloc(d, sizeof(TypeObj)));
    if (!t)
        return oom(), nullptr;
    t->flags |= OBJ_TYPE;
    t->slots     = Type{};
    t->name      = Value();
    t->qualname  = Value();
    t->dict      = Value();
    t->bases     = Value();
    t->mro       = Value();
    t->native    = Value();
    t->origbases = Value();
    t->subs      = Value();
    t->desc      = nullptr;
    t->exc       = nullptr;
    t->nslots    = 0;
    t->slotoff   = sizeof(InstObj);
    t->heap      = false;
    t->meta      = false;
    t->nodict    = false;
    t->hasdel    = false;
    return t;
}

TypeObj *type_alloc()
{
    return type_alloc_at(&type_type);
}

} // namespace

constexpr Type type_type{ .name  = "type",
                          .trace = type_trace,
                          .repr  = type_repr,
                          .binop = union_binop };

constexpr Type method_type{ .name    = "method",
                            .trace   = method_trace,
                            .repr    = method_repr,
                            .getattr = method_getattr };

constexpr Type property_type{ .name    = "property",
                              .trace   = prop_trace,
                              .repr    = plain_repr,
                              .getattr = prop_getattr,
                              .setattr = prop_setattr };

constexpr Type staticmethod_type{ .name    = "staticmethod",
                                  .trace   = wrap_trace,
                                  .repr    = wrap_repr,
                                  .getattr = wrap_getattr,
                                  .setattr = wrap_setattr };

constexpr Type classmethod_type{ .name    = "classmethod",
                                 .trace   = wrap_trace,
                                 .repr    = wrap_repr,
                                 .getattr = wrap_getattr,
                                 .setattr = wrap_setattr };

// No getattr slot: attr.cpp answers a super() lookup, because it goes through
// the descriptor protocol and may therefore have to call Python.
constexpr Type super_type{ .name = "super", .trace = super_trace, .repr = super_repr };

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

    // A dotted descriptor name is `module.name`, as a tp_name is.
    Str full  = t->name;
    usize dot = full.size();
    while (dot && full[dot - 1] != '.')
        dot--;
    Root name{ str_new(full.substr(dot)) };
    if (name.v.is_nil())
        return Value();
    DictObj *d = dict_new();
    if (!d)
        return oom(), Value();
    Root rd{ obj_value(d) };
    // No docstrings are kept, and every type still answers __doc__.
    StrObj *dk = str_intern("__doc__");
    if (!dk || dict_set(dict_at(rd.v), obj_value(dk), value_none()) != R::Ok)
        return err_pending() ? Value() : (oom(), Value());
    if (dot) {
        Root mod{ str_new(full.substr(0, dot - 1)) };
        StrObj *key = str_intern("__module__");
        if (mod.v.is_nil() || !key || dict_set(dict_at(rd.v), obj_value(key), mod.v) != R::Ok)
            return Value();
    }
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
        Value ob = t == &bool_type ? type_wrap(&int_type)
                   : t->base       ? type_wrap(t->base)
                                   : type_object();
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

namespace {

// __slots__ is a name or an iterable of them. Each becomes a member descriptor
// in the class namespace over an index into the instance's slot array, which
// starts where the base's ended.
bool slots_declare(Value cls, u32 base)
{
    Root rc{ cls };
    StrObj *key = str_intern("__slots__");
    if (!key)
        return oom(), false;
    Root spec;
    DictObj *d = dict_at(type_obj(rc.v)->dict);
    R r        = dict_get(d, obj_value(key), spec.v);
    if (r == R::Err)
        return false;
    type_obj(rc.v)->nslots = base;
    if (r != R::Ok)
        return true;

    Root names;
    if (is_str(spec.v)) {
        TupleObj *one = tuple_new(1);
        if (!one)
            return oom(), false;
        one->items()[0] = spec.v;
        names           = obj_value(one);
    } else {
        ListObj *l = py_list_of(spec.v);
        if (!l)
            return false;
        names = obj_value(l);
    }

    ListObj *l = is_list(names.v) ? list_of(names.v) : nullptr;
    usize n    = l ? l->items.size() : tuple_len(names.v);
    for (usize i = 0; i < n; i++) {
        Root one{ l ? l->items[i] : tuple_at(names.v, i) };
        if (!is_str(one.v)) {
            err_set2("TypeError", "__slots__ items must be strings", type_name(one.v));
            return false;
        }
        StrObj *nm = py_mangle(str_of(type_obj(rc.v)->name), str_of(one.v)->str());
        if (!nm)
            return oom(), false;
        Root rn{ obj_value(nm) };
        MemberObj *m = static_cast<MemberObj *>(obj_alloc(&member_type, sizeof(MemberObj)));
        if (!m)
            return oom(), false;
        m->name  = rn.v;
        m->cls   = rc.v;
        m->index = type_obj(rc.v)->nslots++;
        if (dict_set(dict_at(type_obj(rc.v)->dict), rn.v, obj_value(m)) != R::Ok)
            return false;
    }
    return true;
}

} // namespace

Value type_new(Value name, Value bases, Value dict)
{
    return type_new_meta(Value(), name, bases, dict);
}

Value type_new_meta(Value meta, Value name, Value bases, Value dict)
{
    Root rm{ meta }, rn{ name }, rb{ bases }, rd{ dict };
    // A dict subclass's contents become the class's own dict, as CPython
    // copies them.
    if (!is_dict(rd.v)) {
        Value inner = is_inst(rd.v) ? inst_of(rd.v)->native : Value();
        if (!is_dict(inner)) {
            Buf<128> m;
            m.put("type.__new__() argument 3 must be dict, not ").put(type_name(rd.v));
            return err_set("TypeError", m.str()), Value();
        }
        Root src{ inner };
        DictObj *copy = dict_new();
        if (!copy)
            return oom(), Value();
        rd       = obj_value(copy);
        usize at = 0;
        Value k, v;
        while (table_next(dict_at(src.v)->t, at, k, v))
            if (dict_set(dict_at(rd.v), k, v) != R::Ok)
                return Value();
    }
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
    for (usize i = 0; i < tuple_len(rb.v); i++) {
        Value base = tuple_at(rb.v, i);
        if (is_type(base))
            continue;
        // Only a class statement resolves __mro_entries__.
        if (type_has_special(base, "__mro_entries__"))
            return err_set("TypeError",
                           "type() doesn't support MRO entry resolution; "
                           "use types.new_class()"),
                   Value();
        return err_set2("TypeError", "a base is not a class", type_name(base)), Value();
    }
    for (usize i = 0; i < tuple_len(rb.v); i++) {
        const Type *d = type_obj(tuple_at(rb.v, i))->desc;
        if (d && d->final) {
            Buf<128> m;
            m.put("type '").put(d->name).put("' is not an acceptable base type");
            return err_set("TypeError", m.str()), Value();
        }
    }

    // An instance of the metaclass, so that type(C) is M and M's own methods
    // are found on C. Without one it is an instance of `type`.
    TypeObj *t = type_alloc_at(rm.v.is_nil() ? &type_type : &type_obj(rm.v)->slots);
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
    t->slotoff     = sizeof(InstObj);

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
            type_obj(rt.v)->exc     = type_obj(c)->exc;
            type_obj(rt.v)->slotoff = sizeof(ExcObj);
            exc_slots(type_obj(rt.v)->slots);
            break;
        }
        const Type *n = type_obj(c)->desc;
        if (!n)
            continue;
        if (n == &type_type) {
            // A metaclass: its instances are classes, so it keeps type's own
            // handlers rather than delegating to a `type` laid out inside.
            type_obj(rt.v)->meta        = true;
            type_obj(rt.v)->slots.trace = type_trace;
            type_obj(rt.v)->slots.repr  = type_repr;
            type_obj(rt.v)->slots.binop = union_binop;
            break;
        }
        if (n == &object_type || n->plain)
            continue;
        type_obj(rt.v)->native = c;
        copy_slots(type_obj(rt.v)->slots, n);
        break;
    }

    // __slots__ over the whole line: the base's array comes first, and the
    // instance has no dict only when nothing above it has one either.
    u32 base    = 0;
    bool nodict = true;
    for (usize i = 0; i < tuple_len(rb.v); i++) {
        TypeObj *b = type_obj(tuple_at(rb.v, i));
        if (b->nslots > base)
            base = b->nslots;
        if (b->heap && !b->nodict)
            nodict = false;
        if (b->slotoff > type_obj(rt.v)->slotoff)
            type_obj(rt.v)->slotoff = b->slotoff;
    }
    // What `class` was written with, where __mro_entries__ changed it.
    StrObj *obk = str_intern("__orig_bases__");
    if (!obk)
        return oom(), Value();
    Value orig;
    if (dict_get(dict_at(rd.v), obj_value(obk), orig) == R::Ok && is_tuple(orig))
        type_obj(rt.v)->origbases = orig;

    // The cell the body's methods read __class__ from: filled with the class,
    // and not a name in its namespace.
    StrObj *ck = str_intern("__classcell__");
    if (!ck)
        return oom(), Value();
    Value cell;
    R cr = dict_get(dict_at(rd.v), obj_value(ck), cell);
    if (cr == R::Err)
        return Value();
    if (cr == R::Ok) {
        if (!cell.is_obj() || cell.obj()->type != &cell_type) {
            Root rc{ cell };
            String msg;
            if (!msg.append("__classcell__ must be a nonlocal cell, not "))
                return oom(), Value();
            Root ct{ type_of_value(rc.v) };
            if (ct.v.is_nil() || py_repr(ct.v, msg) != R::Ok)
                return Value();
            return err_set("TypeError", msg.str()), Value();
        }
        static_cast<CellObj *>(cell.obj())->v = rt.v;
        if (dict_del(dict_at(rd.v), obj_value(ck)) == R::Err)
            return Value();
    }

    // A class without a docstring still says so in its namespace.
    StrObj *dk = str_intern("__doc__");
    if (!dk)
        return oom(), Value();
    Value had;
    R dr = dict_get(dict_at(rd.v), obj_value(dk), had);
    if (dr == R::Err ||
        (dr == R::NotImpl && dict_set(dict_at(rd.v), obj_value(dk), value_none()) != R::Ok))
        return Value();

    // __qualname__ is the type's own, not a name in its namespace.
    Root qn;
    StrObj *qk = str_intern("__qualname__");
    if (!qk)
        return oom(), Value();
    R qr = dict_get(dict_at(rd.v), obj_value(qk), qn.v);
    if (qr == R::Err)
        return Value();
    if (qr == R::Ok) {
        if (!is_str(qn.v))
            return err_set2("TypeError", "type __qualname__ must be a str", type_name(qn.v)),
                   Value();
        type_obj(rt.v)->qualname = qn.v;
        if (dict_del(dict_at(rd.v), obj_value(qk)) == R::Err)
            return Value();
    }

    Root has;
    StrObj *sk = str_intern("__slots__");
    if (!sk)
        return oom(), Value();
    R sr = dict_get(dict_at(rd.v), obj_value(sk), has.v);
    if (sr == R::Err || !slots_declare(rt.v, base))
        return Value();

    // A class that writes __eq__ and not __hash__ is unhashable: two objects
    // that compare equal would otherwise hash apart, which is the one thing a
    // dict cannot survive.
    StrObj *ek = str_intern("__eq__");
    StrObj *hk = str_intern("__hash__");
    if (!ek || !hk)
        return oom(), Value();
    Value seen;
    if (dict_get(dict_at(rd.v), obj_value(ek), seen) == R::Ok &&
        dict_get(dict_at(rd.v), obj_value(hk), seen) == R::NotImpl &&
        dict_set(dict_at(rd.v), obj_value(hk), value_none()) != R::Ok)
        return Value();
    type_obj(rt.v)->nodict = nodict && sr == R::Ok && type_obj(rt.v)->native.is_nil() &&
                             !type_obj(rt.v)->exc && !type_obj(rt.v)->meta;
    type_note_del(rt.v);
    // Each base remembers what was made under it, which is what
    // __subclasses__ answers and what an ABC's subclass check walks.
    for (usize i = 0; i < tuple_len(rb.v); i++) {
        TypeObj *b = type_obj(tuple_at(rb.v, i));
        if (b->subs.is_nil()) {
            ListObj *l = list_new();
            if (!l)
                return oom(), Value();
            type_obj(tuple_at(rb.v, i))->subs = obj_value(l);
        }
        if (!list_push(list_of(type_obj(tuple_at(rb.v, i))->subs), rt.v))
            return oom(), Value();
    }
    return rt.v;
}

// ------------------------------------------------------------------ attributes

R type_bind(Value found, Value self, Value cls, Value &out)
{
    // A built-in written in C++ binds like a function: BaseException.__init__
    // reached through super() is one.
    if ((is_func(found) || (is_native(found) && !(found.obj()->flags & OBJ_PLAINFN))) &&
        !self.is_nil()) {
        out = method_new(found, self);
        return out.is_nil() ? R::Err : R::Ok;
    }
    Value inner   = found.is_obj() ? descr_inner(found) : found;
    const Type *t = inner.is_obj() ? inner.obj()->type : nullptr;
    if (t == &staticmethod_type) {
        out = static_cast<WrapObj *>(inner.obj())->fn;
        return R::Ok;
    }
    if (t == &classmethod_type) {
        out = method_new(static_cast<WrapObj *>(inner.obj())->fn, cls);
        return out.is_nil() ? R::Err : R::Ok;
    }
    out = found;
    return R::Ok;
}

// ------------------------------------------------------------------ builtins

namespace {

// The keywords of a class statement, less `metaclass`, kept as two tuples so
// they can be handed on to the metaclass call and then to __init_subclass__.
struct Kwds {
    Root names, vals, meta;
};

bool kwds_split(const CallArgs &a, Kwds &kw)
{
    u32 n = 0;
    for (u32 i = 0; i < a.nkw; i++)
        if (!(is_str(a.kwnames[i]) && str_of(a.kwnames[i])->str() == Str("metaclass")))
            n++;
    TupleObj *names = tuple_new(n);
    if (!names)
        return oom(), false;
    kw.names       = obj_value(names);
    TupleObj *vals = tuple_new(n);
    if (!vals)
        return oom(), false;
    kw.vals = obj_value(vals);
    u32 at  = 0;
    for (u32 i = 0; i < a.nkw; i++) {
        if (is_str(a.kwnames[i]) && str_of(a.kwnames[i])->str() == Str("metaclass")) {
            kw.meta = a.kwvals[i];
            continue;
        }
        static_cast<TupleObj *>(kw.names.v.obj())->items()[at] = a.kwnames[i];
        static_cast<TupleObj *>(kw.vals.v.obj())->items()[at]  = a.kwvals[i];
        at++;
    }
    return true;
}

// The most derived of the metaclass asked for and the bases' own: CPython's
// rule, and what makes `class C(A, B)` under two metaclasses an error rather
// than a silent choice.
Value meta_of(Value asked, Value bases)
{
    Root best{ asked };
    for (usize i = 0; i < tuple_len(bases); i++) {
        Value b = tuple_at(bases, i);
        if (!is_type(b))
            continue;
        Value m = type_of_value(b);
        if (m.is_nil())
            return Value();
        if (best.v.is_nil() || type_issub(m, best.v))
            best = m;
        else if (!type_issub(best.v, m))
            return err_set("TypeError",
                           "metaclass conflict: the metaclass of a derived class must be a "
                           "(non-strict) subclass of the metaclasses of all its bases"),
                   Value();
    }
    return best.v;
}

// A base that is not a class may say what to put in its place (PEP 560).
Value mro_entries_of(Value base)
{
    if (is_type(base))
        return Value();
    Value m = type_special(base, "__mro_entries__");
    if (!m.is_nil())
        return m;
    // A built-in object answers out of its own method table rather than a
    // class namespace: a generic alias stands for the class it was made from.
    StrObj *n = str_intern("__mro_entries__");
    Value found;
    if (n && base.is_obj() && method_find(base, n, found) == R::Ok)
        return found;
    err_clear();
    // Or an attribute of the object itself: typing.NamedTuple is a function
    // with one stored on it.
    if (n && base.is_obj() && py_attr_opt(base, n, found, Value(), false) == Got::Ok &&
        !found.is_nil())
        return found;
    err_clear();
    return Value();
}

// s[0] the body, s[1] the name, s[2] the bases, s[3] the namespace, s[4] the
// metaclass, s[5] the keywords; i counts the steps and j the base being
// resolved through __mro_entries__.
R build_step(ContObj *k, Value in)
{
    switch (k->i) {
    case 0: {
        // __mro_entries__ first: a base that is not a class stands for some
        // that are, and the metaclass is chosen from what it stands for.
        for (; k->j < tuple_len(k->s[2]); k->j++) {
            Value e = mro_entries_of(tuple_at(k->s[2], k->j));
            if (e.is_nil())
                continue;
            // The answer arrives at the next step, not at this one again. It
            // is asked with the bases as written.
            k->i = 1;
            return cont_call(k, e, k->s[6].is_nil() ? k->s[2] : k->s[6]);
        }
        k->i = 1;
        return build_step(k, Value());
    }
    case 1: {
        if (!in.is_nil()) {
            // The answer replaces that base, so the tuple grows or shrinks.
            if (!is_tuple(in))
                return err_set2("TypeError", "__mro_entries__ must return a tuple", type_name(in));
            usize n     = tuple_len(k->s[2]) - 1 + tuple_len(in);
            TupleObj *t = tuple_new(n);
            if (!t)
                return oom();
            Root rt{ obj_value(t) };
            usize at = 0;
            for (usize i = 0; i < tuple_len(k->s[2]); i++) {
                if (i == k->j) {
                    for (usize e = 0; e < tuple_len(in); e++)
                        static_cast<TupleObj *>(rt.v.obj())->items()[at++] = tuple_at(in, e);
                    continue;
                }
                static_cast<TupleObj *>(rt.v.obj())->items()[at++] = tuple_at(k->s[2], i);
            }
            k->j += u32(tuple_len(in));
            if (k->s[6].is_nil())
                k->s[6] = k->s[2]; // __orig_bases__: what was written
            k->s[2] = rt.v;
            k->i    = 0;
            return build_step(k, Value());
        }
        // The metaclass decides the namespace the body runs in.
        Value m = meta_of(k->s[4], k->s[2]);
        if (m.is_nil() && err_pending())
            return R::Err;
        k->s[4] = m.is_nil() ? type_wrap(&type_type) : m;
        if (k->s[4].is_nil())
            return R::Err;
        k->i      = 2;
        Value pre = is_type(k->s[4]) ? type_hook(k->s[4], "__prepare__") : Value();
        if (pre.is_nil())
            return build_step(k, Value());
        Root bound;
        if (type_bind(pre, Value(), k->s[4], bound.v) != R::Ok)
            return R::Err;
        TupleObj *two = tuple_new(2);
        if (!two)
            return oom();
        Root args{ obj_value(two) };
        two->items()[0] = k->s[1];
        two->items()[1] = k->s[2];
        TupleObj *pair  = static_cast<TupleObj *>(k->s[5].obj());
        return cont_call_kw(k, bound.v, args.v, pair->items()[0], pair->items()[1]);
    }
    case 2:
        if (!in.is_nil()) {
            if (!is_dict(in) && !is_inst(in)) {
                Buf<128> m;
                m.put(type_obj(k->s[4])->slots.name)
                    .put(".__prepare__() must return a mapping, not ");
                m.put(type_name(in));
                return err_set("TypeError", m.str());
            }
            k->s[3]   = in;
            k->locals = in;
        }
        k->i = 3;
        return cont_call(k, k->s[0], Value(), 0);

    case 3: {
        // The body has run and its namespace is the class. Calling the
        // metaclass is what makes one, so that a metaclass with a __new__ or
        // an __init__ of its own is obeyed, and the class keywords reach it.
        // The body answers its __class__ cell, if it has one.
        k->i    = 4;
        k->s[7] = in;
        if (!k->s[6].is_nil()) {
            StrObj *ob = str_intern("__orig_bases__");
            Value ns   = is_inst(k->s[3]) ? inst_of(k->s[3])->native : k->s[3];
            if (!ob || !is_dict(ns) || dict_set(dict_at(ns), obj_value(ob), k->s[6]) != R::Ok)
                return R::Err;
        }
        TupleObj *t = tuple_new(3);
        if (!t)
            return oom();
        Root args{ obj_value(t) };
        t->items()[0]  = k->s[1];
        t->items()[1]  = k->s[2];
        t->items()[2]  = k->s[3];
        TupleObj *pair = static_cast<TupleObj *>(k->s[5].obj());
        k->s[0]        = k->s[1]; // the name, for the cell check
        k->s[1]        = pair->items()[0];
        k->s[2]        = pair->items()[1];
        return cont_call_kw(k, k->s[4], args.v, k->s[1], k->s[2]);
    }
    default:
        // The hooks ran inside type.__new__, which is where CPython runs them;
        // all that is left is to say what the bases were written as.
        if (is_type(in) && !k->s[6].is_nil())
            type_obj(in)->origbases = k->s[6];
        // type.__new__ fills the cell; a metaclass that drops __classcell__
        // leaves it empty.
        if (is_type(in) && k->s[7].is_obj() && k->s[7].obj()->type == &cell_type) {
            Value held = static_cast<CellObj *>(k->s[7].obj())->v;
            if (held != in) {
                Root cls{ in };
                String m;
                if (held.is_nil() ? !m.append("__class__ not set defining ")
                                  : (!m.append("__class__ set to ") || py_repr(held, m) != R::Ok ||
                                     !m.append(" defining ")))
                    return err_pending() ? R::Err : oom();
                if (py_repr(k->s[0], m) != R::Ok || !m.append(" as ") || py_repr(cls.v, m) != R::Ok)
                    return err_pending() ? R::Err : oom();
                if (held.is_nil()) {
                    if (!m.append(". Was __classcell__ propagated to type.__new__?"))
                        return oom();
                    return err_set("RuntimeError", m.str());
                }
                return err_set("TypeError", m.str());
            }
        }
        return cont_done(k, in);
    }
}

// __build_class__(body, name, *bases, **kwds): the shape the compiler emits.
R b_build_class(const CallArgs &a, Value &out)
{
    if (a.nargs < 2 || !is_func(a.args[0]))
        return err_set("TypeError", "__build_class__() takes a function and a name");

    TupleObj *bases = tuple_new(a.nargs - 2);
    if (!bases)
        return oom();
    for (u32 i = 2; i < a.nargs; i++)
        bases->items()[i - 2] = a.args[i];
    Root rb{ obj_value(bases) };
    Kwds kw;
    if (!kwds_split(a, kw))
        return R::Err;
    TupleObj *pair = tuple_new(2);
    if (!pair)
        return oom();
    pair->items()[0] = kw.names.v;
    pair->items()[1] = kw.vals.v;
    Root rp{ obj_value(pair) };
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
    k->s[4]    = kw.meta.v;
    k->s[5]    = rp.v;
    k->locals  = rn.v;
    out        = kv.v;
    return R::Ok;
}

// type(x), type(name, bases, dict) and type.__new__(mcls, name, bases, dict),
// which is what a metaclass reaches through super(). The hooks a fresh class
// owes run here, as they do in CPython's type.__new__, so that a class made by
// calling `type` gets them too.
// type(name, bases, dict) whose bases carry a metaclass of their own: that
// metaclass is what makes the class, and it may have written a __new__.
R meta_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call_kw(k, k->s[0], k->s[1], k->s[2], k->s[3]);
    return cont_done(k, in);
}

R b_type(const CallArgs &a, Value &out)
{
    Root cls;
    if (a.nargs == 4 && is_type(a.args[0]))
        cls = type_new_meta(a.args[0], a.args[1], a.args[2], a.args[3]);
    else if (a.nargs == 3) {
        Root won{ meta_of(Value(), a.args[1]) };
        if (won.v.is_nil() && err_pending())
            return R::Err;
        Root plain{ type_wrap(&type_type) };
        if (plain.v.is_nil())
            return R::Err;
        if (!won.v.is_nil() && won.v != plain.v) {
            TupleObj *t = tuple_new(3);
            if (!t)
                return oom();
            Root args{ obj_value(t) };
            for (u32 i = 0; i < 3; i++)
                t->items()[i] = a.args[i];
            TupleObj *kn = tuple_new(a.nkw);
            if (!kn)
                return oom();
            Root rn{ obj_value(kn) };
            TupleObj *kv = tuple_new(a.nkw);
            if (!kv)
                return oom();
            Root rv{ obj_value(kv) };
            for (u32 i = 0; i < a.nkw; i++) {
                static_cast<TupleObj *>(rn.v.obj())->items()[i] = a.kwnames[i];
                static_cast<TupleObj *>(rv.v.obj())->items()[i] = a.kwvals[i];
            }
            Root k{ cont_new(meta_step) };
            if (k.v.is_nil())
                return R::Err;
            cont_of(k.v)->s[0] = won.v;
            cont_of(k.v)->s[1] = args.v;
            cont_of(k.v)->s[2] = rn.v;
            cont_of(k.v)->s[3] = rv.v;
            out                = k.v;
            return R::Ok;
        }
        cls = type_new(a.args[0], a.args[1], a.args[2]);
    } else {
        if (!args_only(a, "type", 1, 1))
            return R::Err;
        out = type_of_value(a.args[0]);
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (cls.v.is_nil())
        return R::Err;
    TupleObj *names = tuple_new(a.nkw);
    if (!names)
        return oom();
    Root rn{ obj_value(names) };
    TupleObj *vals = tuple_new(a.nkw);
    if (!vals)
        return oom();
    Root rv{ obj_value(vals) };
    for (u32 i = 0; i < a.nkw; i++) {
        static_cast<TupleObj *>(rn.v.obj())->items()[i] = a.kwnames[i];
        static_cast<TupleObj *>(rv.v.obj())->items()[i] = a.kwvals[i];
    }
    out = type_hooks(cls.v, rn.v, rv.v);
    return out.is_nil() ? R::Err : R::Ok;
}

// The methods `object` lends every class. Finding one of these on a class is
// finding the default, which is not a hook and not an __init__ of its own.
R b_object_init(const CallArgs &a, Value &out)
{
    (void)a;
    out = value_none();
    return R::Ok;
}

R b_object_getattribute(const CallArgs &a, Value &out)
{
    if (!args_only(a, "__getattribute__", 2, 2) || !is_str(a.args[1]))
        return is_str(a.args[1])
                   ? R::Err
                   : err_set2("TypeError", "attribute name must be a string", type_name(a.args[1]));
    StrObj *n = str_intern(str_of(a.args[1])->str());
    if (!n)
        return oom();
    // The default algorithm, which is what this is: py_attr would put the
    // class's own __getattribute__ back in front of it.
    Root args;
    switch (attr_plain(a.args[0], n, out, args.v)) {
    case Got::Ok:
        return R::Ok;
    case Got::Error:
        return R::Err;
    case Got::Call:
        out = attr_invoke(out, args.v);
        return out.is_nil() ? R::Err : R::Ok;
    default:
        break;
    }
    Buf<96> m;
    m.put("'").put(type_name(a.args[0])).put("' object has no attribute '");
    m.put(n->str()).put("'");
    return err_set("AttributeError", m.str());
}

R b_object_setattr(const CallArgs &a, Value &out)
{
    if (!args_only(a, "__setattr__", 3, 3) || !is_str(a.args[1]))
        return err_set("TypeError", "__setattr__() takes an object, a name and a value");
    StrObj *n = str_intern(str_of(a.args[1])->str());
    if (!n)
        return oom();
    Value fn;
    if (attr_plain_store(a.args[0], n, a.args[2], fn) != R::Ok)
        return R::Err;
    out = fn.is_nil() ? value_none() : fn;
    return R::Ok;
}

R b_object_delattr(const CallArgs &a, Value &out)
{
    if (!args_only(a, "__delattr__", 2, 2) || !is_str(a.args[1]))
        return err_set("TypeError", "__delattr__() takes an object and a name");
    StrObj *n = str_intern(str_of(a.args[1])->str());
    if (!n)
        return oom();
    Value fn;
    if (attr_plain_delete(a.args[0], n, fn) != R::Ok)
        return R::Err;
    out = fn.is_nil() ? value_none() : fn;
    return R::Ok;
}

// object.__init_subclass__ takes nothing, which is what makes an unconsumed
// class keyword an error rather than a silence.
R b_object_init_subclass(const CallArgs &a, Value &out)
{
    if (a.nkw)
        return err_set2("TypeError", "__init_subclass__() takes no keyword arguments",
                        is_str(a.kwnames[0]) ? str_of(a.kwnames[0])->str() : Str("?"));
    out = value_none();
    return R::Ok;
}

// object.__repr__(x): the default, whatever the class wrote.
R b_object_repr(const CallArgs &a, Value &out)
{
    if (!args_only(a, "__repr__", 1, 1))
        return R::Err;
    char tmp[24];
    Buf<128> b;
    b.put("<").put(type_name(a.args[0])).put(" object at ");
    b.put(a.args[0].is_obj() ? addr_text(tmp, sizeof tmp, a.args[0].obj()) : Str("0x0"));
    b.put('>');
    out = str_new(b.str());
    return out.is_nil() ? R::Err : R::Ok;
}

// object.__new__(cls), which is where every class's instance comes from.
R b_object(const CallArgs &a, Value &out);

} // namespace

bool is_object_default(Value v)
{
    if (v.is_obj() && v.obj()->type == &classmethod_type)
        v = static_cast<WrapObj *>(v.obj())->fn;
    if (!is_native(v))
        return false;
    R (*f)(const CallArgs &, Value &) = static_cast<NativeObj *>(v.obj())->fn;
    return f == b_object_init || f == b_object_getattribute || f == b_object_setattr ||
           f == b_object_delattr || f == b_object_init_subclass || f == b_object ||
           f == b_object_repr || objmeth_is(f);
}

namespace {

// ----------------------------------------------------------- the class hooks

// s[0] the class, s[1] the keyword names, s[2] their values, s[3] the
// namespace entries still to visit; j is where in them we are.
R hooks_step(ContObj *k, Value in)
{
    (void)in;
    Value ns = type_obj(k->s[0])->dict;
    for (; k->j < tuple_len(k->s[3]);) {
        Value name = tuple_at(k->s[3], k->j++);
        Value val;
        if (dict_get(dict_at(ns), name, val) != R::Ok)
            continue;
        // A class instance answers through its class, a built-in through its
        // own method table: a property is one of the latter.
        Value sn  = type_special(val, "__set_name__");
        StrObj *n = str_intern("__set_name__");
        if (sn.is_nil() && n && val.is_obj() && method_find(val, n, sn) != R::Ok)
            sn = Value();
        if (sn.is_nil())
            continue;
        return cont_call(k, sn, k->s[0], 2, name);
    }
    if (k->i++)
        return cont_done(k, k->s[0]);

    // super(cls, cls).__init_subclass__(**kwds), which is an implicit
    // classmethod: the base is told a subclass of it has been made.
    StrObj *isc = str_intern("__init_subclass__");
    if (!isc)
        return oom();
    Value mro = type_obj(k->s[0])->mro;
    for (usize i = 1; i < tuple_len(mro); i++) {
        Value c = tuple_at(mro, i);
        Value found;
        if (dict_get(dict_at(type_obj(c)->dict), obj_value(isc), found) != R::Ok)
            continue;
        if (is_object_default(found))
            break;
        Root bound;
        // Written plain or as a classmethod, it is bound to the new class.
        if (found.is_obj() && found.obj()->type == &classmethod_type)
            found = static_cast<WrapObj *>(found.obj())->fn;
        bound = method_new(found, k->s[0]);
        if (bound.v.is_nil())
            return R::Err;
        TupleObj *none = tuple_new(0);
        if (!none)
            return oom();
        return cont_call_kw(k, bound.v, obj_value(none), k->s[1], k->s[2]);
    }
    if (tuple_len(k->s[1]))
        return err_set2(
            "TypeError", "__init_subclass__() takes no keyword arguments",
            is_str(tuple_at(k->s[1], 0)) ? str_of(tuple_at(k->s[1], 0))->str() : Str("?"));
    return cont_done(k, k->s[0]);
}

// isinstance and issubclass take a type or a tuple of them.
R any_of(Value t, Value v, bool cls, bool &out)
{
    if (is_union(t)) {
        t = union_as_tuple(t, cls ? "issubclass" : "isinstance");
        if (t.is_nil())
            return R::Err;
    }
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

// A metaclass may answer __instancecheck__ or __subclasscheck__ for its
// classes, which is what abc's registration stands on. Only for a single
// class: a tuple is each of them in turn, and the ordinary answer is enough
// where the metaclass says nothing.
Value check_hook(Value t, Str name)
{
    if (!is_type(t))
        return Value();
    Value meta = type_of_value(t);
    if (meta.is_nil())
        return err_clear(), Value();
    Value fn = type_hook(meta, name);
    if (fn.is_nil())
        return Value();
    Value bound;
    return type_bind(fn, t, meta, bound) == R::Ok ? bound : Value();
}

// s[0] the bound check, s[1] the object, s[2] what is left of a tuple of
// types, s[3] the object again for the plain answer; j says which check.
R check_step(ContObj *k, Value in)
{
    if (k->i++ && py_truth(in))
        return cont_done(k, value_bool(true));
    for (; k->j < tuple_len(k->s[2]);) {
        Value t = tuple_at(k->s[2], k->j++);
        Value h = check_hook(t, k->s[3].is_nil() ? "__instancecheck__" : "__subclasscheck__");
        if (!h.is_nil())
            return cont_call(k, h, k->s[1]);
        bool yes = false;
        if (any_of(t, k->s[1], !k->s[3].is_nil(), yes) != R::Ok)
            return R::Err;
        if (yes)
            return cont_done(k, value_bool(true));
    }
    return cont_done(k, value_bool(false));
}

R check_any(const CallArgs &a, Str who, bool sub, Value &out)
{
    if (!args_only(a, who, 2, 2))
        return R::Err;
    if (sub && !is_type(a.args[0]) && check_hook(a.args[1], "__subclasscheck__").is_nil())
        return err_set2("TypeError", "issubclass() argument 1 must be a class",
                        type_name(a.args[0]));
    // One type or a tuple of them, over one path.
    Root ts{ a.args[1] };
    if (!is_tuple(ts.v)) {
        TupleObj *one = tuple_new(1);
        if (!one)
            return oom();
        one->items()[0] = a.args[1];
        ts              = obj_value(one);
    }
    bool hooked = false;
    for (usize i = 0; i < tuple_len(ts.v) && !hooked; i++)
        hooked = !check_hook(tuple_at(ts.v, i), sub ? "__subclasscheck__" : "__instancecheck__")
                      .is_nil();
    if (!hooked) {
        bool yes = false;
        if (any_of(ts.v, a.args[0], sub, yes) != R::Ok)
            return R::Err;
        out = value_bool(yes);
        return R::Ok;
    }
    Root kv{ cont_new(check_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[1] = a.args[0];
    cont_of(kv.v)->s[2] = ts.v;
    cont_of(kv.v)->s[3] = sub ? a.args[0] : Value();
    out                 = kv.v;
    return R::Ok;
}

R b_isinstance(const CallArgs &a, Value &out)
{
    return check_any(a, "isinstance", false, out);
}

R b_issubclass(const CallArgs &a, Value &out)
{
    return check_any(a, "issubclass", true, out);
}

// type.mro(): the linearization as a fresh list, which is what a metaclass
// overriding __subclasscheck__ walks.
R m_type_mro(const CallArgs &a, Value &out)
{
    if (!args_only(a, "mro", 1, 1) || !is_type(a.args[0]))
        return err_set2("TypeError", "descriptor 'mro' requires a type", type_name(a.args[0]));
    Root rc{ a.args[0] };
    ListObj *l = list_new();
    if (!l)
        return oom();
    Root rl{ obj_value(l) };
    Value m = type_obj(rc.v)->mro;
    for (usize i = 0; i < tuple_len(m); i++)
        if (!list_push(list_of(rl.v), tuple_at(m, i)))
            return oom();
    out = rl.v;
    return R::Ok;
}

// type.__subclasses__(): the classes made directly under this one, in the
// order they were made.
R m_type_subclasses(const CallArgs &a, Value &out)
{
    if (!args_only(a, "__subclasses__", 1, 1) || !is_type(a.args[0]))
        return err_set2("TypeError", "descriptor '__subclasses__' requires a type",
                        type_name(a.args[0]));
    Root rc{ a.args[0] };
    ListObj *l = list_new();
    if (!l)
        return oom();
    Root rl{ obj_value(l) };
    Value subs = type_obj(rc.v)->subs;
    for (usize i = 0; !subs.is_nil() && i < list_of(subs)->items.size(); i++)
        if (!list_push(list_of(rl.v), list_of(subs)->items[i]))
            return oom();
    out = rl.v;
    return R::Ok;
}

constexpr Method TYPE_METHODS[] = { { "mro", m_type_mro },
                                    { "__subclasses__", m_type_subclasses } };

// object.__new__(cls, ...), which is where every class's instance comes from.
// Anything after the class is what a subclass's __init__ will take, and is
// ignored here the way CPython ignores it.
R b_object(const CallArgs &a, Value &out)
{
    Value cls = a.nargs ? a.args[0] : type_object();
    if (!is_type(cls))
        return err_set2("TypeError", "object.__new__() argument must be a type", type_name(cls));
    // A built-in type is laid out by its own constructor, if it has one.
    if (!type_obj(cls)->heap && cls != type_object()) {
        Buf<160> m;
        StrObj *nw = str_intern("__new__");
        Value own;
        if (nw && dict_get(static_cast<DictObj *>(type_obj(cls)->dict.obj()), obj_value(nw), own) ==
                      R::Ok)
            m.put("object.__new__(")
                .put(type_obj(cls)->slots.name)
                .put(") is not safe, use ")
                .put(type_obj(cls)->slots.name)
                .put(".__new__()");
        else
            m.put("cannot create '").put(type_obj(cls)->slots.name).put("' instances");
        return err_set("TypeError", m.str());
    }
    out = inst_new(cls);
    return out.is_nil() ? R::Err : R::Ok;
}

} // namespace

namespace {

Value prop_new(Value get, Value set, Value del, Value doc)
{
    Root a{ get }, b{ set }, c{ del }, d{ doc };
    PropObj *p = static_cast<PropObj *>(obj_alloc(&property_type, sizeof(PropObj)));
    if (!p)
        return oom(), Value();
    p->get   = a.v;
    p->set   = b.v;
    p->del   = c.v;
    p->doc   = d.v;
    p->pname = Value();
    return obj_value(p);
}

} // namespace

Value property_of(Value get, Value set)
{
    return prop_new(get, set, Value(), Value());
}

namespace {

// property.getter/setter/deleter: a copy with one of the three replaced.
R prop_with(const CallArgs &a, u32 which, Value &out)
{
    if (a.nargs != 2 || a.nkw)
        return err_set("TypeError", "property.setter() takes one argument");
    PropObj *p = static_cast<PropObj *>(a.args[0].obj());
    Value g = p->get, s = p->set, d = p->del;
    (which == 0 ? g : which == 1 ? s : d) = a.args[1];
    out                                   = prop_new(g, s, d, p->doc);
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
    PropObj *p = static_cast<PropObj *>(v.obj());
    Str n      = name->str();
    if (n == "fget")
        return out = p->get.is_nil() ? value_none() : p->get, R::Ok;
    if (n == "fset")
        return out = p->set.is_nil() ? value_none() : p->set, R::Ok;
    if (n == "fdel")
        return out = p->del.is_nil() ? value_none() : p->del, R::Ok;
    if (n == "__doc__")
        return out = p->doc.is_nil() ? value_none() : p->doc, R::Ok;
    if (n == "__name__") {
        if (p->pname.is_nil())
            return err_set("AttributeError", "__name__ is not set"), R::Err;
        return out = p->pname, R::Ok;
    }
    if (n == "__isabstractmethod__") {
        // Abstract where any of the three it wraps is, which is what makes an
        // abstract property keep its class abstract.
        StrObj *am     = str_intern("__isabstractmethod__");
        bool yes       = false;
        Value three[3] = { p->get, p->set, p->del };
        for (Value one : three) {
            Value got;
            if (one.is_nil() || !am)
                continue;
            if (py_getattr(one, am, got) == R::Ok)
                yes = yes || py_truth(got);
            else
                err_clear();
        }
        return out = value_bool(yes), R::Ok;
    }
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

// property.__doc__ may be assigned, which is how a decorator that wraps one
// carries the docstring across.
R prop_setattr(Value v, StrObj *name, Value val)
{
    PropObj *p = static_cast<PropObj *>(v.obj());
    if (name->str() == Str("__doc__"))
        return p->doc = val, R::Ok;
    return err_set2("AttributeError", "property has no attribute", name->str());
}

// property.__set_name__(owner, name): what the class it was written in is
// called, kept for the diagnostics and for __name__.
R b_prop_set_name(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__set_name__", 2, 2))
        return R::Err;
    static_cast<PropObj *>(method_self(a.args[0]).obj())->pname = a.args[2];
    out                                                         = value_none();
    return R::Ok;
}

constexpr Method PROPERTY_METHODS[] = { { "__set_name__", b_prop_set_name } };

R b_property(const CallArgs &a, Value &out)
{
    Root get, set, del, doc;
    if (a.nargs > 0)
        get = a.args[0];
    if (a.nargs > 1)
        set = a.args[1];
    if (a.nargs > 2)
        del = a.args[2];
    if (a.nargs > 3)
        doc = a.args[3];
    for (u32 i = 0; i < a.nkw; i++) {
        Str n = is_str(a.kwnames[i]) ? str_of(a.kwnames[i])->str() : Str();
        if (n == "fget")
            get = a.kwvals[i];
        else if (n == "fset")
            set = a.kwvals[i];
        else if (n == "fdel")
            del = a.kwvals[i];
        else if (n == "doc")
            doc = a.kwvals[i];
        else
            return err_set2("TypeError", "property() got an unexpected keyword argument", n);
    }
    // The getter's docstring, where property() was given none of its own.
    if (doc.v.is_nil() && is_func(get.v))
        doc = func_of(get.v)->doc;
    out = prop_new(get.v, set.v, del.v, doc.v);
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
    w->fn   = rf.v;
    w->dict = Value();
    out     = obj_value(w);
    return R::Ok;
}

} // namespace

Value classmethod_new(Value fn)
{
    Value out;
    Value args[1] = { fn };
    CallArgs a;
    a.args  = args;
    a.nargs = 1;
    return wrap_one(a, &classmethod_type, "classmethod", out) == R::Ok ? out : Value();
}

namespace {

R b_staticmethod(const CallArgs &a, Value &out)
{
    return wrap_one(a, &staticmethod_type, "staticmethod", out);
}

R b_classmethod(const CallArgs &a, Value &out)
{
    return wrap_one(a, &classmethod_type, "classmethod", out);
}

// Zero-argument super(): the running function's first argument, and the
// class its __class__ cell holds. A list, set or dict comprehension is part of
// the function it is written in, as CPython inlines it.
R super_here(Value &self, Value &cls)
{
    Value fr = vm_frame();
    if (fr.is_nil())
        return err_set("RuntimeError", "super(): no current frame");
    FrameObj *f = frame_of(fr);
    CodeObj *c  = code_of(f->code);
    Str name    = is_str(c->name) ? str_of(c->name)->str() : Str();
    FrameObj *a = f;
    if ((name == "<listcomp>" || name == "<setcomp>" || name == "<dictcomp>") && !f->back.is_nil())
        a = frame_of(f->back);
    CodeObj *ac = code_of(a->code);
    if (!ac->argcount)
        return err_set("RuntimeError", "super(): no arguments");
    // The first argument, or its cell where a nested scope captured it.
    self = a->slots()[0];
    for (usize i = 0; i < ac->cellvars.size() && !a->cells.is_nil(); i++) {
        if (ac->cellvars[i] != ac->varnames[0])
            continue;
        Value cell = static_cast<TupleObj *>(a->cells.obj())->items()[i];
        self       = cell.is_nil() ? Value() : static_cast<CellObj *>(cell.obj())->v;
    }
    if (self.is_nil())
        return err_set("RuntimeError", "super(): arg[0] deleted");
    usize own = c->cellvars.size();
    for (usize i = 0; i < c->freevars.size(); i++) {
        if (str_of(c->freevars[i])->str() != "__class__")
            continue;
        Value cell =
            f->cells.is_nil() ? Value() : static_cast<TupleObj *>(f->cells.obj())->items()[own + i];
        cls = cell.is_nil() ? Value() : static_cast<CellObj *>(cell.obj())->v;
        if (cls.is_nil())
            return err_set("RuntimeError", "super(): empty __class__ cell");
        if (!is_type(cls)) {
            Buf<128> m;
            m.put("super(): __class__ is not a type (").put(type_name(cls)).put(')');
            return err_set("RuntimeError", m.str());
        }
        return R::Ok;
    }
    return err_set("RuntimeError", "super(): __class__ cell not found");
}

// super(type, obj) wants obj to be an instance or a subclass of type.
R super_check(Value t, Value obj)
{
    bool ok = (is_type(obj) && type_issub(obj, t)) || type_isinstance(obj, t);
    if (ok)
        return R::Ok;
    Buf<256> m;
    m.put("super(type, obj): obj (");
    if (is_type(obj))
        m.put("type ").put(type_obj(obj)->slots.name);
    else
        m.put("instance of ").put(type_name(obj));
    m.put(") is not an instance or subtype of type (").put(type_obj(t)->slots.name).put(").");
    return err_set("TypeError", m.str());
}

R b_super(const CallArgs &a, Value &out)
{
    if (!a.nargs && !a.nkw) {
        Root self, cls;
        if (super_here(self.v, cls.v) != R::Ok || super_check(cls.v, self.v) != R::Ok)
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
    // A class is the second argument twice over: as a subclass of the first,
    // and as an instance of it where the first is a metaclass.
    if (super_check(a.args[0], a.args[1]) != R::Ok)
        return R::Err;
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
};

// The four that are types rather than functions, so that `type(property(f))`
// is `property` and abc.py's `class abstractproperty(property)` is a class.
struct Ctor {
    const Type *t;
    R (*fn)(const CallArgs &, Value &out);
};

constexpr Ctor CLASS_TYPES[] = {
    { &property_type, b_property },
    { &staticmethod_type, b_staticmethod },
    { &classmethod_type, b_classmethod },
    { &super_type, b_super },
};

// What `object` lends every class. Each is named "object" so that finding one
// on a class says the class did not write that method itself.
constexpr Named OBJECT_METHODS[] = {
    { "__new__", b_object },
    { "__init__", b_object_init },
    { "__getattribute__", b_object_getattribute },
    { "__setattr__", b_object_setattr },
    { "__delattr__", b_object_delattr },
    { "__init_subclass__", b_object_init_subclass },
    { "__repr__", b_object_repr },
};

// The built-in types a program can name, subclass or test against.
const Type *const NAMED[] = { &int_type,        &float_type,     &bool_type,    &str_type,
                              &bytes_type,      &bytearray_type, &tuple_type,   &list_type,
                              &dict_type,       &set_type,       &range_type,   &type_type,
                              &frozenset_type,  &slice_type,     &memview_type, &complex_type,
                              &frozendict_type, &sentinel_type };

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
    // `object` answers a call itself; the rest are constructors already. The
    // others are the defaults every class inherits and may override, and they
    // have to be reachable -- super().__setattr__(n, v) is the ordinary way to
    // write a __setattr__ that stores after all.
    for (const Named &e : OBJECT_METHODS)
        if (!put(type_obj(ob.v)->dict, e.name, native_new("object", e.fn)))
            return false;
    if (!objmeth_install(type_obj(ob.v)->dict))
        return false;
    for (const Type *t : NAMED) {
        Value w = type_wrap(t);
        if (w.is_nil() || !put(rd.v, t->name, w))
            return false;
    }
    // type's own methods, which a class reaches through its metatype.
    if (!method_install(&type_type, TYPE_METHODS))
        return false;
    if (!method_install(&property_type, PROPERTY_METHODS))
        return false;
    for (const Ctor &e : CLASS_TYPES) {
        Value w = type_wrap(e.t);
        Root fn{ native_new(e.t->name, e.fn) };
        if (w.is_nil() || !put(rd.v, e.t->name, w) || !type_set_ctor(e.t, fn.v))
            return false;
    }
    // type(x) and type(name, bases, dict) are both calls of the type `type`.
    Root mk{ native_new("type", b_type) };
    return type_set_ctor(&type_type, mk.v);
}

namespace {

void newwrap_trace(Obj *o)
{
    NewObj *w = static_cast<NewObj *>(o);
    gc_mark(w->ctor);
    gc_mark(w->type);
}

R newwrap_repr(Value v, String &out)
{
    Buf<96> b;
    b.put("<built-in method __new__ of type object>");
    (void)v;
    return out.append(b.str()) ? R::Ok : err_set("MemoryError", "out of memory");
}

} // namespace

constexpr Type newwrap_type{ .name  = "builtin_function_or_method",
                             .trace = newwrap_trace,
                             .repr  = newwrap_repr };

Value type_new_attr(Value found, Value owner)
{
    if (!is_native(found) || !(found.obj()->flags & OBJ_CTOR) || owner.is_nil() ||
        type_obj(owner)->heap)
        return found;
    Root rf{ found }, ro{ owner };
    NewObj *w = static_cast<NewObj *>(obj_alloc(&newwrap_type, sizeof(NewObj)));
    if (!w)
        return err_set("MemoryError", "out of memory"), Value();
    w->ctor = rf.v;
    w->type = ro.v;
    return obj_value(w);
}

R newwrap_call(Value wv, const CallArgs &a, Value &out)
{
    NewObj *w = static_cast<NewObj *>(wv.obj());
    Root ctor{ w->ctor }, base{ w->type };
    Str tn = type_obj(base.v)->slots.name;
    if (!a.nargs) {
        Buf<96> b;
        b.put(tn).put(".__new__(): not enough arguments");
        return err_set("TypeError", b.str());
    }
    Value cls = a.args[0];
    if (!is_type(cls)) {
        Buf<96> b;
        b.put(tn).put(".__new__(X): X is not a type object");
        return err_set2("TypeError", b.str(), type_name(cls));
    }
    CallArgs rest = a;
    rest.args     = a.args + 1;
    rest.nargs    = a.nargs - 1;
    if (cls == base.v)
        return static_cast<NativeObj *>(ctor.v.obj())->fn(rest, out);
    if (!type_issub(cls, base.v) || !type_obj(cls)->heap) {
        Buf<128> b;
        b.put(tn).put(".__new__(").put(type_obj(cls)->slots.name).put("): ");
        b.put(type_obj(cls)->slots.name).put(" is not a subtype of ").put(tn);
        return err_set("TypeError", b.str());
    }
    Root rc{ cls };
    Root made;
    if (static_cast<NativeObj *>(ctor.v.obj())->fn(rest, made.v) != R::Ok)
        return R::Err;
    if (is_cont(made.v))
        return err_set("TypeError", "__new__ of a built-in cannot wait on Python here");
    Root self{ inst_new(rc.v) };
    if (self.v.is_nil())
        return R::Err;
    made = weak_adopt(made.v, self.v);
    if (made.v.is_nil())
        return R::Err;
    inst_of(self.v)->native = made.v;
    out                     = self.v;
    return R::Ok;
}

bool type_set_ctor(const Type *t, Value fn)
{
    Root rf{ fn };
    // type(name, bases, ns) is the one that takes its class already.
    if (t != &type_type && is_native(rf.v))
        rf.v.obj()->flags |= OBJ_CTOR;
    Value w = type_wrap(t);
    return !w.is_nil() && put(type_obj(w)->dict, "__new__", rf.v);
}

Value meta_special(Value v, Str name);

Value type_special(Value v, Str name)
{
    // What a weak proxy's referent answers in Python, the proxy answers.
    if (is_weakproxy(v)) {
        v = proxy_target(v);
        if (v.is_nil())
            return err_clear(), Value();
    }
    // A class whose metaclass was written in Python asks the metaclass.
    if (is_meta_inst(v))
        return meta_special(v, name);
    if (!is_inst(v))
        return Value();
    StrObj *n = str_intern(name);
    Root found;
    // object's own default is what the native path already does.
    if (!n || type_lookup(inst_of(v)->cls, n, found.v) != R::Ok || is_object_default(found.v))
        return Value();
    // A staticmethod or a classmethod here too: the wrapper is not callable.
    Value out;
    return type_bind(found.v, v, inst_of(v)->cls, out) == R::Ok ? out : Value();
}

R py_isinstance(const CallArgs &a, Value &out)
{
    return b_isinstance(a, out);
}

Value operand_special(Value v, Str name)
{
    if (!is_meta_inst(v))
        return type_special(v, name);
    return meta_special(v, name);
}

Value meta_special(Value v, Str name)
{
    Root rv{ v };
    Root meta{ obj_value(rv.v.obj()->type->owner) };
    Value fn = type_hook(meta.v, name);
    if (fn.is_nil())
        return Value();
    Value bound;
    return type_bind(fn, rv.v, meta.v, bound) == R::Ok ? bound : Value();
}

Value type_getitem_of(Value v)
{
    if (!is_type(v))
        return Value();
    Root rv{ v };
    Root meta{ type_of_value(rv.v) };
    if (meta.v.is_nil())
        return err_clear(), Value();
    Value g = type_hook(meta.v, "__getitem__");
    if (!g.is_nil()) {
        Value bound;
        return type_bind(g, rv.v, meta.v, bound) == R::Ok ? bound : Value();
    }
    StrObj *n = str_intern("__class_getitem__");
    Root found;
    if (!n || type_lookup(rv.v, n, found.v) != R::Ok)
        return Value();
    if (found.v.is_obj() && found.v.obj()->type == &classmethod_type)
        found = static_cast<WrapObj *>(found.v.obj())->fn;
    return method_new(found.v, rv.v);
}

bool type_unhashable(Value v)
{
    if (!is_inst(v))
        return false;
    StrObj *n = str_intern("__hash__");
    Value found;
    return n && type_lookup(inst_of(v)->cls, n, found) == R::Ok && is_none(found);
}

bool type_has_special(Value v, Str name)
{
    if (is_weakproxy(v)) {
        Obj *t = static_cast<WeakRefObj *>(v.obj())->target;
        if (!t)
            return false;
        v = Value::of_obj(t);
    }
    if (is_meta_inst(v))
        return !type_hook(obj_value(v.obj()->type->owner), name).is_nil();
    if (!is_inst(v))
        return false;
    StrObj *n = str_intern(name);
    Value found;
    return n && type_lookup(inst_of(v)->cls, n, found) == R::Ok && !is_object_default(found);
}

bool type_has_py_special(Value v, Str name)
{
    if (is_weakproxy(v)) {
        Obj *t = static_cast<WeakRefObj *>(v.obj())->target;
        if (!t)
            return false;
        v = Value::of_obj(t);
    }
    if (is_meta_inst(v)) {
        Value fn = type_hook(obj_value(v.obj()->type->owner), name);
        return !fn.is_nil() && !is_native(fn);
    }
    if (!is_inst(v))
        return false;
    StrObj *n = str_intern(name);
    Value found;
    if (!n || type_lookup(inst_of(v)->cls, n, found) != R::Ok)
        return false;
    // A subclass of a built-in inherits the protocol methods from the
    // built-in's own namespace, and each of those is a native over the slot
    // the instance already carries. Only a method written in Python needs a
    // frame, and that is the whole question here.
    return !is_native(found) || (found.obj()->flags & OBJ_PYLIKE);
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
           d == &float_type || d == &weakref_type || d == &frozendict_type || d == &frozenset_type;
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
    Root rbt{ rb.v };
    if (!is_tuple(rb.v)) {
        TupleObj *bases = tuple_new(1);
        if (!bases)
            return oom(), Value();
        bases->items()[0] = rb.v;
        rbt               = obj_value(bases);
    }
    type_obj(ro.v)->bases = rbt.v;
    Value m               = mro_of(ro.v, rbt.v);
    if (m.is_nil())
        return Value();
    type_obj(ro.v)->mro = m;
    return ro.v;
}

void type_note_del(Value cls)
{
    if (!is_type(cls))
        return;
    StrObj *n = str_intern("__del__");
    Value found;
    type_obj(cls)->hasdel = n && type_lookup(cls, n, found) == R::Ok;
}

Obj *type_alloc_inst(Value cls, usize bytes)
{
    Root rc{ cls };
    TypeObj *c = type_obj(rc.v);
    usize need = c->nslots ? c->slotoff + c->nslots * sizeof(Value) : bytes;
    Obj *o     = obj_alloc(&c->slots, need);
    if (!o)
        return oom(), nullptr;
    if (c->hasdel)
        o->flags |= OBJ_FINAL;
    InstObj *i = static_cast<InstObj *>(o);
    i->cls     = rc.v;
    i->dict    = Value();
    i->native  = Value();
    // The slots are Nil, which is what "never assigned" means to a member.
    Value *s = reinterpret_cast<Value *>(reinterpret_cast<char *>(o) + c->slotoff);
    for (u32 k = 0; k < c->nslots; k++)
        s[k] = Value();
    return o;
}

Value type_hook(Value cls, Str name)
{
    StrObj *n = str_intern(name);
    Value found;
    if (!n || type_lookup(cls, n, found) != R::Ok)
        return Value();
    // The default is not a hook: finding it means the class wrote none.
    return is_object_default(found) ? Value() : found;
}

Value type_hooks(Value cls, Value kwnames, Value kwvals)
{
    Root rc{ cls }, rn{ kwnames }, rv{ kwvals };
    // A snapshot of the namespace: __set_name__ may add to it, and walking a
    // table that is being written to is not safe.
    DictObj *ns = dict_at(type_obj(rc.v)->dict);
    TupleObj *t = tuple_new(dict_len(ns));
    if (!t)
        return oom(), Value();
    Root rt{ obj_value(t) };
    usize at = 0, i = 0;
    Value k, v;
    while (table_next(ns->t, at, k, v) && i < t->len)
        static_cast<TupleObj *>(rt.v.obj())->items()[i++] = k;

    if (rn.v.is_nil()) {
        TupleObj *none = tuple_new(0);
        if (!none)
            return oom(), Value();
        rn = obj_value(none);
        rv = rn.v;
    }
    Root kv{ cont_new(hooks_step) };
    if (kv.v.is_nil())
        return Value();
    cont_of(kv.v)->s[0] = rc.v;
    cont_of(kv.v)->s[1] = rn.v;
    cont_of(kv.v)->s[2] = rv.v;
    cont_of(kv.v)->s[3] = rt.v;
    return kv.v;
}
