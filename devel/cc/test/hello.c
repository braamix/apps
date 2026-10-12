#include <stdio.h>

coro(braam_call *) int main(int argc, char **argv)
{
    printf("hello from %s:", argv[0]);
    for (int i = 1; i < argc; i++)
        printf(" [%s]", argv[i]);
    printf("\n");
    return argc;
}
