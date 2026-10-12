#include <coro.h>
#include <stdio.h>

int main(void)
{
    for (int i = 0; i < 5; i++) {
        defer printf("iter %d\n", i);
        if (i == 1)
            continue;
        if (i == 2)
            break;
        printf("body %d\n", i);
    }
    while (1) {
        defer puts("once");
        break;
    }
    int n = 0;
again:
    {
        defer printf("left %d\n", n++);
        if (n < 2)
            goto again;
    }
    int back = 0;
loop:
    if (back < 2) {
        defer back++;
        goto loop;
    }
    printf("n=%d back=%d\n", n, back);
    return 0;
}

