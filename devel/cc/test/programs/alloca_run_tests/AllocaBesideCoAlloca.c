#include <alloca.h>
#include <coro.h>
#include <stdio.h>
#include <stdarg.h>

// On Braam printf is a coroutine, and a function that calls alloca cannot be one:
// what it prints is kept, and main prints it.
static char said[4096];
static int said_len;

static void say(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    said_len += vsnprintf(said + said_len, sizeof said - said_len, fmt, ap);
    va_end(ap);
}

coro(int) void range(int lo, int hi)
{
    for (int i = lo; i < hi; i++)
        yield i;
}

static int run(void)
{
    int *seen = alloca(4 * sizeof(int));
    int n     = 0;
    {
        co_frame(int, void) *f = co_alloca(range, 0, 3, 7);
        while (co_resume(f) == CO_SUSPENDED) {
            seen[n++] = co_value(f);
            say("%d ", co_value(f));
        }
    }
    say("|");
    int total = 0;
    for (int i = 0; i < n; i++) {
        say(" %d", seen[i]);
        total += seen[i];
    }
    return total;
}

static int program(void)
{
    int t = run();
    say(" | %d\n", t);
    return 0;
}

coro(braam_call *) int main(void)
{
    int status = program();
    await fputs(said, stdout);
    return status;
}
