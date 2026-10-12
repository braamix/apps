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

coro(int) int bottom(void)
{
    int r = 0;
    defer say("[bottom %d] ", r);
    for (int i = 0;; i++)
        if (yield i == CO_CANCEL) {
            r = -1;
            return r;
        }
}

coro(int) int middle(void)
{
    int r = await bottom();
    defer say("[middle %d] ", r);
    return r;
}

coro(int) int top(void)
{
    int r = await middle() - 1;
    defer say("[top %d]", r);
    return r;
}

static int run(void)
{
    co_frame(int, int) *t = co_alloca(top, 256);
    for (int i = 0; i < 3; i++) {
        co_resume(t);
        say("%d ", co_value(t));
    }
    int st = co_cancel(t);
    say("\n");
    return st == CO_DONE && co_result(t) == -2 ? 0 : 1;
}

coro(braam_call *) int main(void)
{
    int status = run();
    await fputs(said, stdout);
    return status;
}
