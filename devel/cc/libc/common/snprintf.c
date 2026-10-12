/*
 * Formatted output into a bounded caller buffer.
 *
 * Writes at most size-1 characters plus a terminating NUL into buf, and returns
 * the number of characters that would have been written had the buffer been
 * large enough.  Shares the engine __doprnt with printf (the target's
 * doprnt.c); the variadic arguments are reached via <stdarg.h>.
 */
#include <stdio.h>

extern int __doprnt(const char *fmt, va_list ap, char *buf, int size, int to_buf);

int snprintf(char *buf, int size, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = __doprnt(fmt, ap, buf, size, 1);
    va_end(ap);
    return n;
}
