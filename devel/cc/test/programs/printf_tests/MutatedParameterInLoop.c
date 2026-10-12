#include <stdio.h>
coro(braam_call *) void countdown(int n) {
    while (n > 0) {
        await putchar('0' + n);
        --n;
    }
}
static coro(braam_call *) void body(void) { await countdown(3); await putchar('\n'); }
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

