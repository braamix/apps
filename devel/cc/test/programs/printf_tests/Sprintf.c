#include <stdio.h>
static coro(braam_call *) void body(void) {
    char buf[40];
    int n = sprintf(buf, "%d-%x-%c", 5, 250, 'Z');
    await printf("[%s](%d)\n", buf, n);
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

