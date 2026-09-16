// The generic operations, and the number tower under them.
#include "ops.h"

#include "bigint.h"
#include "complex.h"
#include "gc.h"
#include "kernel/fmt.h"
#include "math/math.h"
#include "type.h"

namespace {

// int and bool on one side, float on neither: the exact arm, whatever the
// width -- bigint.cpp answers it and promotes rather than overflowing.
bool both_int(Value a, Value b)
{
    return is_intval(a) && is_intval(b);
}

// One integer and one float. Comparing those exactly is bigint.cpp's job:
// rounding the integer to a double first would make 2**53 and 2**53 + 1 the
// same number.
bool int_and_float(Value a, Value b, Value &i, f64 &x, bool &flip)
{
    if (is_intval(a) && is_float(b)) {
        i    = a;
        x    = float_of(b);
        flip = false;
        return true;
    }
    if (is_float(a) && is_intval(b)) {
        i    = b;
        x    = float_of(a);
        flip = true;
        return true;
    }
    return false;
}

bool both_number(Value a, Value b)
{
    f64 x, y;
    return as_number(a, x) && as_number(b, y);
}

R float_binop(f64 a, f64 b, Op op, Value &out)
{
    f64 r = 0;
    switch (op) {
    case Op::Add:
        r = a + b;
        break;
    case Op::Sub:
        r = a - b;
        break;
    case Op::Mul:
        r = a * b;
        break;
    case Op::Div:
        if (b == 0)
            return err_set("ZeroDivisionError", "float division by zero");
        r = a / b;
        break;
    case Op::FloorDiv:
        if (b == 0)
            return err_set("ZeroDivisionError", "float floor division by zero");
        r = floor(a / b);
        break;
    case Op::Mod:
        if (b == 0)
            return err_set("ZeroDivisionError", "float modulo");
        r = fmod(a, b);
        if (r != 0 && ((r < 0) != (b < 0)))
            r += b;
        break;
    case Op::Pow:
        r = pow(a, b);
        break;
    default:
        return err_set2("TypeError", "unsupported operand type(s)", op_symbol(op));
    }
    out = float_new(r);
    return out.is_nil() ? R::Err : R::Ok;
}

} // namespace

Str op_symbol(Op op)
{
    switch (op) {
    case Op::Add:
        return "+";
    case Op::Sub:
        return "-";
    case Op::Mul:
        return "*";
    case Op::Div:
        return "/";
    case Op::FloorDiv:
        return "//";
    case Op::Mod:
        return "%";
    case Op::Pow:
        return "**";
    case Op::And:
        return "&";
    case Op::Or:
        return "|";
    case Op::Xor:
        return "^";
    case Op::Lsh:
        return "<<";
    case Op::Rsh:
        return ">>";
    case Op::MatMul:
        return "@";
    }
    return "?";
}

bool py_truth(Value v)
{
    if (v.is_int())
        return v.as_int() != 0;
    if (is_none(v))
        return false;
    const Type *t = type_of(v);
    if (t && t->truth)
        return t->truth(v);
    usize n = 0;
    if (t && t->len && t->len(v, n) == R::Ok)
        return n != 0;
    return true;
}

R py_hash(Value v, u32 &out)
{
    i64 n = 0;
    if (as_index(v, n)) {
        out = u32(n);
        return R::Ok;
    }
    if (type_unhashable(v))
        return err_set2("TypeError", "unhashable type", type_name(v));
    const Type *t = type_of(v);
    if (t && t->hash)
        return t->hash(v, out);
    // A container is unhashable; anything else hashes by identity, which is
    // what CPython's default __hash__ does.
    if (!t || t->len)
        return err_set2("TypeError", "unhashable type", type_name(v));
    out = u32(usize(v.obj())) >> 4;
    return R::Ok;
}

