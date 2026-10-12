#include <stdio.h>
#include <math.h>
static coro(braam_call *) void body(void) {
    int e;
    double m = frexp(12.0, &e);   /* 12 = 0.75 * 2^4 */
    await printf("M=%f E=%d\n", m, e);
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

