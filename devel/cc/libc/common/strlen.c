/*
 * strlen — length of the NUL-terminated string s (C11 §7.24.6.3).
 *
 * Counts bytes up to, but not including, the terminating '\0'.
 */
#include <string.h>

size_t strlen(const char *s)
{
    const char *p = s;
    while (*p != 0) {
        p++;
    }
    return (size_t)(p - s);
}
