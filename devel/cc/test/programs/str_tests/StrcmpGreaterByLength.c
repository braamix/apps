#include <stdio.h>
#include <string.h>
static coro(braam_call *) void body(void) {
    // "ABCD" > "ABC": first extra byte 'D'(68) - '\0'(0) = 68
    await printf("%d\n", strcmp("ABCD", "ABC"));
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

