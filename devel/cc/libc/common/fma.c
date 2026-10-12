/*
 * fma — floating multiply-add, x * y + z (C11 §7.12.13.1).
 *
 * A true fma rounds the product and sum only once; this is an ordinary
 * x*y + z with two roundings.  It serves programs that call fma for the
 * operation rather than the extra accuracy.
 */

#include <math.h>

double fma(double x, double y, double z)
{
    return x * y + z;
}
