#include <stdio.h>
#include <math.h>
static coro(braam_call *) void body(void) {
    await printf("%f %f %f\n", ldexp(1.0, 3), ldexp(0.75, 4), ldexp(1.0, -2));
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

