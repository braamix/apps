//
// Type-checking for declarations.
//
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantic.h"
#include "string_map.h"
#include "structtab.h"
#include "symtab.h"
#include "target.h"
#include "typecheck.h"
#include "typetab.h"
#include "xalloc.h"

const Type *typecheck_function;

//
// A name declared again where it is already declared: in the same scope a redefinition;
// in an inner one it would shadow the outer declaration, which this compiler does not allow.
//
static int param_scope_level = -1; // the scope of the parameters of the function checked

//
// Report an error about a declaration of name, then where name was declared before.
//
static _Noreturn void error_at_previous(const char *name, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    diag_vreport(diag_loc, "error", fmt, ap);
    va_end(ap);
    const Symbol *prev = symtab_get_opt(name);
    if (prev && prev->loc.line > 0)
        diag_note(prev->loc, "previous declaration of '%s' is here", name);
    diag_unwind();
}

static _Noreturn void redeclared(const char *name)
{
    // The parameters and the outermost block of the body share a scope (C11 §6.2.1p4).
    int level = symtab_level(name);
    if (level == param_scope_level && scope_level == param_scope_level + 1)
        level = scope_level;
    if (level < scope_level)
        error_at_previous(name, "declaration of '%s' shadows an earlier one, which is not allowed",
                          name);
    error_at_previous(name, "redefinition of '%s'", name);
}

// A bit-field, for a message: "bit-field 'x'", or "unnamed bit-field".
static const char *bitfield_desc(const Field *f)
{
    static char buf[300];
    if (!f->u.member.name)
        return "unnamed bit-field";
    snprintf(buf, sizeof(buf), "bit-field '%s'", f->u.member.name);
    return buf;
}

static bool is_extern(const DeclSpec *spec)
{
    return spec && (spec->storage == STORAGE_CLASS_EXTERN);
}

static bool is_static(const DeclSpec *spec)
{
    return spec && (spec->storage == STORAGE_CLASS_STATIC);
}

static bool is_noreturn(const DeclSpec *spec)
{
    for (const FunctionSpec *fs = spec ? spec->func_specs : NULL; fs; fs = fs->next) {
        if (fs->kind == FUNC_SPEC_NORETURN) {
            return true;
        }
    }
    return false;
}

// An alignment the target can give: a power of two, or a multiple of its strictest
// scalar alignment (BESM-6 aligns to its 6-byte word).
// The automatic locals of the function being checked.  Shadowing is forbidden, but
// sibling blocks may each declare a name, and the translator puts all of a function's
// locals in one namespace (`%name`).  A repeat of the same type shares the earlier
// name, its lifetime being disjoint; a repeat of another type gets the backend name
// `name$N`, as a static local does, and references are rewritten to it like theirs.
typedef struct AutoLocal {
    struct AutoLocal *next;
    char *source; // source name (owned)
    char *name;   // backend name (owned)
    Type *type;   // owned; NULL for an array sized by its initializer
} AutoLocal;

static AutoLocal *auto_locals;

static void auto_locals_clear(void)
{
    while (auto_locals) {
        AutoLocal *next = auto_locals->next;
        xfree(auto_locals->source);
        xfree(auto_locals->name);
        if (auto_locals->type)
            free_type(auto_locals->type);
        xfree(auto_locals);
        auto_locals = next;
    }
}

void declarations_recover(void)
{
    typecheck_function = NULL;
    param_scope_level  = -1;
    auto_locals_clear();
}

// The backend name of automatic local `source` of type `type` (NULL when its size
// comes from the initializer, so it matches no earlier one).
static const char *auto_local_name(const char *source, const Type *type)
{
    int count = 0;
    for (const AutoLocal *a = auto_locals; a; a = a->next) {
        if (strcmp(a->source, source) != 0)
            continue;
        if (type && a->type && compare_type(type, a->type))
            return a->name;
        count++;
    }
    char name[256];
    if (count == 0)
        snprintf(name, sizeof(name), "%s", source);
    else
        snprintf(name, sizeof(name), "%s$%d", source, count);
    AutoLocal *a = xalloc(sizeof(AutoLocal), __func__, __FILE__, __LINE__);
    a->source    = xstrdup(source);
    a->name      = xstrdup(name);
    a->type      = type ? clone_type(type, __func__, __FILE__, __LINE__) : NULL;
    a->next      = auto_locals;
    auto_locals  = a;
    return a->name;
}

static bool valid_alignment(long a)
{
    size_t max            = target_config->ldouble_align;
    const size_t aligns[] = { target_config->int_align, target_config->long_align,
                              target_config->llong_align, target_config->double_align,
                              target_config->pointer_align };
    for (size_t i = 0; i < sizeof(aligns) / sizeof(aligns[0]); i++)
        if (aligns[i] > max)
            max = aligns[i];
    return a > 0 && ((a & (a - 1)) == 0 || a % (long)max == 0);
}

// The value of an _Alignas, in bytes: 0 for none or _Alignas(0).  The specifier is
// left as an integer literal for alignas_bytes(): typedef names in it may be out of
// scope by the time the translator reads it.
static int alignment_spec_value(AlignmentSpec *as)
{
    if (!as)
        return 0;
    long a;
    if (as->kind == ALIGN_SPEC_EXPR && as->u.expr->kind == EXPR_LITERAL &&
        as->u.expr->u.literal->kind == LITERAL_INT) {
        a = (long)as->u.expr->u.literal->u.int_val;
        if (a != 0 && !valid_alignment(a))
            fatal_error("invalid alignment %ld in '_Alignas'", a);
        return (int)a;
    } else if (as->kind == ALIGN_SPEC_TYPE) {
        as->u.type = resolve_typedef_names(as->u.type);
        validate_type(as->u.type);
        if (!is_complete(as->u.type) || unalias(as->u.type)->kind == TYPE_FUNCTION)
            fatal_error("'_Alignas' of incomplete or function type '%s'", type_to_c(as->u.type));
        a = (long)get_alignment(as->u.type);
        free_type(as->u.type);
    } else {
        as->u.expr = typecheck_and_decay(as->u.expr);
        if (!is_integer(as->u.expr->type) || !try_eval_const_int(as->u.expr, &a))
            fatal_error("'_Alignas' requires an integer constant expression");
        free_expression(as->u.expr);
    }
    if (a != 0 && !valid_alignment(a))
        fatal_error("invalid alignment %ld in '_Alignas'", a);
    as->kind                         = ALIGN_SPEC_EXPR;
    as->u.expr                       = new_expression(EXPR_LITERAL);
    as->u.expr->u.literal            = new_literal(LITERAL_INT);
    as->u.expr->u.literal->u.int_val = a;
    return (int)a;
}

static int alignas_value(DeclSpec *spec)
{
    return spec ? alignment_spec_value(spec->align_spec) : 0;
}

int alignas_bytes(const DeclSpec *spec)
{
    if (!spec || !spec->align_spec || spec->align_spec->kind != ALIGN_SPEC_EXPR)
        return 0;
    const Expr *e = spec->align_spec->u.expr;
    return e->kind == EXPR_LITERAL && e->u.literal->kind == LITERAL_INT
               ? (int)e->u.literal->u.int_val
               : 0;
}

