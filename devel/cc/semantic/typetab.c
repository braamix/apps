#include "typetab.h"

#include <stdint.h>

#include "ast.h"
#include "semantic.h"
#include "string_map.h"
#include "xalloc.h"

StringMap typetab;

static void free_typedef(TypeDef *def)
{
    if (def) {
        free_type(def->type);
        xfree(def->name);
        xfree(def);
    }
}

static void typetab_destroy_callback(intptr_t ptr)
{
    free_typedef((TypeDef *)ptr);
}

//
// Initialize the typedef table (create an empty table)
// Postcondition: Typedef table is empty and ready for use.
//
void typetab_init()
{
    // Empty.
}

//
// Destroy the typedef table (free all memory)
// Postcondition: All TypeDef and Type memory is freed, table is empty.
//
void typetab_destroy()
{
    map_destroy_free(&typetab, typetab_destroy_callback);
}

//
// Add a typedef mapping
// Precondition: name is a non-null string, type is a valid Type*.
// Postcondition: A TypeDef with the name and a clone of type is added/replaced.
//
void typetab_add(const char *ident, const Type *type, int level)
{
    if (semantic_debug) {
        printf("--- %s() %s\n", __func__, ident);
        print_type(stdout, type, 4);
    }
    TypeDef *def = xalloc(sizeof(TypeDef), __func__, __FILE__, __LINE__);
    def->name    = xstrdup(ident);
    def->type    = clone_type(type, __func__, __FILE__, __LINE__);
    def->level   = level;

    map_insert_free(&typetab, ident, (intptr_t)def, level, typetab_destroy_callback);
}

//
// Check if a typedef name exists
// Precondition: name is a non-null string.
// Postcondition: Returns true if name exists in typetab, else false.
//
bool typetab_exists(const char *name)
{
    intptr_t value = 0;
    return map_get(&typetab, name, &value);
}

//
// Get a typedef entry by name (fails if not found)
// Precondition: name is a non-null string.
// Postcondition: Returns non-null TypeDef* if found, else terminates with error.
//
TypeDef *typetab_find(const char *name)
{
    intptr_t value = 0;
    if (!map_get(&typetab, name, &value)) {
        fatal_error("unknown type name '%s'", name);
    }
    return (TypeDef *)value;
}

//
// Resolve a typedef name to its underlying type (fails if not found)
// Precondition: name is a non-null string.
// Postcondition: Returns the underlying Type* for name, else terminates with error.
//
const Type *typetab_resolve(const char *name)
{
    return typetab_find(name)->type;
}

//
// Remove names from the tree which exceed given level.
//
void typetab_purge(int level)
{
    map_remove_level_free(&typetab, level, typetab_destroy_callback);
}
