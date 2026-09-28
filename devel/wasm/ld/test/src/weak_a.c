// Weak definitions: two overridden by weak_b.c, one left alone.
#include "fixture.h"

__attribute__((weak)) const char *weak_who(void)
{
    return "weak";
}

__attribute__((weak)) const char *weak_only(void)
{
    return "weak only";
}

__attribute__((weak)) int weak_value = 1;

void fx_main(void)
{
    fx_puts(weak_who());
    fx_puts("\n");
    fx_puts(weak_only());
    fx_puts("\n");
    fx_putn(weak_value);
    fx_puts("\n");
}
