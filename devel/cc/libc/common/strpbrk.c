// strpbrk — the first byte of s that is in accept, or NULL (C11 §7.24.5.4).
#include <string.h>

char *strpbrk(const char *s, const char *accept)
{
    s += strcspn(s, accept);
    return *s ? (char *)s : NULL;
}
