// Functions whose addresses are taken, and a table of them in .rodata.
#include "fixture.h"

int fn_add(int a, int b)
{
    return a + b;
}

int fn_sub(int a, int b)
{
    return a - b;
}

int fn_mul(int a, int b)
{
    return a * b;
}

int (*const fn_ops[])(int, int) = {fn_add, fn_sub, fn_mul};
const char *const fn_names[] = {"add", "sub", "mul"};
