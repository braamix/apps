#include <stdio.h>
#include <math.h>
static void body(void) {
    int e;
    double m = frexp(-12.0, &e);   /* fraction in (-1, -0.5] */
    printf("%f %d\n", m, e);
}
int main(void) {
    body();
    return 0;
}

