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

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

struct AliasObj : Obj {
    Value origin; // the class subscripted
    Value args;   // TupleObj, always -- `list[int]` keeps a one-item tuple
};

AliasObj *alias_of(Value v)
{
    return static_cast<AliasObj *>(v.obj());
}

void alias_trace(Obj *o)
{
    gc_mark(static_cast<AliasObj *>(o)->origin);
    gc_mark(static_cast<AliasObj *>(o)->args);
}

// `list[int]`, `dict[str, int]`, `tuple[()]`. A class prints as its name and
// anything else as its repr, which is what CPython's does.
R alias_one(Value v, String &out)
{
    if (is_type(v)) {
        Value mod;
        StrObj *n = str_intern("__module__");
        if (n && is_type(v) && !type_obj(v)->dict.is_nil() &&
            dict_get(static_cast<DictObj *>(type_obj(v)->dict.obj()), obj_value(n), mod) == R::Ok &&
            is_str(mod) && str_of(mod)->str() != "builtins") {
            if (!out.append(str_of(mod)->str()) || !out.push('.'))
                return oom();
        }
        err_clear();
        return out.append(str_of(type_obj(v)->name)->str()) ? R::Ok : oom();
    }
    if (is_none(v))
        return out.append("None") ? R::Ok : oom();
    // `tuple[int, ...]` is written with the ellipsis spelled as it was typed.
    if (v.w == value_ellipsis().w)
        return out.append("...") ? R::Ok : oom();
    return py_repr(v, out);
}

R alias_repr(Value v, String &out)
{
    Root rv{ v };
    if (alias_one(alias_of(rv.v)->origin, out) != R::Ok)
        return R::Err;
    if (!out.push('['))
        return oom();
    TupleObj *t = static_cast<TupleObj *>(alias_of(rv.v)->args.obj());
    if (!t->len && !out.append("()"))
        return oom();
    for (u32 i = 0; i < t->len; i++) {
        if (i && !out.append(", "))
            return oom();
        if (alias_one(static_cast<TupleObj *>(alias_of(rv.v)->args.obj())->items()[i], out) !=
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
        // No typing module and so no TypeVar: nothing in an alias is ever a
        // parameter, and this is the empty tuple rather than a guess.
        out = obj_value(tuple_new(0));
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

// A second subscript would need a TypeVar to substitute, and there are none.
R a_class_getitem(const CallArgs &a, Value &out)
{
    (void)a;
    (void)out;
    return err_set("TypeError", "there are no type variables left in this alias");
}

constexpr Method ALIAS_METHODS[] = {
    { "__mro_entries__", a_mro_entries },
    { "__call__", a_call },
    { "__class_getitem__", a_class_getitem },
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
