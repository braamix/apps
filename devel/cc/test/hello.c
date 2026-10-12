#include <stdio.h>

coro(braam_call *) int main(int argc, char **argv)
{
    await printf("hello from %s:", argv[0]);
    for (int i = 1; i < argc; i++)
        await printf(" [%s]", argv[i]);
    await printf("\n");
    return argc;
}
