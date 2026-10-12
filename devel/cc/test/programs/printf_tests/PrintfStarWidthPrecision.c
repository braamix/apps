#include <stdio.h>
static coro(braam_call *) void body(void) {
    await printf("[%*d][%.*f]\n", 6, 42, 2, 3.14159);
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

