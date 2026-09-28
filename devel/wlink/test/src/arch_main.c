// Reaches libone.a, which needs libtwo.a, which needs libone.a again.
#include "fixture.h"

const char *one_a(void);

void fx_main(void)
{
    fx_puts(one_a());
    fx_puts("\n");
}
