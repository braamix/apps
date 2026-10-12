#include <stdio.h>
coro(braam_call *) int foo(int a, int b, int c, int d, int e, int f, int g, int h) {
    await putchar(h);
    return a + g;
}

coro(braam_call *) int main(void) {
    return await foo(1, 2, 3, 4, 5, 6, 7, 65);
}

