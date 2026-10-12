#include <stdio.h>
static coro(braam_call *) void body(void) {
    char b[4];
    b[0] = 'C'; b[1] = 'B'; b[2] = 'A'; b[3] = 0;
    char *p = b + 2;
    while (p >= b) {
        await putchar(*p);
        --p;
    }
    await putchar('\n');
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

