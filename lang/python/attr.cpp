// The descriptor protocol, the attribute algorithm and __slots__.
//
// Phase 9 knew one descriptor -- property -- and looked for it by hand. This is
// the general rule instead: anything with __get__ is a descriptor, anything
// that also has __set__ or __delete__ is a *data* descriptor and comes before
// the instance namespace rather than after it. Functions, staticmethod,
// classmethod and the __slots__ members are descriptors too; they are answered
// in C++ because their __get__ cannot fail and cannot call Python.
//
// Everything a lookup may have to run is Python -- a getter, a __get__, a
// __getattribute__, a __getattr__ -- so py_attr hands back a ContObj and the
// caller drives it. That is ground rule 2: the lookup cannot call.
#include "call.h"
#include "exc.h"
#include "gc.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "method.h"
#include "ops.h"
#include "type.h"
#include "weak.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

DictObj *dict_at(Value v)
{
    return static_cast<DictObj *>(v.obj());
}

usize tuple_len(Value v)
{
    return is_tuple(v) ? static_cast<TupleObj *>(v.obj())->len : 0;
}

Value tuple_at(Value v, usize i)
{
    return static_cast<TupleObj *>(v.obj())->items()[i];
}

// One or two arguments as a tuple, for a continuation that calls with them.
Value args_of(Value a, Value b, u32 n)
{
    Root ra{ a }, rb{ b };
    TupleObj *t = tuple_new(n);
    if (!t)
        return oom(), Value();
    if (n > 0)
        t->items()[0] = ra.v;
    if (n > 1)
        t->items()[1] = rb.v;
    return obj_value(t);
}

// The pending error is an AttributeError, however it was set: a kind and a
// message from C++, or an object a raise named.
bool pending_is_attr_error()
{
    Value e = err_value();
    if (!e.is_nil())
        return is_exc(e) && exc_is(exc_type_of(e), exc_find("AttributeError"));
    return err_kind() == Str("AttributeError");
}

R no_attr(Value v, StrObj *name)
{
    Buf<96> m;
    m.put("'").put(type_name(v)).put("' object has no attribute '").put(name->str()).put("'");
    return err_set("AttributeError", m.str());
}

// ------------------------------------------------------------- the members

void member_trace(Obj *o)
{
    gc_mark(static_cast<MemberObj *>(o)->name);
    gc_mark(static_cast<MemberObj *>(o)->cls);
}

R member_repr(Value v, String &out)
{
    MemberObj *m = static_cast<MemberObj *>(v.obj());
    Buf<96> b;
    b.put("<member '").put(is_str(m->name) ? str_of(m->name)->str() : Str("?"));
    b.put("' of '").put(type_obj(m->cls)->slots.name).put("' objects>");
    return out.append(b.str()) ? R::Ok : oom();
}

} // namespace

constexpr Type member_type{ .name  = "member_descriptor",
                            .trace = member_trace,
                            .repr  = member_repr };

Value *inst_slots(Obj *o, u32 &n)
{
    if (!is_inst(obj_value(o))) {
        n = 0;
        return nullptr;
    }
    TypeObj *c = type_obj(static_cast<InstObj *>(o)->cls);
    n          = c->nslots;
    return n ? reinterpret_cast<Value *>(reinterpret_cast<char *>(o) + c->slotoff) : nullptr;
}

namespace {

// The slot a member names, on this instance. Null when `v` is not one of the
// class's instances after all.
Value *slot_at(Value v, Value d)
{
    MemberObj *m = static_cast<MemberObj *>(d.obj());
    u32 n        = 0;
    Value *s     = inst_slots(v.obj(), n);
    return s && m->index < n ? &s[m->index] : nullptr;
}

} // namespace

// A subclass of a built-in descriptor keeps the real one inside it, and it is
// the real one the protocol acts on.
Value descr_inner(Value d)
{
    if (!is_inst(d))
        return d;
    Value n = inst_of(d)->native;
    return n.is_nil() ? d : n;
}

u8 descr_of(Value d, bool &data)
{
    data = false;
    if (!d.is_obj())
        return D_NONE;
    const Type *t = descr_inner(d).obj()->type;
    // A function from a module is not a descriptor: `f = len` in a class body
    // stays len when read through an instance.
    if (t == &native_type && (d.obj()->flags & OBJ_PLAINFN))
        return D_NONE;
    if (t == &func_type || t == &native_type || t == &staticmethod_type || t == &classmethod_type)
        return D_BIND;
    if (t == &property_type)
        return data = true, D_PROP;
    if (t == &member_type)
        return data = true, D_MEMBER;
    if (!is_inst(d))
        return D_NONE;
    if (!type_has_special(d, "__get__"))
        return D_NONE;
    data = type_has_special(d, "__set__") || type_has_special(d, "__delete__");
    return D_PY;
}

