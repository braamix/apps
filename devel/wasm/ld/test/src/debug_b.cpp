// The other half: more strings for .debug_str to merge.
#include "debug.h"

const char *debug_names[] = { "twice", "never" };

int debug_b(int x)
{
    return debug_twice(x) + 1;
}
