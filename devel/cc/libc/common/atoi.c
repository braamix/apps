/*
 * atoi — convert the initial portion of a string to int (C11 §7.22.1.2).
 *
 * Equivalent to (int)strtol(nptr, NULL, 10) except for error behaviour: skip
 * leading white space, take an optional '+'/'-' sign, then accumulate decimal
 * digits.
 */
#include <stdlib.h>

int atoi(const char *nptr)
{
    const char *p = nptr;
    int neg       = 0;
    int n         = 0;

    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\v' || *p == '\f') {
        p++;
    }
    if (*p == '-') {
        neg = 1;
        p++;
    } else if (*p == '+') {
        p++;
    }
    while (*p >= '0' && *p <= '9') {
        n = n * 10 + (*p - '0');
        p++;
    }
    return neg ? -n : n;
}
