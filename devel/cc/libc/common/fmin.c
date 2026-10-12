/*
 * fmin — the smaller of x and y (C11 §7.12.12.3).
 *
 * A plain FP comparison; as with fmax, the NaN rule is not implemented.
 */

#include <math.h>

double fmin(double x, double y)
{
    return x < y ? x : y;
}
