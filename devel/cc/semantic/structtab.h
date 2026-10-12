//
// Definitions of struct and union.
//
#ifndef STRUCTTAB_H
#define STRUCTTAB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ast.h"

// Structure for a struct member entry
typedef struct FieldDef {
    struct FieldDef *next; // Next member in list
    char *name;            // Member name (Ident, owned copy)
    Type *type;            // Member type (Type* from ast.h)
    int offset;            // Offset within the struct (in bytes); a bit-field's storage unit
    BitField bf;           // A bit-field's place in its unit; width 0 for any other member
    // With Target.bitfield_access_bits, the bytes of clang's access unit holding this
    // bit-field, which the ABI passes as one piece.
    int access_offset, access_size;
    int index; // the field's place among the struct's declarations, from 0
} FieldDef;

// Structure for a struct type entry.  A definition outlives its scope: on purge or
// replacement it is retired, not freed, so an AST node that cached it (cached_def) stays
// valid until structtab_destroy.
typedef struct StructDef {
    struct StructDef *retired_next; // on the retired list
    char *tag;                      // Struct tag (Ident, owned copy)
    TypeKind kind;                  // TYPE_STRUCT or TYPE_UNION
    bool complete;                  // false for a forward declaration, true once defined
    SrcLoc loc;                     // where it was declared or defined, for a note
    int alignment;                  // Alignment requirement (in bytes)
    int size;                       // Total size of the struct (in bytes)
    FieldDef *members;              // List of members, sorted by offset
    // With Target.bitfield_unit_per_field or bitfield_access_bits, the unnamed bit-fields,
    // which are not members:
    // NULL-named, by `index`; a `:0` has width 0 and the offset of the byte it moved to.
    FieldDef *unnamed;
} StructDef;

// Initialize the type table (create an empty table)
void structtab_init(void);
// Postcondition: Type table is empty and ready for use.

// Destroy the type table (free all memory)
void structtab_destroy(void);
// Postcondition: All StructDef and FieldDef memory is freed, table is empty.

// Add a struct definition
void structtab_add_struct(const char *tag, TypeKind kind, bool complete, int alignment, int size,
                          FieldDef *members, int scope_level);
// Precondition: tag is a non-null string, members is a valid list of elements or NULL.
// Postcondition: A StructDef with tag, kind, complete, alignment, size, and copied members is
// added/replaced in structtab.

// Check if a struct tag exists
bool structtab_exists(const char *tag);
// Precondition: tag is a non-null string.
// Postcondition: Returns true if tag exists in structtab, else false.

// Get a struct definition by tag (fails if not found)
StructDef *structtab_find(const char *tag);
// Precondition: tag is a non-null string.
// Postcondition: Returns non-null StructDef* if found, else terminates with error.

// Get a struct definition by tag (returns NULL if not found)
StructDef *structtab_find_opt(const char *tag);
// Precondition: tag is a non-null string.
// Postcondition: Returns StructDef* if found, else NULL.

// Remove names, which exceed given level.
void structtab_purge(int level);

// Print all types.
void structtab_print(void);

// Allocate a FieldDef
FieldDef *new_member(const char *name, Type *type, int offset);

#ifdef __cplusplus
}
#endif

#endif /* STRUCTTAB_H */
