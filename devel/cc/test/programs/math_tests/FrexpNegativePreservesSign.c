#include <stdio.h>
#include <math.h>
static coro(braam_call *) void body(void) {
    int e;
    double m = frexp(-12.0, &e);   /* fraction in (-1, -0.5] */
    await printf("%f %d\n", m, e);
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

