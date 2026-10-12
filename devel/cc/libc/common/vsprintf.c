/*
 * vsprintf — sprintf with its arguments in a va_list (C11 §7.21.6.13).
 */
#include <limits.h>
#include <stdio.h>

#if INT_MAX < 0x1000000
#define UNBOUNDED INT_MAX
#else
#define UNBOUNDED (1 << 24)
#endif

extern int __doprnt(const char *fmt, va_list ap, char *buf, int size, int to_buf);

int vsprintf(char *buf, const char *fmt, va_list ap)
{
    return __doprnt(fmt, ap, buf, UNBOUNDED, 1);
}
