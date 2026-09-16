// `operator`: the operators as functions, plus the three callable objects --
// attrgetter, itemgetter and methodcaller -- that everything from sorted() to
// heapq reaches for.
//
// The work here is that an operand may be a class instance whose operator is
// written in Python, and a builtin cannot make that call: ground rule 2. So
// `add` is not py_binop, it is a small state machine that tries `a.__add__(b)`
// and then `b.__radd__(a)`, one call at a time, with the VM driving it.
#include "bigint.h"
#include "call.h"
#include "compare.h"
#include "complex.h"
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

// ------------------------------------------------- an operator that may call

// One row: the operator, the method a class writes for it, and the reflected
// method the right operand gets its turn with.
struct Dunder {
    Op op;
    Str name, refl, in_place;
};

constexpr Dunder DUNDERS[] = {
    { Op::Add, "__add__", "__radd__", "__iadd__" },
    { Op::Sub, "__sub__", "__rsub__", "__isub__" },
    { Op::Mul, "__mul__", "__rmul__", "__imul__" },
    { Op::Div, "__truediv__", "__rtruediv__", "__itruediv__" },
    { Op::FloorDiv, "__floordiv__", "__rfloordiv__", "__ifloordiv__" },
    { Op::Mod, "__mod__", "__rmod__", "__imod__" },
    { Op::Pow, "__pow__", "__rpow__", "__ipow__" },
    { Op::And, "__and__", "__rand__", "__iand__" },
    { Op::Or, "__or__", "__ror__", "__ior__" },
    { Op::Xor, "__xor__", "__rxor__", "__ixor__" },
    { Op::Lsh, "__lshift__", "__rlshift__", "__ilshift__" },
    { Op::Rsh, "__rshift__", "__rrshift__", "__irshift__" },
};

const Dunder &dunder_of(Op op)
{
    for (const Dunder &d : DUNDERS)
        if (d.op == op)
            return d;
    return DUNDERS[0];
}

// s[0] and s[1] are the operands, s[2] and s[3] the two methods to try in
// order; `i` is how many have been tried.
R binop_step(ContObj *k, Value in)
{
    if (k->i && !in.is_nil() && !is_notimpl(in))
        return cont_done(k, in);
    while (k->i < 2) {
        Value m = k->s[2 + k->i];
        k->i++;
        if (m.is_nil())
            continue;
        return cont_call(k, m, k->s[k->i == 1 ? 1 : 0]);
    }
    // Neither side answered in Python. The built-in operation is what is left,
    // and it raises the TypeError if it cannot do it either.
    Value got;
    if (py_binop(k->s[0], k->s[1], Op(k->j), got) != R::Ok)
        return R::Err;
    return cont_done(k, got);
}

