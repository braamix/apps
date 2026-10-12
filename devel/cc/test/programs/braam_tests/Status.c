#include <unistd.h>
coro(braam_call *) int main(int argc, char **argv)
{
    char buf[512];
    for (;;) {
        ssize_t n = await read(0, buf, sizeof buf);
        if (n <= 0)
            return n < 0;
        if (await write(1, buf, n) != n)
            return 1;
    }
}

