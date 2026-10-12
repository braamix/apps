#include <stdio.h>
void *g = "AB";
struct s { const void *p; } sg = { "CD" };
static coro(braam_call *) void body(void) {
    static void *q = "EF";
    await printf("[%s][%s][%s]\n", (char *)g, (const char *)sg.p, (char *)q);
}
coro(braam_call *) int main(void) {
    await body();
    return 0;
}

