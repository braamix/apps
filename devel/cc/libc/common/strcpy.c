/*
 * strcpy — copy the NUL-terminated string src into dest (C11 §7.24.2.3).
 *
 * Copies up to and including the terminating '\0'.  The objects must not
 * overlap.
 */
#include <string.h>

char *strcpy(char *dest, const char *src)
{
    char *d       = dest;
    const char *s = src;
    while (*s != 0) {
        *d = *s;
        d++;
        s++;
    }
    *d = 0;
    return dest;
}
