#include <stdio.h>
#include <string.h>
static coro(braam_call *) void body(void) {
    char dst[9];
    memmove(dst, "ABCDEFGH", 8);
    dst[8] = 0;
    await printf("%s\n", dst);
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

