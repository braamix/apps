// Layout: this file brings .bss before layout_b.c brings .data and a segment
// of its own name, and a constructor of default priority.
#include "fixture.h"

extern char __heap_base[];

char layout_zero[5];
extern int layout_custom;

__attribute__((constructor)) static void layout_a_init(void)
{
    fx_puts("a-default ");
}

void fx_main(void)
{
    layout_zero[4] = 1;
    fx_puts((unsigned long)__heap_base % 16 == 0 ? "heap aligned " : "heap misaligned ");
    fx_putn(layout_custom + layout_zero[4]);
    fx_puts("\n");
}
