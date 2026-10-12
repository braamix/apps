/*
 * puts — write the string s followed by a newline to stdout (C11 §7.21.7.9).
 *
 * Built on the target's putbyte; on BESM-6 the trailing newline also flushes the
 * line.
 * Returns a non-negative value on success.
 */
#include <stdio.h>

int puts(const char *s)
{
    while (*s != 0) {
        putbyte(*s);
        s++;
    }
    putbyte('\n');
    return 0;
}
