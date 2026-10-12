#include "structtab.h"

#include <stdint.h>

#include "ast.h"
#include "semantic.h"
#include "string_map.h"
#include "xalloc.h"

StringMap structtab;

//
// Allocate a FieldDef
//
FieldDef *new_member(const char *name, Type *type, int offset)
{
    FieldDef *field = xalloc(sizeof(FieldDef), __func__, __FILE__, __LINE__);
    field->name     = xstrdup(name);
    field->type     = type;
    field->offset   = offset;
    return field;
}

//
// Deallocate FieldDef.
//
void free_member(FieldDef *def)
{
    while (def) {
        FieldDef *next = def->next;

        free_type(def->type);
        xfree(def->name);
        xfree(def);
        def = next;
    }
}

//
// Deallocate StructDef.
//
void free_struct(StructDef *def)
{
    if (def) {
        free_member(def->members);
        free_member(def->unnamed);
        xfree(def->tag);
        xfree(def);
    }
}

static StructDef *retired; // purged or replaced definitions, freed by structtab_destroy

static void structtab_destroy_callback(intptr_t ptr)
{
    free_struct((StructDef *)ptr);
}

static void structtab_retire_callback(intptr_t ptr)
{
    StructDef *def    = (StructDef *)ptr;
    def->retired_next = retired;
    retired           = def;
}

//
// Initialize the type table (create an empty table)
// Postcondition: StructDef table is empty and ready for use.
//
void structtab_init()
{
    // Empty.
}

//
// Destroy the type table (free all memory)
// Postcondition: All StructDef and FieldDef memory is freed, table is empty.
//
void structtab_destroy()
{
    map_destroy_free(&structtab, structtab_destroy_callback);
    while (retired) {
        StructDef *next = retired->retired_next;
        free_struct(retired);
        retired = next;
    }
}

//
// Add a struct definition
// Precondition: tag is a non-null string, members is a valid list of elements or NULL.
// Postcondition: A StructDef with tag, alignment, size, and copied members is added/replaced in
// structtab.
//
void structtab_add_struct(const char *tag, TypeKind kind, bool complete, int alignment, int size,
                          FieldDef *members, int level)
{
    if (semantic_debug) {
        printf("--- %s() %s\n", __func__, tag);
    }
    // Build new definition.
    StructDef *def = xalloc(sizeof(StructDef), __func__, __FILE__, __LINE__);
    def->tag       = xstrdup(tag);
    def->kind      = kind;
    def->complete  = complete;
    def->alignment = alignment;
    def->size      = size;
    def->members   = members;
    def->loc       = diag_loc;

    map_insert_free(&structtab, tag, (intptr_t)def, level, structtab_retire_callback);
}

//
// Check if a struct tag exists
// Precondition: tag is a non-null string.
// Postcondition: Returns true if tag exists in structtab, else false.
//
bool structtab_exists(const char *tag)
{
    intptr_t value = 0;
    if (!map_get(&structtab, tag, &value)) {
        return false;
    }
    return true;
}

//
// Get a struct definition by tag (fails if not found)
// Precondition: tag is a non-null string.
// Postcondition: Returns non-null StructDef* if found, else terminates with error.
//
StructDef *structtab_find(const char *tag)
{
    intptr_t value = 0;
    if (!map_get(&structtab, tag, &value)) {
        fatal_error("use of undeclared tag '%s'", tag);
    }
    return (StructDef *)value;
}

StructDef *structtab_find_opt(const char *tag)
{
    intptr_t value = 0;
    if (!map_get(&structtab, tag, &value)) {
        return NULL;
    }
    return (StructDef *)value;
}

//
// Remove names from the tree, which exceed given level.
//
void structtab_purge(int level)
{
    map_remove_level_free(&structtab, level, structtab_retire_callback);
}
