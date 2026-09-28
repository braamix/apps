// Constructor priorities: lower runs first, the default is last.
#include "fixture.h"

__attribute__((constructor(300))) static void third(void)
{
    fx_puts("c300 ");
}

__attribute__((constructor(101))) static void first(void)
{
    fx_puts("c101 ");
}

__attribute__((constructor)) static void last(void)
{
    fx_puts("c-default ");
}

__attribute__((constructor(200))) static void second(void)
{
    fx_puts("c200 ");
}

void fx_main(void)
{
    fx_puts("main\n");
}
