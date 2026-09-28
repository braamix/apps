// Reaches libfxa_back.a through libfxa_back2.a, after libfxa_back.a was
// registered: a member loading pulls another that refers back into it.
#include "fixture.h"

const char *back_p(void);

void fx_main(void)
{
    fx_puts(back_p());
    fx_puts("\n");
}
