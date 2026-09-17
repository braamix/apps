// `_typing`, after CPython's Objects/typevarobject.c; see typevar.h.
//
// A bound, a constraint tuple, a default and an alias's value are evaluated
// lazily: the compiler hands over a function, and the first read of the
// attribute calls it. A native attribute cannot call Python, so the type's
// lazyattr slot answers with the call to make instead, and a TypeVar keeps
// what came back.
#include "typevar.h"

#include "annot.h"
#include "call.h"
#include "frame.h"
#include "func.h"
#include "gc.h"
#include "genalias.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "type.h"
#include "union.h"
#include "vm.h"

extern const Type typevar_type;
extern const Type paramspec_type;
extern const Type typevartuple_type;
extern const Type psargs_type;
extern const Type pskwargs_type;
extern const Type unpack_type;
extern const Type typealias_type;
extern const Type nodefault_type;

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

usize tuple_len(Value v)
{
    return static_cast<TupleObj *>(v.obj())->len;
}

Value tuple_at(Value v, usize i)
{
    return static_cast<TupleObj *>(v.obj())->items()[i];
}

Value tuple_of(const Value *items, usize n)
{
    TupleObj *t = tuple_new(n);
    if (!t)
        return oom(), Value();
    for (usize i = 0; i < n; i++)
        t->items()[i] = items[i];
    return obj_value(t);
}

Value empty_tuple()
{
    return tuple_of(nullptr, 0);
}

struct PsAttrObj : Obj {
    Value origin;
};

struct UnpackObj : Obj {
    Value tvt;
};

struct AliasObj : Obj {
    Value name, module, params, value, eval;
};

R identity_hash(Value v, u32 &out)
{
    out = u32(usize(v.obj()) >> 3);
    return R::Ok;
}

// ------------------------------------------------------------------ NoDefault

R nodefault_repr(Value, String &out)
{
    return out.append("typing.NoDefault") ? R::Ok : oom();
}

// --------------------------------------------------------- the type params

enum : u8 { TF_COV = 1, TF_CONTRA = 2, TF_INFER = 4 };
enum : u32 { EV_BOUND, EV_CONSTRAINTS, EV_DEFAULT, EV_VALUE };

struct TvarObj : Obj {
    Value name;
    Value module;
    Value bound;       // what __bound__ answered, once asked
    Value constraints; // a tuple, once asked
    Value dflt;        // the default, once asked
    Value eval_bound, eval_constraints, eval_default;
    u8 flags;
    bool has_dflt; // a default was given, as a value or to evaluate
};

TvarObj *tv_of(Value v)
{
    return static_cast<TvarObj *>(v.obj());
}

bool is_tv(Value v)
{
    if (!v.is_obj())
        return false;
    const Type *t = v.obj()->type;
    return t == &typevar_type || t == &paramspec_type || t == &typevartuple_type;
}

void tv_trace(Obj *o)
{
    TvarObj *t = static_cast<TvarObj *>(o);
    gc_mark(t->name);
    gc_mark(t->module);
    gc_mark(t->bound);
    gc_mark(t->constraints);
    gc_mark(t->dflt);
    gc_mark(t->eval_bound);
    gc_mark(t->eval_constraints);
    gc_mark(t->eval_default);
}

Value tv_new(const Type *t, Value name, Value module)
{
    Root rn{ name }, rm{ module };
    TvarObj *o = static_cast<TvarObj *>(obj_alloc(t, sizeof(TvarObj)));
    if (!o)
        return oom(), Value();
    o->name   = rn.v;
    o->module = rm.v;
    o->bound = o->constraints = o->dflt = Value();
    o->eval_bound = o->eval_constraints = o->eval_default = Value();
    o->flags                                              = 0;
    o->has_dflt                                           = false;
    return obj_value(o);
}

R tv_repr(Value v, String &out)
{
    TvarObj *t = tv_of(v);
    if (v.obj()->type != &typevartuple_type && !(t->flags & TF_INFER)) {
        char c = (t->flags & TF_COV) ? '+' : (t->flags & TF_CONTRA) ? '-' : '~';
        if (!out.push(c))
            return oom();
    }
    return out.append(str_of(t->name)->str()) ? R::Ok : oom();
}

Value nodefault()
{
    static Obj obj{ &nodefault_type, nullptr, nullptr, OBJ_IMMORTAL };
    return obj_value(&obj);
}

