// <math.h> for the SDK's musl, which libc.a carries: Braam's <math.h>, then what
// the SDK's musl_prologue.h gives clang, in what this compiler has. musl's
// sources reach this directory first through -I.
#ifndef _MUSL_MATH_H
#define _MUSL_MATH_H

#include <float.h>
#include <stdint.h>

// musl's src/internal, less hidden, which the data headers use before they
// include anything and so comes as -Dhidden=. No aliases: the names they would
// add are not in <math.h>.
#define weak_alias(old, new) extern int __no_alias_##new

#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN    4321
#define __BYTE_ORDER    __LITTLE_ENDIAN

// sincos.c and sincosf.c name their parameters sin and cos, and a parameter may
// not shadow a function here: they are compiled with MUSL_SINCOS, which leaves
// the two undeclared.
#ifdef MUSL_SINCOS
#define sin __musl_sin
#define cos __musl_cos
#endif
#include "../include/math.h"
#ifdef MUSL_SINCOS
#undef sin
#undef cos
#endif

// The Bessel functions jn.c and jnf.c call, which Braam's <math.h> leaves out:
// declared only for those two, compiled with MUSL_JN, as elsewhere y0 and y1
// are local names.
#ifdef MUSL_JN
double j0(double);
double j1(double);
double y0(double);
double y1(double);
float j0f(float);
float j1f(float);
float y0f(float);
float y1f(float);
#endif

// cvt.h spells it its own way, after <limits.h>.
#undef ULONG_MAX

// long double is 113-bit quad here. Double-shaped puts libm.h and cvt/ on their
// LDBL_MANT_DIG == 53 paths, as the SDK does.
#undef LDBL_MANT_DIG
#undef LDBL_MIN_EXP
#undef LDBL_MAX_EXP
#undef LDBL_MIN_10_EXP
#undef LDBL_MAX_10_EXP
#undef LDBL_EPSILON
#undef LDBL_MIN
#undef LDBL_MAX
#define LDBL_MANT_DIG   DBL_MANT_DIG
#define LDBL_MIN_EXP    DBL_MIN_EXP
#define LDBL_MAX_EXP    DBL_MAX_EXP
#define LDBL_MIN_10_EXP DBL_MIN_10_EXP
#define LDBL_MAX_10_EXP DBL_MAX_10_EXP
#define LDBL_EPSILON    DBL_EPSILON
#define LDBL_MIN        DBL_MIN
#define LDBL_MAX        DBL_MAX

// wasm has no floating-point environment. Defined so the #ifdefs pass, zero so
// the tests fail.
#define FE_TONEAREST  0
#define FE_DOWNWARD   0
#define FE_UPWARD     0
#define FE_TOWARDZERO 0
#define FE_INEXACT    0
#define FE_INVALID    0
#define FE_DIVBYZERO  0
#define FE_UNDERFLOW  0
#define FE_OVERFLOW   0
#define FE_ALL_EXCEPT 0

#define feclearexcept(e) ((void)(e), 0)
#define feraiseexcept(e) ((void)(e), 0)
#define fetestexcept(e)  ((void)(e), 0)
#define fegetround()     FE_TONEAREST
#define fesetround(r)    ((void)(r), 0)

// fma.c reached this through atomic.h; i64.clz, in math.s.
#define a_clz_64(x) __clz64(x)
int __clz64(uint64_t);

#endif // _MUSL_MATH_H
