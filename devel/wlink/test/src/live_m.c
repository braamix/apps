// libfxa_live.a: loaded, but only dead code wants it, so its constructor
// must not run.
#include "fixture.h"

__attribute__((constructor)) static void live_m_init(void)
{
    fx_puts("live_m init\n");
}

const char *live_m(void)
{
    return "live_m";
}
