#include <coro.h>
#include <stdio.h>

static coro(int) int count(void *arg)
{
    int n = *(int *)arg;
    for (int i = 1; i <= n; i++)
        yield i;
    return n * 10;
}

static coro(int) int twice(void)
{
    yield 100;
    yield 200;
    return 7;
}

static coro(int) int chain(void *arg)
{
    coro_ptr(int, int) p = count;
    int r = await p(arg);
    return r + 1;
}

coro_ptr(int, int) table[] = { count, twice, chain };

int main(void)
{
    int three = 3, two = 2;
    void *args[] = { &three, 0, &two };
    static _Alignas(16) char storage[3][512];
    co_frame(int, int) *f[3];
    for (int i = 0; i < 3; i++) {
        printf("task %d: header %d align %d\n", i,
               co_sizeof(table[i]) > 2 * sizeof(unsigned) + 4 * sizeof(void *),
               co_alignof(table[i]) == _Alignof(void *));
        f[i] = co_init(storage[i], sizeof storage[i], table[i], args[i]);
    }
    int live = 3;
    while (live) {
        live = 0;
        for (int i = 0; i < 3; i++) {
            if (co_done(f[i]))
                continue;
            if (co_resume(f[i]) == CO_SUSPENDED) {
                printf("  %d yields %d\n", i, co_value(f[i]));
                live++;
            } else {
                printf("  %d returns %d\n", i, co_result(f[i]));
            }
        }
    }
    coro_ptr(int, int) q = twice;
    co_frame(int, int) *g = co_alloca(q, 0);
    co_resume(g);
    printf("alloca'd: %d, same %d, null %d\n", co_value(g), q == table[1], q != 0);
    return 0;
}

