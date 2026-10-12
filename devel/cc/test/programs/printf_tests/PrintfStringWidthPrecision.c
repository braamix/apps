#include <stdio.h>
static coro(braam_call *) void body(void) {
    await printf("[%10s][%-10s][%.3s]\n", "hi", "hi", "hello");
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

