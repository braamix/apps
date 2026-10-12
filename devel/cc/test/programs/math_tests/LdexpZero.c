#include <stdio.h>
#include <math.h>
static void body(void) {
    printf("%f\n", ldexp(0.0, 5));   /* scaling zero stays zero */
}
int main(void) {
    body();
    return 0;
}

