#include <coro.h>
#include <stdio.h>

coro(long) int sub(int n)
{
    defer puts("[sub]");
    for (int i = 0; i < n; i++)
        yield 10 + i;
    return n + 3;
}

coro(long) int outer(void)
{
    defer puts("[outer]");
    {
        defer puts("[inner]");
        return await sub(2);
    }
}

coro(long) int explicit(int n)
{
    static _Alignas(16) char storage[128];
    co_frame(long, int) *s = co_init(storage, sizeof storage, sub, n);
    int r = await s;
    return r + n;
}

int main(void)
{
    static _Alignas(16) char storage[256];
    co_frame(long, int) *o = co_init(storage, sizeof storage, outer);
    co_resume(o);
    printf("v %ld\n", co_value(o));
    co_destroy(o);
    printf("done %d\n", co_done(o));

    co_frame(long, int) *e = co_init(storage, sizeof storage, explicit, 2);
    while (co_resume(e) == CO_SUSPENDED)
        printf("v %ld\n", co_value(e));
    printf("explicit 5 %d\n", co_result(e));
    return 0;
}

