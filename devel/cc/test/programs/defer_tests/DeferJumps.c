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

static int run(void)
{
    for (int i = 0; i < 5; i++) {
        defer say("iter %d\n", i);
        if (i == 1)
            continue;
        if (i == 2)
            break;
        say("body %d\n", i);
    }
    while (1) {
        defer say("once\n");
        break;
    }
    int n = 0;
again:
    {
        defer say("left %d\n", n++);
        if (n < 2)
            goto again;
    }
    int back = 0;
loop:
    if (back < 2) {
        defer back++;
        goto loop;
    }
    say("n=%d back=%d\n", n, back);
    return 0;
}

coro(braam_call *) int main(void)
{
    int status = run();
    await fputs(said, stdout);
    return status;
}
