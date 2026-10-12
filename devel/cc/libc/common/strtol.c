// strtol, strtoul, strtoll, strtoull (C11 §7.22.1.4): one scan of the digits
// into an unsigned long long, which notes overflow; each function then clamps
// to its own range and sets ERANGE.
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

// The magnitude of the number at s, with its sign in *neg; *end past the last
// digit taken, or s when there is none.  *over when it did not fit.
static unsigned long long scan(const char *s, char **end, int base, int *neg, int *over)
{
    const char *p = s;
    *neg = *over = 0;
    while (*p == ' ' || (*p >= '\t' && *p <= '\r'))
        p++;
    if (*p == '+' || *p == '-')
        *neg = *p++ == '-';
    if ((base == 0 || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        int d = p[2];
        if ((d >= '0' && d <= '9') || (d >= 'a' && d <= 'f') || (d >= 'A' && d <= 'F')) {
            p += 2;
            base = 16;
        }
    }
    if (base == 0)
        base = *p == '0' ? 8 : 10;
    unsigned long long v = 0;
    const char *first    = p;
    for (;; p++) {
        int d = *p >= '0' && *p <= '9'   ? *p - '0'
                : *p >= 'a' && *p <= 'z' ? *p - 'a' + 10
                : *p >= 'A' && *p <= 'Z' ? *p - 'A' + 10
                                         : 99;
        if (d >= base)
            break;
        if (v > (ULLONG_MAX - d) / base)
            *over = 1;
        else
            v = v * base + d;
    }
    if (end)
        *end = (char *)(p == first ? s : p);
    return v;
}

long long strtoll(const char *s, char **end, int base)
{
    int neg, over;
    unsigned long long v   = scan(s, end, base, &neg, &over);
    unsigned long long lim = neg ? (unsigned long long)LLONG_MAX + 1 : LLONG_MAX;
    if (over || v > lim) {
        errno = ERANGE;
        return neg ? LLONG_MIN : LLONG_MAX;
    }
    return neg ? (long long)(0 - v) : (long long)v;
}

unsigned long long strtoull(const char *s, char **end, int base)
{
    int neg, over;
    unsigned long long v = scan(s, end, base, &neg, &over);
    if (over) {
        errno = ERANGE;
        return ULLONG_MAX;
    }
    return neg ? 0 - v : v;
}

long strtol(const char *s, char **end, int base)
{
    int neg, over;
    unsigned long long v   = scan(s, end, base, &neg, &over);
    unsigned long long lim = neg ? (unsigned long long)LONG_MAX + 1 : LONG_MAX;
    if (over || v > lim) {
        errno = ERANGE;
        return neg ? LONG_MIN : LONG_MAX;
    }
    return neg ? (long)(0 - v) : (long)v;
}

unsigned long strtoul(const char *s, char **end, int base)
{
    int neg, over;
    unsigned long long v = scan(s, end, base, &neg, &over);
    if (over || v > ULONG_MAX) {
        errno = ERANGE;
        return ULONG_MAX;
    }
    return neg ? 0 - (unsigned long)v : (unsigned long)v;
}
