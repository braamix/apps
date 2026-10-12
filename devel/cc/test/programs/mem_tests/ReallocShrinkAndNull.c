#include <stdio.h>
#include <stdlib.h>
#include <string.h>
coro(braam_call *) int main(void) {
    char *p = malloc(16);
    strcpy(p, "ABC");
    char *q = realloc(p, 4);
    char *r = realloc(NULL, 8);
    await printf("%d %s %d\n", q == p, q, r != NULL && r != p);
    return 0;
}

