#include <stdio.h>
#include <string.h>
static coro(braam_call *) void body(void) {
    const char *s = "ABCABC";
    char *p = strrchr(s, 'B');
    await printf("%d\n", (int)(p - s));
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