namespace {

// ----------------------------------------------------- the attribute driver

// What attr_step is to do with the answer, and with an AttributeError.
enum : u32 { G_GUARD = 1, G_FOUND = 2 };

// s[0] the callable and s[1] its arguments; s[2]/s[3] the __getattr__ to try
// if the first raises AttributeError; s[4] what to answer if that one does too
// and `j` says an AttributeError is to be caught at all.
R attr_step(ContObj *k, Value in)
{
    u32 phase = k->i++;
    if (phase == 0) {
        k->catching = !k->s[2].is_nil() || (k->j & G_GUARD) ? CATCH_ATTR : CATCH_NONE;
        return cont_call_v(k, k->s[0], k->s[1]);
    }
    if (!in.is_nil())
        return cont_done(k, k->j & G_FOUND ? value_bool(true) : in);
    if (phase == 1 && !k->s[2].is_nil()) {
        k->catching = (k->j & G_GUARD) ? CATCH_ATTR : CATCH_NONE;
        return cont_call_v(k, k->s[2], k->s[3]);
    }
    if (k->j & G_FOUND)
        return cont_done(k, value_bool(false));
    if (k->j & G_GUARD)
        return cont_done(k, k->s[4]);
    // Nothing was willing to answer, so the exception carries on.
    return err_set_value(k->caught);
}

// The ContObj a Got::Call hands back.
Value attr_cont(Value fn, Value args, Value alt, Value altargs, Value dflt, u32 flags)
{
    Root a{ fn }, b{ args }, c{ alt }, d{ altargs }, e{ dflt };
    Value kv = cont_new(attr_step);
    if (kv.is_nil())
        return Value();
    ContObj *k = cont_of(kv);
    k->s[0]    = a.v;
    k->s[1]    = b.v;
    k->s[2]    = c.v;
    k->s[3]    = d.v;
    k->s[4]    = e.v;
    k->j       = flags;
    return kv;
}

} // namespace

bool attr_cont_guard(Value kv, Value dflt)
{
    if (!is_cont(kv) || cont_of(kv)->step != attr_step)
        return false;
    cont_of(kv)->j |= G_GUARD;
    cont_of(kv)->s[4] = dflt;
    return true;
}

