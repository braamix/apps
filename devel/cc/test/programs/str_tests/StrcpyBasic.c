#include <stdio.h>
#include <string.h>
static coro(braam_call *) void body(void) {
    char dst[10];
    strcpy(dst, "ABCDEFG");
    await printf("%s\n", dst);
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

