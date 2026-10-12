#include <stdio.h>
static coro(braam_call *) void body(void) {
    await printf("%.0f %.0f\n", 0.5, 9.5);
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