namespace {

// ------------------------------------------------------------- descriptor get

// What `d`, found on `cls`, answers for `self` -- Nil when reached through the
// class itself. `args` takes the call's arguments when the answer is a call.
Got descr_get(Value d, u8 kind, Value self, Value cls, Value &out, Value &args)
{
    switch (kind) {
    case D_BIND:
        return type_bind(d, self, cls, out) == R::Ok ? Got::Ok : Got::Error;

    case D_PROP: {
        PropObj *p = static_cast<PropObj *>(descr_inner(d).obj());
        if (self.is_nil())
            return out = d, Got::Ok; // C.prop is the property itself
        if (p->get.is_nil()) {
            err_set("AttributeError", "unreadable attribute");
            return Got::Error;
        }
        // The getter takes self and nothing else, so it is bound rather than
        // handed the (obj, type) pair __get__ would take.
        out  = method_new(p->get, self);
        args = Value();
        return out.is_nil() ? Got::Error : Got::Call;
    }

    case D_MEMBER: {
        if (self.is_nil())
            return out = d, Got::Ok;
        Value *s = slot_at(self, descr_inner(d));
        if (!s || s->is_nil()) {
            MemberObj *m = static_cast<MemberObj *>(descr_inner(d).obj());
            Buf<160> b;
            b.put('\'').put(type_name(self)).put("' object has no attribute '");
            b.put(is_str(m->name) ? str_of(m->name)->str() : Str("?")).put('\'');
            err_set("AttributeError", b.str());
            return Got::Error;
        }
        out = *s;
        return Got::Ok;
    }

    case D_PY: {
        Root rd{ d }, rs{ self }, rc{ cls };
        Value fn = type_special(rd.v, "__get__");
        if (fn.is_nil())
            return Got::Error;
        Root rf{ fn };
        args = args_of(rs.v.is_nil() ? value_none() : rs.v, rc.v, 2);
        if (args.is_nil())
            return Got::Error;
        out = rf.v;
        return Got::Call;
    }

    default:
        out = d;
        return Got::Ok;
    }
}

// --------------------------------------------------------------- the lookup

Got type_attr(Value v, StrObj *name, Value &out, Value &args);
Got inst_attr(Value v, StrObj *name, Value &out, Value &args);
Got super_attr(Value v, StrObj *name, Value &out, Value &args);

// object.__getattribute__, whatever the object is: the whole algorithm bar the
// two hooks, which py_attr puts round it.
Got attr_default(Value v, StrObj *name, Value &out, Value &args)
{
    if (is_super(v))
        return super_attr(v, name, out, args);
    Got g = Got::Missing;
    if (is_type(v))
        g = type_attr(v, name, out, args);
    else if (is_inst(v))
        g = inst_attr(v, name, out, args);
    if (is_type(v) || is_inst(v)) {
        // Every object answers __class__, unless its class said otherwise.
        if (g == Got::Missing && name->str() == Str("__class__")) {
            out = type_of_value(v);
            return out.is_nil() ? Got::Error : Got::Ok;
        }
        return g;
    }

    const Type *t = type_of(v);
    if (t && t->lazyattr) {
        Got lazy = t->lazyattr(v, name, out, args);
        if (lazy != Got::Missing)
            return lazy;
    }
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
    // A module may answer for itself (PEP 562). Its __getattr__ takes the name
    // and nothing else, so it is not bound.
    if (is_module(v)) {
        StrObj *ga = str_intern("__getattr__");
        Value fn;
        if (!ga)
            return oom(), Got::Error;
        if (dict_get(module_dict(v), obj_value(ga), fn) == R::Ok) {
            out  = fn;
            args = args_of(obj_value(name), Value(), 1);
            return args.is_nil() ? Got::Error : Got::Call;
        }
    }
    if (name->str() == Str("__class__")) {
        out = type_of_value(v);
        return out.is_nil() ? Got::Error : Got::Ok;
    }
    return Got::Missing;
}

Got inst_attr(Value v, StrObj *name, Value &out, Value &args)
{
    InstObj *o = inst_of(v);
    Root rv{ v }, rc{ o->cls };
    Root found;
    R r = type_lookup(rc.v, name, found.v);
    if (r == R::Err)
        return Got::Error;
    bool data = false;
    u8 kind   = r == R::Ok ? descr_of(found.v, data) : D_NONE;
    if (data)
        return descr_get(found.v, kind, rv.v, rc.v, out, args);

    Value d = inst_of(rv.v)->dict;
    if (!d.is_nil()) {
        R g = dict_get(dict_at(d), obj_value(name), out);
        if (g == R::Err)
            return Got::Error;
        if (g == R::Ok)
            return Got::Ok;
    }
    if (r == R::Ok) {
        // __new__ is implicitly a staticmethod.
        if (Str("__new__") == name->str())
            return out = found.v, Got::Ok;
        return descr_get(found.v, kind, rv.v, rc.v, out, args);
    }

    if (Str("__dict__") == name->str()) {
        if (type_obj(rc.v)->nodict)
            return Got::Missing;
        if (inst_of(rv.v)->dict.is_nil()) {
            DictObj *nd = dict_new();
            if (!nd)
                return oom(), Got::Error;
            inst_of(rv.v)->dict = obj_value(nd);
        }
        out = inst_of(rv.v)->dict;
        return Got::Ok;
    }

    // What the class's own slots answer -- an exception's `args`, a native
    // base's attributes -- comes after the namespace. Not __class__, which
    // is the class and never the native base's.
    const Type *t = type_of(rv.v);
    Value nat     = inst_of(rv.v)->native;
    if (!nat.is_nil() && nat.is_obj() && nat.obj()->type->lazyattr) {
        Got lazy = nat.obj()->type->lazyattr(nat, name, out, args);
        if (lazy != Got::Missing)
            return lazy;
    }
    if (t->getattr && Str("__class__") != name->str()) {
        R g = t->getattr(rv.v, name, out);
        if (g == R::Ok)
            return Got::Ok;
        if (g == R::Err)
            return Got::Error;
    }
    return Got::Missing;
}

// The names a class answers for itself, which CPython keeps as getset
// descriptors on `type`.
bool type_own_attr(Value v, Str n, Value &out)
{
    TypeObj *t = type_obj(v);
    if (n == "__name__")
        return out = t->name, !out.is_nil();
    if (n == "__qualname__")
        return out = t->qualname.is_nil() ? t->name : t->qualname, !out.is_nil();
    if (n == "__module__" && !t->heap) {
        // A dotted descriptor name put its module in the namespace.
        StrObj *k = str_intern("__module__");
        if (k && dict_get(static_cast<DictObj *>(t->dict.obj()), obj_value(k), out) == R::Ok)
            return true;
        out = str_new("builtins");
        return !out.is_nil();
    }
    if (n == "__bases__")
        return out = t->bases, !out.is_nil();
    if (n == "__mro__")
        return out = t->mro, !out.is_nil();
    if (n == "__dict__")
        return out = mappingproxy_new(t->dict), !out.is_nil();
    if (n == "__base__")
        return out = tuple_len(t->bases) ? tuple_at(t->bases, 0) : Value(), !out.is_nil();
    if (n == "__orig_bases__")
        return out = t->origbases, !out.is_nil();
    if (n == "__type_params__") {
        StrObj *k = str_intern("__type_params__");
        if (k && dict_get(static_cast<DictObj *>(t->dict.obj()), obj_value(k), out) == R::Ok)
            return true;
        TupleObj *none = tuple_new(0);
        out            = none ? obj_value(none) : Value();
        return !out.is_nil();
    }
    return false;
}

Got type_attr(Value v, StrObj *name, Value &out, Value &args)
{
    Root rv{ v };
    Root meta{ type_of_value(rv.v) };
    if (meta.v.is_nil())
        return Got::Error;

    // A data descriptor on the metaclass comes first, the way one on the class
    // comes before an instance's namespace.
    Root mfound;
    R mr = type_lookup(meta.v, name, mfound.v);
    if (mr == R::Err)
        return Got::Error;
    bool mdata = false;
    u8 mkind   = mr == R::Ok ? descr_of(mfound.v, mdata) : D_NONE;
    if (mdata)
        return descr_get(mfound.v, mkind, rv.v, meta.v, out, args);

    Root found, owner;
    R r = type_lookup(rv.v, name, found.v, &owner.v);
    if (r == R::Err)
        return Got::Error;
    if (r == R::Ok) {
        if (Str("__new__") == name->str()) {
            out = type_new_attr(found.v, owner.v);
            return out.is_nil() ? Got::Error : Got::Ok;
        }
        bool data = false;
        u8 kind   = descr_of(found.v, data);
        // Reached through the class, so there is no instance to bind to.
        return descr_get(found.v, kind, Value(), rv.v, out, args);
    }
    if (type_own_attr(rv.v, name->str(), out))
        return Got::Ok;
    if (mr == R::Ok)
        return descr_get(mfound.v, mkind, rv.v, meta.v, out, args);
    return Got::Missing;
}

Got super_attr(Value v, StrObj *name, Value &out, Value &args)
{
    SuperObj *s = static_cast<SuperObj *>(v.obj());
    Root rs{ s->self }, rc{ s->cls };
    // super(C, x) with x a subclass of C walks x's own linearization and binds
    // nothing; anything else walks the type's and binds x. A metaclass method
    // takes a class as its self and is the second kind, not the first.
    bool as_class = is_type(rs.v) && type_issub(rs.v, rc.v);
    // The super object's own members.
    Str n = name->str();
    if (n == "__thisclass__" || n == "__self__" || n == "__self_class__" || n == "__class__") {
        if (n == "__class__")
            out = type_of_value(v);
        else if (n == "__thisclass__")
            out = rc.v;
        else if (rs.v.is_nil())
            out = value_none();
        else if (n == "__self__" || as_class)
            out = rs.v;
        else
            out = type_of_value(rs.v);
        return out.is_nil() ? Got::Error : Got::Ok;
    }
    Value mro;
    if (as_class)
        mro = type_obj(rs.v)->mro;
    else if (!rs.v.is_nil()) {
        Value t = type_of_value(rs.v);
        if (t.is_nil())
            return Got::Error;
        mro = type_obj(t)->mro;
    } else {
        mro = type_obj(rc.v)->mro;
    }
    Root rm{ mro };

    // Everything after the class super() was written in.
    usize at = 0;
    while (at < tuple_len(rm.v) && tuple_at(rm.v, at) != rc.v)
        at++;
    for (usize i = at + 1; i < tuple_len(rm.v); i++) {
        Value c = tuple_at(rm.v, i);
        Root found;
        R r = dict_get(dict_at(type_obj(c)->dict), obj_value(name), found.v);
        if (r == R::Err)
            return Got::Error;
        if (r != R::Ok)
            continue;
        if (Str("__new__") == name->str()) {
            out = type_new_attr(found.v, c);
            return out.is_nil() ? Got::Error : Got::Ok;
        }
        bool data  = false;
        u8 kind    = descr_of(found.v, data);
        Value self = as_class ? Value() : rs.v;
        return descr_get(found.v, kind, self, c, out, args);
    }
    return Got::Missing;
}

// The class's __getattribute__ and __getattr__, bound, or Nil.
Got attr_hooks(Value v, Root &ga, Root &gattr)
{
    if (!is_inst(v))
        return Got::Ok;
    Value cls = inst_of(v)->cls;
    Value h   = type_hook(cls, "__getattribute__");
    if (!h.is_nil()) {
        ga = method_new(h, v);
        if (ga.v.is_nil())
            return Got::Error;
    }
    StrObj *n = str_intern("__getattr__");
    Value f;
    if (!n)
        return oom(), Got::Error;
    if (type_lookup(cls, n, f) == R::Ok) {
        gattr = method_new(f, v);
        if (gattr.v.is_nil())
            return Got::Error;
    }
    return Got::Ok;
}

Got attr_any(Value v, StrObj *name, Value &out, Value dflt, u32 flags)
{
    // A weak proxy is its referent in everything.
    if (is_weakproxy(v)) {
        v = proxy_target(v);
        if (v.is_nil())
            return Got::Error;
    }
    Root rv{ v }, rn{ obj_value(name) }, rd{ dflt };
    Root ga, gattr;
    if (attr_hooks(rv.v, ga, gattr) == Got::Error)
        return Got::Error;
    Root one{ args_of(rn.v, Value(), 1) };
    if (one.v.is_nil())
        return Got::Error;

    if (!ga.v.is_nil()) {
        out = attr_cont(ga.v, one.v, gattr.v, one.v, rd.v, flags);
        return out.is_nil() ? Got::Error : Got::Call;
    }

    Root args;
    Got g = attr_default(rv.v, name, out, args.v);
    if (g == Got::Ok) {
        if (flags & G_FOUND)
            out = value_bool(true);
        return Got::Ok;
    }
    if (g == Got::Error) {
        // A getter that is not there at all, or a slot never assigned: a
        // __getattr__ answers for that too, and a default for want of one.
        if (gattr.v.is_nil() && (flags & (G_GUARD | G_FOUND)) && pending_is_attr_error()) {
            err_clear();
            out = (flags & G_FOUND) ? value_bool(false) : rd.v;
            return Got::Ok;
        }
        if (gattr.v.is_nil() || !pending_is_attr_error())
            return Got::Error;
        err_clear();
        out = attr_cont(gattr.v, one.v, Value(), Value(), rd.v, flags);
        return out.is_nil() ? Got::Error : Got::Call;
    }
    if (g == Got::Call) {
        Root fn{ out };
        out = attr_cont(fn.v, args.v, gattr.v, one.v, rd.v, flags);
        return out.is_nil() ? Got::Error : Got::Call;
    }
    if (!gattr.v.is_nil()) {
        out = attr_cont(gattr.v, one.v, Value(), Value(), rd.v, flags);
        return out.is_nil() ? Got::Error : Got::Call;
    }
    if (flags & G_FOUND)
        return out = value_bool(false), Got::Ok;
    if (flags & G_GUARD)
        return out = rd.v, Got::Ok;
    return Got::Missing;
}

// ------------------------------------------------------------------- storing

// A data descriptor for `name` on the class of `v`, or Nil.
R store_descr(Value cls, StrObj *name, Value &out, u8 &kind)
{
    Value found;
    R r = type_lookup(cls, name, found);
    if (r != R::Ok)
        return r;
    bool data = false;
    kind      = descr_of(found, data);
    if (!data)
        return R::NotImpl;
    out = found;
    return R::Ok;
}

R inst_store(Value v, StrObj *name, Value val, Value &fn)
{
    Root rv{ v }, rn{ obj_value(name) }, rx{ val };
    Root cls{ inst_of(rv.v)->cls };
    Root d;
    u8 kind = D_NONE;
    R r     = store_descr(cls.v, name, d.v, kind);
    if (r == R::Err)
        return R::Err;
    if (r == R::Ok) {
        if (kind == D_PROP) {
            PropObj *p = static_cast<PropObj *>(descr_inner(d.v).obj());
            if (p->set.is_nil())
                return err_set2("AttributeError", "can't set attribute", name->str());
            Root m{ method_new(p->set, rv.v) };
            if (m.v.is_nil())
                return R::Err;
            Root a{ args_of(rx.v, Value(), 1) };
            if (a.v.is_nil())
                return R::Err;
            fn = attr_cont(m.v, a.v, Value(), Value(), Value(), 0);
            return fn.is_nil() ? R::Err : R::Ok;
        }
        if (kind == D_MEMBER) {
            Value *s = slot_at(rv.v, descr_inner(d.v));
            if (!s)
                return err_set2("AttributeError", "not a slot of this class", name->str());
            *s = rx.v;
            return R::Ok;
        }
        Root set{ type_special(d.v, "__set__") };
        if (set.v.is_nil())
            return err_set2("AttributeError", "can't set attribute", name->str());
        Root a{ args_of(rv.v, rx.v, 2) };
        if (a.v.is_nil())
            return R::Err;
        fn = attr_cont(set.v, a.v, Value(), Value(), Value(), 0);
        return fn.is_nil() ? R::Err : R::Ok;
    }

    // An exception's cause and context are its own fields, and so are a
    // UnicodeError's five.
    Str n = name->str();
    if (is_exc(rv.v)) {
        R u = unierr_store(rv.v, n, rx.v);
        if (u == R::NotImpl && is_oserror(rv.v))
            u = oserror_store(rv.v, n, rx.v);
        if (u != R::NotImpl)
            return u;
    }
    bool cause = n == Str("__cause__");
    if (is_exc(rv.v) && (cause || n == Str("__context__"))) {
        if (!is_none(rx.v) && !is_exc(rx.v))
            return err_set("TypeError", cause ? Str("exception cause must be None or derive "
                                                    "from BaseException")
                                              : Str("exception context must be None or derive "
                                                    "from BaseException"));
        ExcObj *e                       = static_cast<ExcObj *>(rv.v.obj());
        (cause ? e->cause : e->context) = is_none(rx.v) ? Value() : rx.v;
        return R::Ok;
    }
    if (type_obj(cls.v)->nodict)
        return no_attr(rv.v, name);
    if (inst_of(rv.v)->dict.is_nil()) {
        DictObj *nd = dict_new();
        if (!nd)
            return oom();
        inst_of(rv.v)->dict = obj_value(nd);
    }
    return dict_set(dict_at(inst_of(rv.v)->dict), rn.v, rx.v);
}

R inst_erase(Value v, StrObj *name, Value &fn)
{
    Root rv{ v }, rn{ obj_value(name) };
    Root cls{ inst_of(rv.v)->cls };
    Root d;
    u8 kind = D_NONE;
    R r     = store_descr(cls.v, name, d.v, kind);
    if (r == R::Err)
        return R::Err;
    if (r == R::Ok) {
        if (kind == D_PROP) {
            PropObj *p = static_cast<PropObj *>(descr_inner(d.v).obj());
            if (p->del.is_nil())
                return err_set2("AttributeError", "can't delete attribute", name->str());
            Root m{ method_new(p->del, rv.v) };
            if (m.v.is_nil())
                return R::Err;
            fn = attr_cont(m.v, Value(), Value(), Value(), Value(), 0);
            return fn.is_nil() ? R::Err : R::Ok;
        }
        if (kind == D_MEMBER) {
            Value *s = slot_at(rv.v, descr_inner(d.v));
            if (!s || s->is_nil())
                return no_attr(rv.v, name);
            *s = Value();
            return R::Ok;
        }
        Root del{ type_special(d.v, "__delete__") };
        if (del.v.is_nil())
            return err_set2("AttributeError", "can't delete attribute", name->str());
        Root a{ args_of(rv.v, Value(), 1) };
        if (a.v.is_nil())
            return R::Err;
        fn = attr_cont(del.v, a.v, Value(), Value(), Value(), 0);
        return fn.is_nil() ? R::Err : R::Ok;
    }

    if (is_exc(rv.v)) {
        R u = unierr_store(rv.v, name->str(), Value());
        if (u == R::NotImpl && is_oserror(rv.v))
            u = oserror_store(rv.v, name->str(), Value());
        if (u != R::NotImpl)
            return u;
    }
    if (inst_of(rv.v)->dict.is_nil())
        return no_attr(rv.v, name);
    R g = dict_del(dict_at(inst_of(rv.v)->dict), rn.v);
    return g == R::NotImpl ? no_attr(rv.v, name) : g;
}

} // namespace

