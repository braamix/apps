#include "symtab.h"

#include <stdint.h>
#include <string.h>

#include "ast.h"
#include "internal.h"
#include "semantic.h"
#include "string_map.h"
#include "xalloc.h"

StringMap symtab;
static int str_id;
static int cl_id;

//
// Build new symbol.
//
Symbol *new_symbol(const char *name, Type *t, SymbolKind kind)
{
    if (semantic_debug) {
        printf("--- %s() %s\n", __func__, name);
        print_type(stdout, t, 4);
    }
    Symbol *sym = xalloc(sizeof(Symbol), __func__, __FILE__, __LINE__);
    sym->name   = xstrdup(name);
    sym->type   = t;
    sym->kind   = kind;
    sym->loc    = diag_loc; // the declaration being checked
    return sym;
}

//
// Deallocate symbol.
//
void free_symbol(Symbol *sym)
{
    if (!sym)
        return;
    switch (sym->kind) {
    case SYM_STATIC:
        tac_free_static_init(sym->u.static_var.init_list);
        break;
    case SYM_CONST:
        tac_free_static_init(sym->u.const_init);
        break;
    case SYM_ENUM:
        break;
    case SYM_FUNC:
        free_type(sym->u.func.yield_type);
        break;
    default:
        break;
    }
    free_type(sym->type);
    xfree(sym->name);
    xfree(sym);
}

static void symtab_destroy_callback(intptr_t ptr)
{
    free_symbol((Symbol *)ptr);
}

static void static_locals_clear(void);

// Names whose declaration was dropped after an error.
static StringMap poisoned;

//
// Initialize the symbol table (create an empty table)
// Postcondition: Symbol table is empty and ready for use.
//
void symtab_init()
{
    // Empty.
}

//
// Destroy the symbol table (free all memory)
// Postcondition: All Symbol and Tac_StaticInit memory is freed, table is empty.
//
void symtab_destroy()
{
    static_locals_clear();
    map_destroy_free(&symtab, symtab_destroy_callback);
    map_destroy(&poisoned);
    str_id = 0;
    cl_id  = 0;
}

//
// Add an automatic (local) variable
// Precondition: name is a non-null string, t is a valid Type*.
// Postcondition: A Symbol with SYM_LOCAL, name, and linkage is added/replaced in symtab.
//
void symtab_add_automatic_var_linkage(const char *name, bool has_linkage, int level)
{
    Symbol *sym = new_symbol(name, NULL, SYM_LOCAL);

    sym->has_linkage = has_linkage;
    map_insert_free(&symtab, name, (intptr_t)sym, level, symtab_destroy_callback);
}

//
// Add an automatic (local) variable
// Precondition: name is a non-null string, t is a valid Type*.
// Postcondition: A Symbol with SYM_LOCAL, name, and t is added/replaced in symtab.
//
void symtab_add_automatic_var_type(const char *name, const Type *t, int level)
{
    const Symbol *const sym =
        new_symbol(name, clone_type(t, __func__, __FILE__, __LINE__), SYM_LOCAL);

    map_insert_free(&symtab, name, (intptr_t)sym, level, symtab_destroy_callback);
}

//
// Add a static variable
// Precondition: name is a non-null string, t is a valid Type*, init_list is valid if init_kind ==
// INIT_INITIALIZED, else NULL. Postcondition: A Symbol with SYM_STATIC, name, t, global, and init
// state is added/replaced in symtab.
//
void symtab_add_static_var(const char *name, const Type *t, bool global, InitKind init_kind,
                           Tac_StaticInit *init_list)
{
    Symbol *sym = new_symbol(name, clone_type(t, __func__, __FILE__, __LINE__), SYM_STATIC);
    sym->u.static_var.global    = global;
    sym->u.static_var.init_kind = init_kind;
    sym->u.static_var.init_list = init_list;

    map_insert_free(&symtab, name, (intptr_t)sym, 0, symtab_destroy_callback);
}

