#include <alloca.h>
#include <stdio.h>
static int sum(int n)
{
    int *v = alloca(n * sizeof(int));
    for (int i = 0; i < n; i++)
        v[i] = i;
    int s = 0;
    for (int i = 0; i < n; i++)
        s += v[i];
    return s;
}
coro(braam_call *) int main(int argc, char **argv)
{
    for (int k = 0; k < 1000; k++)
        sum(100);
    printf("sum %d\n", sum(100));
    return 0;
}

