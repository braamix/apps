#include <stdio.h>
double d(unsigned long u) { return (double)(long)u; }
long double ld(unsigned long u) { return (long double)(long)u; }
coro(braam_call *) int main(void)
{
    await printf("%g %Lg\n", d(-3UL), ld(-3UL));
    return 0;
}

