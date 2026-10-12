/*
 * fmax — the larger of x and y (C11 §7.12.12.2).
 *
 * A plain FP comparison: C's rule that fmax returns the non-NaN argument when
 * exactly one is NaN is not implemented (BESM-6 has no NaN).
 */

#include <math.h>

double fmax(double x, double y)
{
    return x < y ? y : x;
}
