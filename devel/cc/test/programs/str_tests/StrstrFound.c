#include <stdio.h>
#include <string.h>
static coro(braam_call *) void body(void) {
    const char *s = "ABCDEF";
    char *p = strstr(s, "CDE");
    await printf("%d\n", (int)(p - s));
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

