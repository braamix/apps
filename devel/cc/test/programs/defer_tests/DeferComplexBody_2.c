#include <coro.h>
#include <stdio.h>

static int trace[64], n;
static void note(int x) { trace[n++] = x; }

static int returns(int k)
{
    int total = 0;
    defer { note(1); note(2); note(3); note(4); }
    {
        defer { note(10); note(11); note(12); note(13); }
        if (k == 0)
            return total + 100;
        for (int i = 0; i < 3; i++) {
            defer { note(20 + i); note(30 + i); note(40); note(41); }
            if (i == k)
                return total + i;
            if (i == 1 && k == 9)
                continue;
            if (i == 2 && k == 8)
                break;
            total += 1000;
        }
        if (k == 7)
            goto out;
        total += 5;
    }
    note(99);
out:
    return total;
}

static coro(int) void gen(void)
{
    defer { note(50); note(51); note(52); note(53); }
    for (int i = 0; i < 4; i++) {
        defer { note(60 + i); note(70); note(71); note(72); }
        yield i;
    }
}

coro(braam_call *) int main(void)
{
    int ks[] = { 0, 1, 2, 7, 8, 9, 5 };
    for (int j = 0; j < 7; j++) {
        n = 0;
        int r = returns(ks[j]);
        await printf("%d:%d:", ks[j], r);
        for (int i = 0; i < n; i++)
            await printf(" %d", trace[i]);
        await printf("\n");
    }
    for (int stop = 0; stop <= 4; stop++) {
        n = 0;
        static char storage[1024];
        co_frame(int, void) *g = co_init(storage, sizeof storage, gen);
        for (int i = 0; i < stop; i++)
            co_resume(g);
        if (!co_done(g))
            co_destroy(g);
        await printf("destroy after %d:", stop);
        for (int i = 0; i < n; i++)
            await printf(" %d", trace[i]);
        await printf("\n");
    }
    return 0;
}

