#include <coro.h>
#include <stdio.h>

// On Braam printf is a coroutine of its own yield type, which deep cannot await:
// deep yields its depth, and main prints it.
coro(int) int deep(int n)
{
    yield n;
    return n < 1000 ? await deep(n + 1) : n;
}

coro(braam_call *) int main(void)
{
    co_frame(int, int) *d = co_alloca(deep, 2 * co_sizeof(deep), 0);
    while (co_resume(d) == CO_SUSPENDED)
        await printf("depth %d\n", co_value(d));
    await puts("not reached");
    return 0;
}
