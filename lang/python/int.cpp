// int: a 31-bit small integer in the value word. No bignum yet, so what does
// not fit raises.
#include "kernel/fmt.h"
#include "ops.h"

namespace {

R int_repr(Value v, String &out)
{
    char tmp[24];
    return out.append(int_text(tmp, sizeof tmp, v.as_int()))
               ? R::Ok
               : err_set("MemoryError", "out of memory");
}

bool int_truth(Value v)
{
    return v.as_int() != 0;
}

} // namespace

constexpr Type int_type{ .name = "int", .truth = int_truth, .repr = int_repr };

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
    if (!Value::fits_small(n)) {
        err_set("OverflowError", "int too large (no bignum yet)");
        return Value();
    }
    return Value::of_int(i32(n));
}

bool as_index(Value v, i64 &out)
{
    if (v.is_int()) {
        out = v.as_int();
        return true;
    }
    if (is_bool(v)) {
        out = is_true(v) ? 1 : 0;
        return true;
    }
    return false;
}

bool as_number(Value v, f64 &out)
{
    i64 n = 0;
    if (as_index(v, n)) {
        out = f64(n);
        return true;
    }
    if (is_float(v)) {
        out = float_of(v);
        return true;
    }
    return false;
}