R py_eq(Value a, Value b, bool &out)
{
    if (both_int(a, b))
        return int_compare(a, b, Cmp::Eq, out);
    {
        Value i;
        f64 x   = 0;
        bool fl = false;
        if (int_and_float(a, b, i, x, fl))
            return intfloat_compare(i, x, fl, Cmp::Eq, out);
    }
    if (both_number(a, b)) {
        f64 x, y;
        as_number(a, x);
        as_number(b, y);
        out = x == y;
        return R::Ok;
    }

    const Type *t = type_of(a);
    if (t && t->eq) {
        R r = t->eq(a, b, out);
        if (r != R::NotImpl)
            return r;
    }
    const Type *u = type_of(b);
    if (u && u->eq) {
        R r = u->eq(b, a, out);
        if (r != R::NotImpl)
            return r;
    }
    // Two objects of types that do not compare are equal only if identical.
    out = a == b;
    return R::Ok;
}

R py_cmp(Value a, Value b, Cmp op, bool &out)
{
    // `in` and `is` are not orderings; the VM answers them itself.
    if (op >= Cmp::In)
        return err_set2("SystemError", "not an ordering", cmp_symbol(op));
    if (op == Cmp::Eq || op == Cmp::Ne) {
        R r = py_eq(a, b, out);
        if (r == R::Ok && op == Cmp::Ne)
            out = !out;
        return r;
    }

    if (both_int(a, b))
        return int_compare(a, b, op, out);
    {
        Value i;
        f64 x   = 0;
        bool fl = false;
        if (int_and_float(a, b, i, x, fl))
            return intfloat_compare(i, x, fl, op, out);
    }
    if (both_number(a, b)) {
        f64 x, y;
        as_number(a, x);
        as_number(b, y);
        out = op == Cmp::Lt ? x < y : op == Cmp::Le ? x <= y : op == Cmp::Gt ? x > y : x >= y;
        return R::Ok;
    }

    const Type *t = type_of(a);
    if (t && t->order) {
        R r = t->order(a, b, op, out);
        if (r != R::NotImpl)
            return r;
    }
    // The other side's turn, with the operator reflected: a subclass of a
    // built-in answers for a plain one on the left.
    const Type *u = type_of(b);
    if (u && u != t && u->order) {
        Cmp back = op == Cmp::Lt   ? Cmp::Gt
                   : op == Cmp::Le ? Cmp::Ge
                   : op == Cmp::Gt ? Cmp::Lt
                                   : Cmp::Le;
        R r      = u->order(b, a, back, out);
        if (r != R::NotImpl)
            return r;
    }

    Buf<96> b2;
    b2.put("'").put(cmp_symbol(op)).put("' not supported between instances of '");
    b2.put(type_name(a)).put("' and '").put(type_name(b)).put("'");
    return err_set("TypeError", b2.str());
}

R py_repr(Value v, String &out)
{
    const Type *t = type_of(v);
    if (!t)
        return err_set("SystemError", "repr of no value");
    if (!t->repr)
        return err_set2("SystemError", "no repr", t->name);
    return t->repr(v, out);
}

R py_str(Value v, String &out)
{
    const Type *t = type_of(v);
    if (t && t->str)
        return t->str(v, out);
    return py_repr(v, out);
}

R py_len(Value v, usize &out)
{
    const Type *t = type_of(v);
    if (!t || !t->len)
        return err_set2("TypeError", "object of this type has no len()", type_name(v));
    return t->len(v, out);
}

R py_getitem(Value v, Value key, Value &out)
{
    const Type *t = type_of(v);
    if (!t || !t->getitem)
        return err_set2("TypeError", "object is not subscriptable", type_name(v));
    return t->getitem(v, key, out);
}

R py_setitem(Value v, Value key, Value item)
{
    const Type *t = type_of(v);
    if (!t || !t->setitem)
        return err_set2("TypeError", "object does not support item assignment", type_name(v));
    return t->setitem(v, key, item);
}