// `a op b`. `out` may come back a ContObj, which the VM lands.
R binary(Value a, Value b, Op op, Value &out)
{
    if (!is_inst(a) && !is_inst(b))
        return py_binop(a, b, op, out);
    Root ra{ a }, rb{ b };
    const Dunder &d = dunder_of(op);
    Root left{ type_special(ra.v, d.name) };
    Root right;
    if (type_of(ra.v) != type_of(rb.v))
        right = type_special(rb.v, d.refl);
    if (left.v.is_nil() && right.v.is_nil())
        return py_binop(ra.v, rb.v, op, out);
    Root kv{ cont_new(binop_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = ra.v;
    k->s[1]    = rb.v;
    k->s[2]    = left.v;
    k->s[3]    = right.v;
    k->j       = u32(op);
    out        = kv.v;
    return R::Ok;
}

// The same for `a op= b`: the in-place method first, then the plain one.
R inplace_step(ContObj *k, Value in)
{
    if (k->i && !in.is_nil() && !is_notimpl(in))
        return cont_done(k, in);
    if (k->i++ == 0 && !k->s[2].is_nil())
        return cont_call(k, k->s[2], k->s[1]);
    Value got;
    if (binary(k->s[0], k->s[1], Op(k->j), got) != R::Ok)
        return R::Err;
    return cont_done(k, got);
}

R inplace(Value a, Value b, Op op, Value &out)
{
    if (!is_inst(a))
        return py_inplace(a, b, op, out);
    Root ra{ a }, rb{ b };
    Root m{ type_special(ra.v, dunder_of(op).in_place) };
    if (m.v.is_nil())
        return binary(ra.v, rb.v, op, out);
    Root kv{ cont_new(inplace_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = ra.v;
    k->s[1]    = rb.v;
    k->s[2]    = m.v;
    k->j       = u32(op);
    out        = kv.v;
    return R::Ok;
}

struct CmpDunder {
    Cmp op;
    Str name, refl;
};

constexpr CmpDunder CMPS[] = {
    { Cmp::Eq, "__eq__", "__eq__" }, { Cmp::Ne, "__ne__", "__ne__" },
    { Cmp::Lt, "__lt__", "__gt__" }, { Cmp::Le, "__le__", "__ge__" },
    { Cmp::Gt, "__gt__", "__lt__" }, { Cmp::Ge, "__ge__", "__le__" },
};

// The comparison's answer is a bool whatever the method returned, except that
// NotImplemented means the other side gets its turn.
R cmp_step(ContObj *k, Value in)
{
    if (k->i && !in.is_nil() && !is_notimpl(in))
        return cont_done(k, value_bool(py_truth(in)));
    while (k->i < 2) {
        Value m = k->s[2 + k->i];
        k->i++;
        if (m.is_nil())
            continue;
        return cont_call(k, m, k->s[k->i == 1 ? 1 : 0]);
    }
    bool got = false;
    if (py_cmp(k->s[0], k->s[1], Cmp(k->j), got) != R::Ok)
        return R::Err;
    return cont_done(k, value_bool(got));
}

R compare(Value a, Value b, Cmp op, Value &out)
{
    if (!is_inst(a) && !is_inst(b)) {
        bool got = false;
        if (py_cmp(a, b, op, got) != R::Ok)
            return R::Err;
        out = value_bool(got);
        return R::Ok;
    }
    Root ra{ a }, rb{ b };
    const CmpDunder *d = &CMPS[0];
    for (const CmpDunder &one : CMPS)
        if (one.op == op)
            d = &one;
    // The mirror method is tried even when both sides are the same type: `a >
    // b` falls back to `b.__lt__(a)`, which is how a class that writes only
    // __lt__ is orderable at all. A binary operator's reflected method is not
    // tried that way, and binary() above keeps that difference.
    Root left{ type_special(ra.v, d->name) };
    Root right{ type_special(rb.v, d->refl) };
    Root kv{ cont_new(cmp_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = ra.v;
    k->s[1]    = rb.v;
    k->s[2]    = left.v;
    k->s[3]    = right.v;
    k->j       = u32(op);
    out        = kv.v;
    return R::Ok;
}

// ------------------------------------------------------------- the operators

template <usize I>
R op_at(const CallArgs &a, Value &out)
{
    if (!args_only(a, DUNDERS[I].name.substr(2, DUNDERS[I].name.size() - 4), 2, 2))
        return R::Err;
    return binary(a.args[0], a.args[1], DUNDERS[I].op, out);
}

template <usize I>
R iop_at(const CallArgs &a, Value &out)
{
    if (!args_only(a, DUNDERS[I].in_place.substr(2, DUNDERS[I].in_place.size() - 4), 2, 2))
        return R::Err;
    return inplace(a.args[0], a.args[1], DUNDERS[I].op, out);
}

template <usize I>
R cmp_at(const CallArgs &a, Value &out)
{
    if (!args_only(a, CMPS[I].name.substr(2, CMPS[I].name.size() - 4), 2, 2))
        return R::Err;
    return compare(a.args[0], a.args[1], CMPS[I].op, out);
}

// A unary operator a class may write in Python, the same shape as binary().
R unary_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call(k, k->s[0], Value(), 0);
    return cont_done(k, in);
}

R unary(Value v, Str name, R (*plain)(Value, Value &), Value &out)
{
    if (!is_inst(v))
        return plain(v, out);
    Root rv{ v };
    Root m{ type_special(rv.v, name) };
    if (m.v.is_nil())
        return plain(rv.v, out);
    Root kv{ cont_new(unary_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = m.v;
    out                 = kv.v;
    return R::Ok;
}

R o_neg(const CallArgs &a, Value &out)
{
    if (!args_only(a, "neg", 1, 1))
        return R::Err;
    return unary(a.args[0], "__neg__", py_neg, out);
}

R o_pos(const CallArgs &a, Value &out)
{
    if (!args_only(a, "pos", 1, 1))
        return R::Err;
    return unary(a.args[0], "__pos__", py_pos, out);
}

R o_invert(const CallArgs &a, Value &out)
{
    if (!args_only(a, "invert", 1, 1))
        return R::Err;
    return unary(a.args[0], "__invert__", py_invert, out);
}

R plain_abs(Value v, Value &out)
{
    if (is_intval(v))
        return int_absolute(v, out);
    if (is_float(v)) {
        f64 x = float_of(v);
        out   = float_new(x < 0 ? -x : x);
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (is_complex(v))
        return complex_abs(v, out);
    return err_set2("TypeError", "bad operand type for abs()", type_name(v));
}

R o_abs(const CallArgs &a, Value &out)
{
    if (!args_only(a, "abs", 1, 1))
        return R::Err;
    return unary(a.args[0], "__abs__", plain_abs, out);
}

R o_not(const CallArgs &a, Value &out)
{
    if (!args_only(a, "not_", 1, 1))
        return R::Err;
    out = value_bool(!py_truth(a.args[0]));
    return R::Ok;
}

R o_truth(const CallArgs &a, Value &out)
{
    if (!args_only(a, "truth", 1, 1))
        return R::Err;
    out = value_bool(py_truth(a.args[0]));
    return R::Ok;
}

R o_is(const CallArgs &a, Value &out)
{
    if (!args_only(a, "is_", 2, 2))
        return R::Err;
    out = value_bool(a.args[0] == a.args[1]);
    return R::Ok;
}

R o_is_not(const CallArgs &a, Value &out)
{
    if (!args_only(a, "is_not", 2, 2))
        return R::Err;
    out = value_bool(!(a.args[0] == a.args[1]));
    return R::Ok;
}

R o_index(const CallArgs &a, Value &out)
{
    if (!args_only(a, "index", 1, 1))
        return R::Err;
    i64 n = 0;
    if (!as_index(a.args[0], n))
        return err_set2("TypeError", "object cannot be interpreted as an integer",
                        type_name(a.args[0]));
    out = a.args[0];
    return R::Ok;
}

R o_length(const CallArgs &a, Value &out)
{
    if (!args_only(a, "length", 1, 1))
        return R::Err;
    usize n = 0;
    if (py_len(a.args[0], n) != R::Ok)
        return R::Err;
    out = int_from_i64(i64(n));
    return out.is_nil() ? R::Err : R::Ok;
}

R o_length_hint(const CallArgs &a, Value &out)
{
    if (!args_only(a, "length_hint", 1, 2))
        return R::Err;
    usize n = 0;
    if (py_len(a.args[0], n) == R::Ok) {
        out = int_from_i64(i64(n));
        return out.is_nil() ? R::Err : R::Ok;
    }
    err_clear();
    out = a.nargs > 1 ? a.args[1] : Value::of_int(0);
    return R::Ok;
}

R o_getitem(const CallArgs &a, Value &out)
{
    if (!args_only(a, "getitem", 2, 2))
        return R::Err;
    return py_getitem(a.args[0], a.args[1], out);
}

R o_setitem(const CallArgs &a, Value &out)
{
    if (!args_only(a, "setitem", 3, 3))
        return R::Err;
    if (py_setitem(a.args[0], a.args[1], a.args[2]) != R::Ok)
        return R::Err;
    out = value_none();
    return R::Ok;
}

R o_delitem(const CallArgs &a, Value &out)
{
    if (!args_only(a, "delitem", 2, 2))
        return R::Err;
    if (py_delitem(a.args[0], a.args[1]) != R::Ok)
        return R::Err;
    out = value_none();
    return R::Ok;
}

R o_contains(const CallArgs &a, Value &out)
{
    if (!args_only(a, "contains", 2, 2))
        return R::Err;
    bool got = false;
    if (py_contains(a.args[0], a.args[1], got) != R::Ok)
        return R::Err;
    out = value_bool(got);
    return R::Ok;
}

R o_concat(const CallArgs &a, Value &out)
{
    if (!args_only(a, "concat", 2, 2))
        return R::Err;
    return binary(a.args[0], a.args[1], Op::Add, out);
}

R o_iconcat(const CallArgs &a, Value &out)
{
    if (!args_only(a, "iconcat", 2, 2))
        return R::Err;
    return inplace(a.args[0], a.args[1], Op::Add, out);
}

// countOf and indexOf walk the sequence themselves, so an item whose __eq__ is
// Python is answered by cmp_find rather than refused.
R count_or_index(const CallArgs &a, Value &out, bool count)
{
    Str who = count ? Str("countOf") : Str("indexOf");
    if (!args_only(a, who, 2, 2))
        return R::Err;
    ListObj *items = py_list_of(a.args[0]);
    if (!items)
        return R::Err;
    Root rl{ obj_value(items) };
    out = cmp_find(rl.v, a.args[1], count ? CMP_COUNT : CMP_INDEX, 0, list_of(rl.v)->items.size());
    return out.is_nil() ? R::Err : R::Ok;
}

R o_countOf(const CallArgs &a, Value &out)
{
    return count_or_index(a, out, true);
}

R o_indexOf(const CallArgs &a, Value &out)
{
    return count_or_index(a, out, false);
}

// ---------------------------------------------------------- the getters

// attrgetter('a', 'b.c'): the names, split on their dots ahead of time.
struct GetObj : Obj {
    Value keys; // TupleObj: names for attrgetter, keys for itemgetter
    Value args; // methodcaller's own arguments
    Value kwn;  // and their keyword names
    Value kwv;
    bool attr;   // an attrgetter rather than an itemgetter
    bool method; // a methodcaller
};

GetObj *get_of(Value v)
{
    return static_cast<GetObj *>(v.obj());
}

void get_trace(Obj *o)
{
    GetObj *g = static_cast<GetObj *>(o);
    gc_mark(g->keys);
    gc_mark(g->args);
    gc_mark(g->kwn);
    gc_mark(g->kwv);
}

R get_repr(Value v, String &out)
{
    GetObj *g   = get_of(v);
    TupleObj *k = static_cast<TupleObj *>(g->keys.obj());
    Str who     = g->method ? Str("operator.methodcaller")
                  : g->attr ? Str("operator.attrgetter")
                            : Str("operator.itemgetter");
    if (!out.append(who) || !out.push('('))
        return oom();
    for (u32 i = 0; i < k->len; i++) {
        if (i && !out.append(", "))
            return oom();
        if (py_repr(k->items()[i], out) != R::Ok)
            return R::Err;
    }
    return out.push(')') ? R::Ok : oom();
}

extern const Type getter_type;

// methodcaller: the arguments it kept, applied to the method just found.
R call_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call_kw(k, k->s[0], k->s[1], k->s[2], k->s[3]);
    return cont_done(k, in);
}

// One name of an attrgetter, which may be dotted: `a.b` is two lookups.
R dotted_get(Value from, Str name, Value &out)
{
    Root cur{ from };
    usize at = 0;
    while (at <= name.size()) {
        usize dot = at;
        while (dot < name.size() && name[dot] != '.')
            dot++;
        StrObj *n = str_intern(name.substr(at, dot - at));
        if (!n)
            return oom();
        Value got;
        if (py_getattr(cur.v, n, got) != R::Ok)
            return R::Err;
        cur = got;
        if (dot >= name.size())
            break;
        at = dot + 1;
    }
    out = cur.v;
    return R::Ok;
}

R getter_call(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__call__", 1, 1))
        return R::Err;
    Root self{ method_self(a.args[0]) }, obj{ a.args[1] };
    GetObj *g   = get_of(self.v);
    TupleObj *k = static_cast<TupleObj *>(g->keys.obj());

    if (g->method) {
        // A method call is a lookup and then a call, and the call may be
        // Python, so the answer is a continuation.
        Root fn;
        if (dotted_get(obj.v, str_of(k->items()[0])->str(), fn.v) != R::Ok)
            return R::Err;
        Root kv{ cont_new(call_step) };
        if (kv.v.is_nil())
            return R::Err;
        GetObj *g2          = get_of(self.v);
        cont_of(kv.v)->s[0] = fn.v;
        cont_of(kv.v)->s[1] = g2->args;
        cont_of(kv.v)->s[2] = g2->kwn;
        cont_of(kv.v)->s[3] = g2->kwv;
        out                 = kv.v;
        return R::Ok;
    }

    if (k->len == 1) {
        if (g->attr)
            return dotted_get(obj.v, str_of(k->items()[0])->str(), out);
        return py_getitem(obj.v, k->items()[0], out);
    }
    TupleObj *made = tuple_new(k->len);
    if (!made)
        return oom();
    Root rm{ obj_value(made) };
    for (u32 i = 0; i < static_cast<TupleObj *>(get_of(self.v)->keys.obj())->len; i++) {
        Value key = static_cast<TupleObj *>(get_of(self.v)->keys.obj())->items()[i];
        Value got;
        R r = get_of(self.v)->attr ? dotted_get(obj.v, str_of(key)->str(), got)
                                   : py_getitem(obj.v, key, got);
        if (r != R::Ok)
            return R::Err;
        static_cast<TupleObj *>(rm.v.obj())->items()[i] = got;
    }
    out = rm.v;
    return R::Ok;
}

constexpr Method GETTER_METHODS[] = { { "__call__", getter_call } };

constexpr Type getter_type{ .name = "attrgetter", .trace = get_trace, .repr = get_repr };

Value getter_new(const CallArgs &a, bool attr, bool method, Str who)
{
    if (a.nargs < 1) {
        Buf<64> b;
        b.put(who).put(" expected 1 argument, got 0");
        return err_set("TypeError", b.str()), Value();
    }
    u32 n       = method ? 1 : a.nargs;
    TupleObj *k = tuple_new(n);
    if (!k)
        return oom(), Value();
    Root rk{ obj_value(k) };
    for (u32 i = 0; i < n; i++) {
        if ((attr || method) && !is_str(a.args[i]))
            return err_set("TypeError", "attribute name must be a string"), Value();
        static_cast<TupleObj *>(rk.v.obj())->items()[i] = a.args[i];
    }
    // methodcaller keeps the rest of the call to make again each time.
    Root ra, rn, rv;
    if (method) {
        TupleObj *rest = tuple_new(a.nargs - 1);
        if (!rest)
            return oom(), Value();
        for (u32 i = 1; i < a.nargs; i++)
            rest->items()[i - 1] = a.args[i];
        ra           = obj_value(rest);
        TupleObj *n1 = tuple_new(a.nkw);
        if (!n1)
            return oom(), Value();
        for (u32 i = 0; i < a.nkw; i++)
            n1->items()[i] = a.kwnames[i];
        rn           = obj_value(n1);
        TupleObj *v1 = tuple_new(a.nkw);
        if (!v1)
            return oom(), Value();
        for (u32 i = 0; i < a.nkw; i++)
            v1->items()[i] = a.kwvals[i];
        rv = obj_value(v1);
    } else if (a.nkw) {
        Buf<64> b;
        b.put(who).put("() takes no keyword arguments");
        return err_set("TypeError", b.str()), Value();
    }

    GetObj *g = static_cast<GetObj *>(obj_alloc(&getter_type, sizeof(GetObj)));
    if (!g)
        return oom(), Value();
    g->keys   = rk.v;
    g->args   = ra.v;
    g->kwn    = rn.v;
    g->kwv    = rv.v;
    g->attr   = attr;
    g->method = method;
    return obj_value(g);
}

R o_attrgetter(const CallArgs &a, Value &out)
{
    out = getter_new(a, true, false, "attrgetter");
    return out.is_nil() ? R::Err : R::Ok;
}

R o_itemgetter(const CallArgs &a, Value &out)
{
    out = getter_new(a, false, false, "itemgetter");
    return out.is_nil() ? R::Err : R::Ok;
}

R o_methodcaller(const CallArgs &a, Value &out)
{
    out = getter_new(a, true, true, "methodcaller");
    return out.is_nil() ? R::Err : R::Ok;
}

#define OP_DEF(i, plain, dund) \
    { plain, op_at<i> },       \
    {                          \
        dund, op_at<i>         \
    }

constexpr ModDef BINARY_DEFS[] = {
    OP_DEF(0, "add", "__add__"),
    OP_DEF(1, "sub", "__sub__"),
    OP_DEF(2, "mul", "__mul__"),
    OP_DEF(3, "truediv", "__truediv__"),
    OP_DEF(4, "floordiv", "__floordiv__"),
    OP_DEF(5, "mod", "__mod__"),
    OP_DEF(6, "pow", "__pow__"),
    OP_DEF(7, "and_", "__and__"),
    OP_DEF(8, "or_", "__or__"),
    OP_DEF(9, "xor", "__xor__"),
    OP_DEF(10, "lshift", "__lshift__"),
    OP_DEF(11, "rshift", "__rshift__"),
};

#undef OP_DEF

#define IOP_DEF(i, plain, dund) \
    { plain, iop_at<i> },       \
    {                           \
        dund, iop_at<i>         \
    }

constexpr ModDef INPLACE_DEFS[] = {
    IOP_DEF(0, "iadd", "__iadd__"),
    IOP_DEF(1, "isub", "__isub__"),
    IOP_DEF(2, "imul", "__imul__"),
    IOP_DEF(3, "itruediv", "__itruediv__"),
    IOP_DEF(4, "ifloordiv", "__ifloordiv__"),
    IOP_DEF(5, "imod", "__imod__"),
    IOP_DEF(6, "ipow", "__ipow__"),
    IOP_DEF(7, "iand", "__iand__"),
    IOP_DEF(8, "ior", "__ior__"),
    IOP_DEF(9, "ixor", "__ixor__"),
    IOP_DEF(10, "ilshift", "__ilshift__"),
    IOP_DEF(11, "irshift", "__irshift__"),
};

#undef IOP_DEF

#define CMP_DEF(i, plain, dund) \
    { plain, cmp_at<i> },       \
    {                           \
        dund, cmp_at<i>         \
    }

constexpr ModDef CMP_DEFS[] = {
    CMP_DEF(0, "eq", "__eq__"), CMP_DEF(1, "ne", "__ne__"), CMP_DEF(2, "lt", "__lt__"),
    CMP_DEF(3, "le", "__le__"), CMP_DEF(4, "gt", "__gt__"), CMP_DEF(5, "ge", "__ge__"),
};

#undef CMP_DEF

constexpr ModDef REST_DEFS[] = {
    { "neg", o_neg },
    { "__neg__", o_neg },
    { "pos", o_pos },
    { "__pos__", o_pos },
    { "invert", o_invert },
    { "inv", o_invert },
    { "__invert__", o_invert },
    { "__inv__", o_invert },
    { "abs", o_abs },
    { "__abs__", o_abs },
    { "not_", o_not },
    { "__not__", o_not },
    { "truth", o_truth },
    { "is_", o_is },
    { "is_not", o_is_not },
    { "index", o_index },
    { "__index__", o_index },
    { "length", o_length },
    { "__len__", o_length },
    { "length_hint", o_length_hint },
    { "getitem", o_getitem },
    { "__getitem__", o_getitem },
    { "setitem", o_setitem },
    { "__setitem__", o_setitem },
    { "delitem", o_delitem },
    { "__delitem__", o_delitem },
    { "contains", o_contains },
    { "__contains__", o_contains },
    { "concat", o_concat },
    { "__concat__", o_concat },
    { "iconcat", o_iconcat },
    { "__iconcat__", o_iconcat },
    { "countOf", o_countOf },
    { "indexOf", o_indexOf },
    { "attrgetter", o_attrgetter },
    { "itemgetter", o_itemgetter },
    { "methodcaller", o_methodcaller },
};

} // namespace

bool operator_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    if (!method_install(&getter_type, GETTER_METHODS))
        return false;
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    return mod_defs(d, BINARY_DEFS) && mod_defs(d, INPLACE_DEFS) && mod_defs(d, CMP_DEFS) &&
           mod_defs(d, REST_DEFS);
}
