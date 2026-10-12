#include <stdio.h>
#include <string.h>
#include <math.h>
double croot(double x);
float crootf(float x);
static void bits(double r)
{
    unsigned long long b;
    memcpy(&b, &r, sizeof b);
    printf("%08x%08x\n", (unsigned)(b >> 32), (unsigned)b);
}
static void fbits(float r)
{
    unsigned b;
    memcpy(&b, &r, sizeof b);
    printf("%08x\n", b);
}
int main(void)
{
    double (*f)(double) = sqrt;
    float (*g)(float)   = sqrtf;
    bits(f(2.0));
    bits(croot(3.0));
    fbits(sqrtf(2.0f));
    fbits(g(0.5f));
    fbits(crootf(3.0f));
    fbits(sqrtf(-0.0f));
    double m = f(-1.0);
    printf("%d\n", m != m);
    return 0;
}

