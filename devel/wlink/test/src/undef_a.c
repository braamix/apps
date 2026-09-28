// Weak references: two nobody defines, two undef_b.c does.
#include "fixture.h"

extern int absent_fn(void) __attribute__((weak));
extern int absent_var __attribute__((weak));
extern int present_fn(void) __attribute__((weak));
extern int present_var __attribute__((weak));

void fx_main(void)
{
    fx_puts(absent_fn ? "absent_fn present\n" : "absent_fn absent\n");
    fx_puts(&absent_var ? "absent_var present\n" : "absent_var absent\n");
    if (present_fn) {
        fx_puts("present_fn ");
        fx_putn(present_fn());
        fx_puts("\n");
    }
    if (&present_var) {
        fx_puts("present_var ");
        fx_putn(present_var);
        fx_puts("\n");
    }
}
