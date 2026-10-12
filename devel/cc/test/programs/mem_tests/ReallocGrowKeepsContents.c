#include <stdio.h>
#include <stdlib.h>
#include <string.h>
coro(braam_call *) int main(void) {
    char *p = malloc(6);
    strcpy(p, "HELLO");
    malloc(32);
    p = realloc(p, 12);
    strcat(p, " WORLD");
    await printf("%s\n", p);
    return 0;
}

