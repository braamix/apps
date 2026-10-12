#include <coro.h>
#include <stdio.h>

struct pt { int x; long long y; };

coro(struct pt) struct pt line(int n)
{
    struct pt sum = { 0, 0 };
    for (int i = 1; i <= n; i++) {
        struct pt p = { i, 10 * i };
        yield p;
        sum.x += p.x;
        sum.y += p.y;
    }
    return sum;
}

coro(struct pt) long double outer(void)
{
    struct pt s = await line(3);
    yield (struct pt){ 9, 9 };
    printf("-> %d %lld | ", s.x, s.y);
    return 1.5L + (await line(0)).x + s.x;
}

int main(void)
{
    co_frame(struct pt, long double) *o = co_alloca(outer, 512);
    while (co_resume(o) == CO_SUSPENDED) {
        struct pt p = co_value(o);
        printf("(%d,%lld) ", p.x, p.y);
    }
    printf("%g\n", (double)co_result(o));
    return 0;
}

