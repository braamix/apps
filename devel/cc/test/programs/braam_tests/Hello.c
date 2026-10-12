#include <stdio.h>
coro(braam_call *) int main(int argc, char **argv)
{
    await printf("hello from %s: %d args\n", argv[0], argc);
    for (int i = 1; i < argc; i++)
        await printf("  %s\n", argv[i]);
    return 0;
}

