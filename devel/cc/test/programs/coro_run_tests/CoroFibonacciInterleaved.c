#include <coro.h>
#include <stdio.h>

coro(long long) int fib(int n)
{
    long long a = 0, b = 1;
    for (int i = 0; i < n; i++) {
        yield a;
        long long t = a + b;
        a = b;
        b = t;
    }
    return n;
}

static _Alignas(16) char storage[256];

coro(braam_call *) int main(void)
{
    co_frame(long long, int) *f = co_init(storage, sizeof storage, fib, 10);
    co_frame(long long, int) *g = co_alloca(fib, 0, 5);
    while (co_resume(f) == CO_SUSPENDED) {
        await printf("%lld ", co_value(f));
        if (!co_done(g) && co_resume(g) == CO_SUSPENDED)
            await printf("(%lld) ", co_value(g));
    }
    await printf("-> %d %d\n", co_result(f), co_result(g));
    return 0;
}

