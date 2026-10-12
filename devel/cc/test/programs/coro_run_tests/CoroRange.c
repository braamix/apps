#include <coro.h>
#include <stdio.h>

coro(int) void range(int lo, int hi)
{
    for (int i = lo; i < hi; i++)
        if (yield i == CO_CANCEL)
            return;
}

int main(void)
{
    co_frame(int, void) *f = co_alloca(range, 0, 3, 7);
    while (co_resume(f) == CO_SUSPENDED)
        printf("%d\n", co_value(f));
    printf("done %d\n", co_done(f));
    return 0;
}

