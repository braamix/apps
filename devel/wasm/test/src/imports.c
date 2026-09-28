// A kernel import declared here as well as in the runtime: one import results.
#include "fixture.h"

__attribute__((import_module("kernel"), import_name("sys"))) int
kernel_sys(unsigned op, unsigned a0, unsigned a1, unsigned a2);

enum { SYS_GETPID = 2 };

void fx_main(void)
{
    unsigned pid = (unsigned)kernel_sys(SYS_GETPID, 0, 0, 0);
    fx_puts(pid == fx_pid() && pid != 0 ? "pid matches\n" : "pid differs\n");
}
