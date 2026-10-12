#include <stdio.h>
unsigned g(unsigned x, int k)
{
    unsigned s = 0;
    if (k & 1)
        goto e1;
    goto e2;
e1:
    s += 1;
a:
    s = s * 3 + x;
    if (s > 100000)
        return s;
    if (s & 1)
        goto e1;
e2:
    s += 2;
    goto a;
}
coro(braam_call *) int main(void)
{
    unsigned t = 0;
    for (int k = 0; k < 6; k++)
        t = t * 7 + g(k * 13, k);
    await printf("%u\n", t);
    return 0;
}

