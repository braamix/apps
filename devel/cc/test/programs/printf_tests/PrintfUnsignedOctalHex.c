#include <stdio.h>
static coro(braam_call *) void body(void) {
    await printf("%u %o %x %X\n", 100, 64, 255, 255);
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

