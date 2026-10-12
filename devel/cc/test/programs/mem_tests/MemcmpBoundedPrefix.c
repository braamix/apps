#include <stdio.h>
#include <string.h>
static coro(braam_call *) void body(void) {
    await printf("%d\n", memcmp("ABCX", "ABCY", 3));
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

