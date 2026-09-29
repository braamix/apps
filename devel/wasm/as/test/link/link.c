// The C half of test/link.mjs: it calls functions defined in WAT, and
// defines one that WAT calls.
#include "fixture.h"

int wat_fib(int n);
int wat_sum(int a, int b);
void wat_report(void);

int c_twice(int x)
{
    return 2 * x;
}

void fx_main(void)
{
    fx_puts("fib(10) = ");
    fx_putn(wat_fib(10));
    fx_puts("\nsum = ");
    fx_putn(wat_sum(3, 4));
    fx_puts("\n");
    wat_report();
    wat_report();
}
