#include <stdio.h>
coro(braam_call *) int main(void) {
    await printf("%.0f %.0f %.0f %.0f %.0f\n", 0.5, 1.5, 2.5, 3.5, -2.5);
    await printf("%.1f %.1f %.2f %.2f\n", 0.25, 0.75, 1.125, 0.35);
    await printf("%.0e %.0e %.1e %.2e\n", 25.0, 35.0, 1.25, 1.125);
    await printf("%.1g %.2g %g %g\n", 2.5, 0.125, 0.5, 1e-5);
    return 0;
}

