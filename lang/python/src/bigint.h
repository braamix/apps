// Integers past the value word: sign and magnitude over 32-bit limbs.
//
// A `Value` with bit 0 set is still a 31-bit int and always will be -- that is
// the fast path and most integers never leave it. What does not fit becomes a
// BigObj whose `type` is `int_type`, so `type(x)` is `int` either way and the
// difference is invisible from Python.
//
// The invariant every operation keeps: **a BigObj never holds a value that
// would fit in a small int.** Everything that builds one ends at big_make,
// which hands back a small Value when it can, so `a == b` on two ints is never
// true of one small and one big, and a BigObj is never zero.
//
// Limbs are little-endian base 2^32, the top one non-zero, with the sign
// beside them rather than in them. Two's complement is what Python's bitwise
// operators mean, and the conversion happens inside those alone.
#pragma once

#include "obj.h"

struct BigObj : Obj {
    u32 len; // limbs in use, at least one, the last non-zero
    bool neg;

    u32 *limbs() { return reinterpret_cast<u32 *>(this + 1); }

    const u32 *limbs() const { return reinterpret_cast<const u32 *>(this + 1); }
};

inline bool is_big(Value v)
{
    return v.is_obj() && v.obj()->type == &int_type;
}

inline BigObj *big_of(Value v)
{
    return static_cast<BigObj *>(v.obj());
}

// int or bool: everything the integer arm of the number tower answers for.
inline bool is_intval(Value v)
{
    return v.is_int() || is_big(v) || is_bool(v);
}

// A big from a magnitude, normalised: leading zero limbs dropped, and a small
// Value returned where the result fits in one. Nil with the error pending.
Value big_make(const u32 *limbs, usize len, bool neg);

// Any integer as a big, so one code path can take both. Nil on failure.
Value big_of_value(Value v);

Value big_from_i64(i64 n);

// False when the value is past i64, in which case `out` is untouched.
bool int_to_i64(Value v, i64 &out);

// Nearest f64, inf past the range.
f64 int_to_f64(Value v);

// Truncated toward zero. The caller has already refused nan and inf.
Value int_from_f64(f64 x);

// The whole integer arm: both sides are ints, the result is exact, and an
// overflow promotes rather than raising.
R int_arith(Value a, Value b, Op op, Value &out);

R int_compare(Value a, Value b, Cmp op, bool &out);

// int against float, exactly -- a float is converted to an integer and a
// fraction rather than the integer being rounded to a float.
R intfloat_compare(Value i, f64 y, bool flip, Cmp op, bool &out);

R int_negate(Value v, Value &out);
R int_invert_op(Value v, Value &out);
R int_absolute(Value v, Value &out);

// sys.hash_info.modulus: every number hashes as its value modulo this prime.
constexpr u64 HASH_MODULUS = 0x7fffffff;

u32 int_hash_of(Value v);

// int.bit_length and int.bit_count, both over the magnitude.
usize int_bits(Value v);
usize int_ones(Value v);

bool int_truth_of(Value v);

// The magnitude in `base`, without a sign. `upper` picks the digit case.
R int_digits(Value v, u32 base, bool upper, String &out);

// sys.set_int_max_str_digits: how many decimal digits int() and str() will
// convert, 0 for no limit. int_digits obeys it, and int_parse when `limited`.
u32 int_max_str_digits();
void int_set_max_str_digits(u32 n);

inline bool int_is_neg(Value v)
{
    return v.is_int() ? v.as_int() < 0 : is_big(v) && big_of(v)->neg;
}

// A literal or int(s, base). `base` of 0 reads the 0x/0o/0b prefix. Nil with
// a ValueError pending on anything that is not wholly a number.
Value int_parse(Str s, u32 base, bool limited = false);

// a to the b, both non-negative ints. `mod` is Nil for the two-argument form.
R int_power(Value a, Value b, Value mod, Value &out);

// a / b as a float, rounded the way one division would be: the quotient's
// leading bits are taken exactly and rounded once.
R int_truediv(Value a, Value b, Value &out);

// int.to_bytes and int.from_bytes over any width.
R int_to_octets(Value v, usize n, bool little, bool sign, String &out);
Value int_from_octets(Str s, bool little, bool sign);

// The exact numerator and denominator of a float, which is what
// float.as_integer_ratio and Fraction want.
R float_ratio(f64 x, Value &num, Value &den);
