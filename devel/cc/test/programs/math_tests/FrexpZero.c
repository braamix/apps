#include <stdio.h>
#include <math.h>
static coro(braam_call *) void body(void) {
    int e;
    double m = frexp(0.0, &e);   /* returns 0.0, *e = 0 (branchless path) */
    await printf("%f %d\n", m, e);
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

