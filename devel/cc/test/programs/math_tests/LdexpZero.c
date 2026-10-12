#include <stdio.h>
#include <math.h>
static coro(braam_call *) void body(void) {
    await printf("%f\n", ldexp(0.0, 5));   /* scaling zero stays zero */
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