// The alignment a declaration asks of its variable of type `t` beyond the type's own,
// or 0 (C11 §6.7.5).
static int declared_alignment(DeclSpec *spec, const Type *t)
{
    int a = alignas_value(spec);
    if (a == 0 || !is_complete(t))
        return 0;
    int natural = (int)get_alignment(t);
    if (a < natural)
        fatal_error("'_Alignas(%d)' is less strict than the alignment of '%s'", a, type_to_c(t));
    return a > natural ? a : 0;
}

// Raise the recorded alignment of static variable `name` to `a`.
static void note_alignment(const char *name, int a)
{
    Symbol *sym = symtab_get(name);
    if (sym->kind == SYM_STATIC && a > sym->u.static_var.alignment)
        sym->u.static_var.alignment = a;
}

// _Alignas is not allowed on a typedef, a function, or a register variable.
static void reject_alignas(const DeclSpec *spec, const char *what)
{
    if (spec && spec->align_spec)
        fatal_error("'_Alignas' cannot be applied to %s", what);
}

// Reject two parameters with the same name in a function declaration or
// definition (C11 §6.7.6.3p9, §6.9.1p6).  Param lists are short, so a simple
// O(n^2) name comparison is fine.  The f(void) sentinel has no name and so is
// harmless here.
static void check_duplicate_params(const Type *fn_type)
{
    for (const Param *a = fn_type->u.function.params; a; a = a->next) {
        if (!a->name)
            continue;
        for (const Param *b = a->next; b; b = b->next) {
            if (b->name && strcmp(a->name, b->name) == 0) {
                diag_enter(b->loc);
                fatal_error("redefinition of parameter '%s'", a->name);
            }
        }
    }
}

// Register a function declaration (a prototype, not a definition) that arrives
// as a DECL_VAR declarator with function type.  Used for both file-scope
// prototypes ("int f(int);") and block-scope (local) function declarations.
// Functions have external linkage, so symtab_add_fun stores the symbol at file
// scope (level 0) regardless of where the declaration textually appears; this
// lets a later definition or sibling-scope declaration find it and so detects
// redefinitions and conflicting declarations across function bodies.
// Adjust a function type's parameters in place per C11 §6.7.6.3:
//  - a sole unnamed `void` param means "no parameters";
//  - an array param decays to a pointer to its element type
//    (int a[5] -> int *a, int a[2][3] -> int (*a)[3]);
//  - any other void param is an error.
static void adjust_function_params(Type *fn_type)
{
    Param *p = fn_type->u.function.params;
    if (p && !p->next && unalias(p->type)->kind == TYPE_VOID && !p->name) {
        free_param(p);
        fn_type->u.function.params = NULL;
        return;
    }
    for (; p; p = p->next) {
        reject_alignas(p->specifiers, "a parameter");
        const Type *pt = unalias(p->type);
        if (pt->kind == TYPE_ARRAY) {
            Type *ptr             = new_type(TYPE_POINTER, __func__, __FILE__, __LINE__);
            ptr->u.pointer.target = clone_type(pt->u.array.element, __func__, __FILE__, __LINE__);
            // pt aliases p->type and its element is already cloned, so free the
            // original (cloned) array type before replacing it with the pointer.
            free_type(p->type);
            p->type = ptr;
        } else if (pt->kind == TYPE_VOID) {
            fatal_error("'void' must be the only parameter");
        }
    }
}

static void register_function_declaration(InitDeclarator *decl, const DeclSpec *specifiers)
{
    const Type *var_type = unalias(decl->type); // may be a typedef'd function type
    if (decl->init) {
        fatal_error("function '%s' cannot have an initializer", decl->name);
    }
    validate_type(var_type);
    // A block-scope function declaration cannot specify a storage class other
    // than extern (C11 §6.7.1p7): "static int foo(void);" inside a body is
    // illegal, though it is legal at file scope (internal linkage).
    if (is_static(specifiers) && scope_level > 0) {
        fatal_error("function '%s' declared in block scope cannot be 'static'", decl->name);
    }
    bool global       = !is_static(specifiers);
    const Type *yield = check_coroutine_decl(decl->name, specifiers, var_type);

    // Strip the void sentinel and decay array params to pointers — same
    // normalization as in typecheck_fn_decl so prototypes and definitions store
    // the same parameter types (and call-site argument-count checks see zero
    // params for f(void)).
    Type *adj = clone_type(var_type, __func__, __FILE__, __LINE__);
    adjust_function_params(adj);
    check_duplicate_params(adj);

    Symbol *existing = symtab_get_opt(decl->name);
    if (existing) {
        if (existing->kind != SYM_FUNC) {
            // A function declaration clashes with a non-function of the same
            // name (e.g. a variable) in scope.
            redeclared(decl->name);
        }
        if (!compatible_type(existing->type, adj)) {
            error_at_previous(decl->name, "conflicting types for '%s'", decl->name);
        }
        agree_coroutine(existing, yield, decl->name);
    }
    bool defined   = existing && existing->kind == SYM_FUNC && existing->u.func.defined;
    bool fn_global = (existing && existing->kind == SYM_FUNC) ? existing->u.func.global : global;
    // _Noreturn is sticky across declarations (C11 §6.7.4): once any declaration
    // marks the function _Noreturn, it stays noreturn.
    bool noret = is_noreturn(specifiers) ||
                 (existing && existing->kind == SYM_FUNC && existing->u.func.noret);
    symtab_add_fun(decl->name, adj, fn_global, defined, noret);
    if (yield)
        symtab_set_coro(decl->name, yield);
    free_type(decl->type);
    decl->type = adj; // normalized type now owned by AST
}

// Fail the compilation unless a _Static_assert condition folds to a nonzero constant.
static void check_static_assert(const Expr *condition, const char *message)
{
    long val;
    if (!try_eval_const_int(condition, &val))
        fatal_error("'_Static_assert' condition is not a constant expression");
    if (!val) {
        if (message)
            fatal_error("static assertion failed: %s", message);
        else
            fatal_error("static assertion failed");
    }
}

// Type-check a _Static_assert declaration and evaluate it.  The typechecked condition
// replaces the original: eval_const() reads the type that typecheck fills in (a sizeof
// operand's type, an enum constant's symbol), so the folded expression must be this one.
static void typecheck_static_assert_decl(Declaration *d)
{
    d->u.static_assrt.condition = typecheck_and_decay(d->u.static_assrt.condition);
    if (!is_scalar(d->u.static_assrt.condition->type)) {
        fatal_error("'_Static_assert' condition must have scalar type");
    }
    check_static_assert(d->u.static_assrt.condition, d->u.static_assrt.message);
}