// ------------------------------------------------------------------- the API

Got attr_plain(Value v, StrObj *name, Value &out, Value &args)
{
    return attr_default(v, name, out, args);
}

Value attr_invoke(Value fn, Value args)
{
    return attr_cont(fn, args, Value(), Value(), Value(), 0);
}

R attr_plain_store(Value v, StrObj *name, Value val, Value &fn)
{
    fn = Value();
    if (!is_inst(v))
        return attr_store(v, name, val, fn);
    return inst_store(v, name, val, fn);
}

R attr_plain_delete(Value v, StrObj *name, Value &fn)
{
    fn = Value();
    if (!is_inst(v))
        return attr_delete(v, name, fn);
    return inst_erase(v, name, fn);
}

Got py_attr(Value v, StrObj *name, Value &out)
{
    return attr_any(v, name, out, Value(), 0);
}

Got py_attr_opt(Value v, StrObj *name, Value &out, Value dflt, bool found)
{
    return attr_any(v, name, out, dflt, G_GUARD | (found ? G_FOUND : 0));
}

R attr_store(Value v, StrObj *name, Value val, Value &fn)
{
    fn = Value();
    if (is_weakproxy(v)) {
        v = proxy_target(v);
        if (v.is_nil())
            return R::Err;
    }
    if (is_type(v)) {
        if (!type_obj(v)->heap)
            return err_set2("TypeError", "cannot set an attribute on a built-in type",
                            type_obj(v)->slots.name);
        Root rv{ v }, rn{ obj_value(name) }, rx{ val };
        if (dict_set(dict_at(type_obj(rv.v)->dict), rn.v, rx.v) != R::Ok)
            return R::Err;
        // `C.__del__ = f` is legal, and the collector has to know.
        if (name->str() == Str("__del__"))
            type_note_del(rv.v);
        return R::Ok;
    }
    if (!is_inst(v)) {
        const Type *t = type_of(v);
        if (t && t->setattr)
            return t->setattr(v, name, val);
        return no_attr(v, name);
    }

    // A __setattr__ of one's own takes every assignment, descriptors included.
    Root rv{ v }, rn{ obj_value(name) }, rx{ val };
    Value hook = type_hook(inst_of(rv.v)->cls, "__setattr__");
    if (!hook.is_nil()) {
        Root m{ method_new(hook, rv.v) };
        if (m.v.is_nil())
            return R::Err;
        Root a{ args_of(rn.v, rx.v, 2) };
        if (a.v.is_nil())
            return R::Err;
        fn = attr_cont(m.v, a.v, Value(), Value(), Value(), 0);
        return fn.is_nil() ? R::Err : R::Ok;
    }
    return inst_store(rv.v, name, rx.v, fn);
}

