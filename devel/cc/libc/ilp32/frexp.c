/*
 * frexp — break a double into a fraction in [0.5, 1) and a power of two (C11
 * §7.12.6.4), on the IEEE-754 bits: a target with 32-bit long, where a double is
 * two words, the low one first.
 */
#include <math.h>

double frexp(double x, int *e)
{
    unsigned long *w = (unsigned long *)&x;
    int be           = (int)(w[1] >> 20) & 0x7ff;

    *e = 0;
    if (be == 0x7ff || x == 0)
        return x;
    if (be == 0) {
        /* subnormal: scale by 2^64 to make it normal */
        x = frexp(x * 18446744073709551616.0, e);
        *e -= 64;
        return x;
    }
    *e   = be - 1022;
    w[1] = (w[1] & 0x800fffffUL) | 0x3fe00000UL;
    return x;
}
