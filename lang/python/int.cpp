// int: a 31-bit small integer in the value word. No bignum yet, so what does
// not fit raises.
#include "kernel/fmt.h"
#include "ops.h"

namespace {

R int_repr(Value v, String &out)
{
    Buf<16> b;
    i64 n = v.as_int();
    if (n < 0) {
        b.put('-');
        n = -n;
    }
    b.put(u64(n));
    return out.append(b.str()) ? R::Ok : err_set("MemoryError", "out of memory");
}

bool int_truth(Value v)
{
    return v.as_int() != 0;
}

} // namespace

constexpr Type int_type{ .name = "int", .truth = int_truth, .repr = int_repr };

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
