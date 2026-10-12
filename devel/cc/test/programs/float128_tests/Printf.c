#include <stdio.h>
static long double big = 1e100L;
coro(braam_call *) int main(void)
{
    long double x = -1;
    await printf("%.3Lf %Lg %Lg %d\n", 2.5L, x / 8, big, 7);
    return 0;
}

