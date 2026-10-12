#include <stdio.h>
#include <string.h>
static coro(braam_call *) void body(void) {
    // 'A'(65) - 'B'(66) = -1
    await printf("%d\n", memcmp("AAAA", "ABAA", 4));
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

