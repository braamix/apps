// Layout: .data and a segment of its own name after layout_a.c's .bss, and a
// constructor that runs before layout_a.c's although it comes later.
#include "fixture.h"

const char layout_text[] = "b150 ";
int layout_data = 3;
__attribute__((section("fx_custom"))) int layout_custom = 7;

__attribute__((constructor(150))) static void layout_b_init(void)
{
    fx_puts(layout_text);
    layout_data++;
}
