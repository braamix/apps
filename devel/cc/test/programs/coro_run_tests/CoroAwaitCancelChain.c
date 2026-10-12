#include <coro.h>
#include <stdio.h>

coro(int) int bottom(void)
{
    int r = 0;
    defer printf("[bottom %d] ", r);
    for (int i = 0;; i++)
        if (yield i == CO_CANCEL) {
            r = -1;
            return r;
        }
}

coro(int) int middle(void)
{
    int r = await bottom();
    defer printf("[middle %d] ", r);
    return r;
}

coro(int) int top(void)
{
    int r = await middle() - 1;
    defer printf("[top %d]", r);
    return r;
}

int main(void)
{
    co_frame(int, int) *t = co_alloca(top, 256);
    for (int i = 0; i < 3; i++) {
        co_resume(t);
        printf("%d ", co_value(t));
    }
    int st = co_cancel(t);
    printf("\n");
    return st == CO_DONE && co_result(t) == -2 ? 0 : 1;
}

