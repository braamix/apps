#include <stdio.h>
#include <math.h>
static void body(void) {
    int e1, e2, e3;
    double m1 = frexp(8.0, &e1);    /* 0.5 * 2^4  */
    double m2 = frexp(1.0, &e2);    /* 0.5 * 2^1  */
    double m3 = frexp(0.25, &e3);   /* 0.5 * 2^-1 */
    printf("%f %d %f %d %f %d\n", m1, e1, m2, e2, m3, e3);
}
int main(void) {
    body();
    return 0;
}

