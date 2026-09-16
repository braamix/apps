// `int | str`: the union type.
//
// It is `typing.Union` and `types.UnionType` at once, as in 3.14: a tuple of
// members, flattened and without repeats, that prints as `int | str` and is
// equal to another union with the same members in any order. `type`, a
// generic alias and a union all answer `|` with one, and isinstance takes one
// the way it takes a tuple.
//
// CPython hands a member that is not a class to typing._type_check, which is
// Python; a string becomes a ForwardRef there. There is no typing module yet,
// so a string is refused and anything else is kept as it is.
#include "union.h"

#include "call.h"
#include "gc.h"
#include "genalias.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "method.h"
#include "ops.h"
#include "type.h"
#include "typevar.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

struct UnionObj : Obj {
    Value args;   // TupleObj
    Value params; // TupleObj, made on first use
};

UnionObj *union_of(Value v)
{
    return static_cast<UnionObj *>(v.obj());
}

usize tuple_len(Value v)
{
    return static_cast<TupleObj *>(v.obj())->len;
}

Value tuple_at(Value v, usize i)
{
    return static_cast<TupleObj *>(v.obj())->items()[i];
}

void union_trace(Obj *o)
{
    gc_mark(static_cast<UnionObj *>(o)->args);
    gc_mark(static_cast<UnionObj *>(o)->params);
}

R union_repr(Value v, String &out)
{
    Root rv{ v };
    for (usize i = 0; i < tuple_len(union_of(rv.v)->args); i++) {
        if (i && !out.append(" | "))
            return oom();
        if (typing_repr(tuple_at(union_of(rv.v)->args, i), out) != R::Ok)
            return R::Err;
    }
    return R::Ok;
}

// Whether `x` is among `args`, by equality.
R holds(Value args, Value x, bool &out)
{
    out = false;
    for (usize i = 0; i < tuple_len(args) && !out; i++)
        if (py_eq(tuple_at(args, i), x, out) != R::Ok)
            return R::Err;
    return R::Ok;
}

// The same members, in any order: they are unique, so a count and one
// direction of containment settle it.
R union_eq(Value a, Value b, bool &out)
{
    if (!is_union(b))
        return R::NotImpl;
    Value x = union_of(a)->args, y = union_of(b)->args;
    out = tuple_len(x) == tuple_len(y);
    for (usize i = 0; i < tuple_len(x) && out; i++)
        if (holds(y, tuple_at(x, i), out) != R::Ok)
            return R::Err;
    return R::Ok;
}

// Independent of the order, as a frozenset's is.
R union_hash(Value v, u32 &out)
{
    u32 h = 0x5a17;
    for (usize i = 0; i < tuple_len(union_of(v)->args); i++) {
        u32 one = 0;
        if (py_hash(tuple_at(union_of(v)->args, i), one) != R::Ok)
            return R::Err;
        h ^= one * 0x9e3779b1u;
    }
    out = h;
    return R::Ok;
}

// The members a union is made of, each added once. `checked` is typing's
// rule, which a string fails; `|` has already checked its two sides.
struct Builder {
    Root list;

    bool init()
    {
        list = obj_value(list_new());
        return !list.v.is_nil() || oom() == R::Ok;
    }

    bool add(Value v, bool checked)
    {
        Root rv{ v };
        if (is_none(rv.v)) {
            rv = type_wrap(&none_type);
            if (rv.v.is_nil())
                return false;
        } else if (is_union(rv.v)) {
            Value args = union_of(rv.v)->args;
            for (usize i = 0; i < tuple_len(args); i++)
                if (!add(tuple_at(union_of(rv.v)->args, i), false))
                    return false;
            return true;
        } else if (checked && is_str(rv.v)) {
            return err_set("TypeError",
                           "a forward reference in a union needs typing, which is not here yet") ==
                   R::Ok;
        }
        ListObj *l = list_of(list.v);
        for (usize i = 0; i < l->items.size(); i++) {
            bool same = false;
            if (py_eq(l->items[i], rv.v, same) != R::Ok)
                return false;
            if (same)
                return true;
        }
        return list_push(list_of(list.v), rv.v) || oom() == R::Ok;
    }

    Value make()
    {
        ListObj *l = list_of(list.v);
        if (!l->items.size())
            return err_set("TypeError", "Cannot take a Union of no types."), Value();
        if (l->items.size() == 1)
            return l->items[0];
        TupleObj *t = tuple_new(l->items.size());
        if (!t)
            return oom(), Value();
        Root rt{ obj_value(t) };
        l = list_of(list.v);
        for (usize i = 0; i < l->items.size(); i++)
            static_cast<TupleObj *>(rt.v.obj())->items()[i] = l->items[i];
        UnionObj *u = static_cast<UnionObj *>(obj_alloc(&union_type, sizeof(UnionObj)));
        if (!u)
            return oom(), Value();
        u->args   = rt.v;
        u->params = Value();
        return obj_value(u);
    }
};

