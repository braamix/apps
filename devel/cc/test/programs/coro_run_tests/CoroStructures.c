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

struct pt { int x, y; long long z; };

coro(struct pt) struct pt walk(struct pt from, int steps)
{
    defer say("walk done\n");
    for (int i = 0; i < steps; i++) {
        from.x++;
        from.z += 100;
        yield from;
    }
    return from;
}

static int run(void)
{
    struct pt p = { 1, 2, 3 };
    co_frame(struct pt, struct pt) *w = co_alloca(walk, 0, p, 3);
    while (co_resume(w) == CO_SUSPENDED) {
        struct pt q = co_value(w);
        say("%d %d %lld\n", q.x, q.y, q.z);
    }
    struct pt r = co_result(w);
    say("result %d %d %lld\n", r.x, r.y, r.z);

    co_frame(struct pt, struct pt) *w2 = co_alloca(walk, 0, p, 5);
    co_resume(w2);
    int st = co_destroy(w2);
    say("destroy %d done %d\n", st, co_done(w2));
    return 0;
}

coro(braam_call *) int main(void)
{
    int status = run();
    await fputs(said, stdout);
    return status;
}
