/* Test calling string manipulation functions from the standard library */

int strcmp(char *s1, char *s2);
#include <stdio.h>
unsigned long strlen(char *s);
int atoi(char *s);

coro(braam_call *) int main(void) {
    if (strcmp("abc", "abc")) {
        return 1;
    }

    // "ab" should compare less than "xy"
    if (strcmp("ab", "xy") >= 0) {
        return 2;
    }

    await puts("HELLO, WORLD!");

    if (strlen("")) {
        return 3;
    }

    int i = atoi("10");
    if (i != 10) {
        return 4;
    }

    return 0;
}
