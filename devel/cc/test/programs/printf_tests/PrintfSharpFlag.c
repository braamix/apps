#include <stdio.h>
static coro(braam_call *) void body(void) {
    await printf("%#x %#o\n", 255, 64);
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

