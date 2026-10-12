#include <errno.h>
#include <poll.h>
#include <stdio.h>
coro(braam_call *) int main(int argc, char **argv)
{
    struct pollfd p[2] = { { 0, POLLIN, 0 }, { 1, POLLOUT | POLLIN, 0 } };
    int n = await poll(p, 2, -1);
    await printf("%d: %d %d\n", n, p[0].revents, p[1].revents);
    struct pollfd q = { 1, POLLIN, 0 };
    n = await poll(&q, 1, 0);
    await printf("%d: %d\n", n, q.revents);
    struct pollfd r = { 9, POLLIN, 0 };
    n = await poll(&r, 1, 0);
    await printf("%d %d\n", n, errno == EINVAL);
    return 0;
}

