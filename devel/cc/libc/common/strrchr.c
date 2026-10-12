/*
 * strrchr — locate the last occurrence of c in s (C11 §7.24.5.5).
 *
 * c is converted to char.  The terminating '\0' is part of the string, so
 * strrchr(s, 0) returns a pointer to it.  Returns NULL if c is not found.
 * Scans forward, remembering the last hit.
 */
#include <string.h>

char *strrchr(const char *s, int c)
{
    char ch          = (char)c;
    const char *p    = s;
    const char *last = 0;
    for (;;) {
        if (*p == ch) {
            last = p;
        }
        if (*p == 0) {
            return (char *)last;
        }
        p++;
    }
}
