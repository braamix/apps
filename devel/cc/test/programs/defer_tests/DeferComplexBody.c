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

static void f(int x)
{
    int n = 3;
    defer {
        defer say("in\n");
        for (int i = 0; i < 10; i++) {
            if (i == n)
                break;
            say("%d ", i);
        }
        switch (n) {
        case 2:
        case 3:
            say("two ");
            break;
        default:
            say("other ");
        }
        goto done;
    done:
        say("| ");
    }
    if (x == 1)
        return;
    if (x == 2)
        n = 2;
}

static int run(void)
{
    f(0);
    f(1);
    f(2);
    return 0;
}

coro(braam_call *) int main(void)
{
    int status = run();
    await fputs(said, stdout);
    return status;
}
