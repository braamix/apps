// strspn — the length of the prefix of s made of bytes in accept (C11 §7.24.5.6).
#include <string.h>

size_t strspn(const char *s, const char *accept)
{
    size_t k = 0;
    while (s[k] && strchr(accept, s[k]))
        k++;
    return k;
}
