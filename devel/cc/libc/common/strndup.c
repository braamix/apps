// strndup — a copy of at most n bytes of s, terminated, from malloc (POSIX).
#include <stdlib.h>
#include <string.h>

char *strndup(const char *s, size_t n)
{
    size_t len = strnlen(s, n);
    char *p    = malloc(len + 1);
    if (!p)
        return NULL;
    memcpy(p, s, len);
    p[len] = 0;
    return p;
}
