// libone.a: nothing refers to it, so it is never loaded and this never runs.
#include "fixture.h"

__attribute__((constructor)) static void one_unused_init(void)
{
    fx_puts("one_unused loaded\n");
}

int one_unused(void)
{
    return 0;
}
