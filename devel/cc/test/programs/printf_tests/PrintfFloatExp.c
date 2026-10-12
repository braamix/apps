#include <stdio.h>
static coro(braam_call *) void body(void) {
    await printf("%e\n", 12.34);
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

