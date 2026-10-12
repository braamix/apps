#include <stdio.h>
#include <math.h>
static void body(void) {
    int e;
    double m = frexp(12.0, &e);   /* 12 = 0.75 * 2^4 */
    printf("M=%f E=%d\n", m, e);
}
int main(void) {
    body();
    return 0;
}

