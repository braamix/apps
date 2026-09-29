// The C half of test/link.mjs: it calls functions defined in WAT, defines
// one that WAT calls, and reads and writes WAT's data.
#include "fixture.h"

int wat_fib(int n);
int wat_sum(int a, int b);
void wat_report(void);
int wat_bump(void);

extern int wat_counter;
extern const char wat_greeting[];
extern int *wat_counter_ptr;
extern int wat_buf[2];

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
    fx_puts("\n");
    fx_puts(wat_greeting);
    fx_puts("\n");
    fx_putn(wat_counter);
    fx_puts(" ");
    wat_counter = 40;
    fx_putn(wat_bump());
    fx_puts(" ");
    fx_putn(*wat_counter_ptr);
    fx_puts(" ");
    fx_putn(wat_buf[1]);
    fx_puts("\n");
}
