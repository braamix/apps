#include <stdio.h>
#include <string.h>
static coro(braam_call *) void body(void) {
    char *p = memchr("ABCDEF", 'D', 3);
    await printf("%d\n", p == 0);
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

