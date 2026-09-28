// Calls through another object's table, and a pointer in .data.
#include "fixture.h"

int fn_add(int, int);
int fn_mul(int, int);
extern int (*const fn_ops[])(int, int);
extern const char *const fn_names[];

int (*fn_current)(int, int) = fn_mul;

static int apply(int (*f)(int, int), int a, int b)
{
    return f(a, b);
}

void fx_main(void)
{
    for (int i = 0; i < 3; i++) {
        fx_puts(fn_names[i]);
        fx_puts(" ");
        fx_putn(apply(fn_ops[i], 7, 3));
        fx_puts("\n");
    }
    fx_puts("current ");
    fx_putn(fn_current(6, 7));
    fx_puts("\n");
    fx_puts(fn_ops[0] == fn_add ? "same address\n" : "different address\n");
    fx_puts(fn_ops[0] != fn_ops[2] ? "distinct\n" : "not distinct\n");
}
