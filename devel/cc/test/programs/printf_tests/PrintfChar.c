#include <stdio.h>
static coro(braam_call *) void body(void) {
    await printf("%c%c%c\n", 'A', 'B', 'C');
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

