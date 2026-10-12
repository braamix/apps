#include <coro.h>
#include <stdio.h>

struct pt { int x, y; long long z; };

coro(struct pt) struct pt walk(struct pt from, int steps)
{
    defer printf("walk done\n");
    for (int i = 0; i < steps; i++) {
        from.x++;
        from.z += 100;
        yield from;
    }
    return from;
}

int main(void)
{
    struct pt p = { 1, 2, 3 };
    co_frame(struct pt, struct pt) *w = co_alloca(walk, 0, p, 3);
    while (co_resume(w) == CO_SUSPENDED) {
        struct pt q = co_value(w);
        printf("%d %d %lld\n", q.x, q.y, q.z);
    }
    struct pt r = co_result(w);
    printf("result %d %d %lld\n", r.x, r.y, r.z);

    co_frame(struct pt, struct pt) *w2 = co_alloca(walk, 0, p, 5);
    co_resume(w2);
    int st = co_destroy(w2);
    printf("destroy %d done %d\n", st, co_done(w2));
    return 0;
}

