#include <stdio.h>
#include <math.h>
static void body(void) {
    int e;
    double m = frexp(0.0, &e);   /* returns 0.0, *e = 0 (branchless path) */
    printf("%f %d\n", m, e);
}
int main(void) {
    body();
    return 0;
}

