// Two strong definitions of one variable and one function: an error.
#include "fixture.h"

int dup_value = 1;

int dup_fn(void)
{
    return 1;
}

void fx_main(void)
{
    fx_putn(dup_value + dup_fn());
}
