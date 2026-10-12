// strdup — a copy of s in memory from malloc (POSIX), or NULL.
#include <stdlib.h>
#include <string.h>

char *strdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p  = malloc(n);
    return p ? memcpy(p, s, n) : NULL;
}
