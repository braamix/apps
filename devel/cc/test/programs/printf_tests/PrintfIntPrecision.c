#include <stdio.h>
static coro(braam_call *) void body(void) {
    await printf("%.5d %05.3d\n", 42, 42);
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

