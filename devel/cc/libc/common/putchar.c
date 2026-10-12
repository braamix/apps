/*
 * putchar — write the byte c to stdout, return it (C11 §7.21.7.3).
 *
 * Built on the target's putbyte.  Returns the character written (as an unsigned char cast to int).
 */
#include <stdio.h>

int putchar(int c)
{
    putbyte(c);
    return c & 0377;
}
