/*
 * strnlen — the length of s, but no more than n (POSIX).
 */
#include <string.h>

size_t strnlen(const char *s, size_t n)
{
    size_t k = 0;
    while (k < n && s[k])
        k++;
    return k;
}
