#include <alloca.h>
#include <stdio.h>
#include <string.h>

static int sum12(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j,
                 int k, int l)
{
    return a + b + c + d + e + f + g + h + i + j + k + l;
}

int main(void)
{
    char *p = alloca(13);
    strcpy(p, "abcdefghijkl");
    int s = sum12(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
    char *q = alloca(4);
    strcpy(q, "xyz");
    printf("%d %s\n", sum12(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12) == s ? s : -1, p);
    return 0;
}

