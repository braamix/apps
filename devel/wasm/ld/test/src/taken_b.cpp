// A direct call, whose signature replaces the placeholder.
#include "fixture.h"
#include "kernel/task.h"

Task<int> fx_taken(int a, int b);
extern Task<int> (*fx_taken_table[])(int, int);

// Runs a task that never suspends and takes its value.
static int now(Task<int> t)
{
    t.handle().resume();
    return t.handle().promise().value.value();
}

void fx_main(void)
{
    fx_puts("called ");
    fx_putn(now(fx_taken(4, 2)));
    fx_puts("\nthrough the table ");
    fx_putn(now(fx_taken_table[0](5, 3)));
    fx_puts("\n");
}
