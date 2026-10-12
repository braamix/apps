/*
 * hello — the smallest program: `cc hello.c && ./a.out`.
 */
#include <stdio.h>

coro(braam_call *) int main(int argc, char **argv)
{
    printf("Hello, %s!\n", argc > 1 ? argv[1] : "world");
    return 0;
}
