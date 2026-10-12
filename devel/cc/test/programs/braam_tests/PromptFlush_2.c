#include <stdio.h>
coro(braam_call *) int depth(int n)
{
    char pad[400];
    pad[n % 400] = (char)n;
    return n == 0 ? pad[0] : await depth(n - 1) + 1;
}
coro(braam_call *) int main(int argc, char **argv)
{
    printf("%d\n", await depth(300));
    return 0;
}

