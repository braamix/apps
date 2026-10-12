#include <coro.h>
#include <stdio.h>
coro(int) long squares(int n);
coro(char) void later(void);
int main(void)
{
    // The header, then the value and the result at least; aligned as a pointer or a long.
    size_t header = 2 * sizeof(unsigned) + 4 * sizeof(void *);
    size_t align  = _Alignof(long) > _Alignof(void *) ? _Alignof(long) : _Alignof(void *);
    printf("size %d align %d\n", co_sizeof(squares) >= header + sizeof(int) + sizeof(long),
           co_alignof(squares) == align);
    co_frame(int, long) *s = co_alloca(squares, 0, 4);
    while (co_resume(s) == CO_SUSPENDED)
        printf("%d ", co_value(s));
    printf("= %ld\n", co_result(s));
    co_frame(char, void) *l = co_alloca(later, 0);
    while (co_resume(l) == CO_SUSPENDED)
        putchar(co_value(l));
    putchar('\n');
    return 0;
}
coro(char) void later(void)
{
    for (const char *p = "hello"; *p; p++)
        yield *p;
}

