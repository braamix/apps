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

struct pt { int x; long long y; };

coro(struct pt) struct pt line(int n)
{
    struct pt sum = { 0, 0 };
    for (int i = 1; i <= n; i++) {
        struct pt p = { i, 10 * i };
        yield p;
        sum.x += p.x;
        sum.y += p.y;
    }
    return sum;
}

coro(struct pt) long double outer(void)
{
    struct pt s = await line(3);
    yield (struct pt){ 9, 9 };
    say("-> %d %lld | ", s.x, s.y);
    return 1.5L + (await line(0)).x + s.x;
}

static int run(void)
{
    co_frame(struct pt, long double) *o = co_alloca(outer, 512);
    while (co_resume(o) == CO_SUSPENDED) {
        struct pt p = co_value(o);
        say("(%d,%lld) ", p.x, p.y);
    }
    say("%g\n", (double)co_result(o));
    return 0;
}

coro(braam_call *) int main(void)
{
    int status = run();
    await fputs(said, stdout);
    return status;
}
