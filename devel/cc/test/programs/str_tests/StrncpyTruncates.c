#include <stdio.h>
#include <string.h>
static coro(braam_call *) void body(void) {
    char dst[8];
    strncpy(dst, "ABCDEF", 3);
    dst[3] = 0;
    await printf("%s\n", dst);
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

