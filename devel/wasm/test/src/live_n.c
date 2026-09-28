// libfxa_live.a: wanted by live code, so its constructor runs.
#include "fixture.h"

__attribute__((constructor)) static void live_n_init(void)
{
    fx_puts("live_n init\n");
}

const char *live_n(void)
{
    return "live_n";
}
