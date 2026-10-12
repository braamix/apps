/*
 * strtod and strtof over musl's scanner, __floatscan in the SDK's libbraam_math.a
 * (src/math/cvt/), which rounds correctly at either precision.  ERANGE is set for a
 * result that overflows or underflows to zero; a subnormal one does not set it.
 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* src/math/cvt/api.h: a cursor over a bounded string, and the scanner. */
typedef struct {
    const unsigned char *p, *start, *end;
    int err; /* 0, 34 (ERANGE) or 22 (EINVAL) */
} Cur;

double __floatscan(Cur *c, int prec, int pok);

/* prec 0 is float's, 1 double's. */
static double scan(const char *s, char **end, int prec)
{
    const unsigned char *p = (const unsigned char *)s;
    size_t len             = strlen(s);
    Cur c                  = { p, p, p + len, 0 };
    double v               = __floatscan(&c, prec, 1);
    size_t used            = (size_t)(c.p - c.start);
    if (used > len) /* a lookahead past the end, ungot */
        used = len;
    if (c.err == 22 || used == 0) {
        used = 0;
        v    = 0;
    } else if (c.err == 34) {
        errno = ERANGE;
    }
    if (end)
        *end = (char *)s + used;
    return v;
}

double strtod(const char *s, char **end)
{
    return scan(s, end, 1);
}

float strtof(const char *s, char **end)
{
    return (float)scan(s, end, 0);
}
