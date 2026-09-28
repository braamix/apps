// Calls whose signatures differ from the definitions in sigs_b.c. Each
// becomes a stub that traps, so none is made; a stub's address is null.
#include "fixture.h"

int sig_one(int a);
int sig_two(int a);
double sig_three(double a);
int sig_right(void);

void fx_main(void)
{
    if (fx_pid() == 0) {
        fx_putn(sig_one(1));
        fx_putn(sig_two(2));
        fx_putn((long)sig_three(3));
    }
    fx_puts("stub ");
    fx_putn((long)sig_one);
    fx_puts("\n");
    fx_puts("right ");
    fx_putn(sig_right());
    fx_puts("\n");
}
