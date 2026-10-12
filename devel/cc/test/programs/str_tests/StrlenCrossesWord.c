#include <stdio.h>
#include <string.h>
static coro(braam_call *) void body(void) {
    await printf("%d\n", (int)strlen("ABCDEFGH"));
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

