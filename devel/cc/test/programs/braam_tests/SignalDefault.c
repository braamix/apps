#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
coro(braam_call *) int main(int argc, char **argv)
{
    if (await sig_catch(SIGKILL, 1) != -1 || errno != EINVAL)
        return 1;
    printf("waiting\n");
    await fflush(stdout);
    char buf[8];
    await read(0, buf, sizeof buf);
    printf("not reached\n");
    return 0;
}

