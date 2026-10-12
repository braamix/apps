#include <stdio.h>
#include <string.h>
#include <math.h>
double croot(double x);
float crootf(float x);
static coro(braam_call *) void bits(double r)
{
    unsigned long long b;
    memcpy(&b, &r, sizeof b);
    await printf("%08x%08x\n", (unsigned)(b >> 32), (unsigned)b);
}
static coro(braam_call *) void fbits(float r)
{
    unsigned b;
    memcpy(&b, &r, sizeof b);
    await printf("%08x\n", b);
}
coro(braam_call *) int main(void)
{
    double (*f)(double) = sqrt;
    float (*g)(float)   = sqrtf;
    await bits(f(2.0));
    await bits(croot(3.0));
    await fbits(sqrtf(2.0f));
    await fbits(g(0.5f));
    await fbits(crootf(3.0f));
    await fbits(sqrtf(-0.0f));
    double m = f(-1.0);
    await printf("%d\n", m != m);
    return 0;
}

