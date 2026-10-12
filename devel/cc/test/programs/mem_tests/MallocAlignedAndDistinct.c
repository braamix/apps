#include <stdio.h>
#include <stdlib.h>
coro(braam_call *) int main(void) {
    char *a = malloc(3);
    char *b = malloc(5);
    await printf("%d %d %d\n", ((long)a & 15) == 0, ((long)b & 15) == 0, b >= a + 3);
    return 0;
}

