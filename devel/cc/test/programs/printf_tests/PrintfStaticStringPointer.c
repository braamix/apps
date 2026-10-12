#include <stdio.h>
static coro(braam_call *) void body(void) {
    static char *p = "ABC";
    await printf("[%s]\n", p);
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

