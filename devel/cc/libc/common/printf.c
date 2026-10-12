/*
 * Formatted output to standard output.
 *
 * Thin wrapper over the target's formatting engine __doprnt, which documents the
 * supported conversions (libc/besm6/doprnt.c on BESM-6).
 */
#include <stdio.h>

extern int __doprnt(const char *fmt, va_list ap, char *buf, int size, int to_buf);

int printf(const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = __doprnt(fmt, ap, 0, 0, 0);
    va_end(ap);
    return n;
}