R attr_delete(Value v, StrObj *name, Value &fn)
{
    fn = Value();
    if (is_weakproxy(v)) {
        v = proxy_target(v);
        if (v.is_nil())
            return R::Err;
    }
    if (is_type(v)) {
        if (!type_obj(v)->heap)
            return err_set2("TypeError", "cannot delete an attribute of a built-in type",
                            type_obj(v)->slots.name);
        R r = dict_del(dict_at(type_obj(v)->dict), obj_value(name));
        return r == R::NotImpl ? err_set2("AttributeError", "no attribute", name->str()) : r;
    }
    if (!is_inst(v)) {
        // A built-in object with a setattr slot deletes through it: a Nil
        // value is what `del` means there.
        const Type *t = type_of(v);
        if (t && t->setattr)
            return t->setattr(v, name, Value());
        return no_attr(v, name);
    }

    Root rv{ v }, rn{ obj_value(name) };
    Value hook = type_hook(inst_of(rv.v)->cls, "__delattr__");
    if (!hook.is_nil()) {
        Root m{ method_new(hook, rv.v) };
        if (m.v.is_nil())
            return R::Err;
        Root a{ args_of(rn.v, Value(), 1) };
        if (a.v.is_nil())
            return R::Err;
        fn = attr_cont(m.v, a.v, Value(), Value(), Value(), 0);
        return fn.is_nil() ? R::Err : R::Ok;
    }
    return inst_erase(rv.v, name, fn);
}