R py_delitem(Value v, Value key)
{
    const Type *t = type_of(v);
    if (!t || !t->delitem)
        return err_set2("TypeError", "object does not support item deletion", type_name(v));
    return t->delitem(v, key);
}

// Anything that iterates answers `in` by walking itself.
R py_contains(Value v, Value item, bool &out)
{
    const Type *t = type_of(v);
    if (t && t->contains)
        return t->contains(v, item, out);
    if (!t || !t->iter) {
        Buf<96> m;
        m.put("argument of type '").put(type_name(v)).put("' is not a container or iterable");
        return err_set("TypeError", m.str());
    }

    Root ri{ item };
    Root it{ py_iter(v) };
    if (it.v.is_nil())
        return R::Err;
    for (;;) {
        Root got;
        R r = py_next(it.v, got.v);
        if (r == R::Err)
            return R::Err;
        if (r == R::NotImpl) {
            out = false;
            return R::Ok;
        }
        bool same = false;
        if (py_eq(got.v, ri.v, same) != R::Ok)
            return R::Err;
        if (same) {
            out = true;
            return R::Ok;
        }
    }
}

R py_getattr(Value v, StrObj *name, Value &out)
{
    switch (py_attr(v, name, out)) {
    case Got::Ok:
        return R::Ok;
    case Got::Error:
        return R::Err;
    case Got::Call:
        // A property getter is Python; only the VM can run one.
        return err_set2("TypeError", "this attribute needs the interpreter", name->str());
    case Got::Missing:
        break;
    }
    Buf<96> m;
    m.put("'").put(type_name(v)).put("' object has no attribute '").put(name->str()).put("'");
    return err_set("AttributeError", m.str());
}

R not_iterable(Value v)
{
    Buf<96> m;
    m.put("'").put(type_name(v)).put("' object is not iterable");
    return err_set("TypeError", m.str());
}

Value py_iter(Value v)
{
    const Type *t = type_of(v);
    if (!t || !t->iter)
        return not_iterable(v), Value();
    return t->iter(v);
}

R py_next(Value it, Value &out)
{
    const Type *t = type_of(it);
    if (!t || !t->next) {
        Buf<96> m;
        m.put("'").put(type_name(it)).put("' object is not an iterator");
        return err_set("TypeError", m.str());
    }
    return t->next(it, out);
}

ListObj *py_list_of(Value v)
{
    Root rv{ v };
    ListObj *l = list_new();
    if (!l)
        return err_set("MemoryError", "out of memory"), nullptr;
    Root rl{ obj_value(l) };
    Root it{ py_iter(rv.v) };
    if (it.v.is_nil())
        return nullptr;
    for (;;) {
        Root got;
        R r = py_next(it.v, got.v);
        if (r == R::Err)
            return nullptr;
        if (r == R::NotImpl)
            break;
        if (!list_push(list_of(rl.v), got.v))
            return err_set("MemoryError", "out of memory"), nullptr;
    }
    return list_of(rl.v);
}

R py_binop_try(Value a, Value b, Op op, Value &out)
{
    if (op == Op::MatMul && !is_inst(a) && !is_inst(b))
        return R::NotImpl; // no built-in multiplies matrices
    if (both_int(a, b))
        return int_arith(a, b, op, out);
    if (both_number(a, b)) {
        f64 x, y;
        as_number(a, x);
        as_number(b, y);
        return float_binop(x, y, op, out);
    }

    const Type *t = type_of(a);
    if (t && t->binop) {
        R r = t->binop(a, b, op, out);
        if (r != R::NotImpl)
            return r;
    }
    const Type *u = type_of(b);
    if (u && u != t && u->binop) {
        R r = u->binop(a, b, op, out);
        if (r != R::NotImpl)
            return r;
    }

    return R::NotImpl;
}

