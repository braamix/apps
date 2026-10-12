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

static int f(int x)
{
    defer say("f\n");
    {
        defer say("A\n");
        defer say("B\n");
        if (x)
            return x * 10;
    }
    defer say("C\n");
    return 1;
}

static int run(void)
{
    say("%d\n", f(0));
    say("%d\n", f(2));
    if (1)
        defer say("now\n");
    say("end\n");
    return 0;
}

coro(braam_call *) int main(void)
{
    int status = run();
    await fputs(said, stdout);
    return status;
}
