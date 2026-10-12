#include <stdio.h>
#include <string.h>
static coro(braam_call *) void body(void) {
    char dst[8];
    int i;
    for (i = 0; i < 8; i++) dst[i] = 'Z';
    strncpy(dst, "AB", 6);
    dst[6] = 0;
    /* "AB" then four NULs; printed length stops at first NUL. */
    await printf("%d\n", (int)strlen(dst));
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

