/*
 * Formatted output into an unbounded caller buffer.
 *
 * Equivalent to snprintf with no size limit: writes the formatted string plus a
 * terminating NUL into buf and returns its length.  Shares the engine __doprnt
 * with printf (the target's doprnt.c).  A large nominal size stands in for
 * "unbounded": 2^24, or INT_MAX where int is narrower.
 */
#include <limits.h>
#include <stdio.h>

#if INT_MAX < 0x1000000
#define UNBOUNDED INT_MAX
#else
#define UNBOUNDED (1 << 24)
#endif

extern int __doprnt(const char *fmt, va_list ap, char *buf, int size, int to_buf);

int sprintf(char *buf, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = __doprnt(fmt, ap, buf, UNBOUNDED, 1);
    va_end(ap);
    return n;
}