// Validate struct members for uniqueness and completeness.
static void validate_struct_definition(const char *tag, const Field *members)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }

    // Check for duplicate member names.
    StringMap names;
    map_init(&names);
    for (const Field *m = members; m; m = m->next) {
        if (m->kind == FIELD_STATIC_ASSERT) {
            check_static_assert(m->u.static_assrt.condition, m->u.static_assrt.message);
            continue;
        }
        if (unalias(m->u.member.type)->kind == TYPE_FUNCTION) {
            fatal_error("member '%s' has function type '%s'",
                        m->u.member.name ? m->u.member.name : "", type_to_c(m->u.member.type));
        }
        if (!is_complete(m->u.member.type)) {
            fatal_error("member '%s' has incomplete type '%s'",
                        m->u.member.name ? m->u.member.name : "", type_to_c(m->u.member.type));
        }
        if (m->u.member.name && map_get(&names, m->u.member.name, NULL)) {
            fatal_error("duplicate member '%s'", m->u.member.name);
        }
        if (m->u.member.name)
            map_insert(&names, m->u.member.name, 0, 0);
        validate_type(m->u.member.type);
    }
    map_destroy(&names);
}

// Register enum constants for an enum type definition.
static void register_enum_constants(const Type *enum_type)
{
    long next_val = 0;
    for (const Enumerator *e = enum_type->u.enum_t.enumerators; e; e = e->next) {
        long val;
        if (e->value) {
            if (!try_eval_const_int(e->value, &val))
                fatal_error("value of enumerator '%s' is not a constant expression", e->name);
        } else {
            val = next_val;
        }
        next_val = val + 1;
        // C11 §6.7.2.2p2: the value is representable as an int.  One that fits only
        // unsigned int wraps, as gcc and clang accept it.
        int bits      = target_config->int_bits;
        uint64_t umax = unsigned_narrow(~(uint64_t)0, (int)target_config->int_size * 8);
        if (bits < 64 && (val < 0 ? val < -(1L << (bits - 1)) : (uint64_t)val > umax))
            fatal_error("value %ld of enumerator '%s' does not fit in 'int'", val, e->name);
        symtab_add_enum_const(e->name, (int)sign_narrow((uint64_t)val, bits), scope_level);
    }
}

// Reject a struct/union tag reference whose keyword disagrees with an existing tag of the
// same name (C11 §6.7.2.3): e.g. using `union x` where `struct x` is already in scope.
void check_tag_kind(const Type *t)
{
    if ((t->kind != TYPE_STRUCT && t->kind != TYPE_UNION) || !t->u.struct_t.name)
        return;
    const StructDef *existing = structtab_find_opt(t->u.struct_t.name);
    if (existing && existing->kind != t->kind) {
        fatal_error("'%s' was declared as a different kind of tag", t->u.struct_t.name);
    }
}

// The width of bit-field `f`, a constant expression checked against its type
// (C11 §6.7.2.1p4-5).  Plain int is signed, as with GCC and clang, and every integer
// type is accepted, as they do.
static int bitfield_width(const Field *f)
{
    const Type *t = unalias(f->u.member.type);
    if (!is_integer(t))
        fatal_error("%s has non-integer type '%s'", bitfield_desc(f), type_to_c(t));
    long width;
    if (!try_eval_const_int(f->u.member.bitfield, &width))
        fatal_error("width of %s is not an integer constant expression", bitfield_desc(f));
    if (width < 0)
        fatal_error("%s has negative width", bitfield_desc(f));
    if (width > integer_value_bits(t))
        fatal_error("width of %s exceeds its type '%s'", bitfield_desc(f), type_to_c(t));
    if (width == 0 && f->u.member.name)
        fatal_error("named %s has zero width", bitfield_desc(f));
    return (int)width;
}

// Place bit-field `f` at or after bit `start` by the target's rule (Target.bitfield_layout),
// raising `*alignment`, the struct's, as that rule says.  Returns the bit it starts at; a
// zero-width one only moves the start to its boundary.
static int layout_bitfield(const Field *f, int start, int *alignment)
{
    BitfieldLayout rule = target_config ? target_config->bitfield_layout : BITFIELD_SYSV;
    int width           = bitfield_width(f);
    int type_align      = (int)get_alignment(f->u.member.type);
    int type_bits       = (int)get_size(f->u.member.type) * 8;
    int raise           = 0; // the alignment the struct takes from this field
    if (rule == BITFIELD_PACKED) {
        if (width == 0) {
            start = round_away_from_zero(64, start);
            raise = 8;
        }
    } else {
        if (width == 0 || (start % (type_align * 8)) + width > type_bits)
            start = round_away_from_zero(type_align * 8, start);
        if (f->u.member.name || rule == BITFIELD_AAPCS)
            raise = type_align;
    }
    if (raise > *alignment)
        *alignment = raise;
    return start;
}

// Group the bit-fields of `members` and `unnamed` (each in declaration order, by `index`),
// whose offsets still hold the bit each starts at, into clang's access units
// (Target.bitfield_access_bits): a run of bit-fields, named or not, ends at another
// member or a `:0`; a field that starts inside a byte joins the unit before it, one on a
// byte boundary only while the unit stays within `max_bits`.
static void access_units(FieldDef *members, FieldDef *unnamed, int max_bits)
{
    int count = 0;
    for (const FieldDef *m = members; m; m = m->next)
        count++;
    for (const FieldDef *m = unnamed; m; m = m->next)
        count++;
    FieldDef **field = xalloc((count + 1) * sizeof(*field), __func__, __FILE__, __LINE__);
    FieldDef *a = members, *b = unnamed;
    for (int i = 0; i < count; i++) {
        if (a && (!b || a->index < b->index)) {
            field[i] = a;
            a        = a->next;
        } else {
            field[i] = b;
            b        = b->next;
        }
    }
    int first = -1;     // of the current unit
    int lo = 0, hi = 0; // its bytes
    for (int i = 0;; i++) {
        FieldDef *m = i < count ? field[i] : NULL;
        bool join   = false;
        if (m && m->bf.width && first >= 0) {
            int end = (m->offset + m->bf.width + 7) / 8;
            join    = m->offset % 8 != 0 || (end - lo) * 8 <= max_bits;
        }
        if (!join && first >= 0) {
            for (int j = first; j < i; j++) {
                field[j]->access_offset = lo;
                field[j]->access_size   = hi - lo;
            }
            first = -1;
        }
        if (!m)
            break;
        if (!m->bf.width)
            continue;
        if (first < 0) {
            first = i;
            lo    = m->offset / 8;
        }
        hi = (m->offset + m->bf.width + 7) / 8;
    }
    xfree(field);
}