R union_getattr(Value v, StrObj *name, Value &out)
{
    Str n = name->str();
    if (n == "__args__")
        return out = union_of(v)->args, R::Ok;
    if (n == "__name__" || n == "__qualname__") {
        out = str_new("Union");
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (n == "__module__") {
        out = str_new("typing");
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (n == "__origin__") {
        out = type_wrap(&union_type);
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (n == "__parameters__") {
        if (union_of(v)->params.is_nil()) {
            Root rv{ v };
            Value p = typing_params(union_of(rv.v)->args);
            if (p.is_nil())
                return R::Err;
            union_of(rv.v)->params = p;
            v                      = rv.v;
        }
        return out = union_of(v)->params, R::Ok;
    }
    return R::NotImpl;
}

// `(T | None)[int]`: the members with the variables replaced.
R union_getitem(Value v, Value item, Value &out)
{
    Root self{ v }, ri{ item };
    Value params;
    StrObj *pn = str_intern("__parameters__");
    if (!pn || union_getattr(self.v, pn, params) != R::Ok)
        return R::Err;
    Root subst{ typing_subst(self.v, union_of(self.v)->args, params, ri.v) };
    if (subst.v.is_nil())
        return R::Err;
    out = union_from(subst.v);
    return out.is_nil() ? R::Err : R::Ok;
}

} // namespace

// `X | Y`, from either side: the two checks CPython makes on the way in.
R union_binop(Value a, Value b, Op op, Value &out)
{
    if (op != Op::Or)
        return R::NotImpl;
    if (!is_union(a) && !is_union(b))
        return union_or(a, b, out);
    Builder ub;
    if (!ub.init() || !ub.add(a, true) || !ub.add(b, true))
        return R::Err;
    out = ub.make();
    return out.is_nil() ? R::Err : R::Ok;
}

namespace {

R u_mro_entries(const CallArgs &a, Value &out)
{
    (void)out;
    if (!meth_args(a, "__mro_entries__", 1, 1))
        return R::Err;
    String text;
    if (union_repr(a.args[0], text) != R::Ok)
        return R::Err;
    Buf<160> b;
    b.put("Cannot subclass ").put(text.str());
    return err_set("TypeError", b.str());
}

R u_getitem(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__getitem__", 1, 1))
        return R::Err;
    return union_getitem(a.args[0], a.args[1], out);
}

R u_or(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__or__", 1, 1))
        return R::Err;
    R r = union_binop(a.args[0], a.args[1], Op::Or, out);
    if (r == R::NotImpl)
        out = value_notimpl();
    return r == R::Err ? R::Err : R::Ok;
}

R u_ror(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__ror__", 1, 1))
        return R::Err;
    R r = union_binop(a.args[1], a.args[0], Op::Or, out);
    if (r == R::NotImpl)
        out = value_notimpl();
    return r == R::Err ? R::Err : R::Ok;
}

// `typing.Union[int, str]`, an implicit class method.
R u_class_getitem(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__class_getitem__", 1, 1))
        return R::Err;
    out = union_from(a.args[1]);
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr Method UNION_METHODS[] = {
    { "__mro_entries__", u_mro_entries },
    { "__getitem__", u_getitem },
    { "__or__", u_or },
    { "__ror__", u_ror },
    { "__class_getitem__", u_class_getitem },
};

// `type.__or__` and its reflection, which is also what a generic alias has.
constexpr Method OR_METHODS[] = {
    { "__or__", u_or },
    { "__ror__", u_ror },
};

} // namespace

constexpr Type union_type{ .name    = "typing.Union",
                           .trace   = union_trace,
                           .hash    = union_hash,
                           .eq      = union_eq,
                           .repr    = union_repr,
                           .getitem = union_getitem,
                           .binop   = union_binop,
                           .getattr = union_getattr };

Value union_args(Value v)
{
    return union_of(v)->args;
}

bool is_unionable(Value v)
{
    return is_none(v) || is_type(v) || is_genalias(v) || is_union(v) || is_typealias(v);
}

R union_or(Value a, Value b, Value &out)
{
    if (!is_unionable(a) || !is_unionable(b))
        return R::NotImpl;
    Builder ub;
    if (!ub.init() || !ub.add(a, false) || !ub.add(b, false))
        return R::Err;
    out = ub.make();
    return out.is_nil() ? R::Err : R::Ok;
}

Value union_from(Value args)
{
    Root ra{ args };
    Builder ub;
    if (!ub.init())
        return Value();
    if (is_tuple(ra.v)) {
        for (usize i = 0; i < tuple_len(ra.v); i++)
            if (!ub.add(tuple_at(ra.v, i), true))
                return Value();
    } else if (!ub.add(ra.v, true)) {
        return Value();
    }
    return ub.make();
}

bool union_install()
{
    return method_install(&union_type, UNION_METHODS) && method_install(&type_type, OR_METHODS) &&
           method_install(&genalias_type, OR_METHODS);
}

Value union_as_tuple(Value v, Str who)
{
    if (!is_union(v))
        return v;
    Value args = union_of(v)->args;
    for (usize i = 0; i < tuple_len(args); i++)
        if (is_genalias(tuple_at(args, i))) {
            Buf<96> b;
            b.put(who).put("() argument 2 cannot be a parameterized generic");
            return err_set("TypeError", b.str()), Value();
        }
    return args;
}
