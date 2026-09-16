// `list[int]`: the generic alias, which is the rest of PEP 560 beside the
// `__class_getitem__` phase 17 wrote.
//
// It is a small object that remembers a class and the arguments it was
// subscripted with, and otherwise behaves as that class: calling it calls the
// class, an attribute it does not answer itself comes off the class, and
// __mro_entries__ hands the class back so `class C(list[int])` derives from
// list. There is no typing module yet and so no TypeVar, which is why
// __parameters__ is always empty.
//
// `types.GenericAlias` is `type(list[int])`, so the library gets the name the
// moment types.py is borrowed; nothing here has to be imported.
#include "genalias.h"

#include "call.h"
#include "gc.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "method.h"
#include "ops.h"
#include "type.h"
#include "typevar.h"
#include "union.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

struct AliasObj : Obj {
    Value origin; // the class subscripted
    Value args;   // TupleObj, always -- `list[int]` keeps a one-item tuple
};

usize tuple_len(Value v)
{
    return static_cast<TupleObj *>(v.obj())->len;
}

Value tuple_at(Value v, usize i)
{
    return static_cast<TupleObj *>(v.obj())->items()[i];
}

Value list_to_tuple(Value l)
{
    Root rl{ l };
    TupleObj *t = tuple_new(list_of(rl.v)->items.size());
    if (!t)
        return oom(), Value();
    for (usize i = 0; i < t->len; i++)
        t->items()[i] = list_of(rl.v)->items[i];
    return obj_value(t);
}

AliasObj *alias_of(Value v)
{
    return static_cast<AliasObj *>(v.obj());
}

void alias_trace(Obj *o)
{
    gc_mark(static_cast<AliasObj *>(o)->origin);
    gc_mark(static_cast<AliasObj *>(o)->args);
}

R alias_repr(Value v, String &out)
{
    Root rv{ v };
    if (typing_repr(alias_of(rv.v)->origin, out) != R::Ok)
        return R::Err;
    if (!out.push('['))
        return oom();
    TupleObj *t = static_cast<TupleObj *>(alias_of(rv.v)->args.obj());
    if (!t->len && !out.append("()"))
        return oom();
    for (u32 i = 0; i < t->len; i++) {
        if (i && !out.append(", "))
            return oom();
        if (typing_repr(static_cast<TupleObj *>(alias_of(rv.v)->args.obj())->items()[i], out) !=
            R::Ok)
            return R::Err;
    }
    return out.push(']') ? R::Ok : oom();
}

R alias_eq(Value a, Value b, bool &out)
{
    if (!is_genalias(b))
        return R::NotImpl;
    bool same = false;
    if (py_eq(alias_of(a)->origin, alias_of(b)->origin, same) != R::Ok)
        return R::Err;
    if (!same) {
        out = false;
        return R::Ok;
    }
    return py_eq(alias_of(a)->args, alias_of(b)->args, out);
}

R alias_hash(Value v, u32 &out)
{
    u32 a = 0, b = 0;
    if (py_hash(alias_of(v)->origin, a) != R::Ok || py_hash(alias_of(v)->args, b) != R::Ok)
        return R::Err;
    out = a ^ b;
    return R::Ok;
}