// Choose the storage unit of bit-field `m`, whose offset still holds the bit it starts at,
// in a struct of `size` bytes aligned to `alignment`: the smallest unsigned integer that
// covers its bits, lies inside the struct and is aligned there, else its bytes one by
// one.  A word-addressed target loads whole words.
static void place_bitfield(FieldDef *m, int size, int alignment)
{
    int start = m->offset;
    int width = m->bf.width;
    int first = start / 8;               // first byte holding the field
    int end   = (start + width + 7) / 8; // past its last byte
    int base  = first;
    int unit  = end - first;
    bool fits = false;
    if (target_word_addressed()) {
        int word = (int)target_config->int_size;
        base     = first / word * word;
        unit     = word;
        if (end > base + word)
            fatal_error("bit-field '%s' straddles a word", m->name);
        fits = true;
    }
    for (int sz = 1; !fits && sz <= 8; sz *= 2) {
        TypeKind k = unsigned_kind_of_size(sz);
        if (sz < end - first || k == TYPE_VOID)
            continue;
        Type ut   = { .kind = k };
        int align = (int)get_alignment(&ut);
        if (align > alignment)
            continue;
        // The lowest aligned start that still covers the last byte, kept inside the struct.
        int b = first / align * align;
        if (b + sz > size)
            b = (size - sz) / align * align;
        if (b >= 0 && b <= first && b + sz >= end) {
            base = b;
            unit = sz;
            fits = true;
        }
    }
    int bit         = start - base * 8; // from the unit's first byte in memory
    m->offset       = base;
    m->bf.unit_size = unit;
    m->bf.bytewise  = !fits;
    m->bf.pos       = target_config && target_config->big_endian ? unit * 8 - bit - width : bit;
}

// Register a struct/union type definition in the struct table.
// Precondition: t is TYPE_STRUCT or TYPE_UNION with non-NULL fields and a non-NULL tag
// (synthetic for an anonymous definition), not yet in structtab.
static void register_struct_type(const Type *t)
{
    // Every anonymous struct/union definition is tagged by the parser at the single point where
    // its Type node is built (fuse_type_specifiers), *before* the base type is cloned per
    // declarator -- that is what makes `struct { int x; } a, b;` one type instead of two.
    // Minting the tag here instead would mint one per cloned node.  A definition reaching here
    // untagged means the AST did not come from that path (e.g. a stale .ast stream).
    if (!t->u.struct_t.name) {
        internal_error("untagged struct/union definition reached the type registrar");
    }
    // Resolve typedef names in field types in-place, and type the bit-field widths, which
    // eval_const reads.
    for (Field *f = (Field *)t->u.struct_t.fields; f; f = f->next) {
        if (f->kind == FIELD_STATIC_ASSERT)
            continue;
        f->u.member.type = resolve_typedef_names(f->u.member.type);
        if (f->u.member.bitfield)
            f->u.member.bitfield = typecheck_and_decay(f->u.member.bitfield);
    }
    // Pre-register inline struct/union defs in member types so is_complete() finds them.
    for (const Field *f = t->u.struct_t.fields; f; f = f->next) {
        if (f->kind != FIELD_STATIC_ASSERT)
            register_inline_struct_defs(f->u.member.type);
    }
    TypeKind kind = t->kind;
    // A tag may already be present as a forward declaration (incomplete, same kind — which we
    // now complete) or as a clash: a second full definition, or a different keyword.
    const StructDef *existing = structtab_find_opt(t->u.struct_t.name);
    if (existing) {
        if (existing->complete) {
            diag_error(diag_loc, "redefinition of '%s'", type_to_c(t));
            if (existing->loc.line > 0)
                diag_note(existing->loc, "previous definition of '%s' is here", type_to_c(t));
            diag_unwind();
        }
        if (existing->kind != kind)
            fatal_error("'%s' was declared as a different kind of tag", t->u.struct_t.name);
    }
    validate_struct_definition(t->u.struct_t.name, t->u.struct_t.fields);
    FieldDef *members = NULL;
    FieldDef **tail   = &members;
    // The layout advances in bits, for the bit-fields; every other member starts on a
    // byte, at the next multiple of its alignment.
    int current_bits = 0;
    // The unnamed bit-fields, for a target whose ABI sees them (Target.bitfield_unit_per_field,
    // Target.bitfield_access_bits).
    bool keep_unnamed       = target_config && (target_config->bitfield_unit_per_field ||
                                                target_config->bitfield_access_bits);
    FieldDef *unnamed       = NULL;
    FieldDef **unnamed_tail = &unnamed;
    int declared            = 0; // fields declared so far
    // On a word-addressed target every aggregate is padded to at least one machine word
    // (Target.aggregate_align) so array element strides stay word multiples and &arr[i]
    // never lands mid-word; byte-addressed targets use natural C packing (align 1).
    int current_alignment = target_config ? (int)target_config->aggregate_align : 1;
    if (current_alignment < 1)
        current_alignment = 1;
    for (const Field *f = t->u.struct_t.fields; f; f = f->next) {
        if (f->kind == FIELD_STATIC_ASSERT)
            continue; /* already evaluated in validate_struct_definition */
        declared++;
        int member_alignment = get_alignment(f->u.member.type);
        int alignas          = alignment_spec_value(f->u.member.align_spec);
        if (alignas && f->u.member.bitfield)
            fatal_error("'_Alignas' cannot be applied to a bit-field");
        if (alignas && alignas < member_alignment)
            fatal_error("'_Alignas(%d)' is less strict than the alignment of '%s'", alignas,
                        type_to_c(f->u.member.type));
        if (alignas > member_alignment)
            member_alignment = alignas;
        if (f->u.member.bitfield) {
            int start =
                layout_bitfield(f, kind == TYPE_STRUCT ? current_bits : 0, &current_alignment);
            int width = bitfield_width(f);
            if (f->u.member.name) {
                // The storage unit is chosen once the struct's size is known.
                *tail =
                    new_member(f->u.member.name,
                               clone_type(f->u.member.type, __func__, __FILE__, __LINE__), start);
                (*tail)->bf.width = width;
                (*tail)->index    = declared;
                tail              = &(*tail)->next;
            } else if (keep_unnamed) {
                *unnamed_tail =
                    new_member(NULL, clone_type(f->u.member.type, __func__, __FILE__, __LINE__),
                               width ? start : start / 8);
                (*unnamed_tail)->bf.width = width;
                (*unnamed_tail)->index    = declared;
                unnamed_tail              = &(*unnamed_tail)->next;
            }
            if (kind == TYPE_STRUCT || start + width > current_bits)
                current_bits = start + width;
            continue;
        }
        int offset = 0;
        if (kind == TYPE_STRUCT)
            offset = round_away_from_zero(member_alignment, (current_bits + 7) / 8);
        *tail = new_member(f->u.member.name,
                           clone_type(f->u.member.type, __func__, __FILE__, __LINE__), offset);
        (*tail)->index = declared;
        tail           = &(*tail)->next;
        current_alignment =
            current_alignment > member_alignment ? current_alignment : member_alignment;
        // A struct grows past each member in turn; a union's size is the size of
        // its largest member (every member is at offset 0), so take the maximum.
        int member_end = (offset + (int)get_size(f->u.member.type)) * 8;
        if (kind == TYPE_STRUCT || member_end > current_bits)
            current_bits = member_end;
    }
    int size = round_away_from_zero(current_alignment, (current_bits + 7) / 8);
    if (target_config && target_config->bitfield_access_bits)
        access_units(members, unnamed, target_config->bitfield_access_bits);
    for (FieldDef *m = members; m; m = m->next) {
        if (m->bf.width)
            place_bitfield(m, size, current_alignment);
    }
    for (FieldDef *m = unnamed; m; m = m->next) {
        if (m->bf.width)
            place_bitfield(m, size, current_alignment);
    }
    structtab_add_struct(t->u.struct_t.name, kind, true, current_alignment, size, members,
                         scope_level);
    structtab_find(t->u.struct_t.name)->unnamed = unnamed;
}

