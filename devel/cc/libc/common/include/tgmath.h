/*
 * <tgmath.h> — type-generic math (C11 §7.25).
 *
 * The <math.h> functions are double-typed only and there is no <complex.h>, so
 * type-generic dispatch is degenerate: the macros below promote the argument(s)
 * to double and call the one function.  Integer arguments promote to double
 * exactly as the standard requires.
 *
 * The function names below are self-referential macros; per the C preprocessor's
 * no-recursive-expansion rule the inner name resolves to the real <math.h>
 * function, not back to the macro.
 */
#ifndef _TGMATH_H
#define _TGMATH_H

#include <math.h>

/* unary real functions */
#define fabs(x)  fabs((double)(x))
#define floor(x) floor((double)(x))
#define ceil(x)  ceil((double)(x))
#define round(x) round((double)(x))
#define trunc(x) trunc((double)(x))
#define sqrt(x)  sqrt((double)(x))
#define exp(x)   exp((double)(x))
#define log(x)   log((double)(x))
#define log10(x) log10((double)(x))
#define sin(x)   sin((double)(x))
#define cos(x)   cos((double)(x))
#define tan(x)   tan((double)(x))
#define asin(x)  asin((double)(x))
#define acos(x)  acos((double)(x))
#define atan(x)  atan((double)(x))
#define sinh(x)  sinh((double)(x))
#define cosh(x)  cosh((double)(x))
#define tanh(x)  tanh((double)(x))

/* binary real functions */
#define pow(x, y)      pow((double)(x), (double)(y))
#define atan2(y, x)    atan2((double)(y), (double)(x))
#define fmod(x, y)     fmod((double)(x), (double)(y))
#define hypot(x, y)    hypot((double)(x), (double)(y))
#define fmin(x, y)     fmin((double)(x), (double)(y))
#define fmax(x, y)     fmax((double)(x), (double)(y))
#define copysign(x, y) copysign((double)(x), (double)(y))

/* mixed-argument functions: only the real argument is type-generic */
#define frexp(x, ep) frexp((double)(x), (ep))
#define ldexp(x, n)  ldexp((double)(x), (n))
#define modf(x, ip)  modf((double)(x), (ip))

#endif /* _TGMATH_H */
