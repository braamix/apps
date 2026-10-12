#include <coro.h>
#include <stdio.h>

static int gone;

coro(int) void count(int id, int n)
{
    defer {
        if (id < 100)
            printf("[%d gone]\n", id);
        else
            gone++;
    }
    for (int i = 0; i < n; i++)
        if (yield i == CO_CANCEL) {
            printf("[%d cancelled]\n", id);
            return;
        }
}

static unsigned long depth(void)
{
    volatile int here;
    return (unsigned long)&here;
}

static int first_over(int limit)
{
    co_frame(int, void) *c = co_alloca(count, 0, 1, 100);
    while (co_resume(c) == CO_SUSPENDED)
        if (co_value(c) > limit)
            return co_value(c);
    return -1;
}

int main(void)
{
    unsigned long before = depth();
    for (int k = 0; k < 1000; k++) {
        co_frame(int, void) *c = co_alloca(count, 64, 100 + k, 2);
        co_resume(c);
        if (k == 49)
            break;
        if (k == 48)
            continue;
    }
    printf("loop %d kept %d\n", gone, depth() == before);
    printf("first over 3: %d\n", first_over(3));

    int tries = 0;
again:
    {
        co_frame(int, void) *c = co_alloca(count, 0, 7, 10);
        co_resume(c);
        if (++tries < 2)
            goto again;
    }
    {
        co_frame(int, void) *c = co_alloca(count, 0, 8, 10);
        co_resume(c);
        printf("cancel %d\n", co_cancel(c));
    }
    co_frame(int, void) *c = co_alloca(count, 0, 9, 10);
    co_resume(c);
    return 0;
}