// Register any inline struct/union definitions embedded in a type tree.
// Must be called before validate_type() so is_complete() finds them in structtab.
void register_inline_struct_defs(const Type *t)
{
    if (!t)
        return;
    switch (t->kind) {
    case TYPE_STRUCT:
    case TYPE_UNION: {
        // Register an inline definition when the tag is absent or only forward-declared.
        const StructDef *e = structtab_find_opt(t->u.struct_t.name);
        if (t->u.struct_t.fields && (!e || !e->complete))
            register_struct_type(t);
        break;
    }
    case TYPE_ENUM:
        // An enum defined in a typedef, a variable or a member declaration
        // (`typedef enum { A, B } K;`) declares its constants too.  Every declarator
        // carries a clone of the list, and registering it again only rewrites the same
        // values: the parser has already rejected a constant declared twice.
        if (t->u.enum_t.enumerators)
            register_enum_constants(t);
        break;
    case TYPE_ARRAY:
        register_inline_struct_defs(t->u.array.element);
        break;
    case TYPE_POINTER:
        register_inline_struct_defs(t->u.pointer.target);
        break;
    case TYPE_FUNCTION:
        register_inline_struct_defs(t->u.function.return_type);
        for (const Param *p = t->u.function.params; p; p = p->next)
            register_inline_struct_defs(p->type);
        break;
    default:
        break;
    }
}

// Type-check a struct/union/enum tag declaration.
static void typecheck_tag_decl(const Declaration *d)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    if (!d->u.empty.type)
        return;
    TypeKind kind = d->u.empty.type->kind;
    if (kind == TYPE_ENUM) {
        if (d->u.empty.type->u.enum_t.enumerators)
            register_enum_constants(d->u.empty.type);
        return;
    }
    if (kind != TYPE_STRUCT && kind != TYPE_UNION)
        return;
    const Type *t = d->u.empty.type;
    if (!t->u.struct_t.fields) {
        // Forward declaration or tag reference — not a definition. Record the kind so a later
        // conflicting use (`struct x; union x;`) is caught, but keep the tag incomplete.
        check_tag_kind(t);
        if (t->u.struct_t.name && !structtab_exists(t->u.struct_t.name))
            structtab_add_struct(t->u.struct_t.name, kind, false, 0, 0, NULL, scope_level);
        return;
    }
    register_struct_type(t);
}

// Register a typedef name.  C11 §6.7p3: a typedef may be repeated in its scope with the
// same type; elsewhere the name is taken, since no name may shadow another.
static void add_typedef(const char *name, const Type *type)
{
    if (typetab_exists(name)) {
        const TypeDef *old = typetab_find(name);
        if (old->level != scope_level || !compatible_type(old->type, type))
            fatal_error("redefinition of typedef '%s'", name);
        return;
    }
    typetab_add(name, type, scope_level);
}

// Type-check a local variable declaration.
static void typecheck_local_var_decl(const Declaration *d)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    if (d->u.var.specifiers && d->u.var.specifiers->storage == STORAGE_CLASS_TYPEDEF) {
        reject_alignas(d->u.var.specifiers, "a typedef");
        for (InitDeclarator *decl = d->u.var.declarators; decl; decl = decl->next) {
            diag_enter(decl->loc); // the declaration restores it
            reject_coro_spec(d->u.var.specifiers, decl->name);
            decl->type = resolve_typedef_names(decl->type);
            register_inline_struct_defs(decl->type);
            validate_type(decl->type);
            add_typedef(decl->name, decl->type);
        }
        return;
    }
    for (InitDeclarator *decl = d->u.var.declarators; decl; decl = decl->next) {
        diag_enter(decl->loc); // the declaration restores it
        decl->type     = resolve_typedef_names(decl->type);
        Type *var_type = decl->type;
        // A block-scope function declaration ("int f(int);" inside a body) has
        // external linkage; register it like a file-scope prototype so later
        // definitions and sibling-scope declarations resolve against it.
        if (unalias(var_type)->kind == TYPE_FUNCTION) {
            reject_alignas(d->u.var.specifiers, "a function");
            register_function_declaration(decl, d->u.var.specifiers);
            continue;
        }
        reject_coro_spec(d->u.var.specifiers, decl->name);
        if (unalias(var_type)->kind == TYPE_VOID) {
            fatal_error("variable '%s' has incomplete type 'void'", decl->name);
        }
        register_inline_struct_defs(var_type);
        validate_type(var_type);
        if (is_extern(d->u.var.specifiers)) {
            if (decl->init) {
                fatal_error("'extern' variable '%s' cannot have an initializer", decl->name);
            }
            const Symbol *existing = symtab_get_opt(decl->name);
            // A block-scope extern declaration may link to a prior declaration
            // with internal or external linkage, but not to one with no linkage
            // at all (C11 §6.7p3).  A SYM_LOCAL is an automatic local or a
            // parameter, and a block-scope static has static storage but no
            // linkage — an extern following either conflicts.  (A file-scope
            // static has internal linkage, so an extern may follow it.)
            if (existing && (existing->kind == SYM_LOCAL ||
                             (existing->kind == SYM_STATIC && existing->block_scope &&
                              !existing->u.static_var.global))) {
                error_at_previous(decl->name, "'%s' is declared both with and without linkage",
                                  decl->name);
            }
            if (existing && unalias(existing->type)->kind != unalias(var_type)->kind) {
                error_at_previous(decl->name, "conflicting types for '%s'", decl->name);
            }
            if (!existing) {
                // Scope the synthesized symbol to this block so its identifier does
                // not leak past the block (C11 §6.2.1); it is purged on block exit.
                symtab_add_static_var_scoped(decl->name, var_type, true, INIT_NONE, NULL,
                                             scope_level);
            }
            note_alignment(decl->name, declared_alignment(d->u.var.specifiers, var_type));
            continue;
        }
        if (!is_complete(var_type)) {
            fatal_error("variable '%s' has incomplete type '%s'", decl->name, type_to_c(var_type));
        }
        if (is_static(d->u.var.specifiers)) {
            // A static local has no linkage, so it cannot share a scope with
            // another declaration of the same name (C11 §6.7p3) — e.g.
            // "int x = 1; static int x;".
            if (symtab_get_opt(decl->name)) {
                redeclared(decl->name);
            }
            Tac_StaticInit *static_init = build_static_init(var_type, &decl->init);
            // The storage is emitted inside the owning function's module as a module-local
            // label.  Capture it (the symbol is scoped and gets purged on block exit) and give
            // it a backend name unique within this function; the symbol is keyed by the source
            // name so in-scope references still resolve, while its display name carries the
            // (possibly suffixed) backend name that typecheck_var propagates to references.
            const char *backend             = static_locals_add(decl->name, var_type, static_init);
            static_locals_head()->alignment = declared_alignment(d->u.var.specifiers, var_type);
            symtab_add_static_var_scoped(decl->name, var_type, false, INIT_INITIALIZED, NULL,
                                         scope_level);
            Symbol *sym = symtab_get(decl->name);
            xfree(sym->name);
            sym->name = xstrdup(backend);
            // Carry the backend (possibly $N-suffixed) name on the declarator itself so the
            // translator's array-decay record (stmt.c) matches the backend name that
            // references already use; otherwise a suffixed static-local array never decays
            // to its address.  Done after the symtab_add/symtab_get calls above, which key
            // on the source name.
            if (strcmp(decl->name, backend) != 0) {
                xfree(decl->name);
                decl->name = xstrdup(backend);
            }
            // Drop initializer
            free_initializer(decl->init);
            decl->init = NULL;
            continue;
        }
        if (d->u.var.specifiers && d->u.var.specifiers->storage == STORAGE_CLASS_REGISTER)
            reject_alignas(d->u.var.specifiers, "a register variable");
        declared_alignment(d->u.var.specifiers, var_type); // the translator reads it
        const Symbol *dup = symtab_get_opt(decl->name);
        if (dup && (!dup->has_linkage || dup->kind == SYM_FUNC)) {
            // A no-linkage variable clashing with another no-linkage variable
            // (shadowing, forbidden by design) or with a function of the same
            // name in scope (external vs no linkage, C11 §6.7p3).
            redeclared(decl->name);
        }
        bool unsized = unalias(var_type)->kind == TYPE_ARRAY && !unalias(var_type)->u.array.size;
        symtab_add_automatic_var_type(decl->name, var_type, scope_level);
        Symbol *sym         = symtab_get(decl->name);
        const char *backend = auto_local_name(decl->name, unsized ? NULL : var_type);
        bool renamed        = strcmp(decl->name, backend) != 0;
        if (renamed) {
            // Set before the initializer, which may refer to the variable itself.
            xfree(sym->name);
            sym->name = xstrdup(backend);
        }
        decl->init = typecheck_init(var_type, decl->init);
        if (decl->init && decl->init->kind == INITIALIZER_SINGLE)
            coro_lint_bind(sym->name, scope_level, decl->init->u.expr);
        if (unsized) {
            // The initializer gave the array its length; refresh the symbol's copy.
            free_type(sym->type);
            sym->type = clone_type(var_type, __func__, __FILE__, __LINE__);
        }
        if (renamed) {
            xfree(decl->name);
            decl->name = xstrdup(backend);
        }
    }
}

