#include <stdio.h>
#include <math.h>
static void body(void) {
    int e;
    double m = frexp(12.0, &e);
    printf("M=%f E=%d\n", m, e);
    printf("BACK=%f\n", ldexp(m, e));
    double n = frexp(100.0, &e);       /* not a power of two */
    printf("BACK=%f\n", ldexp(n, e));
}
int main(void) {
    body();
    return 0;
}

