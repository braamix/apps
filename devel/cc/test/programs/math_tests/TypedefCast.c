#include <stdio.h>
/* A cast through a typedef of a floating type converts, as musl's (double_t)x does. */
typedef double D;
typedef float F;
static double widen(float x, double r) { return ((D)x + x + r) / (x + r + r); }
static float narrow(double d) { return (F)d; }
coro(braam_call *) int main(void) {
    await printf("%d %d %d\n", widen(1.5f, 2.0) == 5.0 / 5.5, narrow(0.1) == 0.1f,
                 (D)(F)0.1 == (double)0.1f);
    return 0;
}
