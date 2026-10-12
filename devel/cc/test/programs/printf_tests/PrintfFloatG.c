#include <stdio.h>
static coro(braam_call *) void body(void) {
    await printf("%g %g\n", 12.34, 100000.0);
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

