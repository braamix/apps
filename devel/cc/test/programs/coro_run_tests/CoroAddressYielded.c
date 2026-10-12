#include <coro.h>
#include <stdio.h>

coro(int *) void cells(int n)
{
    struct { int v[4]; } box = { { 0 } };
    for (int i = 0; i < n; i++) {
        box.v[i & 3] = i * 10;
        yield &box.v[i & 3];
    }
}

int main(void)
{
    co_frame(int *, void) *c = co_alloca(cells, 0, 6);
    while (co_resume(c) == CO_SUSPENDED)
        printf("%d ", *co_value(c));
    printf("\n");
    return 0;
}

