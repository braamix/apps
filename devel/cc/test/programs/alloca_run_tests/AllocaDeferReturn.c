#include <alloca.h>
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

static int f(int early)
{
    int *v = alloca(3 * sizeof(int));
    v[0] = 5;
    v[1] = 7;
    v[2] = 18;
    defer say("defer %d\n", v[1]);
    if (early) {
        for (int i = 0; i < 3; i++)
            if (v[i] == 7)
                return v[0] + v[i];
    }
    return v[1] + v[2] + v[0];
}

static int run(void)
{
    int a = f(1);
    int b = f(0);
    say("first %d then %d\n", a, b);
    return 0;
}

coro(braam_call *) int main(void)
{
    int status = run();
    await fputs(said, stdout);
    return status;
}
