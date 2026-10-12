#include <stdio.h>
static coro(braam_call *) void body(void) {
    await printf("%+d % d %+d\n", 42, 42, -42);
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

