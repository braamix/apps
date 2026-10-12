#include <stdio.h>
int main(void) {
    long double x = 2.5L, y = 12345.678L;
    printf("%d %Lf %d %Le %lld %Lg %d\n", 1, x, 3, y, -7LL, 0.25L, 9);
    return 0;
}

