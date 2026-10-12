#include <stdint.h>

#include "string_map.h"

static StringMap nametab;

//
// Find name in the symbol table and return value.
// When not found, return 0.
//
int nametab_find(const char *name)
{
    intptr_t value = 0;
    if (map_get(&nametab, name, &value)) {
        return value;
    }
    return 0;
}

//
// Add name to the symbol table, with given value, at given level.
// Values can be:
//      TOKEN_TYPEDEF_NAME
//      TOKEN_ENUMERATION_CONSTANT
//
void nametab_define(const char *name, int value, int level)
{
    // printf("--- define %s as %s at level %d\n", name,
    // value == TOKEN_TYPEDEF_NAME ? "TOKEN_TYPEDEF_NAME" :
    // value == TOKEN_ENUMERATION_CONSTANT ? "TOKEN_ENUMERATION_CONSTANT" : "???", level);
    map_insert(&nametab, name, value, level);
}

//
// Remove one name.
//
void nametab_remove(const char *name)
{
    map_remove_key(&nametab, name);
}

//
// Remove names from the tree, which exceed given level.
//
void nametab_purge(int level)
{
    map_remove_level(&nametab, level);
}

//
// Deallocate the symbol table.
//
void nametab_destroy()
{
    map_destroy(&nametab);
}
