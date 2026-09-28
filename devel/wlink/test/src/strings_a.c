// The same literal in two objects: STRINGS segments a linker may merge.
#include "fixture.h"

const char *strings_b(void);

static const char *strings_a(void)
{
    return "a literal both objects hold";
}

void fx_main(void)
{
    fx_puts(strings_a());
    fx_puts("\n");
    fx_puts(strings_b());
    fx_puts("\n");
    fx_puts(strings_a() + 2);
    fx_puts("\n");
}
