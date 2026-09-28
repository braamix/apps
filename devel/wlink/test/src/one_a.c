// libone.a: pulled by arch_main.c. Its constructor must run.
#include "fixture.h"

const char *two_b(void);

__attribute__((constructor)) static void one_a_init(void)
{
    fx_puts("one_a init\n");
}

const char *one_a(void)
{
    return two_b();
}