R tv_getattr(Value v, StrObj *name, Value &out)
{
    TvarObj *t    = tv_of(v);
    Str n         = name->str();
    bool is_tuple = v.obj()->type == &typevartuple_type;
    if (n == "__name__")
        return out = t->name, R::Ok;
    if (n == "__module__")
        return out = t->module.is_nil() ? value_none() : t->module, R::Ok;
    if (n == "__default__" && (!t->has_dflt || !t->dflt.is_nil()))
        return out = t->has_dflt ? t->dflt : nodefault(), R::Ok;
    if (n == "evaluate_default")
        return out = t->eval_default.is_nil() ? value_none() : t->eval_default, R::Ok;
    if (is_tuple)
        return R::NotImpl;
    if (n == "__bound__" && (t->eval_bound.is_nil() || !t->bound.is_nil()))
        return out = t->bound.is_nil() ? value_none() : t->bound, R::Ok;
    if (n == "__covariant__")
        return out = value_bool(t->flags & TF_COV), R::Ok;
    if (n == "__contravariant__")
        return out = value_bool(t->flags & TF_CONTRA), R::Ok;
    if (n == "__infer_variance__")
        return out = value_bool(t->flags & TF_INFER), R::Ok;
    if (n == "evaluate_bound")
        return out = t->eval_bound.is_nil() ? value_none() : t->eval_bound, R::Ok;
    if (v.obj()->type == &paramspec_type) {
        if (n != "args" && n != "kwargs")
            return R::NotImpl;
        Root rv{ v };
        const Type *pt = n == "kwargs" ? &pskwargs_type : &psargs_type;
        PsAttrObj *p   = static_cast<PsAttrObj *>(obj_alloc(pt, sizeof(PsAttrObj)));
        if (!p)
            return oom();
        p->origin = rv.v;
        out       = obj_value(p);
        return R::Ok;
    }
    if (n == "__constraints__" && (t->eval_constraints.is_nil() || !t->constraints.is_nil())) {
        if (!t->constraints.is_nil())
            return out = t->constraints, R::Ok;
        out = empty_tuple();
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (n == "evaluate_constraints")
        return out = t->eval_constraints.is_nil() ? value_none() : t->eval_constraints, R::Ok;
    return R::NotImpl;
}

// s[0] the type param or alias, s[1] the evaluator; j which attribute.
R eval_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call(k, k->s[1], Value::of_int(ANN_VALUE));
    if (k->j != EV_VALUE) {
        TvarObj *t = tv_of(k->s[0]);
        (k->j == EV_BOUND ? t->bound : k->j == EV_CONSTRAINTS ? t->constraints : t->dflt) = in;
    }
    return cont_done(k, in);
}

// `self.__bound__` and the like: the value, or a ContObj evaluating it.
R n_evaluate(const CallArgs &a, Value &out)
{
    Root self{ a.args[0] };
    u32 which = u32(a.args[1].as_int());
    Value fn;
    if (which == EV_VALUE) {
        fn = static_cast<AliasObj *>(self.v.obj())->eval;
    } else {
        TvarObj *t = tv_of(self.v);
        fn         = which == EV_BOUND         ? t->eval_bound
                     : which == EV_CONSTRAINTS ? t->eval_constraints
                                               : t->eval_default;
    }
    Root rf{ fn };
    Root kv{ cont_new(eval_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = self.v;
    cont_of(kv.v)->s[1] = rf.v;
    cont_of(kv.v)->j    = which;
    out                 = kv.v;
    return R::Ok;
}

Got lazy_call(Value v, u32 which, Value &out, Value &args)
{
    Root rv{ v };
    Root fn{ native_new("_evaluate", n_evaluate) };
    if (fn.v.is_nil())
        return Got::Error;
    Root bound{ method_new(fn.v, rv.v) };
    if (bound.v.is_nil())
        return Got::Error;
    Value one = Value::of_int(i32(which));
    args      = tuple_of(&one, 1);
    if (args.is_nil())
        return Got::Error;
    out = bound.v;
    return Got::Call;
}

Got tv_lazy(Value v, StrObj *name, Value &out, Value &args)
{
    TvarObj *t = tv_of(v);
    Str n      = name->str();
    if (n == "__bound__" && !t->eval_bound.is_nil() && t->bound.is_nil())
        return lazy_call(v, EV_BOUND, out, args);
    if (n == "__constraints__" && !t->eval_constraints.is_nil() && t->constraints.is_nil())
        return lazy_call(v, EV_CONSTRAINTS, out, args);
    if (n == "__default__" && !t->eval_default.is_nil() && t->dflt.is_nil())
        return lazy_call(v, EV_DEFAULT, out, args);
    return Got::Missing;
}

R tv_binop(Value a, Value b, Op op, Value &out)
{
    if (op != Op::Or)
        return R::NotImpl;
    Value two[2] = { a, b };
    Root pair{ tuple_of(two, 2) };
    if (pair.v.is_nil())
        return R::Err;
    out = union_from(pair.v);
    return out.is_nil() ? R::Err : R::Ok;
}

// `*Ts`: iterating a TypeVarTuple gives its unpacked form, once.
Value tvt_iter(Value v)
{
    Root rv{ v };
    UnpackObj *u = static_cast<UnpackObj *>(obj_alloc(&unpack_type, sizeof(UnpackObj)));
    if (!u)
        return oom(), Value();
    u->tvt    = rv.v;
    Value one = obj_value(u);
    Root ru{ one };
    Root t{ tuple_of(&one, 1) };
    return t.v.is_nil() ? Value() : py_iter(t.v);
}

// --------------------------------------------------------- their methods

R m_has_default(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "has_default", 0, 0))
        return R::Err;
    out = value_bool(tv_of(a.args[0])->has_dflt);
    return R::Ok;
}

R m_reduce(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__reduce__", 0, 0))
        return R::Err;
    out = tv_of(a.args[0])->name;
    return R::Ok;
}

R m_mro_entries(const CallArgs &a, Value &out)
{
    (void)out;
    if (!meth_args(a, "__mro_entries__", 1, 1))
        return R::Err;
    Buf<96> b;
    b.put("Cannot subclass an instance of ");
    Str full = type_name(a.args[0]);
    usize at = full.size();
    while (at && full[at - 1] != '.')
        at--;
    b.put(full.substr(at));
    return err_set("TypeError", b.str());
}

R m_subst(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__typing_subst__", 1, 1))
        return R::Err;
    if (a.args[0].obj()->type == &typevartuple_type)
        return err_set("TypeError", "Substitution of bare TypeVarTuple is not supported");
    out = a.args[1];
    return R::Ok;
}

R m_or(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__or__", 1, 1))
        return R::Err;
    return tv_binop(a.args[0], a.args[1], Op::Or, out);
}

R m_ror(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__ror__", 1, 1))
        return R::Err;
    return tv_binop(a.args[1], a.args[0], Op::Or, out);
}

