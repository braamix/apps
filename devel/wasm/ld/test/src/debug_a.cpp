// Compiled with -g: line tables, variables, and a function nothing calls,
// whose debug info must point at the tombstone.
#include "debug.h"
#include "fixture.h"

static int debug_counter = 3;

int debug_b(int);

int debug_unused(int x)
{
    return x * debug_counter;
}

extern "C" void fx_main(void)
{
    int local = debug_twice(debug_counter);
    fx_puts(debug_names[0]);
    fx_puts(" ");
    fx_putn(local + debug_b(4));
    fx_puts("\n");
}
