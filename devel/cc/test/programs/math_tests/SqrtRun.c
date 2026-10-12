#include <stdio.h>
#include <string.h>
#include <math.h>
double root(double x) { return sqrt(x); }
double rootp(const double *p) { return sqrt(*p); }
static coro(braam_call *) void bits(double r)
{
    unsigned long long b;
    memcpy(&b, &r, sizeof b);
    await printf("%08x%08x\n", (unsigned)(b >> 32), (unsigned)b);
}
coro(braam_call *) int main(void)
{
    static const double v[] = { 0.0, -0.0, 1.0, 2.0, 3.0, 0.25, 1e-310,
                                1.7976931348623157e308, 1e300, 5e-324 };
    for (int i = 0; i < 10; i++)
        await bits(root(v[i]));
    double inf = v[7] * 2, nan = inf - inf;
    await bits(root(inf));
    await bits(rootp(&v[4]));
    double r1 = root(-1.0), r2 = root(nan), r3 = root(-inf);
    await printf("%d %d %d\n", r1 != r1, r2 != r2, r3 != r3);
    await printf("%g\n", sqrt(16.0) + sqrt(0.25));
    return 0;
}

