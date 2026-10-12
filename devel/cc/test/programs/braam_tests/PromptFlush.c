#include <stdio.h>
#include <unistd.h>
coro(braam_call *) int main(int argc, char **argv)
{
    char name[32];
    await printf("name? ");
    ssize_t n = await read(0, name, sizeof name - 1);
    if (n > 0 && name[n - 1] == '\n')
        n--;
    name[n < 0 ? 0 : n] = 0;
    await printf("hello, %s\n", name);
    return 0;
}

