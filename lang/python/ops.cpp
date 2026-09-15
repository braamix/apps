// The generic operations, and the number tower under them.
#include "ops.h"

#include "kernel/fmt.h"
#include "math/math.h"

namespace {

// int and bool on one side, float on neither: the exact arm.
bool both_int(Value a, Value b)
{
    i64 x, y;
    return as_index(a, x) && as_index(b, y);
}

bool both_number(Value a, Value b)
{
    f64 x, y;
    return as_number(a, x) && as_number(b, y);
}

i64 floor_div(i64 a, i64 b)
{
    i64 q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0)))
        q--;
    return q;
}

i64 floor_mod(i64 a, i64 b)
{
    i64 r = a % b;
    if (r != 0 && ((r < 0) != (b < 0)))
        r += b;
    return r;
}

R int_pow(i64 base, i64 exp, Value &out)
{
    i64 acc = 1;
    for (i64 i = 0; i < exp; i++) {
        i64 next = acc * base;
        if (base != 0 && next / base != acc)
            return err_set("OverflowError", "int too large (no bignum yet)");
        acc = next;
    }
    out = int_from_i64(acc);
    return out.is_nil() ? R::Err : R::Ok;
}

R int_binop(i64 a, i64 b, Op op, Value &out)
{
    i64 r = 0;
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
            return err_set("ZeroDivisionError", "division by zero");
        out = float_new(f64(a) / f64(b));
        return out.is_nil() ? R::Err : R::Ok;
    case Op::FloorDiv:
        if (b == 0)
            return err_set("ZeroDivisionError", "integer division or modulo by zero");
        r = floor_div(a, b);
        break;
    case Op::Mod:
        if (b == 0)
            return err_set("ZeroDivisionError", "integer division or modulo by zero");
        r = floor_mod(a, b);
        break;
    case Op::Pow:
        if (b < 0) {
            out = float_new(pow(f64(a), f64(b)));
            return out.is_nil() ? R::Err : R::Ok;
        }
        return int_pow(a, b, out);
    case Op::And:
        r = a & b;
        break;
    case Op::Or:
        r = a | b;
        break;
    case Op::Xor:
        r = a ^ b;
        break;
    case Op::Lsh:
        if (b < 0)
            return err_set("ValueError", "negative shift count");
        if (b > 62)
            return err_set("OverflowError", "int too large (no bignum yet)");
        r = a << b;
        if (b > 0 && (r >> b) != a)
            return err_set("OverflowError", "int too large (no bignum yet)");
        break;
    case Op::Rsh:
        if (b < 0)
            return err_set("ValueError", "negative shift count");
        r = b > 62 ? (a < 0 ? -1 : 0) : (a >> b);
        break;
    }
    out = int_from_i64(r);
    return out.is_nil() ? R::Err : R::Ok;
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
    const Type *t = type_of(v);
    if (!t || !t->hash)
        return err_set2("TypeError", "unhashable type", type_name(v));
    return t->hash(v, out);
}

R py_eq(Value a, Value b, bool &out)
{
    if (both_int(a, b)) {
        i64 x, y;
        as_index(a, x);
        as_index(b, y);
        out = x == y;
        return R::Ok;
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

    if (both_int(a, b)) {
        i64 x, y;
        as_index(a, x);
        as_index(b, y);
        out = op == Cmp::Lt ? x < y : op == Cmp::Le ? x <= y : op == Cmp::Gt ? x > y : x >= y;
        return R::Ok;
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

R py_contains(Value v, Value item, bool &out)
{
    const Type *t = type_of(v);
    if (!t || !t->contains)
        return err_set2("TypeError", "argument of type is not iterable", type_name(v));
    return t->contains(v, item, out);
}

R py_binop(Value a, Value b, Op op, Value &out)
{
    if (both_int(a, b)) {
        i64 x, y;
        as_index(a, x);
        as_index(b, y);
        return int_binop(x, y, op, out);
    }
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

    Buf<96> m;
    m.put("unsupported operand type(s) for ").put(op_symbol(op));
    m.put(": '").put(type_name(a)).put("' and '").put(type_name(b)).put("'");
    return err_set("TypeError", m.str());
}

R py_neg(Value a, Value &out)
{
    i64 n = 0;
    if (as_index(a, n)) {
        out = int_from_i64(-n);
        return out.is_nil() ? R::Err : R::Ok;
    }
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
