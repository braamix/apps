//
// Typedef name table: maps typedef names to their underlying types.
//
#ifndef TYPETAB_H
#define TYPETAB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ast.h"

// A typedef entry: name → underlying type
typedef struct {
    char *name; // typedef name (owned copy)
    Type *type; // underlying type (owned clone)
    int level;  // scope level at which the typedef was declared (0 = file scope)
} TypeDef;

// Initialize the typedef table (create an empty table)
void typetab_init(void);
// Postcondition: Typedef table is empty and ready for use.

// Destroy the typedef table (free all memory)
void typetab_destroy(void);
// Postcondition: All TypeDef and Type memory is freed, table is empty.

// Add a typedef mapping
void typetab_add(const char *ident, const Type *type, int level);
// Precondition: name is a non-null string, type is a valid Type*.
// Postcondition: A TypeDef with the name and a clone of type is added/replaced.

// Check if a typedef name exists
bool typetab_exists(const char *name);
// Precondition: name is a non-null string.
// Postcondition: Returns true if name exists in typetab, else false.

// Get a typedef entry by name (fails if not found)
TypeDef *typetab_find(const char *name);
// Precondition: name is a non-null string.
// Postcondition: Returns non-null TypeDef* if found, else terminates with error.

// Resolve a typedef name to its underlying type (fails if not found)
const Type *typetab_resolve(const char *name);
// Precondition: name is a non-null string.
// Postcondition: Returns the underlying Type* for name, else terminates with error.

// Remove names which exceed given level
void typetab_purge(int level);

// Print all typedef entries
void typetab_print(void);

#ifdef __cplusplus
}
#endif

#endif /* TYPETAB_H */