void symtab_add_static_var_scoped(const char *name, const Type *t, bool global, InitKind init_kind,
                                  Tac_StaticInit *init_list, int level)
{
    Symbol *sym = new_symbol(name, clone_type(t, __func__, __FILE__, __LINE__), SYM_STATIC);
    sym->u.static_var.global    = global;
    sym->u.static_var.init_kind = init_kind;
    sym->u.static_var.init_list = init_list;
    sym->block_scope            = true;

    map_insert_free(&symtab, name, (intptr_t)sym, level, symtab_destroy_callback);
}

char *symtab_add_compound_literal(const Type *t, Tac_StaticInit *init_list)
{
    char *name = xstruniq("_cl", &cl_id);
    symtab_add_static_var(name, t, false, INIT_INITIALIZED, init_list);
    symtab_get(name)->u.static_var.literal = true;
    return name;
}

//
// Block-scope static-local capture (see symtab.h).
//
static StaticLocalRec *static_locals_list;
static const char *static_locals_current_func;

void static_locals_set_function(const char *fn)
{
    static_locals_current_func = fn;
}

const char *static_locals_add(const char *source, const Type *type, Tac_StaticInit *init)
{
    // Count earlier statics with the same source name anywhere in this translation unit so a
    // repeat gets a unique backend name.  Both sibling-block repeats (same function) and
    // same-named statics in *different* functions must stay distinct: the Madlen dialect keeps
    // them apart by wrapping each function in its own module, but the Unix (b6as) dialect emits
    // one flat object where all labels share a namespace, so a plain source name would collide.
    // The first occurrence keeps the plain name; later ones get a `source$N` suffix.  `$` is
    // the canonical separator both emitters handle (Madlen `$`->`/`, Unix keeps `$`) and cannot
    // clash with a C-level global (C identifiers have no `$`).
    int count = 0;
    for (const StaticLocalRec *r = static_locals_list; r; r = r->next)
        if (strcmp(r->source, source) == 0)
            count++;

    char backend[256];
    if (count == 0)
        snprintf(backend, sizeof(backend), "%s", source);
    else
        snprintf(backend, sizeof(backend), "%s$%d", source, count);

    StaticLocalRec *rec = xalloc(sizeof(StaticLocalRec), __func__, __FILE__, __LINE__);
    rec->func           = xstrdup(static_locals_current_func);
    rec->source         = xstrdup(source);
    rec->name           = xstrdup(backend);
    rec->type           = type;
    rec->init_list      = init;
    rec->next           = static_locals_list;
    static_locals_list  = rec;
    return rec->name;
}

StaticLocalRec *static_locals_head(void)
{
    return static_locals_list;
}

static void static_locals_clear(void)
{
    StaticLocalRec *r = static_locals_list;
    while (r) {
        StaticLocalRec *next = r->next;
        xfree(r->func);
        xfree(r->source);
        xfree(r->name);
        tac_free_static_init(r->init_list); // NULL once the translator has transferred it
        xfree(r);
        r = next;
    }
    static_locals_list         = NULL;
    static_locals_current_func = NULL;
}

//
// Add a function
// Precondition: name is a non-null string, t is a valid Type* (function type).
// Postcondition: A Symbol with SYM_FUNC, name, t, global, defined, and noret is added/replaced in
// symtab.
//
void symtab_add_fun(const char *name, const Type *t, bool global, bool defined, bool noret)
{
    Symbol *sym         = new_symbol(name, clone_type(t, __func__, __FILE__, __LINE__), SYM_FUNC);
    sym->has_linkage    = true;
    sym->u.func.global  = global;
    sym->u.func.defined = defined;
    sym->u.func.noret   = noret;

    map_insert_free(&symtab, name, (intptr_t)sym, 0, symtab_destroy_callback);
}

void symtab_set_coro(const char *name, const Type *yield_type)
{
    Symbol *sym            = symtab_get(name);
    sym->u.func.coro       = true;
    sym->u.func.yield_type = clone_type(yield_type, __func__, __FILE__, __LINE__);
}

