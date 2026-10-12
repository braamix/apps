#include <stdio.h>
coro(braam_call *) int incr_and_print(int c);

coro(braam_call *) int main(void) {
    await incr_and_print(70);
    return 0;
}

coro(braam_call *) int incr_and_print(int b) {
    await putchar(b + 2);
    return 0;
}

