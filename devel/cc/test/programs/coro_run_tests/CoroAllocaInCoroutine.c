#include <coro.h>
#include <stdio.h>

coro(int) void digits(int n)
{
    defer printf("[digits %d gone] ", n);
    for (int i = 0; i < n; i++)
        yield '0' + i;
}

coro(char) void letters(int n)
{
    defer printf("[letters gone] ");
    for (int i = 0; i < n; i++)
        yield 'a' + i;
    co_frame(int, void) *d = co_alloca(digits, 0, 2);
    co_resume(d);
    yield 'x';
    yield 'y';
}

coro(char) int root(void)
{
    defer printf("[root gone]\n");
    for (int k = 0; k < 2; k++) {
        co_frame(char, void) *l = co_alloca(letters, 64, 3);
        for (int i = 0; i < 3 && co_resume(l) == CO_SUSPENDED; i++)
            yield co_value(l);
        yield '|';
    }
    co_frame(char, void) *l = co_alloca(letters, 64, 3);
    while (co_resume(l) == CO_SUSPENDED)
        yield co_value(l);
    return 0;
}

int main(void)
{
    static _Alignas(16) char storage[(96 + 2 * 160) * sizeof(void *) / 4];
    co_frame(char, int) *r = co_init(storage, sizeof storage, root);
    for (int i = 0; co_resume(r) == CO_SUSPENDED; i++) {
        printf(i ? " %c" : "%c", co_value(r));
        if (co_value(r) == 'x')
            break;
    }
    printf("\n");
    co_destroy(r);
    return 0;
}

