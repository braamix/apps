#include <alloca.h>
#include <coro.h>
#include <stdio.h>

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
            printf("%d ", co_value(f));
        }
    }
    printf("|");
    int total = 0;
    for (int i = 0; i < n; i++) {
        printf(" %d", seen[i]);
        total += seen[i];
    }
    return total;
}

int main(void)
{
    int t = run();
    printf(" | %d\n", t);
    return 0;
}

