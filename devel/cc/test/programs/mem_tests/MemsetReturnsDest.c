#include <stdio.h>
#include <string.h>
static coro(braam_call *) void body(void) {
    char buf[4];
    char *r = memset(buf, 'X', 4);
    await printf("%d\n", r == buf);
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