R m_iter(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__iter__", 0, 0))
        return R::Err;
    out = tvt_iter(a.args[0]);
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr Method TYPEVAR_METHODS[] = {
    { "has_default", m_has_default },
    { "__reduce__", m_reduce },
    { "__mro_entries__", m_mro_entries },
    { "__typing_subst__", m_subst },
    { "__or__", m_or },
    { "__ror__", m_ror },
};

constexpr Method TYPEVARTUPLE_METHODS[] = {
    { "has_default", m_has_default },
    { "__reduce__", m_reduce },
    { "__mro_entries__", m_mro_entries },
    { "__typing_subst__", m_subst },
    { "__iter__", m_iter },
};

// ------------------------------------------------- P.args and P.kwargs

PsAttrObj *ps_of(Value v)
{
    return static_cast<PsAttrObj *>(v.obj());
}

void ps_trace(Obj *o)
{
    gc_mark(static_cast<PsAttrObj *>(o)->origin);
}

bool ps_kwargs(Value v)
{
    return v.obj()->type == &pskwargs_type;
}

R ps_repr(Value v, String &out)
{
    Value o = ps_of(v)->origin;
    if (is_tv(o)) {
        if (!out.append(str_of(tv_of(o)->name)->str()))
            return oom();
    } else if (py_repr(o, out) != R::Ok) {
        return R::Err;
    }
    return out.append(ps_kwargs(v) ? Str(".kwargs") : Str(".args")) ? R::Ok : oom();
}

R ps_eq(Value a, Value b, bool &out)
{
    if (!b.is_obj() || b.obj()->type != a.obj()->type)
        return R::NotImpl;
    out = ps_of(a)->origin == ps_of(b)->origin;
    return R::Ok;
}

R ps_hash(Value v, u32 &out)
{
    identity_hash(ps_of(v)->origin, out);
    out ^= ps_kwargs(v) ? 1 : 0;
    return R::Ok;
}

R ps_getattr(Value v, StrObj *name, Value &out)
{
    if (name->str() != Str("__origin__"))
        return R::NotImpl;
    out = ps_of(v)->origin;
    return R::Ok;
}

// ------------------------------------------------------------- Unpack[Ts]

void unpack_trace(Obj *o)
{
    gc_mark(static_cast<UnpackObj *>(o)->tvt);
}

R unpack_repr(Value v, String &out)
{
    if (!out.append("typing.Unpack["))
        return oom();
    if (py_repr(static_cast<UnpackObj *>(v.obj())->tvt, out) != R::Ok)
        return R::Err;
    return out.push(']') ? R::Ok : oom();
}

R unpack_eq(Value a, Value b, bool &out)
{
    if (!b.is_obj() || b.obj()->type != &unpack_type)
        return R::NotImpl;
    out = static_cast<UnpackObj *>(a.obj())->tvt == static_cast<UnpackObj *>(b.obj())->tvt;
    return R::Ok;
}

R unpack_hash(Value v, u32 &out)
{
    identity_hash(static_cast<UnpackObj *>(v.obj())->tvt, out);
    out ^= 0x55;
    return R::Ok;
}

R unpack_getattr(Value v, StrObj *name, Value &out)
{
    Str n = name->str();
    if (n == "__typing_is_unpacked_typevartuple__")
        return out = value_bool(true), R::Ok;
    if (n == "__args__" || n == "__parameters__") {
        Value one = static_cast<UnpackObj *>(v.obj())->tvt;
        out       = tuple_of(&one, 1);
        return out.is_nil() ? R::Err : R::Ok;
    }
    return R::NotImpl;
}

// ------------------------------------------------------------ TypeAliasType

AliasObj *ta_of(Value v)
{
    return static_cast<AliasObj *>(v.obj());
}

void ta_trace(Obj *o)
{
    AliasObj *a = static_cast<AliasObj *>(o);
    gc_mark(a->name);
    gc_mark(a->module);
    gc_mark(a->params);
    gc_mark(a->value);
    gc_mark(a->eval);
}

R ta_repr(Value v, String &out)
{
    return out.append(str_of(ta_of(v)->name)->str()) ? R::Ok : oom();
}

// The type params with each TypeVarTuple unpacked, as __parameters__ and a
// generic base want them.
Value unpacked_params(Value params)
{
    Root rp{ params };
    TupleObj *t = tuple_new(tuple_len(rp.v));
    if (!t)
        return oom(), Value();
    Root rt{ obj_value(t) };
    for (usize i = 0; i < tuple_len(rp.v); i++) {
        Value p = tuple_at(rp.v, i);
        if (p.is_obj() && p.obj()->type == &typevartuple_type) {
            Root it{ tvt_iter(p) };
            Value one;
            if (it.v.is_nil() || py_next(it.v, one) != R::Ok)
                return Value();
            p = one;
        }
        static_cast<TupleObj *>(rt.v.obj())->items()[i] = p;
    }
    return rt.v;
}

R ta_getattr(Value v, StrObj *name, Value &out)
{
    AliasObj *a = ta_of(v);
    Str n       = name->str();
    if (n == "__name__")
        return out = a->name, R::Ok;
    if (n == "__module__")
        return out = a->module.is_nil() ? value_none() : a->module, R::Ok;
    if (n == "__type_params__") {
        out = a->params.is_nil() ? empty_tuple() : a->params;
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (n == "__parameters__") {
        out = a->params.is_nil() ? empty_tuple() : unpacked_params(a->params);
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (n == "__value__" && !a->value.is_nil())
        return out = a->value, R::Ok;
    if (n == "evaluate_value")
        return out = a->eval.is_nil() ? value_none() : a->eval, R::Ok;
    return R::NotImpl;
}

Got ta_lazy(Value v, StrObj *name, Value &out, Value &args)
{
    if (name->str() != Str("__value__") || !ta_of(v)->value.is_nil())
        return Got::Missing;
    return lazy_call(v, EV_VALUE, out, args);
}

R ta_setattr(Value v, StrObj *name, Value val)
{
    (void)v;
    (void)val;
    Buf<128> b;
    Str n = name->str();
    if (n == "__value__" || n == "__name__" || n == "__type_params__" || n == "__module__" ||
        n == "__parameters__") {
        b.put("attribute '").put(n).put("' of 'typing.TypeAliasType' objects is not writable");
        return err_set("AttributeError", b.str());
    }
    b.put("'typing.TypeAliasType' object has no attribute '").put(n).put("'");
    return err_set("AttributeError", b.str());
}

R ta_getitem(Value v, Value item, Value &out)
{
    AliasObj *a = ta_of(v);
    if (a->params.is_nil() || !tuple_len(a->params))
        return err_set("TypeError", "Only generic type aliases are subscriptable");
    out = genalias_new(v, item);
    return out.is_nil() ? R::Err : R::Ok;
}

R ta_reduce(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__reduce__", 0, 0))
        return R::Err;
    out = ta_of(a.args[0])->name;
    return R::Ok;
}

constexpr Method ALIAS_METHODS[] = {
    { "__reduce__", ta_reduce },
    { "__or__", m_or },
    { "__ror__", m_ror },
};

Value alias_new(Value name, Value params, Value value, Value eval, Value module)
{
    Root rn{ name }, rp{ params }, rv{ value }, re{ eval }, rm{ module };
    AliasObj *a = static_cast<AliasObj *>(obj_alloc(&typealias_type, sizeof(AliasObj)));
    if (!a)
        return oom(), Value();
    a->name   = rn.v;
    a->module = rm.v;
    a->params = is_none(rp.v) ? Value() : rp.v;
    a->value  = rv.v;
    a->eval   = re.v;
    return obj_value(a);
}

// ------------------------------------------------------------------ Generic

R generic_repr(Value v, String &out)
{
    char tmp[24];
    Buf<96> b;
    b.put("<typing.Generic object at ").put(addr_text(tmp, sizeof tmp, v.obj())).put('>');
    return out.append(b.str()) ? R::Ok : oom();
}

Value generic_class()
{
    return type_wrap(&generic_type);
}

// A plain object, as Generic() makes.
R n_generic(const CallArgs &a, Value &out)
{
    (void)a;
    Root cls{ generic_class() };
    if (cls.v.is_nil())
        return R::Err;
    out = inst_new(cls.v);
    return out.is_nil() ? R::Err : R::Ok;
}

bool typevar_like_param(Value v)
{
    return is_typevar_like(v) || !typing_unpacked(v).is_nil();
}

bool holds(const Vec<Value> &v, Value x)
{
    for (usize i = 0; i < v.size(); i++)
        if (v[i] == x)
            return true;
    return false;
}

StrObj *params_key()
{
    return str_intern("__parameters__");
}

// A class's own __parameters__, or Nil.
Value class_params(Value cls)
{
    Value out;
    StrObj *k = params_key();
    Value found;
    if (!k || type_lookup(cls, k, found) != R::Ok || !is_tuple(found))
        return err_clear(), out;
    return found;
}

String class_text(Value cls)
{
    String s;
    s.append("<class '");
    typing_repr(cls, s);
    s.append("'>");
    return s;
}

// s[0] the class, s[1] the arguments so far (a list), s[2] its params.
R getitem_step(ContObj *k, Value in)
{
    ListObj *args = list_of(k->s[1]);
    if (k->i++ && !list_push(args, in))
        return oom();
    Value params = k->s[2];
    usize np     = tuple_len(params);
    bool star    = false;
    for (usize i = 0; i < np; i++)
        star = star || tuple_at(params, i).obj()->type == &typevartuple_type;
    // A missing argument takes the parameter's default, which may need a call.
    usize have = list_of(k->s[1])->items.size();
    if (!star && have < np) {
        Value p = tuple_at(params, have);
        if (tv_of(p)->has_dflt) {
            if (!tv_of(p)->dflt.is_nil())
                return getitem_step(k, tv_of(p)->dflt);
            Root fn{ native_new("_evaluate", n_evaluate) };
            Root bound{ fn.v.is_nil() ? Value() : method_new(fn.v, p) };
            if (bound.v.is_nil())
                return R::Err;
            return cont_call(k, bound.v, Value::of_int(EV_DEFAULT));
        }
    }
    have = list_of(k->s[1])->items.size();
    if (star ? have + 1 < np : have != np) {
        usize expected = np;
        bool at_least  = star;
        if (star)
            expected = np - 1;
        else if (have < np) {
            usize with_dflt = 0;
            for (usize i = 0; i < np; i++)
                with_dflt += tv_of(tuple_at(params, i))->has_dflt ? 1 : 0;
            expected -= with_dflt;
            at_least = with_dflt > 0;
        }
        String cls = class_text(k->s[0]);
        char tmp[24];
        Buf<192> b;
        b.put("Too ").put(have > np ? "many" : "few").put(" arguments for ").put(cls.str());
        b.put("; actual ").put(int_text(tmp, sizeof tmp, i64(have))).put(", expected ");
        if (at_least)
            b.put("at least ");
        b.put(int_text(tmp, sizeof tmp, i64(expected)));
        return err_set("TypeError", b.str());
    }
    Value t = tuple_of(list_of(k->s[1])->items.data(), list_of(k->s[1])->items.size());
    if (t.is_nil())
        return R::Err;
    Root rt{ t };
    Value alias = genalias_new(k->s[0], rt.v);
    return alias.is_nil() ? R::Err : cont_done(k, alias);
}

// Generic.__class_getitem__(cls, params).
R g_class_getitem(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__class_getitem__", 1, 1))
        return R::Err;
    Root cls{ a.args[0] };
    Root args{ a.args[1] };
    if (!is_tuple(args.v)) {
        args = tuple_of(&a.args[1], 1);
        if (args.v.is_nil())
            return R::Err;
    }
    // None stands for NoneType; a forward reference needs typing.
    TupleObj *copy = tuple_new(tuple_len(args.v));
    if (!copy)
        return oom();
    Root rc{ obj_value(copy) };
    for (usize i = 0; i < tuple_len(args.v); i++) {
        Value x = tuple_at(args.v, i);
        if (is_none(x))
            x = type_wrap(&none_type);
        else if (is_str(x))
            return err_set("TypeError", "a forward reference needs typing, which is not here yet");
        if (x.is_nil())
            return R::Err;
        static_cast<TupleObj *>(rc.v.obj())->items()[i] = x;
    }
    if (is_generic_class(cls.v)) {
        Buf<128> b;
        if (!tuple_len(rc.v))
            return err_set("TypeError", "Parameter list to Generic[...] cannot be empty");
        Vec<Value> seen;
        for (usize i = 0; i < tuple_len(rc.v); i++) {
            Value p = tuple_at(rc.v, i);
            if (!typevar_like_param(p))
                return err_set("TypeError",
                               "Parameters to Generic[...] must all be type "
                               "variables or parameter specification variables.");
            Value key = typing_unpacked(p);
            if (key.is_nil())
                key = p;
            if (holds(seen, key))
                return err_set("TypeError", "Parameters to Generic[...] must all be unique");
            if (!seen.push(key))
                return oom();
        }
        out = genalias_new(cls.v, rc.v);
        return out.is_nil() ? R::Err : R::Ok;
    }
    Root params{ class_params(cls.v) };
    if (params.v.is_nil() || !tuple_len(params.v)) {
        String text = class_text(cls.v);
        Buf<160> b;
        b.put(text.str()).put(" is not a generic class");
        return err_set("TypeError", b.str());
    }
    ListObj *l = list_new();
    if (!l)
        return oom();
    Root rl{ obj_value(l) };
    for (usize i = 0; i < tuple_len(rc.v); i++)
        if (!list_push(list_of(rl.v), tuple_at(rc.v, i)))
            return oom();
    Root kv{ cont_new(getitem_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = cls.v;
    cont_of(kv.v)->s[1] = rl.v;
    cont_of(kv.v)->s[2] = params.v;
    out                 = kv.v;
    return R::Ok;
}

// The type params in `bases`, in order and each once, as typing's
// _collect_type_parameters finds them.
bool collect(Value bases, Vec<Value> &out)
{
    for (usize i = 0; i < tuple_len(bases); i++) {
        Value b = tuple_at(bases, i);
        if (is_type(b))
            continue;
        Root params;
        if (typevar_like_param(b)) {
            params = tuple_of(&b, 1);
        } else {
            params = typing_params(tuple_of(&b, 1));
        }
        if (params.v.is_nil())
            return false;
        for (usize k = 0; k < tuple_len(params.v); k++) {
            Value p   = tuple_at(params.v, k);
            Value key = typing_unpacked(p);
            if (!key.is_nil())
                p = key;
            if (!holds(out, p) && !out.push(p))
                return oom() == R::Ok;
        }
    }
    // A parameter without a default may not follow one with.
    bool dflt = false;
    for (usize i = 0; i < out.size(); i++) {
        if (!is_tv(out[i]))
            continue;
        if (tv_of(out[i])->has_dflt) {
            dflt = true;
        } else if (dflt && out[i].obj()->type != &typevartuple_type) {
            Buf<128> b;
            b.put("Type parameter ").put(str_of(tv_of(out[i])->name)->str());
            b.put(" without a default follows type parameter with a default");
            return err_set("TypeError", b.str()) == R::Ok;
        }
    }
    return true;
}

// Generic.__init_subclass__(cls): what the class's __parameters__ are.
R g_init_subclass(const CallArgs &a, Value &out)
{
    if (a.nkw)
        return err_set("TypeError", "__init_subclass__() takes no keyword arguments");
    if (a.nargs != 1 || !is_type(a.args[0]))
        return err_set("TypeError", "__init_subclass__() takes no arguments");
    Root cls{ a.args[0] };
    Root generic{ generic_class() };
    if (generic.v.is_nil())
        return R::Err;
    // Plain `Generic` is not a base, with the two exceptions typing.py's own
    // version makes for the classes that have to be written that way.
    Value orig  = type_obj(cls.v)->origbases;
    Value bases = orig.is_nil() ? type_obj(cls.v)->bases : orig;
    bool named  = is_str(type_obj(cls.v)->name) && str_of(type_obj(cls.v)->name)->str() ==
                                                      Str("Protocol");
    Root meta{ type_of_value(cls.v) };
    bool typed = !meta.v.is_nil() && is_str(type_obj(meta.v)->name) &&
                 str_of(type_obj(meta.v)->name)->str() == Str("_TypedDictMeta");
    if (orig.is_nil() && (named || typed))
        bases = Value();
    for (usize i = 0; !bases.is_nil() && i < tuple_len(bases); i++)
        if (tuple_at(bases, i) == generic.v)
            return err_set("TypeError", "Cannot inherit from plain Generic");
    Vec<Value> tvars;
    if (!orig.is_nil()) {
        Root ro{ orig };
        if (!collect(ro.v, tvars))
            return R::Err;
        Root gvars;
        for (usize i = 0; i < tuple_len(ro.v); i++) {
            Value b = tuple_at(ro.v, i);
            Value o;
            StrObj *on = str_intern("__origin__");
            if (!is_genalias(b) || py_getattr(b, on, o) != R::Ok || o != generic.v) {
                err_clear();
                continue;
            }
            if (!gvars.v.is_nil())
                return err_set("TypeError", "Cannot inherit from Generic[...] multiple times.");
            gvars = typing_params(tuple_of(&b, 1));
            if (gvars.v.is_nil())
                return R::Err;
        }
        if (!gvars.v.is_nil()) {
            String missing;
            for (usize i = 0; i < tvars.size(); i++) {
                bool in = false;
                for (usize k = 0; k < tuple_len(gvars.v); k++) {
                    Value g   = tuple_at(gvars.v, k);
                    Value key = typing_unpacked(g);
                    in        = in || (key.is_nil() ? g : key) == tvars[i];
                }
                if (in)
                    continue;
                if (missing.size())
                    missing.append(", ");
                py_str(tvars[i], missing);
            }
            if (missing.size()) {
                String listed;
                for (usize k = 0; k < tuple_len(gvars.v); k++) {
                    if (k)
                        listed.append(", ");
                    py_str(tuple_at(gvars.v, k), listed);
                }
                Buf<256> b;
                b.put("Some type variables (").put(missing.str()).put(") are not listed in ");
                b.put("Generic[").put(listed.str()).put("]");
                return err_set("TypeError", b.str());
            }
            tvars.clear();
            for (usize k = 0; k < tuple_len(gvars.v); k++) {
                Value g   = tuple_at(gvars.v, k);
                Value key = typing_unpacked(g);
                if (!tvars.push(key.is_nil() ? g : key))
                    return oom();
            }
        }
    }
    Root params{ tuple_of(tvars.data(), tvars.size()) };
    StrObj *k = params_key();
    if (params.v.is_nil() || !k)
        return params.v.is_nil() ? R::Err : oom();
    if (dict_set(static_cast<DictObj *>(type_obj(cls.v)->dict.obj()), obj_value(k), params.v) !=
        R::Ok)
        return R::Err;
    out = value_none();
    return R::Ok;
}

// ------------------------------------------------------- the constructors

// Each keyword of the call into `out`, by `names`; anything else refused.
bool keywords(const CallArgs &a, Str who, const Str *names, usize n, Value *out)
{
    for (u32 k = 0; k < a.nkw; k++) {
        Str key = str_of(a.kwnames[k])->str();
        usize i = 0;
        while (i < n && names[i] != key)
            i++;
        if (i == n) {
            Buf<128> b;
            b.put(who).put("() got an unexpected keyword argument '").put(key).put("'");
            return err_set("TypeError", b.str()), false;
        }
        out[i] = a.kwvals[k];
    }
    return true;
}

// The __name__ of the module the call was made from.
Value caller_module()
{
    Value f = vm_frame();
    if (f.is_nil())
        return value_none();
    StrObj *k = str_intern("__name__");
    Value name;
    if (!k ||
        dict_get(static_cast<DictObj *>(frame_of(f)->globals.obj()), obj_value(k), name) != R::Ok)
        return err_clear(), value_none();
    return name;
}

bool truth_of(Value v)
{
    return !v.is_nil() && py_truth(v);
}

R variance(Value tv, Value cov, Value contra, Value infer)
{
    bool c = truth_of(cov), d = truth_of(contra), i = truth_of(infer);
    if (c && d)
        return err_set("ValueError", "Bivariant types are not supported.");
    if (i && (c || d))
        return err_set("ValueError", "Variance cannot be specified with infer_variance.");
    tv_of(tv)->flags = u8((c ? TF_COV : 0) | (d ? TF_CONTRA : 0) | (i ? TF_INFER : 0));
    return R::Ok;
}

void set_default(Value tv, Value dflt)
{
    if (dflt.is_nil() || dflt == nodefault())
        return;
    tv_of(tv)->dflt     = is_none(dflt) ? type_wrap(&none_type) : dflt;
    tv_of(tv)->has_dflt = true;
}

R n_typevar(const CallArgs &a, Value &out)
{
    if (!a.nargs || !is_str(a.args[0]))
        return err_set("TypeError", "TypeVar() argument 'name' must be str");
    constexpr Str NAMES[] = { "bound", "covariant", "contravariant", "infer_variance", "default" };
    Value kw[5];
    if (!keywords(a, "TypeVar", NAMES, 5, kw))
        return R::Err;
    Root rk0{ kw[0] }, rk4{ kw[4] };
    if (a.nargs == 2)
        return err_set("TypeError", "A single constraint is not allowed");
    bool bound = !kw[0].is_nil() && !is_none(kw[0]);
    if (a.nargs > 2 && bound)
        return err_set("TypeError", "Constraints cannot be combined with bound=...");
    Root tv{ tv_new(&typevar_type, a.args[0], caller_module()) };
    if (tv.v.is_nil() || variance(tv.v, kw[1], kw[2], kw[3]) != R::Ok)
        return R::Err;
    if (bound)
        tv_of(tv.v)->bound = rk0.v;
    if (a.nargs > 2) {
        Value c = tuple_of(a.args + 1, a.nargs - 1);
        if (c.is_nil())
            return R::Err;
        tv_of(tv.v)->constraints = c;
    }
    set_default(tv.v, rk4.v);
    out = tv.v;
    return R::Ok;
}

R n_paramspec(const CallArgs &a, Value &out)
{
    if (a.nargs != 1 || !is_str(a.args[0]))
        return err_set("TypeError", "ParamSpec() takes exactly one positional argument, a str");
    constexpr Str NAMES[] = { "bound", "covariant", "contravariant", "infer_variance", "default" };
    Value kw[5];
    if (!keywords(a, "ParamSpec", NAMES, 5, kw))
        return R::Err;
    Root rk0{ kw[0] }, rk4{ kw[4] };
    Root tv{ tv_new(&paramspec_type, a.args[0], caller_module()) };
    if (tv.v.is_nil() || variance(tv.v, kw[1], kw[2], kw[3]) != R::Ok)
        return R::Err;
    if (!rk0.v.is_nil() && !is_none(rk0.v))
        tv_of(tv.v)->bound = rk0.v;
    set_default(tv.v, rk4.v);
    out = tv.v;
    return R::Ok;
}

R n_typevartuple(const CallArgs &a, Value &out)
{
    if (a.nargs != 1 || !is_str(a.args[0]))
        return err_set("TypeError", "TypeVarTuple() takes exactly one positional argument, a str");
    constexpr Str NAMES[] = { "default" };
    Value kw[1];
    if (!keywords(a, "TypeVarTuple", NAMES, 1, kw))
        return R::Err;
    Root rk{ kw[0] };
    Root tv{ tv_new(&typevartuple_type, a.args[0], caller_module()) };
    if (tv.v.is_nil())
        return R::Err;
    set_default(tv.v, rk.v);
    out = tv.v;
    return R::Ok;
}

R n_psattr(const CallArgs &a, Value &out, bool kwargs)
{
    if (a.nargs != 1 || a.nkw)
        return err_set("TypeError", "takes exactly one argument, the ParamSpec");
    Root origin{ a.args[0] };
    const Type *t = kwargs ? &pskwargs_type : &psargs_type;
    PsAttrObj *p  = static_cast<PsAttrObj *>(obj_alloc(t, sizeof(PsAttrObj)));
    if (!p)
        return oom();
    p->origin = origin.v;
    out       = obj_value(p);
    return R::Ok;
}

R n_psargs(const CallArgs &a, Value &out)
{
    return n_psattr(a, out, false);
}

R n_pskwargs(const CallArgs &a, Value &out)
{
    return n_psattr(a, out, true);
}

R n_typealias(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "type_params" };
    Value kw[1];
    if (!keywords(a, "TypeAliasType", NAMES, 1, kw))
        return R::Err;
    if (a.nargs != 2)
        return err_set("TypeError", "TypeAliasType() takes exactly 2 positional arguments");
    if (!is_str(a.args[0])) {
        Buf<128> b;
        b.put("TypeAliasType.__new__() argument 'name' must be str, not ");
        b.put(type_name(a.args[0]));
        return err_set("TypeError", b.str());
    }
    Root params{ kw[0] };
    if (!params.v.is_nil()) {
        if (!is_tuple(params.v))
            return err_set("TypeError", "type_params must be a tuple");
        for (usize i = 0; i < tuple_len(params.v); i++)
            if (!is_tv(tuple_at(params.v, i))) {
                Buf<128> b;
                b.put("Expected a type param, got ").put(type_name(tuple_at(params.v, i)));
                return err_set("TypeError", b.str());
            }
    }
    out = alias_new(a.args[0], params.v.is_nil() ? value_none() : params.v, a.args[1], Value(),
                    caller_module());
    return out.is_nil() ? R::Err : R::Ok;
}

R n_idfunc(const CallArgs &a, Value &out)
{
    if (a.nargs != 1 || a.nkw)
        return err_set("TypeError", "_typing._idfunc() takes exactly one argument");
    out = a.args[0];
    return R::Ok;
}

constexpr ModDef TYPING_DEFS[] = {
    { "_idfunc", n_idfunc },
};

} // namespace

constexpr Type nodefault_type{ .name = "NoDefaultType", .repr = nodefault_repr };

constexpr Type typevar_type{ .name     = "typing.TypeVar",
                             .trace    = tv_trace,
                             .hash     = identity_hash,
                             .repr     = tv_repr,
                             .binop    = tv_binop,
                             .getattr  = tv_getattr,
                             .lazyattr = tv_lazy };

constexpr Type paramspec_type{ .name     = "typing.ParamSpec",
                               .trace    = tv_trace,
                               .hash     = identity_hash,
                               .repr     = tv_repr,
                               .binop    = tv_binop,
                               .getattr  = tv_getattr,
                               .lazyattr = tv_lazy };

constexpr Type typevartuple_type{ .name     = "typing.TypeVarTuple",
                                  .trace    = tv_trace,
                                  .hash     = identity_hash,
                                  .repr     = tv_repr,
                                  .iter     = tvt_iter,
                                  .getattr  = tv_getattr,
                                  .lazyattr = tv_lazy };

constexpr Type psargs_type{ .name    = "typing.ParamSpecArgs",
                            .trace   = ps_trace,
                            .hash    = ps_hash,
                            .eq      = ps_eq,
                            .repr    = ps_repr,
                            .getattr = ps_getattr };

constexpr Type pskwargs_type{ .name    = "typing.ParamSpecKwargs",
                              .trace   = ps_trace,
                              .hash    = ps_hash,
                              .eq      = ps_eq,
                              .repr    = ps_repr,
                              .getattr = ps_getattr };

constexpr Type unpack_type{ .name    = "typing._UnpackedTypeVarTuple",
                            .trace   = unpack_trace,
                            .hash    = unpack_hash,
                            .eq      = unpack_eq,
                            .repr    = unpack_repr,
                            .getattr = unpack_getattr };

constexpr Type typealias_type{ .name     = "typing.TypeAliasType",
                               .trace    = ta_trace,
                               .hash     = identity_hash,
                               .repr     = ta_repr,
                               .getitem  = ta_getitem,
                               .binop    = union_binop,
                               .getattr  = ta_getattr,
                               .setattr  = ta_setattr,
                               .lazyattr = ta_lazy };

constexpr Type generic_type{ .name = "typing.Generic", .repr = generic_repr, .plain = true };

bool is_typevar_like(Value v)
{
    return is_tv(v);
}

Value typing_unpacked(Value v)
{
    if (!v.is_obj() || v.obj()->type != &unpack_type)
        return Value();
    return static_cast<UnpackObj *>(v.obj())->tvt;
}

bool is_typealias(Value v)
{
    return v.is_obj() && v.obj()->type == &typealias_type;
}

bool is_generic_class(Value v)
{
    return is_type(v) && type_obj(v)->desc == &generic_type;
}

Str intrinsic_name(u32 kind)
{
    constexpr Str NAMES[] = { "TYPEVAR",
                              "TYPEVAR_WITH_BOUND",
                              "TYPEVAR_WITH_CONSTRAINTS",
                              "PARAMSPEC",
                              "TYPEVARTUPLE",
                              "SET_TYPEPARAM_DEFAULT",
                              "SUBSCRIPT_GENERIC",
                              "SET_FUNCTION_TYPE_PARAMS",
                              "TYPEALIAS" };
    return kind < sizeof NAMES / sizeof NAMES[0] ? NAMES[kind] : Str("?");
}

u32 intrinsic_arity(u32 kind)
{
    switch (kind) {
    case TI_TYPEVAR_BOUND:
    case TI_TYPEVAR_CONSTRAINTS:
    case TI_SET_DEFAULT:
    case TI_FUNCTION_TYPE_PARAMS:
        return 2;
    default:
        return 1;
    }
}

R typing_intrinsic(u32 kind, const Value *args, Value module, Value &out)
{
    Root typing{ str_new("typing") };
    if (typing.v.is_nil())
        return R::Err;
    switch (kind) {
    case TI_TYPEVAR:
    case TI_TYPEVAR_BOUND:
    case TI_TYPEVAR_CONSTRAINTS:
    case TI_PARAMSPEC:
    case TI_TYPEVARTUPLE: {
        const Type *t = kind == TI_PARAMSPEC      ? &paramspec_type
                        : kind == TI_TYPEVARTUPLE ? &typevartuple_type
                                                  : &typevar_type;
        Root ra{ args[0] },
            rb{ kind == TI_TYPEVAR_BOUND || kind == TI_TYPEVAR_CONSTRAINTS ? args[1] : Value() };
        out = tv_new(t, ra.v, typing.v);
        if (out.is_nil())
            return R::Err;
        if (kind != TI_TYPEVARTUPLE)
            tv_of(out)->flags = TF_INFER;
        if (kind == TI_TYPEVAR_BOUND)
            tv_of(out)->eval_bound = rb.v;
        if (kind == TI_TYPEVAR_CONSTRAINTS)
            tv_of(out)->eval_constraints = rb.v;
        return R::Ok;
    }
    case TI_SET_DEFAULT:
        tv_of(args[0])->eval_default = args[1];
        tv_of(args[0])->has_dflt     = true;
        out                          = args[0];
        return R::Ok;
    case TI_SUBSCRIPT_GENERIC: {
        Root params{ unpacked_params(args[0]) };
        Root cls{ generic_class() };
        if (params.v.is_nil() || cls.v.is_nil())
            return R::Err;
        out = genalias_new(cls.v, params.v);
        return out.is_nil() ? R::Err : R::Ok;
    }
    case TI_FUNCTION_TYPE_PARAMS: {
        Root fn{ args[0] }, params{ args[1] };
        StrObj *k = str_intern("__type_params__");
        if (!k || !is_func(fn.v))
            return err_set("SystemError", "type params for something that is not a function");
        Value later;
        if (attr_store(fn.v, k, params.v, later) != R::Ok)
            return R::Err;
        out = fn.v;
        return R::Ok;
    }
    case TI_TYPEALIAS: {
        Value t = args[0];
        out     = alias_new(tuple_at(t, 0), tuple_at(t, 1), Value(), tuple_at(t, 2), module);
        return out.is_nil() ? R::Err : R::Ok;
    }
    }
    return err_set("SystemError", "no such intrinsic");
}

bool typing_methods()
{
    if (!method_install(&typevar_type, TYPEVAR_METHODS) ||
        !method_install(&paramspec_type, TYPEVAR_METHODS) ||
        !method_install(&typevartuple_type, TYPEVARTUPLE_METHODS) ||
        !method_install(&typealias_type, ALIAS_METHODS))
        return false;
    constexpr Method GENERIC_METHODS[] = {
        { "__class_getitem__", g_class_getitem },
        { "__init_subclass__", g_init_subclass },
    };
    if (!method_install(&generic_type, GENERIC_METHODS))
        return false;
    // A Generic instance is a plain object, traced as one.
    Root generic{ generic_class() };
    if (generic.v.is_nil())
        return false;
    type_obj(generic.v)->slots.trace = object_type.trace;
    return true;
}

bool typing_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    Root union_cls{ type_wrap(&union_type) };
    return mod_type(d, &typevar_type, n_typevar) && mod_type(d, &paramspec_type, n_paramspec) &&
           mod_type(d, &typevartuple_type, n_typevartuple) && mod_type(d, &psargs_type, n_psargs) &&
           mod_type(d, &pskwargs_type, n_pskwargs) && mod_type(d, &typealias_type, n_typealias) &&
           mod_type(d, &generic_type, n_generic) && !union_cls.v.is_nil() &&
           mod_put(d, "Union", union_cls.v) && mod_put(d, "NoDefault", nodefault()) &&
           mod_defs(d, TYPING_DEFS);
}
