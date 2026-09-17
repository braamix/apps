// int: a 31-bit small integer in the value word, and a BigObj past that.
// bigint.cpp has the magnitude arithmetic; this is the type they share.
#include "bigint.h"
#include "kernel/fmt.h"
#include "ops.h"
#include "type.h"

namespace {

R int_repr(Value v, String &out)
{
    if (v.is_int()) {
        char tmp[24];
        return out.append(int_text(tmp, sizeof tmp, v.as_int()))
                   ? R::Ok
                   : err_set("MemoryError", "out of memory");
    }
    if (int_is_neg(v) && !out.push('-'))
        return err_set("MemoryError", "out of memory");
    return int_digits(v, 10, false, out);
}

R int_hash(Value v, u32 &out)
{
    out = int_hash_of(v);
    return R::Ok;
}

R int_eq(Value a, Value b, bool &out)
{
    if (!is_intval(b))
        return R::NotImpl;
    return int_compare(a, b, Cmp::Eq, out);
}

R int_order(Value a, Value b, Cmp op, bool &out)
{
    if (!is_intval(b))
        return R::NotImpl;
    return int_compare(a, b, op, out);
}

R int_binop_slot(Value a, Value b, Op op, Value &out)
{
    if (!is_intval(a) || !is_intval(b))
        return R::NotImpl;
    return int_arith(a, b, op, out);
}

} // namespace

// A big int's type is this one, so `type(2**99)` is `int` and nothing outside
// bigint.cpp has to know which shape a value has.
// An int is its own real part and numerator, as numbers.Integral says.
R int_getattr(Value v, StrObj *name, Value &out)
{
    Str n = name->str();
    if (n == "real" || n == "numerator")
        out = is_bool(v) ? Value::of_int(is_true(v) ? 1 : 0) : v;
    else if (n == "imag")
        out = Value::of_int(0);
    else if (n == "denominator")
        out = Value::of_int(1);
    else
        return R::NotImpl;
    return R::Ok;
}

constexpr Type int_type{ .name    = "int",
                         .truth   = int_truth_of,
                         .hash    = int_hash,
                         .eq      = int_eq,
                         .order   = int_order,
                         .repr    = int_repr,
                         .binop   = int_binop_slot,
                         .getattr = int_getattr,
                         .patma   = PATMA_SELF };

Str addr_text(char *out, usize cap, const Obj *o)
{
    usize v = usize(o);
    char tmp[16];
    usize n = 0;
    do {
        tmp[n++] = "0123456789abcdef"[v & 0xf];
        v >>= 4;
    } while (v && n < sizeof tmp);
    usize at = 0;
    if (cap < n + 3)
        return Str();
    out[at++] = '0';
    out[at++] = 'x';
    while (n)
        out[at++] = tmp[--n];
    return Str(out, at);
}

Str int_text(char *out, usize cap, i64 v)
{
    char digits[24];
    usize n    = 0;
    bool minus = v < 0;
    u64 m      = minus ? u64(-(v + 1)) + 1 : u64(v);
    do {
        digits[n++] = char('0' + m % 10);
        m /= 10;
    } while (m);

    usize k = 0;
    if (minus && k < cap)
        out[k++] = '-';
    while (n && k < cap)
        out[k++] = digits[--n];
    return Str(out, k);
}

Value int_from_i64(i64 n)
{
    return big_from_i64(n);
}

// True only where the value fits an i64: a wider integer is an int all the
// same, and int_to_i64 is what asks the question without losing it.
bool as_index(Value v, i64 &out)
{
    return int_to_i64(v, out);
}

bool as_int_arg(Value v, i64 &out)
{
    if (int_to_i64(v, out))
        return true;
    // An instance of a subclass of int -- an IntEnum member -- is an int.
    if (is_inst(v)) {
        Value n = inst_of(v)->native;
        return !n.is_nil() && (n.is_int() || is_big(n)) && int_to_i64(n, out);
    }
    return false;
}

bool int_too_wide(Value v, f64 x)
{
    if (!is_big(v) || (x != __builtin_inf() && x != -__builtin_inf()))
        return false;
    err_set("OverflowError", "int too large to convert to float");
    return true;
}

bool as_number(Value v, f64 &out)
{
    i64 n = 0;
    if (as_index(v, n)) {
        out = f64(n);
        return true;
    }
    if (is_big(v)) {
        out = int_to_f64(v);
        return true;
    }
    if (is_float(v)) {
        out = float_of(v);
        return true;
    }
    return false;
}
