#include <stdio.h>
#include <string.h>
static coro(braam_call *) void body(void) {
    char dst[4];
    char *r = memmove(dst, "XYZ", 3);
    await printf("%d\n", r == dst);
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

