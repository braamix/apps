// int: a 31-bit small integer in the value word, and a BigObj past that.
// bigint.cpp has the magnitude arithmetic; this is the type they share.
#include "bigint.h"
#include "kernel/fmt.h"
#include "ops.h"

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
constexpr Type int_type{ .name  = "int",
                         .truth = int_truth_of,
                         .hash  = int_hash,
                         .eq    = int_eq,
                         .order = int_order,
                         .repr  = int_repr,
                         .binop = int_binop_slot,
                         .patma = PATMA_SELF };

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
