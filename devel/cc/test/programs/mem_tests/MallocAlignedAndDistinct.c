#include <stdio.h>
#include <stdlib.h>
int main(void) {
    char *a = malloc(3);
    char *b = malloc(5);
    printf("%d %d %d\n", ((long)a & 15) == 0, ((long)b & 15) == 0, b >= a + 3);
    return 0;
}

