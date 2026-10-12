#include <stdio.h>
static int counter;
static coro(braam_call *) int count(void)
{
    for (int i = 0; i < 5; i++) {
        await braam_yield();
        counter++;
    }
    return 0;
}
coro(braam_call *) int main(int argc, char **argv)
{
    static char storage[2048];
    braam_task *t = co_init(storage, sizeof storage, count);
    braam_spawn(t);
    int bad = 0;
    while (!co_done(t))
        bad |= await braam_yield();
    await printf("yields %d, counter %d\n", bad, counter);
    return 0;
}

