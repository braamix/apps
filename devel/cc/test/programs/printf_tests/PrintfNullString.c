#include <stdio.h>
static coro(braam_call *) void body(void) {
    await printf("[%s]\n", (char *)0);
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

