#include <stdio.h>
#include <stdlib.h>
coro(braam_call *) int main(void) {
    int *p = calloc(10, sizeof(int));
    int sum = 0;
    for (int i = 0; i < 10; i++)
        sum += p[i];
    await printf("%d\n", sum);
    return 0;
}

