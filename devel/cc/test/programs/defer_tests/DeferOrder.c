#include <coro.h>
#include <stdio.h>

static int f(int x)
{
    defer puts("f");
    {
        defer puts("A");
        defer puts("B");
        if (x)
            return x * 10;
    }
    defer puts("C");
    return 1;
}

int main(void)
{
    printf("%d\n", f(0));
    printf("%d\n", f(2));
    if (1)
        defer puts("now");
    puts("end");
    return 0;
}

