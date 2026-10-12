#include <stdio.h>
#include <math.h>
static void body(void) {
    printf("%f %f %f\n", ldexp(1.0, 3), ldexp(0.75, 4), ldexp(1.0, -2));
}
int main(void) {
    body();
    return 0;
}