//
// Add a string literal
// Precondition: s is a non-null buffer of len decoded bytes.  The byte count is passed in
// rather than measured with strlen, because a decoded literal may hold embedded NUL bytes
// ("a\0c" is three bytes long).
// Postcondition: A Symbol with SYM_CONST, a unique name, type Array(Char, len+1), and string
// initializer is added. Returns: The unique name (owned by caller) for the string literal.
//
char *symtab_add_string(const char *s, size_t len)
{
    if (!s) {
        internal_error("symtab_add_string: NULL string input");
        return NULL; // cannot happen
    }

    char *name = xstruniq("_str", &str_id);

    // Create array type: char[len + 1]
    Type *t            = new_type(TYPE_ARRAY, __func__, __FILE__, __LINE__);
    t->u.array.element = new_type(TYPE_CHAR, __func__, __FILE__, __LINE__);
    t->u.array.size    = new_expression(EXPR_LITERAL);

    // Set array size
    t->u.array.size->u.literal            = new_literal(LITERAL_INT);
    t->u.array.size->u.literal->u.int_val = len + 1;

    // Create Tac_StaticInit
    Tac_StaticInit *init           = tac_new_static_init(TAC_STATIC_INIT_STRING);
    init->u.string.val             = xmemdup(s, len);
    init->u.string.len             = len;
    init->u.string.null_terminated = true;

    // Add to symbol table
    Symbol *sym       = new_symbol(name, t, SYM_CONST);
    sym->u.const_init = init;
    map_insert_free(&symtab, name, (intptr_t)sym, 0, symtab_destroy_callback);

    char *ret = xstrdup(name);
    xfree(name);
    return ret;
}

//
// Add an enum constant
// Precondition: name is a non-null string.
// Postcondition: A Symbol with SYM_ENUM, name, type int, and integer value is added.
//
void symtab_add_enum_const(const char *ident, int val, int level)
{
    Type *t         = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
    Symbol *sym     = new_symbol(ident, t, SYM_ENUM);
    sym->u.enum_val = val;
    map_insert_free(&symtab, ident, (intptr_t)sym, level, symtab_destroy_callback);
}

//
// Get a symbol by name (fails if not found)
// Precondition: name is a non-null string.
// Postcondition: Returns non-null Symbol* if found, else terminates with error.
//
Symbol *symtab_get(const char *name)
{
    Symbol *sym = symtab_get_opt(name);
    if (!sym) {
        if (map_get(&poisoned, name, NULL)) {
            diag_abandon(); // its declaration was reported
        }
        fatal_error("use of undeclared identifier '%s'", name);
    }
    return sym;
}

void symtab_poison(const char *name)
{
    if (name && !map_get(&poisoned, name, NULL)) {
        map_insert(&poisoned, name, 0, 0);
    }
}

//
// Get a symbol by name (returns NULL if not found)
// Precondition: name is a non-null string.
// Postcondition: Returns Symbol* if found, else NULL.
//
int symtab_level(const char *name)
{
    int level;
    return map_get_level(&symtab, name, &level) ? level : -1;
}

Symbol *symtab_get_opt(const char *name)
{
    intptr_t value = 0;
    if (!map_get(&symtab, name, &value)) {
        return NULL;
    }
    return (Symbol *)value;
}

//
// Check if a symbol has global linkage
// Precondition: name is a non-null string, exists in symtab.
// Postcondition: Returns true if the symbol is global (SYM_FUNC or SYM_STATIC with global=true),
// else false.
//
bool symtab_is_global(const char *name)
{
    Symbol *sym = symtab_get_opt(name);
    if (!sym) {
        return false;
    }
    switch (sym->kind) {
    case SYM_FUNC:
        return sym->u.func.global;
    case SYM_STATIC:
        return sym->u.static_var.global;
    default:
        return false;
    }
}

//
// Remove names from the tree, which exceed given level.
//
void symtab_purge(int level)
{
    map_remove_level_free(&symtab, level, symtab_destroy_callback);
}