R inst_setattr(Value v, StrObj *name, Value val)
{
    Value fn;
    R r = attr_store(v, name, val, fn);
    if (r != R::Ok || fn.is_nil())
        return r;
    return err_set2("TypeError", "this attribute needs the interpreter", name->str());
}

R inst_delattr(Value v, StrObj *name)
{
    Value fn;
    R r = attr_delete(v, name, fn);
    if (r != R::Ok || fn.is_nil())
        return r;
    return err_set2("TypeError", "this attribute needs the interpreter", name->str());
}

// ------------------------------------------------------- __get__ and __set__

namespace {

// The name a descriptor is known by, and the class it is read on, for the
// messages a missing setter or deleter makes.
Str prop_name(const PropObj *p)
{
    return is_str(p->pname) ? str_of(p->pname)->str() : Str("?");
}

R no_accessor(Value desc, Value obj, Str what)
{
    PropObj *p = static_cast<PropObj *>(descr_inner(desc).obj());
    Buf<160> m;
    m.put("property '").put(prop_name(p)).put("' of '").put(type_name(obj));
    m.put("' object has no ").put(what);
    return err_set("AttributeError", m.str());
}

// d.__get__(obj, type=None): what reading d off obj, or off type, answers.
R d_get(const CallArgs &a, Value &out)
{
    if (a.nkw)
        return err_set("TypeError", "__get__() takes no keyword arguments");
    if (a.nargs < 2 || a.nargs > 3) {
        Buf<96> m;
        m.put(a.nargs < 2 ? "__get__ expected at least 1 argument, got 0"
                          : "__get__ expected at most 2 arguments");
        return err_set("TypeError", m.str());
    }
    Value obj  = a.args[1];
    Value type = a.nargs > 2 ? a.args[2] : value_none();
    if (is_none(obj) && is_none(type))
        return err_set("TypeError", "__get__(None, None) is invalid");
    Root self{ is_none(obj) ? Value() : obj };
    Root cls{ is_none(type) ? type_of_value(obj) : type };
    if (cls.v.is_nil())
        return R::Err;
    bool data = false;
    u8 kind   = descr_of(a.args[0], data);
    Root args;
    switch (descr_get(a.args[0], kind, self.v, cls.v, out, args.v)) {
    case Got::Ok:
        return R::Ok;
    case Got::Call:
        out = attr_invoke(out, args.v);
        return out.is_nil() ? R::Err : R::Ok;
    default:
        if (err_pending())
            return R::Err;
        out = a.args[0];
        return R::Ok;
    }
}

R member_miss(Value obj, Value desc)
{
    MemberObj *m = static_cast<MemberObj *>(descr_inner(desc).obj());
    Buf<160> b;
    b.put('\'').put(type_name(obj)).put("' object has no attribute '");
    b.put(is_str(m->name) ? str_of(m->name)->str() : Str("?")).put('\'');
    return err_set("AttributeError", b.str());
}

// d.__set__(obj, value) and d.__delete__(obj), for a property and a slot.
R d_set_or_delete(const CallArgs &a, Value &out, bool del)
{
    Str who = del ? Str("__delete__") : Str("__set__");
    if (!args_only(a, who, del ? 2 : 3, del ? 2 : 3))
        return R::Err;
    Root desc{ a.args[0] }, obj{ a.args[1] }, val{ del ? Value() : a.args[2] };
    bool data = false;
    u8 kind   = descr_of(desc.v, data);
    if (kind == D_MEMBER) {
        Value *s = slot_at(obj.v, descr_inner(desc.v));
        if (!s)
            return member_miss(obj.v, desc.v);
        if (del && s->is_nil())
            return member_miss(obj.v, desc.v);
        *s  = val.v;
        out = value_none();
        return R::Ok;
    }
    if (kind != D_PROP)
        return err_set2("TypeError", "not a data descriptor", type_name(desc.v));
    PropObj *p = static_cast<PropObj *>(descr_inner(desc.v).obj());
    Value fn   = del ? p->del : p->set;
    if (fn.is_nil())
        return no_accessor(desc.v, obj.v, del ? Str("deleter") : Str("setter"));
    Root rf{ fn };
    Root args{ args_of(obj.v, val.v, del ? 1 : 2) };
    if (args.v.is_nil())
        return R::Err;
    out = attr_invoke(rf.v, args.v);
    return out.is_nil() ? R::Err : R::Ok;
}

R d_set(const CallArgs &a, Value &out)
{
    return d_set_or_delete(a, out, false);
}

R d_delete(const CallArgs &a, Value &out)
{
    return d_set_or_delete(a, out, true);
}

constexpr Method GET_ONLY[] = { { "__get__", d_get } };

constexpr Method GET_SET[] = {
    { "__get__", d_get },
    { "__set__", d_set },
    { "__delete__", d_delete },
};

} // namespace

bool descr_methods()
{
    return method_install(&func_type, GET_ONLY) && method_install(&staticmethod_type, GET_ONLY) &&
           method_install(&classmethod_type, GET_ONLY) && method_install(&property_type, GET_SET) &&
           method_install(&member_type, GET_SET);
}