// --- Missing-return analysis (helpers for typecheck_fn_decl) ----------------

// True if `cond` is a compile-time integer constant that evaluates to non-zero,
// i.e. an always-true guard such as `while (1)`.
static bool is_const_true(const Expr *cond)
{
    long v;
    return cond && try_eval_const_int(cond, &v) && v != 0;
}

// True if `cond` is a compile-time integer constant that evaluates to zero.
static bool is_const_false(const Expr *cond)
{
    long v;
    return cond && try_eval_const_int(cond, &v) && v == 0;
}

// True if `s` contains a `break` that targets the loop/switch we are analysing.
// A `break` inside a *nested* loop or switch binds to that inner construct, so we
// do not descend into STMT_WHILE / STMT_DO_WHILE / STMT_FOR / STMT_SWITCH.
static bool contains_own_break(const Stmt *s)
{
    if (!s) {
        return false;
    }
    switch (s->kind) {
    case STMT_BREAK:
        return true;
    case STMT_IF:
        return contains_own_break(s->u.if_stmt.then_stmt) ||
               contains_own_break(s->u.if_stmt.else_stmt);
    case STMT_COMPOUND:
        for (const DeclOrStmt *it = s->u.compound; it; it = it->next) {
            if (it->kind == DECL_OR_STMT_STMT && contains_own_break(it->u.stmt)) {
                return true;
            }
        }
        return false;
    case STMT_LABELED:
        return contains_own_break(s->u.labeled.stmt);
    case STMT_CASE:
        return contains_own_break(s->u.case_stmt.stmt);
    case STMT_DEFAULT:
        return contains_own_break(s->u.default_stmt);
    default:
        // STMT_WHILE/STMT_DO_WHILE/STMT_FOR/STMT_SWITCH capture their own break;
        // everything else cannot contain a break for this loop.
        return false;
    }
}

// True if `s` is an expression statement that calls a `_Noreturn` function, so
// control does not continue past it.  The callee's `_Noreturn` is recorded on its
// symbol (see symtab_add_fun), so a symtab lookup recognises both standard-library
// no-return functions (exit/abort via <stdlib.h>, longjmp via <setjmp.h>, declared
// `_Noreturn`) and user-defined ones.
static bool stmt_is_noreturn_call(const Stmt *s)
{
    if (!s || s->kind != STMT_EXPR || !s->u.expr || s->u.expr->kind != EXPR_CALL) {
        return false;
    }
    const Expr *callee = s->u.expr->u.call.func;
    if (!callee || callee->kind != EXPR_VAR) {
        return false;
    }
    const Symbol *sym = symtab_get_opt(callee->u.var);
    return sym && sym->kind == SYM_FUNC && sym->u.func.noret;
}

