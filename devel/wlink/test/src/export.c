// An extra export, a symbol kept although unused, and one that is dropped.
#include "fixture.h"

__attribute__((export_name("fx_extra"))) int fx_extra(int x)
{
    return x + 1;
}

__attribute__((used)) int fx_kept(int x)
{
    return x * 2;
}

int fx_dropped(int x)
{
    return x * 3;
}

void fx_main(void)
{
    fx_puts("exported\n");
}
