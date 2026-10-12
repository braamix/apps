//
// Bit counts of an unsigned word: GCC's and clang's builtins where the compiler has them,
// else plain C, which is what lets vcc compile itself.
//
#ifndef BITOPS_H
#define BITOPS_H

#include <stdint.h>

// The number of one bits.
static inline int popcount32(uint32_t x)
{
#ifdef __GNUC__
    return __builtin_popcount(x);
#else
    int n = 0;
    for (; x; x &= x - 1)
        n++;
    return n;
#endif
}

static inline int popcount64(uint64_t x)
{
#ifdef __GNUC__
    return __builtin_popcountll(x);
#else
    int n = 0;
    for (; x; x &= x - 1)
        n++;
    return n;
#endif
}

// The number of zero bits below the lowest one; x is not 0.
static inline int ctz32(uint32_t x)
{
#ifdef __GNUC__
    return __builtin_ctz(x);
#else
    int n = 0;
    for (; !(x & 1); x >>= 1)
        n++;
    return n;
#endif
}

// The number of zero bits above the highest one; x is not 0.
static inline int clz32(uint32_t x)
{
#ifdef __GNUC__
    return __builtin_clz(x);
#else
    int n = 0;
    for (; !(x & 0x80000000u); x <<= 1)
        n++;
    return n;
#endif
}

#endif // BITOPS_H
