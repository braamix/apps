/*
 * fabs — absolute value of x (C11 §7.12.7.2).
 *
 * A plain sign test and FP negate: NaN and -0.0 get no special handling.
 */

#include <math.h>

double fabs(double x)
{
    return x < 0 ? -x : x;
}
