/*
 * modf — split a double into integer and fractional parts (C11 §7.12.6.12).  The
 * integer part clears the fraction bits of the IEEE-754 representation, a double
 * being two 32-bit words, the low one first: no integer type is wide enough to
 * convert through on this target.
 */
#include <math.h>

double modf(double x, double *iptr)
{
    double i         = x, frac;
    unsigned long *w = (unsigned long *)&i;
    int e            = (int)((w[1] >> 20) & 0x7ff) - 1023; /* unbiased exponent */

    if (x != x) { /* NaN */
        *iptr = x;
        return x;
    }
    if (e >= 52) {
        *iptr = x; /* includes infinities */
        frac  = 0.0;
    } else {
        if (e < 0) {
            w[1] &= 0x80000000UL;
            w[0] = 0;
        } else if (e < 20) {
            w[1] &= ~(0x000fffffUL >> e);
            w[0] = 0;
        } else {
            w[0] &= ~(0xffffffffUL >> (e - 20));
        }
        *iptr = i;
        frac  = x - i;
    }
    if (frac == 0 && x < 0)
        frac = -0.0;
    return frac;
}
