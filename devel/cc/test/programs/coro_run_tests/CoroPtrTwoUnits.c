#include <coro.h>
#include <stdio.h>
coro(int) int upto(void *arg);
coro(int) int once(void);
static coro_ptr(int, int) table[] = { upto, once };
coro(braam_call *) int main(void)
{
    int three = 3;
    for (int i = 0; i < 2; i++) {
        co_frame(int, int) *p = co_alloca(table[i], 0, &three);
        while (co_resume(p) == CO_SUSPENDED)
            await printf("%d ", co_value(p));
        await printf("(%d) ", co_result(p));
    }
    await printf("\n");
    return 0;
}

