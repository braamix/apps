// The port kit: many members of the SDK's archives, and a callback into them.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fixture.h"

namespace {

int by_value(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;
    return (x > y) - (x < y);
}

} // namespace

extern "C" void fx_main(void)
{
    int n = 8;
    int *v = (int *)malloc(n * sizeof *v);
    for (int i = 0; i < n; i++)
        v[i] = (i * 37 + 11) % 23;
    qsort(v, n, sizeof *v, by_value);

    char line[128];
    int at = 0;
    for (int i = 0; i < n; i++)
        at += snprintf(line + at, sizeof line - at, "%s%d", i ? " " : "", v[i]);
    free(v);
    fx_puts(line);
    fx_puts("\n");

    snprintf(line, sizeof line, "%-6s|%5x|%s", "left", 0xbeef, strchr("find:me", ':') + 1);
    fx_puts(line);
    fx_puts(strcmp(line, "left  | beef|me") == 0 ? "\nformatted\n" : "\nmisformatted\n");
}
