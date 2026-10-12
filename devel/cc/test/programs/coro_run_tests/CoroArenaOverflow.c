#include <coro.h>
#include <stdio.h>

coro(void) int deep(int n)
{
    printf("depth %d\n", n);
    return n < 1000 ? await deep(n + 1) : n;
}

int main(void)
{
    co_frame(void, int) *d = co_alloca(deep, 2 * co_sizeof(deep), 0);
    co_resume(d);
    puts("not reached");
    return 0;
}

