// Liveness: what --gc-sections keeps, and what it drops.
#include "fixture.h"

void nowhere_dead(void);                           // defined nowhere
const char *live_m(void);                          // libfxa_live.a, only from dead code
const char *live_n(void);                          // libfxa_live.a
extern void live_stub(void) __attribute__((weak)); // defined nowhere, called

__attribute__((used)) static void kept_local(void) {}
__attribute__((used)) void kept_global(void) {}
__attribute__((used, retain)) static const char kept_retained[] = "retained";

static void dead(void)
{
    nowhere_dead();
    fx_puts(live_m());
}

void dead_global(void)
{
    dead();
}

void fx_main(void)
{
    fx_puts(live_n());
    if (live_stub)
        live_stub();
    fx_puts("\n");
}
