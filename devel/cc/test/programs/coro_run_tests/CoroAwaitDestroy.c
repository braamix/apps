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

coro(long) int sub(int n)
{
    defer say("[sub]\n");
    for (int i = 0; i < n; i++)
        yield 10 + i;
    return n + 3;
}

coro(long) int outer(void)
{
    defer say("[outer]\n");
    {
        defer say("[inner]\n");
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

static int run(void)
{
    static _Alignas(16) char storage[256];
    co_frame(long, int) *o = co_init(storage, sizeof storage, outer);
    co_resume(o);
    say("v %ld\n", co_value(o));
    co_destroy(o);
    say("done %d\n", co_done(o));

    co_frame(long, int) *e = co_init(storage, sizeof storage, explicit, 2);
    while (co_resume(e) == CO_SUSPENDED)
        say("v %ld\n", co_value(e));
    say("explicit 5 %d\n", co_result(e));
    return 0;
}

coro(braam_call *) int main(void)
{
    int status = run();
    await fputs(said, stdout);
    return status;
}
