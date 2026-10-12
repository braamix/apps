#include <stdio.h>
static coro(braam_call *) void body(void) {
    await printf("%f %f %f\n", 1.5, 0.0, -2.5);
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

