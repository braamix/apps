#include <stdio.h>
static coro(braam_call *) void body(void) {
    char buf[32];
    int n = snprintf(buf, 32, "n=%d s=%s", 7, "abc");
    await printf("[%s](%d)\n", buf, n);
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

