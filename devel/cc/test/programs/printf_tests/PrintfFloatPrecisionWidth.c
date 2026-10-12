#include <stdio.h>
static coro(braam_call *) void body(void) {
    await printf("[%.2f][%10.3f][%-8.3f]\n", 3.14159, 3.14159, 3.14159);
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

