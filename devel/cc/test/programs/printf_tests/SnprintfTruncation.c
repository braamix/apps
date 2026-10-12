#include <stdio.h>
static coro(braam_call *) void body(void) {
    char buf[5];
    int n = snprintf(buf, 5, "%s", "abcdefg");
    await printf("[%s](%d)\n", buf, n);
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