// Everything the alias does not answer itself comes off the class, which is
// what makes `list[int].append` work.
R alias_getattr(Value v, StrObj *name, Value &out)
{
    Str n = name->str();
    if (n == "__origin__") {
        out = alias_of(v)->origin;
        return R::Ok;
    }
    if (n == "__args__") {
        out = alias_of(v)->args;
        return R::Ok;
    }
    if (n == "__parameters__") {
        out = typing_params(alias_of(v)->args);
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (n == "__class_getitem__" || n == "__mro_entries__" || n == "__reduce__")
        return R::NotImpl; // the method table answers these
    Root rv{ v };
    R r = py_getattr(alias_of(rv.v)->origin, name, out);
    return r == R::Err && err_kind() == "AttributeError" ? (err_clear(), R::NotImpl) : r;
}

// class C(list[int]): the base the class really gets is list.
R a_mro_entries(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__mro_entries__", 1, 1))
        return R::Err;
    Root self{ method_self(a.args[0]) };
    TupleObj *t = tuple_new(1);
    if (!t)
        return oom();
    t->items()[0] = alias_of(self.v)->origin;
    out           = obj_value(t);
    return R::Ok;
}

// list[int]() makes a list: the alias is not a class of its own.
R a_call(const CallArgs &a, Value &out)
{
    Root self{ method_self(a.args[0]) };
    TupleObj *args = tuple_new(a.nargs ? a.nargs - 1 : 0);
    if (!args)
        return oom();
    for (u32 i = 1; i < a.nargs; i++)
        args->items()[i - 1] = a.args[i];
    Root ra{ obj_value(args) };
    TupleObj *names = tuple_new(a.nkw);
    if (!names)
        return oom();
    for (u32 i = 0; i < a.nkw; i++)
        names->items()[i] = a.kwnames[i];
    Root rn{ obj_value(names) };
    TupleObj *vals = tuple_new(a.nkw);
    if (!vals)
        return oom();
    for (u32 i = 0; i < a.nkw; i++)
        vals->items()[i] = a.kwvals[i];
    Root rv{ obj_value(vals) };
    Root kv{ cont_new(alias_call_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = alias_of(self.v)->origin;
    k->s[1]    = ra.v;
    k->s[2]    = rn.v;
    k->s[3]    = rv.v;
    out        = kv.v;
    return R::Ok;
}

// `list[T][int]`: the same origin over the arguments with T replaced.
R alias_getitem(Value v, Value item, Value &out)
{
    Root self{ v }, ri{ item };
    Root params{ typing_params(alias_of(self.v)->args) };
    if (params.v.is_nil())
        return R::Err;
    Root args{ typing_subst(self.v, alias_of(self.v)->args, params.v, ri.v) };
    if (args.v.is_nil())
        return R::Err;
    out = genalias_new(alias_of(self.v)->origin, args.v);
    return out.is_nil() ? R::Err : R::Ok;
}

R a_getitem(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__getitem__", 1, 1))
        return R::Err;
    return alias_getitem(method_self(a.args[0]), a.args[1], out);
}

constexpr Method ALIAS_METHODS[] = {
    { "__mro_entries__", a_mro_entries },
    { "__call__", a_call },
    { "__getitem__", a_getitem },
};

// `cls[item]`, as a built-in type's __class_getitem__ answers it.
R b_class_getitem(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__class_getitem__", 1, 1))
        return R::Err;
    out = genalias_new(a.args[0], a.args[1]);
    return out.is_nil() ? R::Err : R::Ok;
}

} // namespace

// `list[int]`, `dict[str, int]`, `tuple[()]`. A class prints as its name and
// anything else as its repr, which is what CPython's does.
R typing_repr(Value v, String &out)
{
    if (v.w == value_ellipsis().w)
        return out.append("...") ? R::Ok : oom();
    if (is_none(v) || (is_type(v) && type_obj(v)->desc == &none_type))
        return out.append("None") ? R::Ok : oom();
    if (is_type(v)) {
        Value mod;
        StrObj *n = str_intern("__module__");
        if (n && !type_obj(v)->dict.is_nil() &&
            dict_get(static_cast<DictObj *>(type_obj(v)->dict.obj()), obj_value(n), mod) == R::Ok &&
            is_str(mod) && str_of(mod)->str() != "builtins") {
            if (!out.append(str_of(mod)->str()) || !out.push('.'))
                return oom();
        }
        err_clear();
        Value q = type_obj(v)->qualname.is_nil() ? type_obj(v)->name : type_obj(v)->qualname;
        return out.append(str_of(q)->str()) ? R::Ok : oom();
    }
    return py_repr(v, out);
}

Value typing_params(Value args)
{
    Root ra{ args };
    ListObj *l = list_new();
    if (!l)
        return oom(), Value();
    Root rl{ obj_value(l) };
    for (usize i = 0; i < tuple_len(ra.v); i++) {
        Value t = tuple_at(ra.v, i);
        Root sub;
        if (is_typevar_like(t)) {
            TupleObj *one = tuple_new(1);
            if (!one)
                return oom(), Value();
            one->items()[0] = t;
            sub             = obj_value(one);
        } else if (is_genalias(t)) {
            sub = typing_params(alias_of(t)->args);
        } else if (is_union(t)) {
            sub = typing_params(union_args(t));
        } else if (is_tuple(t) || is_list(t)) {
            Root seq{ is_list(t) ? list_to_tuple(t) : t };
            if (seq.v.is_nil())
                return Value();
            sub = typing_params(seq.v);
        } else {
            continue;
        }
        if (sub.v.is_nil())
            return Value();
        for (usize k = 0; k < tuple_len(sub.v); k++) {
            Value p   = tuple_at(sub.v, k);
            bool seen = false;
            for (usize j = 0; j < list_of(rl.v)->items.size() && !seen; j++)
                seen = list_of(rl.v)->items[j] == p;
            if (!seen && !list_push(list_of(rl.v), p))
                return oom(), Value();
        }
    }
    return list_to_tuple(rl.v);
}

// One argument with the variables replaced: the variable itself, or an alias
// or a union with some inside it, rebuilt.
Value subst_one(Value arg, Value params, Value items)
{
    Root ra{ arg }, rp{ params }, rs{ items };
    if (is_typevar_like(ra.v)) {
        for (usize i = 0; i < tuple_len(rp.v); i++)
            if (tuple_at(rp.v, i) == ra.v)
                return tuple_at(rs.v, i);
        return ra.v;
    }
    bool alias = is_genalias(ra.v);
    if (!alias && !is_union(ra.v))
        return ra.v;
    Root inner{ alias ? alias_of(ra.v)->args : union_args(ra.v) };
    TupleObj *t = tuple_new(tuple_len(inner.v));
    if (!t)
        return oom(), Value();
    Root rt{ obj_value(t) };
    for (usize i = 0; i < tuple_len(inner.v); i++) {
        Value one = subst_one(tuple_at(inner.v, i), rp.v, rs.v);
        if (one.is_nil())
            return Value();
        static_cast<TupleObj *>(rt.v.obj())->items()[i] = one;
    }
    return alias ? genalias_new(alias_of(ra.v)->origin, rt.v) : union_from(rt.v);
}

Value typing_subst(Value self, Value args, Value params, Value item)
{
    Root rself{ self }, ra{ args }, rp{ params }, items{ item };
    if (!tuple_len(rp.v)) {
        String text;
        if (py_repr(rself.v, text) != R::Ok)
            return Value();
        Buf<160> b;
        b.put(text.str()).put(" is not a generic class");
        return err_set("TypeError", b.str()), Value();
    }
    if (!is_tuple(items.v)) {
        TupleObj *one = tuple_new(1);
        if (!one)
            return oom(), Value();
        one->items()[0] = items.v;
        items           = obj_value(one);
    }
    usize want = tuple_len(rp.v), have = tuple_len(items.v);
    if (want != have) {
        String text;
        if (py_repr(rself.v, text) != R::Ok)
            return Value();
        char tmp[24];
        Buf<192> b;
        b.put("Too ").put(have > want ? "many" : "few").put(" arguments for ").put(text.str());
        b.put("; actual ").put(int_text(tmp, sizeof tmp, i64(have)));
        b.put(", expected ").put(int_text(tmp, sizeof tmp, i64(want)));
        return err_set("TypeError", b.str()), Value();
    }
    TupleObj *t = tuple_new(tuple_len(ra.v));
    if (!t)
        return oom(), Value();
    Root rt{ obj_value(t) };
    for (usize i = 0; i < tuple_len(ra.v); i++) {
        Value one = subst_one(tuple_at(ra.v, i), rp.v, items.v);
        if (one.is_nil())
            return Value();
        static_cast<TupleObj *>(rt.v.obj())->items()[i] = one;
    }
    return rt.v;
}

R alias_call_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call_kw(k, k->s[0], k->s[1], k->s[2], k->s[3]);
    return cont_done(k, in);
}

constexpr Type genalias_type{ .name    = "GenericAlias",
                              .trace   = alias_trace,
                              .hash    = alias_hash,
                              .eq      = alias_eq,
                              .repr    = alias_repr,
                              .getitem = alias_getitem,
                              .binop   = union_binop,
                              .getattr = alias_getattr };

Value genalias_new(Value origin, Value item)
{
    Root ro{ origin }, ri{ item };
    // `dict[str, int]` subscripts with a tuple; everything else is one item.
    Root args;
    if (is_tuple(ri.v)) {
        args = ri.v;
    } else {
        TupleObj *t = tuple_new(1);
        if (!t)
            return oom(), Value();
        t->items()[0] = ri.v;
        args          = obj_value(t);
    }
    AliasObj *o = static_cast<AliasObj *>(obj_alloc(&genalias_type, sizeof(AliasObj)));
    if (!o)
        return oom(), Value();
    o->origin = ro.v;
    o->args   = args.v;
    return obj_value(o);
}

bool genalias_install(const Type *const *types, usize n)
{
    if (!method_install(&genalias_type, ALIAS_METHODS))
        return false;
    for (usize i = 0; i < n; i++) {
        Root cls{ type_wrap(types[i]) };
        Root fn{ native_new("__class_getitem__", b_class_getitem) };
        StrObj *name = str_intern("__class_getitem__");
        if (cls.v.is_nil() || fn.v.is_nil() || !name)
            return false;
        if (dict_set(static_cast<DictObj *>(type_obj(cls.v)->dict.obj()), obj_value(name), fn.v) !=
            R::Ok)
            return false;
    }
    return true;
}