// True only when control is *certain* to reach the statement immediately
// following `s` (i.e. `s` can "fall through").  This is a deliberate
// under-approximation: when the outcome is uncertain it returns false ("does not
// fall through").  That keeps the caller's missing-return diagnostic free of
// false positives — it never rejects code that actually always returns — at the
// cost of occasionally under-warning (e.g. a switch that is in fact exhaustive,
// or a goto join point, is treated as not falling through).
static bool stmt_falls_through(const Stmt *s)
{
    if (!s) {
        return true; // empty / null statement
    }
    switch (s->kind) {
    case STMT_RETURN:
    case STMT_GOTO:
    case STMT_BREAK:
    case STMT_CONTINUE:
        return false;
    case STMT_EXPR:
        // A call to a no-return library function does not fall through.
        return !stmt_is_noreturn_call(s);
    case STMT_COMPOUND: {
        bool reachable = true;
        for (const DeclOrStmt *it = s->u.compound; it; it = it->next) {
            if (it->kind != DECL_OR_STMT_STMT) {
                continue; // a declaration does not change reachability
            }
            if (reachable) {
                reachable = stmt_falls_through(it->u.stmt);
            }
        }
        return reachable;
    }
    case STMT_IF: {
        const Expr *cond = s->u.if_stmt.condition;
        bool then_ft     = stmt_falls_through(s->u.if_stmt.then_stmt);
        if (!s->u.if_stmt.else_stmt) {
            // The body-skipping path falls through, except when the condition is
            // a compile-time truth (then only the then-branch can run).
            return is_const_true(cond) ? then_ft : true;
        }
        bool else_ft = stmt_falls_through(s->u.if_stmt.else_stmt);
        if (is_const_true(cond)) {
            return then_ft;
        }
        if (is_const_false(cond)) {
            return else_ft;
        }
        return then_ft || else_ft;
    }
    case STMT_WHILE:
        // A non-constant guard may be false on entry, so the loop is skippable
        // and certainly falls through.  An always-true guard falls through only
        // via a break that exits this loop.
        return is_const_true(s->u.while_stmt.condition) ? contains_own_break(s->u.while_stmt.body)
                                                        : true;
    case STMT_FOR:
        // for(;;) has a NULL condition, an always-true guard.
        return (!s->u.for_stmt.condition || is_const_true(s->u.for_stmt.condition))
                   ? contains_own_break(s->u.for_stmt.body)
                   : true;
    case STMT_DO_WHILE:
        // The body always runs once.  With an always-true guard the loop falls
        // through only via a break; otherwise it falls through if the body can
        // reach the guard (and the guard can be false) or a break exits it.
        if (is_const_true(s->u.do_while.condition)) {
            return contains_own_break(s->u.do_while.body);
        }
        return stmt_falls_through(s->u.do_while.body) || contains_own_break(s->u.do_while.body);
    case STMT_LABELED:
        return stmt_falls_through(s->u.labeled.stmt);
    case STMT_CASE:
        return stmt_falls_through(s->u.case_stmt.stmt);
    case STMT_DEFAULT:
        return stmt_falls_through(s->u.default_stmt);
    case STMT_DEFER:
        return true; // only registers its statement; control goes on past it
    case STMT_SWITCH:
    default:
        // A switch is not analysed for exhaustiveness; treat it as not certainly
        // falling through so an exhaustive switch is never flagged.
        return false;
    }
}

// Type-check a function declaration/definition.
static void typecheck_fn_decl(ExternalDecl *d)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    d->u.function.type   = resolve_typedef_names(d->u.function.type);
    const Type *fun_type = d->u.function.type;
    validate_type(fun_type);
    Type *adjusted_type = clone_type(fun_type, __func__, __FILE__, __LINE__);
    if (fun_type->kind == TYPE_FUNCTION) {
        if (unalias(fun_type->u.function.return_type)->kind == TYPE_ARRAY) {
            fatal_error("function cannot return array type '%s'",
                        type_to_c(fun_type->u.function.return_type));
        }
        // Strip the f(void) sentinel and decay array params to pointers.
        adjust_function_params(adjusted_type);
    } else {
        fatal_error("'%s' is defined as a function but has type '%s'", d->u.function.name,
                    type_to_c(fun_type));
    }
    check_duplicate_params(adjusted_type);
    const Type *yield =
        check_coroutine_decl(d->u.function.name, d->u.function.specifiers, fun_type);
    bool has_body            = d->u.function.body != NULL;
    const Param *params      = adjusted_type->u.function.params;
    bool all_params_complete = true;
    for (const Param *p = params; p; p = p->next) {
        if (!is_complete(p->type)) {
            all_params_complete = false;
            break;
        }
    }
    const Type *ret = unalias(fun_type->u.function.return_type);
    bool ret_ok     = (ret->kind == TYPE_VOID) || is_complete(ret);
    if (has_body && (!ret_ok || !all_params_complete)) {
        fatal_error("function '%s' has an incomplete return or parameter type", d->u.function.name);
    }
    bool global      = !is_static(d->u.function.specifiers);
    Symbol *existing = symtab_get_opt(d->u.function.name);
    bool defined     = has_body;
    if (existing) {
        if (unalias(existing->type)->kind != fun_type->kind) {
            error_at_previous(d->u.function.name,
                              "redefinition of '%s' as a different kind of symbol",
                              d->u.function.name);
        }
        if (existing->kind == SYM_FUNC) {
            if (!compatible_type(existing->type, adjusted_type)) {
                error_at_previous(d->u.function.name, "conflicting types for '%s'",
                                  d->u.function.name);
            }
            if (existing->u.func.defined && has_body) {
                error_at_previous(d->u.function.name, "redefinition of '%s'", d->u.function.name);
            }
            if (existing->u.func.global && is_static(d->u.function.specifiers)) {
                error_at_previous(d->u.function.name,
                                  "static declaration of '%s' follows non-static declaration",
                                  d->u.function.name);
            }
            agree_coroutine(existing, yield, d->u.function.name);
            defined = has_body || existing->u.func.defined;
            global  = existing->u.func.global;
        }
    }
    // _Noreturn is sticky across declarations (C11 §6.7.4).
    bool noret = is_noreturn(d->u.function.specifiers) ||
                 (existing && existing->kind == SYM_FUNC && existing->u.func.noret);
    symtab_add_fun(d->u.function.name, adjusted_type, global, defined, noret);
    if (yield)
        symtab_set_coro(d->u.function.name, yield);
    if (has_body) {
        if (d->u.function.param_decls) {
            fatal_error("K&R-style parameter declarations are not supported");
        }
        scope_increment();
        param_scope_level = scope_level;
        for (const Param *p = params; p; p = p->next) {
            if (p->name) {
                const Symbol *dup = symtab_get_opt(p->name);
                if (dup && (!dup->has_linkage || dup->kind == SYM_FUNC)) {
                    // A parameter shadowing an enclosing-scope variable, or a
                    // file-scope function of the same name, is forbidden by the
                    // no-shadowing design (external vs no linkage, C11 §6.7p3).
                    diag_enter(p->loc);
                    redeclared(p->name);
                }
            }
            symtab_add_automatic_var_type(p->name, p->type, scope_level);
        }
        static_locals_set_function(d->u.function.name);
        coro_lint_function(d->u.function.name);
        coro_begin_body(yield);
        typecheck_function = fun_type;
        d->u.function.body =
            typecheck_statement(fun_type->u.function.return_type, d->u.function.body);
        typecheck_function = NULL;
        param_scope_level  = -1;
        coro_end_body();

        // A non-void function whose body can fall off the end yields an
        // indeterminate value (C11 §6.9.1p12).  Reject that — except for main(),
        // which by §5.1.2.2.3 implicitly returns 0: synthesize the return instead.
        const Type *rt = unalias(fun_type->u.function.return_type);
        if (rt->kind != TYPE_VOID && d->u.function.body &&
            d->u.function.body->kind == STMT_COMPOUND && stmt_falls_through(d->u.function.body)) {
            if (strcmp(d->u.function.name, "main") == 0) {
                Expr *zero                 = new_expression(EXPR_LITERAL);
                zero->u.literal            = new_literal(LITERAL_INT);
                zero->u.literal->u.int_val = 0;
                Stmt *ret_stmt             = new_stmt(STMT_RETURN);
                ret_stmt->u.expr           = zero;
                ret_stmt                   = typecheck_statement(rt, ret_stmt);

                DeclOrStmt *item  = new_decl_or_stmt(DECL_OR_STMT_STMT);
                item->u.stmt      = ret_stmt;
                DeclOrStmt **tail = &d->u.function.body->u.compound;
                while (*tail) {
                    tail = &(*tail)->next;
                }
                *tail = item;
            } else {
                fatal_error(
                    "non-void function '%s' may reach its end without "
                    "returning a value",
                    d->u.function.name);
            }
        }

        static_locals_set_function(NULL);
        coro_lint_function(NULL);
        auto_locals_clear();
        scope_decrement();
    }
    free_type(d->u.function.type);
    d->u.function.type = adjusted_type; // Update type in place
}

