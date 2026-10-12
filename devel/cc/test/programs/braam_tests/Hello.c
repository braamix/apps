#include <stdio.h>
coro(braam_call *) int main(int argc, char **argv)
{
    printf("hello from %s: %d args\n", argv[0], argc);
    for (int i = 1; i < argc; i++)
        printf("  %s\n", argv[i]);
    return 0;
}

