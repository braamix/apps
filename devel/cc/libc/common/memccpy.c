// memccpy — copy up to n bytes, stopping after the first c; a pointer past that
// c in dest, or NULL when it was not met (POSIX).
#include <string.h>

void *memccpy(void *dest, const void *src, int c, size_t n)
{
    unsigned char *d       = dest;
    const unsigned char *s = src;
    for (size_t k = 0; k < n; k++) {
        d[k] = s[k];
        if (s[k] == (unsigned char)c)
            return d + k + 1;
    }
    return NULL;
}
