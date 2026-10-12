#include <alloca.h>
#include <stdio.h>
#include <string.h>

static void down(char *out, int n)
{
    if (n == 0)
        return;
    char *p = alloca(n + 1);
    memset(p, 'a' + n - 1, n);
    p[n] = 0;
    down(out, n - 1);
    for (int i = 0; i < n; i++)
        if (p[i] != 'a' + n - 1)
            return;
    strncat(out, p, 1);
}

int main(void)
{
    char out[8] = "";
    down(out, 5);
    printf("%s\n", out);
    return 0;
}

