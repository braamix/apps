#include <stdio.h>
#include <string.h>
static coro(braam_call *) void body(void) {
    char buf[6];
    buf[0] = 'A';
    buf[1] = 'B';
    buf[2] = 'C';
    buf[3] = 'D';
    buf[4] = 'E';
    buf[5] = 0;
    memset(buf + 1, 'Z', 3);
    await printf("%s\n", buf);
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

