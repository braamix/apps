#include <inttypes.h>

#include "string_map.h"
#include "typetab.h"

#ifndef __braam__ // debug dumps nothing calls; on Braam they would be coroutines

extern StringMap typetab;

//
// Print a single typedef entry.
//
static void print_typedef(const TypeDef *def)
{
    trace_printf("    typedef %s:\n", def->name);
    print_type(stdout, def->type, 8);
}

static void typetab_print_callback(const char *key, intptr_t ptr, const void *arg)
{
    (void)key;
    (void)arg;
    print_typedef((const TypeDef *)ptr);
}

//
// Print all typedef entries.
//
void typetab_print()
{
    map_iterate(&typetab, typetab_print_callback, NULL);
}
#endif
