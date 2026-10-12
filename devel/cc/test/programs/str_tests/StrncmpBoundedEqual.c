#include <stdio.h>
#include <string.h>
static coro(braam_call *) void body(void) {
    // Differ at index 3, only 3 compared -> equal.
    await printf("%d\n", strncmp("ABCX", "ABCY", 3));
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

