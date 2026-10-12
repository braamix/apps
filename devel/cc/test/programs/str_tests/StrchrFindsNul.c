#include <stdio.h>
#include <string.h>
static coro(braam_call *) void body(void) {
    const char *s = "ABC";
    char *p = strchr(s, 0);
    await printf("%d\n", (int)(p - s));
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

