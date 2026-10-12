/*
 * strcspn — the length of the prefix of s with no byte of reject (C11 §7.24.5.3).
 */
#include <string.h>

size_t strcspn(const char *s, const char *reject)
{
    size_t k = 0;
    while (s[k] && !strchr(reject, s[k]))
        k++;
    return k;
}
