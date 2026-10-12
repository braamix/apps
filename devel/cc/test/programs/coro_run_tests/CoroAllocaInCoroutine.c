#include <coro.h>
#include <stdio.h>
#include <stdarg.h>

// On Braam printf is a coroutine of its own yield type, which neither a deferred
// statement nor a coroutine of another one can await: what they print is kept, and
// main prints it.
static char said[4096];
static int said_len;

static void say(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    said_len += vsnprintf(said + said_len, sizeof said - said_len, fmt, ap);
    va_end(ap);
}

coro(int) void digits(int n)
{
    defer say("[digits %d gone] ", n);
    for (int i = 0; i < n; i++)
        yield '0' + i;
}

coro(char) void letters(int n)
{
    defer say("[letters gone] ");
    for (int i = 0; i < n; i++)
        yield 'a' + i;
    co_frame(int, void) *d = co_alloca(digits, 0, 2);
    co_resume(d);
    yield 'x';
    yield 'y';
}

coro(char) int root(void)
{
    defer say("[root gone]\n");
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

static int run(void)
{
    static _Alignas(16) char storage[(96 + 2 * 160) * sizeof(void *) / 4];
    co_frame(char, int) *r = co_init(storage, sizeof storage, root);
    for (int i = 0; co_resume(r) == CO_SUSPENDED; i++) {
        say(i ? " %c" : "%c", co_value(r));
        if (co_value(r) == 'x')
            break;
    }
    say("\n");
    co_destroy(r);
    return 0;
}

coro(braam_call *) int main(void)
{
    int status = run();
    await fputs(said, stdout);
    return status;
}
