//
// IEEE-754 binary128 in software, for long double constants on any host: parsing,
// arithmetic rounded to nearest even, comparison, conversion and formatting.  The
// arithmetic and conversions are also the RISC-V runtime (libc/common/float128.c
// includes float128.c with F128_RUNTIME defined), so the compiler folds exactly as
// the target computes.
//
#ifndef LIBUTIL_FLOAT128_H
#define LIBUTIL_FLOAT128_H

#include <stdint.h>

#ifndef F128_API
#define F128_API
#endif

typedef struct {
    uint64_t lo, hi;
} Float128;

F128_API Float128 f128_add(Float128 x, Float128 y);
F128_API Float128 f128_sub(Float128 x, Float128 y);
F128_API Float128 f128_mul(Float128 x, Float128 y);
F128_API Float128 f128_div(Float128 x, Float128 y);
F128_API Float128 f128_neg(Float128 x);
// -1, 0 or 1 as x is less than, equal to or greater than y; 2 when unordered.
F128_API int f128_cmp(Float128 x, Float128 y);
F128_API int f128_is_zero(Float128 x);
F128_API int f128_is_nan(Float128 x);

F128_API Float128 f128_from_i64(int64_t v);
F128_API Float128 f128_from_u64(uint64_t v);
// Truncated toward zero; out of range saturates (a negative one to 0 when unsigned).
F128_API int64_t f128_to_i64(Float128 x, int bits);
F128_API uint64_t f128_to_u64(Float128 x, int bits);
// From and to the bits of a binary64 or binary32.
F128_API Float128 f128_from_double_bits(uint64_t bits);
F128_API uint64_t f128_to_double_bits(Float128 x);
F128_API Float128 f128_from_float_bits(uint32_t bits);
F128_API uint32_t f128_to_float_bits(Float128 x);

#ifndef F128_RUNTIME
Float128 f128_from_double(double d);
double f128_to_double(Float128 x);
float f128_to_float(Float128 x);

// The value of a C floating constant (decimal or hexadecimal, a suffix ignored),
// rounded to nearest even.  *end, when not NULL, is set past the number.
Float128 f128_from_string(const char *s, const char **end);

// x as a C hexadecimal floating constant ("0x1.8p+0", "-0x0.0000001p-16382"),
// or "inf"/"-inf"/"nan"; exact.  `buf` holds at least F128_BUFSIZE bytes.
#define F128_BUFSIZE 48
char *f128_format(Float128 x, char *buf);

// x rounded to nearest even to a significand of `mant_dig` bits (2..113), the
// exponent range unchanged: a narrower long double carried in binary128.  64 gives
// the x87 extended format's values, subnormals on its coarser grid included.
Float128 f128_round(Float128 x, int mant_dig);

// To and from the 10 bytes of an x87 extended-precision value, little-endian: the
// 64-bit significand with its explicit integer bit, then sign and 15-bit exponent.
// To rounds as f128_round(x, 64); from is exact, an unnormal reading as a NaN.
void f128_to_x87(Float128 x, uint8_t out[10]);
Float128 f128_from_x87(const uint8_t in[10]);
#endif

#endif // LIBUTIL_FLOAT128_H
