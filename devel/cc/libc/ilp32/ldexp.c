/*
 * ldexp — multiply a double by a power of two (C11 §7.12.6.6): at most three
 * multiplications by powers built from their IEEE-754 bits, for a target with 32-bit
 * long, where a double is two words, the low one first.
 */
#include <math.h>

static double pow2(int n)
{
    double d;
    unsigned long *w = (unsigned long *)&d;
    w[0]             = 0;
    w[1]             = (unsigned long)(n + 1023) << 20;
    return d;
}

double ldexp(double x, int n)
{
    if (n > 1023) {
        x *= pow2(1023);
        n -= 1023;
        if (n > 1023) {
            x *= pow2(1023);
            n -= 1023;
            if (n > 1023)
                n = 1023;
        }
    } else if (n < -1022) {
        /* 2^-969 = 2^-1022 * 2^53: stays normal, loses no bits */
        x *= pow2(-969);
        n += 969;
        if (n < -1022) {
            x *= pow2(-969);
            n += 969;
            if (n < -1022)
                n = -1022;
        }
    }
    return x * pow2(n);
}
