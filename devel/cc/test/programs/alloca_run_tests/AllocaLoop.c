#include <alloca.h>
#include <stdio.h>

static int sum(void)
{
    int total = 0;
    for (int i = 0; i < 10; i++) {
        char *p = alloca(16);
        for (int j = 0; j < 16; j++)
            p[j] = (char)i;
        total += p[0] + p[15];
    }
    return total;
}

int main(void)
{
    int s = 0, n = 0;
    for (; n < 1000; n++)
        s = sum();
    printf("sum %d calls %d\n", s, n);
    return 0;
}

