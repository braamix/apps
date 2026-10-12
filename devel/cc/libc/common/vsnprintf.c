/*
 * vsnprintf — snprintf with its arguments in a va_list (C11 §7.21.6.12).
 */
#include <stdio.h>

extern int __doprnt(const char *fmt, va_list ap, char *buf, int size, int to_buf);

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
    return __doprnt(fmt, ap, buf, (int)size, 1);
}
