#include <stdio.h>

coro(braam_call *) int print_alphabet(void) {
    static int count = 0;
    await putchar(count + 65);
    count = count + 1;
    if (count < 26) {
        await print_alphabet();
    }
    return count;
}

coro(braam_call *) int main(void) {
    await print_alphabet();
}
