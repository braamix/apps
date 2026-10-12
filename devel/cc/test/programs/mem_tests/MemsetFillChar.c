#include <stdio.h>
#include <string.h>
static coro(braam_call *) void body(void) {
    char buf[9];
    memset(buf, 'A', 8);
    buf[8] = 0;
    await printf("%s\n", buf);
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