R py_binop(Value a, Value b, Op op, Value &out)
{
    R r = py_binop_try(a, b, op, out);
    if (r != R::NotImpl)
        return r;
    return binop_failed(a, b, op);
}

R binop_failed(Value a, Value b, Op op)
{
    Buf<160> m;
    // A subclass of a built-in says what the built-in would.
    Value seq = is_inst(a) && !inst_of(a)->native.is_nil() ? inst_of(a)->native : a;
    if (op == Op::Add && (is_str(seq) || is_list(seq) || is_tuple(seq))) {
        m.put("can only concatenate ").put(type_name(seq)).put(" (not \"").put(type_name(b));
        m.put("\") to ").put(type_name(seq));
    } else if (op == Op::Add && (is_bytes(seq) || is_bytearray(seq))) {
        m.put("can't concat ").put(type_name(b)).put(" to ").put(type_name(seq));
    } else {
        m.put("unsupported operand type(s) for ").put(op_symbol(op));
        m.put(": '").put(type_name(a)).put("' and '").put(type_name(b)).put("'");
    }
    return err_set("TypeError", m.str());
}

R py_inplace(Value a, Value b, Op op, Value &out)
{
    // The one mutating case: `a += b` on a list extends it.
    if (op == Op::Add && is_list(a)) {
        Root ra{ a }, rb{ b };
        Root it{ py_iter(rb.v) };
        if (it.v.is_nil())
            return R::Err;
        for (;;) {
            Root got;
            R r = py_next(it.v, got.v);
            if (r == R::Err)
                return R::Err;
            if (r == R::NotImpl)
                break;
            if (!list_push(list_of(ra.v), got.v))
                return err_set("MemoryError", "out of memory");
        }
        out = ra.v;
        return R::Ok;
    }
    // `s |= t` on a set replaces its members: the name keeps the same set.
    if (is_set(a) && (op == Op::Or || op == Op::And || op == Op::Sub || op == Op::Xor)) {
        Root ra{ a }, rb{ b };
        Root made;
        if (py_binop(ra.v, rb.v, op, made.v) != R::Ok)
            return R::Err;
        SetObj *self = set_at(ra.v);
        self->t.entries.clear();
        self->t.index.clear();
        self->t.live = 0;
        usize at     = 0;
        Value k, val;
        while (table_next(set_at(made.v)->t, at, k, val))
            if (set_add(set_at(ra.v), k) != R::Ok)
                return R::Err;
        out = ra.v;
        return R::Ok;
    }
    return py_binop(a, b, op, out);
}

R py_pos(Value a, Value &out)
{
    if (is_intval(a)) {
        // `+True` is 1, so bool does not simply pass through.
        out = is_bool(a) ? Value::of_int(is_true(a) ? 1 : 0) : a;
        return R::Ok;
    }
    if (is_complex(a)) {
        out = a;
        return R::Ok;
    }
    if (is_float(a)) {
        out = a;
        return R::Ok;
    }
    return err_set2("TypeError", "bad operand type for unary +", type_name(a));
}

R py_invert(Value a, Value &out)
{
    if (is_intval(a))
        return int_invert_op(a, out);
    return err_set2("TypeError", "bad operand type for unary ~", type_name(a));
}

R py_neg(Value a, Value &out)
{
    if (is_intval(a))
        return int_negate(a, out);
    if (is_complex(a))
        return complex_negate(a, out);
    if (is_float(a)) {
        out = float_new(-float_of(a));
        return out.is_nil() ? R::Err : R::Ok;
    }
    return err_set2("TypeError", "bad operand type for unary -", type_name(a));
}

R index_of(Value key, usize len, usize &out)
{
    i64 n = 0;
    if (!as_index(key, n))
        return err_set2("TypeError", "indices must be integers", type_name(key));
    if (n < 0)
        n += i64(len);
    if (n < 0 || n >= i64(len))
        return err_set("IndexError", "index out of range");
    out = usize(n);
    return R::Ok;
}
