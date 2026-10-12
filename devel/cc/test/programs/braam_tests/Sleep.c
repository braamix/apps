#include <stdio.h>
coro(braam_call *) int main(int argc, char **argv)
{
    unsigned t0 = braam_now();
    printf("before\n");
    await fflush(stdout);
    await sleep_ms(100);
    printf("after %d\n", braam_now() - t0 >= 90);
    return 0;
}