// Type-check a local declaration.
void typecheck_local_decl(Declaration *d);

static void typecheck_local_decl_at(Declaration *d)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    switch (d->kind) {
    case DECL_VAR:
        typecheck_local_var_decl(d);
        break;
    case DECL_EMPTY:
        typecheck_tag_decl(d);
        break;
    case DECL_STATIC_ASSERT:
        typecheck_static_assert_decl(d);
        break;
    default:
        internal_error("unsupported local declaration kind %d", d->kind);
    }
}

// typecheck_local_decl with diag_loc at the node, for the errors found in it.
void typecheck_local_decl(Declaration *d)
{
    SrcLoc saved = diag_enter(d ? d->loc : diag_loc);
    typecheck_local_decl_at(d);
    diag_loc = saved;
}

// Type-check a global (file-scope) variable declaration.
static void typecheck_file_scope_var_decl(Declaration *d)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
        print_declaration(stdout, d, 4);
    }
    if (d->u.var.specifiers && d->u.var.specifiers->storage == STORAGE_CLASS_TYPEDEF) {
        reject_alignas(d->u.var.specifiers, "a typedef");
        for (InitDeclarator *decl = d->u.var.declarators; decl; decl = decl->next) {
            diag_enter(decl->loc); // the declaration restores it
            reject_coro_spec(d->u.var.specifiers, decl->name);
            decl->type = resolve_typedef_names(decl->type);
            register_inline_struct_defs(decl->type);
            validate_type(decl->type);
            add_typedef(decl->name, decl->type);
        }
        return;
    }
    bool global = !is_static(d->u.var.specifiers);
    for (InitDeclarator *decl = d->u.var.declarators; decl; decl = decl->next) {
        diag_enter(decl->loc); // the declaration restores it
        decl->type     = resolve_typedef_names(decl->type);
        Type *var_type = decl->type;

        // A function prototype at file scope (e.g. "int f(int);") arrives here
        // as a DECL_VAR with a function type. Register it as SYM_FUNC so that
        // resolve() can find it when it later processes the definition, and so
        // has_linkage is set correctly to allow the redeclaration.
        if (unalias(var_type)->kind == TYPE_FUNCTION) {
            reject_alignas(d->u.var.specifiers, "a function");
            register_function_declaration(decl, d->u.var.specifiers);
            continue;
        }
        reject_coro_spec(d->u.var.specifiers, decl->name);

        if (unalias(var_type)->kind == TYPE_VOID) {
            fatal_error("variable '%s' has incomplete type 'void'", decl->name);
        }
        register_inline_struct_defs(var_type);
        validate_type(var_type);

        InitKind init_kind        = is_extern(d->u.var.specifiers) ? INIT_NONE : INIT_TENTATIVE;
        Tac_StaticInit *init_list = NULL;
        if (decl->init) {
            // An incomplete type (e.g. a forward-declared struct) can't be initialized;
            // reject before building the initializer so the diagnostic is meaningful.
            if (!is_complete(var_type)) {
                fatal_error("variable '%s' has incomplete type '%s'", decl->name,
                            type_to_c(var_type));
            }
            // Pre-register tentatively so the variable's own initializer can reference
            // it via sizeof (e.g. int foo = sizeof(foo); — valid C11 §6.2.1p7).
            if (!symtab_get_opt(decl->name)) {
                symtab_add_static_var(decl->name, var_type, global, INIT_TENTATIVE, NULL);
            }
            init_kind = INIT_INITIALIZED;
            init_list = build_static_init(var_type, &decl->init);
        }
        if (!is_complete(var_type) && init_kind != INIT_NONE) {
            fatal_error("variable '%s' has incomplete type '%s'", decl->name, type_to_c(var_type));
        }
        Symbol *existing = symtab_get_opt(decl->name);
        if (existing) {
            if (unalias(existing->type)->kind != unalias(var_type)->kind) {
                error_at_previous(decl->name, "conflicting types for '%s'", decl->name);
            }
            if (!compatible_type(existing->type, var_type)) {
                error_at_previous(decl->name, "conflicting types for '%s'", decl->name);
            }
            if (existing->kind == SYM_STATIC) {
                if (!is_extern(d->u.var.specifiers) && existing->u.static_var.global != global) {
                    error_at_previous(decl->name, "conflicting linkage for '%s'", decl->name);
                }
                if (existing->u.static_var.init_kind == INIT_INITIALIZED &&
                    init_kind == INIT_INITIALIZED) {
                    error_at_previous(decl->name, "redefinition of '%s'", decl->name);
                }
                init_kind = existing->u.static_var.init_kind == INIT_INITIALIZED
                                ? existing->u.static_var.init_kind
                                : init_kind;
                init_list = existing->u.static_var.init_kind == INIT_INITIALIZED
                                ? existing->u.static_var.init_list
                                : init_list;
                global    = is_extern(d->u.var.specifiers) ? existing->u.static_var.global : global;
            }
        }
        int alignment =
            existing && existing->kind == SYM_STATIC ? existing->u.static_var.alignment : 0;
        symtab_add_static_var(decl->name, var_type, global, init_kind, init_list);
        note_alignment(decl->name, alignment);
        note_alignment(decl->name, declared_alignment(d->u.var.specifiers, var_type));

        // Drop initializer
        free_initializer(decl->init);
        decl->init = NULL;
    }
}

// Type-check a global declaration.
void typecheck_global_decl(ExternalDecl *d);

static void typecheck_global_decl_at(ExternalDecl *d)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
        print_external_decl(stdout, d, 4);
    }
    switch (d->kind) {
    case EXTERNAL_DECL_FUNCTION:
        typecheck_fn_decl(d);
        if (semantic_debug) {
            printf("--- result:\n");
            print_external_decl(stdout, d, 4);
        }
        break;
    case EXTERNAL_DECL_DECLARATION:
        switch (d->u.declaration->kind) {
        case DECL_VAR:
            typecheck_file_scope_var_decl(d->u.declaration);
            break;
        case DECL_EMPTY:
            typecheck_tag_decl(d->u.declaration);
            break;
        case DECL_STATIC_ASSERT:
            typecheck_static_assert_decl(d->u.declaration);
            break;
        }
        if (semantic_debug) {
            printf("--- result:\n");
            print_declaration(stdout, d->u.declaration, 4);
        }
        break;
    }
}

// typecheck_global_decl with diag_loc at the node, for the errors found in it.
void typecheck_global_decl(ExternalDecl *d)
{
    SrcLoc saved = diag_enter(d ? d->loc : diag_loc);
    typecheck_global_decl_at(d);
    diag_loc = saved;
}
