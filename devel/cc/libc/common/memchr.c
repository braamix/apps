/*
 * memchr — locate the byte c within the first n bytes of s (C11 §7.24.5.1).
 *
 * c is converted to unsigned char.  Returns a pointer to the first matching
 * byte, or NULL if c does not occur.
 */
#include <string.h>

void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = s;
    unsigned char ch       = (unsigned char)c;
    while (n > 0) {
        if (*p == ch) {
            return (void *)p;
        }
        p++;
        n--;
    }
    return 0;
}
