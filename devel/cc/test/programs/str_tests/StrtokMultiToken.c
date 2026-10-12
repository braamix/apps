#include <stdio.h>
#include <string.h>
static coro(braam_call *) void body(void) {
    char buf[16];
    strcpy(buf, "AB,CD,EF");
    char *t = strtok(buf, ",");
    while (t != 0) {
        await printf("%s\n", t);
        t = strtok(0, ",");
    }
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

