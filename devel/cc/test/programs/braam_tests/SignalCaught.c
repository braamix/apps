#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static coro(braam_call *) int ticker(int n)
{
    while (await sleep_ms(5) == 0)
        ;
    printf("ticker: %s\n", strerror(errno));
    return n + 1;
}

coro(braam_call *) int main(int argc, char **argv)
{
    static char storage[4096];
    braam_task *t = co_init(storage, sizeof storage, ticker, 41);
    printf("spawned %d\n", braam_spawn(t));
    if (await sig_catch(SIGINT, 1) < 0)
        return 1;
    char buf[64];
    for (;;) {
        ssize_t n = await read(0, buf, sizeof buf);
        if (n < 0 && errno == EINTR) {
            printf("interrupted, took %d, pending %u\n", sig_take(SIGINT), sig_pending());
            continue;
        }
        if (n <= 0)
            break;
        printf("read %d\n", (int)n);
    }
    printf("ticker done %d result %d\n", co_done(t), co_done(t) ? co_result(t) : -1);
    return 0;
}

