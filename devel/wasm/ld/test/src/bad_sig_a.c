// A direct call with one signature to a function nothing defines.
#include "fixture.h"

int sig_fn(int a);

void fx_main(void)
{
    fx_putn(sig_fn(1));
}
