int i = 0;

#include <stdio.h>
coro(braam_call *) int *print_A(void) {
    await putchar(65);
    return &i;
}

coro(braam_call *) int *print_B(void) {
    await putchar(66);
    return &i;
}

coro(braam_call *) int main(void) {

    *await print_A() += 5;
    if (i != 5) {
        return 1;
    }

    *await print_B() += 5l;
    if (i != 10) {
        return 2;
    }

    return 0;
}
