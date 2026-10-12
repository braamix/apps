#include <stdio.h>
#include <string.h>
static coro(braam_call *) void body(void) {
    // 'B'(66) - 'A'(65) = 1
    await printf("%d\n", memcmp("ABAA", "AAAA", 4));
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

