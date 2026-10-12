#include <stdio.h>
#include <stdlib.h>

static coro(braam_call *) int nap(int k)
{
    await sleep_ms(10 * (8 - k));
    await printf("done %d\n", k);
    return k;
}

coro(braam_call *) int main(int argc, char **argv)
{
    static char storage[9][2048];
    braam_task *t[9];
    await printf("spawned");
    for (int k = 1; k <= 7; k++)
        await printf(" %d", braam_spawn(t[k] = co_init(storage[k], 2048, nap, k)));
    await printf(", then %d\n", braam_spawn(co_init(storage[8], 2048, nap, 8)));
    while (await sleep_ms(10) == 0) {
        int busy = 0;
        for (int k = 1; k <= 7; k++)
            busy += !co_done(t[k]);
        if (!busy)
            break;
    }
    braam_task *gone = co_init(storage[1], 2048, nap, 0);
    braam_spawn(gone);
    co_destroy(gone);
    await printf("after destroy: %d\n", braam_spawn(co_init(storage[2], 2048, nap, 9)));
    await sleep_ms(200);
    return 0;
}

