#include <stdio.h>
#include <string.h>
static coro(braam_call *) void body(void) {
    char buf[8];
    memcpy(buf, "ABCDEF", 6);
    buf[6] = 0;
    memmove(buf + 1, buf, 5);
    await printf("%s\n", buf);
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

