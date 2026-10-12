#include <stdio.h>
#include <math.h>
static void body(void) {
    double xs[6];
    xs[0] = 12.0;
    xs[1] = -12.0;
    xs[2] = 0.25;
    xs[3] = 100.0;
    xs[4] = 1.0;
    xs[5] = 0.0;
    int i, e;
    for (i = 0; i < 6; i = i + 1) {
        double x = xs[i];
        double y = ldexp(frexp(x, &e), e);
        if (y != x) {
            printf("FAIL %d\n", i);
            return;
        }
    }
    printf("OK\n");
}
int main(void) {
    body();
    return 0;
}

