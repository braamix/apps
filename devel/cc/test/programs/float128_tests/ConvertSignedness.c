#include <stdio.h>
double d(unsigned long u) { return (double)(long)u; }
long double ld(unsigned long u) { return (long double)(long)u; }
int main(void)
{
    printf("%g %Lg\n", d(-3UL), ld(-3UL));
    return 0;
}

