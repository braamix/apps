#include <coro.h>
#include <stdarg.h>
#include <stdio.h>

// Yields each of its n int arguments, suspending between two va_arg.
coro(int) int each(int n, ...)
{
    va_list ap;
    va_start(ap, n);
    int s = 0;
    for (int i = 0; i < n; i++) {
        int v = va_arg(ap, int);
        s += v;
        yield v;
    }
    va_end(ap);
    return s;
}

// Mixed classes, a copy of the list kept across a suspension.
coro(int) long mixed(const char *fmt, ...)
{
    va_list ap, again;
    va_start(ap, fmt);
    va_copy(again, ap);
    long n = 0;
    for (const char *f = fmt; *f; f++) {
        yield 0;
        switch (*f) {
        case 'i': n += va_arg(ap, int); break;
        case 'd': n += (long)(va_arg(ap, double) * 10); break;
        case 'L': n += (long)(va_arg(ap, long double) * 100); break;
        case 'l': n += (long)va_arg(ap, long long); break;
        case 's': n += *va_arg(ap, const char *); break;
        }
    }
    yield 0;
    if (*fmt == 'i')
        n += 1000 * va_arg(again, int);
    va_end(again);
    va_end(ap);
    return n;
}

// Awaits the variadic ones, an argument itself awaited.
coro(int) long outer(int k)
{
    long a = await each(3, k, await each(1, 20), 30);
    long b = await mixed("idLls", 1, 2.5, (long double)0.25, 1000000LL, "A");
    long c = await each(0);
    return a + b + c;
}

coro(braam_call *) int main(void)
{
    co_frame(int, long) *f = co_alloca(outer, 512, 10);
    while (co_resume(f) == CO_SUSPENDED)
        await printf("%d ", co_value(f));
    await printf("= %ld\n", co_result(f));

    static _Alignas(16) char storage[256];
    co_frame(int, int) *g = co_init(storage, sizeof storage, each, 2, 7, 8);
    while (co_resume(g) == CO_SUSPENDED)
        await printf("%d ", co_value(g));
    await printf("= %d\n", co_result(g));

    co_frame(int, long) *h = co_alloca(mixed, 0, "LL", 1.0L, 2.0L);
    co_resume(h);
    await puts("not reached");
    return 0;
}
