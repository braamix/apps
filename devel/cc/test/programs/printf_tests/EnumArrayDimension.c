#include <stdio.h>
enum { N = 4 };
static coro(braam_call *) void body(void) { await printf("%d\n", (int)sizeof(char[N])); }
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

