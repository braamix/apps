#include <stdio.h>
#include <stdlib.h>
#include <string.h>
unsigned long __vcc_memory_size(void);
static int round_trip(void)
{
    char *p[64];
    for (int i = 0; i < 64; i++) {
        size_t n = 1 + (i * 37) % 3000;
        p[i]     = malloc(n);
        if (!p[i])
            return 0;
        memset(p[i], i, n);
    }
    for (int k = 0; k < 2; k++)
        for (int i = k; i < 64; i += 2) {
            size_t n = 1 + (i * 37) % 3000;
            for (size_t j = 0; j < n; j++)
                if (p[i][j] != (char)i)
                    return 0;
            free(p[i]);
        }
    return 1;
}
coro(braam_call *) int main(int argc, char **argv)
{
    if (!round_trip())
        return 1;
    unsigned long pages = __vcc_memory_size();
    for (int r = 0; r < 200; r++)
        if (!round_trip())
            return 2;
    char *big = realloc(malloc(10), 100000);
    if (!big)
        return 3;
    free(big);
    await printf("ok %d\n", __vcc_memory_size() <= pages + 2);
    return 0;
}

