/*
 * strtok_r — strtok with its position in *save rather than in a static (POSIX).
 */
#include <string.h>

char *strtok_r(char *str, const char *delim, char **save)
{
    char *p = str ? str : *save;
    if (!p)
        return NULL;
    p += strspn(p, delim);
    if (!*p) {
        *save = NULL;
        return NULL;
    }
    char *tok = p;
    p += strcspn(p, delim);
    if (*p)
        *p++ = 0;
    else
        p = NULL;
    *save = p;
    return tok;
}
