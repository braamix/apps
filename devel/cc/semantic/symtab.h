//
// Types for symbol table.
//
#ifndef SYMTAB_H
#define SYMTAB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "ast.h"
#include "tac.h"

// Enum for symbol kinds (corresponding to identifier_attrs)
typedef enum {
    SYM_FUNC,   // FunAttr
    SYM_STATIC, // StaticAttr
    SYM_CONST,  // string literal — const_init
    SYM_ENUM,   // enum constant  — enum_val
    SYM_LOCAL   // LocalAttr
} SymbolKind;

// Enum for initialization state (corresponding to initial_value)
typedef enum {
    INIT_TENTATIVE,   // Tentative
    INIT_INITIALIZED, // Initial
    INIT_NONE         // NoInitializer
} InitKind;

// Structure for a symbol entry
typedef struct Symbol {
    char *name;       // Symbol name (Ident, owned copy)
    Type *type;       // Symbol type (Type* from ast.h)
    SymbolKind kind;  // Kind of symbol (func, static, const, local)
    bool has_linkage; // When function or global/extern variable
    bool block_scope; // True for a static/extern declared inside a block (not at file scope)
    SrcLoc loc;       // where it was declared, for a note after an error about it
    union {
        struct {
            bool defined;     // True if function body is defined
            bool global;      // True if function has global linkage
            bool noret;       // True if declared/defined _Noreturn
            bool coro;        // True if declared _Coro(Y): a coroutine
            Type *yield_type; // its Y (owned), void included; NULL for a function
        } func;               // For SYM_FUNC

        struct {
            bool global;               // True if variable has global linkage
            InitKind init_kind;        // Initialization state
            Tac_StaticInit *init_list; // For INIT_INITIALIZED
            bool literal;              // A file-scope compound literal (_clN), emitted on use
            int alignment;             // bytes from _Alignas, beyond the type's own; else 0
            // No data needed for INIT_TENTATIVE or INIT_NONE
        } static_var; // For SYM_STATIC

        Tac_StaticInit *const_init; // For SYM_CONST (string literals)
        int enum_val;               // For SYM_ENUM (enum constants)
        // No data needed for SYM_LOCAL
    } u;
} Symbol;

// Initialize the symbol table (create an empty table)
void symtab_init(void);
// Postcondition: Symbol table is empty and ready for use.

// Destroy the symbol table (free all memory)
void symtab_destroy(void);
// Postcondition: All Symbol and Tac_StaticInit memory is freed, table is empty.

// Add an automatic (local) variable
void symtab_add_automatic_var_linkage(const char *name, bool has_linkage, int scope_level);
// Precondition: name is a non-null string.
// Postcondition: A Symbol with SYM_LOCAL, name and linkage is added/replaced in symtab.

// Add an automatic (local) variable
void symtab_add_automatic_var_type(const char *name, const Type *t, int scope_level);
// Precondition: name is a non-null string, t is a valid Type*.
// Postcondition: A Symbol with SYM_LOCAL, name, and t is added/replaced in symtab.

// Add a static variable
void symtab_add_static_var(const char *name, const Type *t, bool global, InitKind init_kind,
                           Tac_StaticInit *init_list);
// Precondition: name is a non-null string, t is a valid Type*, init_list is valid if init_kind ==
// INIT_INITIALIZED, else NULL. Postcondition: A Symbol with SYM_STATIC, name, t, global, and init
// state is added/replaced in symtab.

// Add a static variable at a given scope level (block-scope statics, which must be purged on
// block exit so the no-shadowing dup-check and visibility are correct).
void symtab_add_static_var_scoped(const char *name, const Type *t, bool global, InitKind init_kind,
                                  Tac_StaticInit *init_list, int level);

//
// Block-scope static locals.  A `static` declared inside a function has static storage
// duration but no linkage: its storage is emitted inside the owning function's BESM-6 module
// as a module-local label.  Such a symbol is scoped (purged on block exit), so its storage is
// captured here at declaration time and read back by the translator (which runs in the same
// pass, before symtab_destroy).
//
typedef struct StaticLocalRec {
    struct StaticLocalRec *next;
    char *func;                // owning function name (owned)
    char *source;              // source name, for intra-function collision counting (owned)
    char *name;                // backend (possibly suffixed) name (owned)
    const Type *type;          // AST type (borrowed; the AST outlives lowering of this unit)
    Tac_StaticInit *init_list; // owned until the translator transfers it (then NULL)
    int alignment;             // bytes from _Alignas, beyond the type's own; else 0
} StaticLocalRec;

// Set the function whose body is being type-checked (NULL when outside any function body).
void static_locals_set_function(const char *fn);
// Record a block-scope static local for the current function, returning the assigned backend
// name (the source name, or a `name.N` suffix when it repeats within the same function).
const char *static_locals_add(const char *source, const Type *type, Tac_StaticInit *init);
// Head of the captured-static-local list (the translator iterates and filters by ->func).
StaticLocalRec *static_locals_head(void);

// Mark function `name`, just added, a coroutine yielding `yield_type` (copied).
void symtab_set_coro(const char *name, const Type *yield_type);

// Add a function
void symtab_add_fun(const char *name, const Type *t, bool global, bool defined, bool noret);
// Precondition: name is a non-null string, t is a valid Type* (function type).
// Postcondition: A Symbol with SYM_FUNC, name, t, global, defined, and noret is added/replaced in
// symtab.

// Add a string literal: len decoded bytes, which may include embedded NULs.
char *symtab_add_string(const char *s, size_t len);

// Add the anonymous static object of a file-scope compound literal; returns its _clN name.
char *symtab_add_compound_literal(const Type *t, Tac_StaticInit *init_list);

// Add an enum constant
void symtab_add_enum_const(const char *ident, int val, int level);
// Precondition: name is a non-null string.
// Postcondition: A Symbol with SYM_ENUM, name, type int, and integer value is added.

// Get a symbol by name (fails if not found)
Symbol *symtab_get(const char *name);

// A name whose declaration failed: a use of it is no new error.
void symtab_poison(const char *name);
// Precondition: name is a non-null string.
// Postcondition: Returns non-null Symbol* if found, else terminates with error.

// Get a symbol by name (returns NULL if not found)
Symbol *symtab_get_opt(const char *name);
// Precondition: name is a non-null string.
// Postcondition: Returns Symbol* if found, else NULL.

// The scope level a symbol was declared at, or -1 when there is none of that name.
int symtab_level(const char *name);

// Check if a symbol has global linkage
bool symtab_is_global(const char *name);
// Precondition: name is a non-null string, exists in symtab.
// Postcondition: Returns true if the symbol is global (SYM_FUNC or SYM_STATIC with global=true),
// else false.

// Remove names, which exceed given level.
void symtab_purge(int level);

// Print all symbols.
void symtab_print(void);

//
// Allocation, deallocation.
//
Symbol *new_symbol(const char *name, Type *t, SymbolKind kind);
void free_symbol(Symbol *sym);

#ifdef __cplusplus
}
#endif

#endif /* SYMTAB_H */
