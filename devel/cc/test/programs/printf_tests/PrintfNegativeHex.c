#include <stdio.h>
static coro(braam_call *) void body(void) {
    await printf("%x\n", -1);
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

