#include <stdio.h>
#include <math.h>
static coro(braam_call *) void body(void) {
    int e;
    double m = frexp(12.0, &e);
    await printf("M=%f E=%d\n", m, e);
    await printf("BACK=%f\n", ldexp(m, e));
    double n = frexp(100.0, &e);       /* not a power of two */
    await printf("BACK=%f\n", ldexp(n, e));
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

