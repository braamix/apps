#include <stdio.h>
static coro(braam_call *) void body(void) {
    await printf("100%%\n");
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

