//
// Helpers for Type.
//
#include <assert.h>

#include "semantic.h"
#include "structtab.h"
#include "target.h"
#include "typecheck.h"
#include "typetab.h"

// Resolve typedef names in a type tree. A *local* (block-scope) typedef name is
// expanded in place into a cloned copy of its underlying type, because the typetab
// entry is purged on block exit and a surviving reference would dangle. A *global*
// (file-scope) typedef name is left untouched: its typetab entry lives for the whole
// compilation, so downstream code resolves it on demand (via unalias / the type
// predicates) — this avoids deep-cloning the resolved type at every use.  But for one
// that names an array of unspecified size: an initializer completes the array type of
// its own object (C11 §6.7.9p22), in place, so each object needs a copy of its own.
// Returns the (potentially new) root pointer; caller must use the return value.
Type *resolve_typedef_names(Type *t)
{
    if (!t)
        return NULL;
    if (t->kind == TYPE_TYPEDEF_NAME) {
        const TypeDef *def = typetab_find(t->u.typedef_name.name);
        const Type *ut     = unalias(def->type);
        if (def->level == 0 && !(ut->kind == TYPE_ARRAY && !ut->u.array.size))
            return t; // global typedef: keep the reference, no clone
        Type *cloned = clone_type(def->type, __func__, __FILE__, __LINE__);
        free_type(t);
        return resolve_typedef_names(cloned); // local: expand, handle chains/nesting
    }
    switch (t->kind) {
    case TYPE_POINTER:
        t->u.pointer.target = resolve_typedef_names(t->u.pointer.target);
        break;
    case TYPE_ARRAY:
        t->u.array.element = resolve_typedef_names(t->u.array.element);
        // Fold a non-integer-literal dimension (e.g. an enum constant or a
        // constant expression) to a LITERAL_INT so get_size() sees a real length.
        if (t->u.array.size && !(t->u.array.size->kind == EXPR_LITERAL &&
                                 t->u.array.size->u.literal->kind == LITERAL_INT)) {
            long n;
            if (try_eval_const_int(t->u.array.size, &n)) {
                free_expression(t->u.array.size);
                t->u.array.size = NULL; // set_array_size assigns without freeing
                set_array_size(t, (size_t)n);
            }
        }
        break;
    case TYPE_FUNCTION:
        t->u.function.return_type = resolve_typedef_names(t->u.function.return_type);
        for (Param *p = t->u.function.params; p; p = p->next)
            p->type = resolve_typedef_names(p->type);
        break;
    case TYPE_STRUCT:
        // _Coro_frame(Y, T): a local typedef in Y or T is gone by the time it is lowered.
        t->u.struct_t.frame_yield  = resolve_typedef_names(t->u.struct_t.frame_yield);
        t->u.struct_t.frame_result = resolve_typedef_names(t->u.struct_t.frame_result);
        break;
    default:
        break; // primitive, struct, union, enum — no child types
    }
    return t;
}

//
// Resolve a typedef-name root to its underlying type, following chains of
// (global) typedefs. Any non-typedef type passes through unchanged. Used to
// look through the global typedef references that resolve_typedef_names now
// leaves in place.
//
const Type *unalias(const Type *t)
{
    while (t && t->kind == TYPE_TYPEDEF_NAME)
        t = typetab_resolve(t->u.typedef_name.name);
    return t;
}

//
// Get size in bytes for a given type.
//
size_t get_size(const Type *t)
{
    t = unalias(t);
    switch (t->kind) {
    case TYPE_CHAR:
    case TYPE_SCHAR:
    case TYPE_UCHAR:
        return 1;
    // _Bool is the one integer type whose width the target descriptor decides: on a
    // word-addressed machine a 1-byte object means byte-packed storage and a fat byte
    // pointer, which is the wrong representation for a one-bit type (see Target).
    case TYPE_BOOL:
        return target_config->bool_size;
    case TYPE_SHORT:
    case TYPE_USHORT:
        return target_config->short_size;
    case TYPE_INT:
    case TYPE_UINT:
    case TYPE_ENUM:
        return target_config->int_size;
    case TYPE_FLOAT:
        return target_config->float_size;
    case TYPE_LONG:
    case TYPE_ULONG:
        return target_config->long_size;
    case TYPE_LONG_LONG:
    case TYPE_ULONG_LONG:
        return target_config->llong_size;
    case TYPE_DOUBLE:
        return target_config->double_size;
    case TYPE_POINTER:
        return target_config->pointer_size;
    case TYPE_LONG_DOUBLE:
        return target_config->ldouble_size;
    case TYPE_ARRAY:
        if (!t->u.array.size) {
            internal_error("get_size: Array size not specified");
        }
        assert(t->u.array.size);
        if (t->u.array.size->kind != EXPR_LITERAL) {
            internal_error("get_size: Array size is not literal");
        }
        return t->u.array.size->u.literal->u.int_val * get_size(t->u.array.element);
    case TYPE_STRUCT:
    case TYPE_UNION: {
        // A block-local tag is purged from structtab on block exit, but its
        // size was cached on the AST node by validate_type while it was live.
        // Refresh the cache on every hit as well, so a node validate_type never
        // reached still carries the size once anyone has asked for it in scope.
        const StructDef *d = structtab_find_opt(t->u.struct_t.name);
        if (d) {
            if (d->complete) {
                ((Type *)t)->u.struct_t.cached_size = (int)d->size;
                ((Type *)t)->u.struct_t.cached_def  = d;
            }
            return d->size;
        }
        if (t->u.struct_t.cached_size)
            return t->u.struct_t.cached_size;
        return structtab_find(t->u.struct_t.name)->size; // not found: fatal_error
    }
    case TYPE_FUNCTION:
    case TYPE_VOID:
    default:
        internal_error("get_size: Type %s doesn't have size", type_kind_str[t->kind]);
    }
    return 0; // Unreachable
}

size_t get_alignment(const Type *t)
{
    t = unalias(t);
    switch (t->kind) {
    case TYPE_CHAR:
    case TYPE_SCHAR:
    case TYPE_UCHAR:
        return 1;
    case TYPE_BOOL: // see get_size
        return target_config->bool_align;
    case TYPE_SHORT:
    case TYPE_USHORT:
        return target_config->short_align;
    case TYPE_INT:
    case TYPE_UINT:
    case TYPE_ENUM:
        return target_config->int_align;
    case TYPE_FLOAT:
        return target_config->float_align;
    case TYPE_LONG:
    case TYPE_ULONG:
        return target_config->long_align;
    case TYPE_LONG_LONG:
    case TYPE_ULONG_LONG:
        return target_config->llong_align;
    case TYPE_DOUBLE:
        return target_config->double_align;
    case TYPE_POINTER:
        return target_config->pointer_align;
    case TYPE_LONG_DOUBLE:
        return target_config->ldouble_align;
    case TYPE_ARRAY:
        return get_alignment(t->u.array.element);
    case TYPE_STRUCT:
    case TYPE_UNION: {
        // See get_size: fall back to the cached alignment for a purged block-local tag.
        const StructDef *d = structtab_find_opt(t->u.struct_t.name);
        if (d) {
            if (d->complete) {
                ((Type *)t)->u.struct_t.cached_align = (int)d->alignment;
                ((Type *)t)->u.struct_t.cached_def   = d;
            }
            return d->alignment;
        }
        if (t->u.struct_t.cached_align)
            return t->u.struct_t.cached_align;
        return structtab_find(t->u.struct_t.name)->alignment; // not found: fatal_error
    }
    case TYPE_FUNCTION:
    case TYPE_VOID:
    default:
        internal_error("get_alignment: Type %s doesn't have alignment", type_kind_str[t->kind]);
    }
    return 0; // Unreachable
}

TypeKind unsigned_kind_of_size(int size)
{
    static const TypeKind kinds[] = { TYPE_UCHAR, TYPE_UINT, TYPE_USHORT, TYPE_ULONG,
                                      TYPE_ULONG_LONG };
    for (size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++) {
        Type t = { .kind = kinds[i] };
        if ((int)get_size(&t) == size)
            return kinds[i];
    }
    return TYPE_VOID;
}

TypeKind signed_kind_of(TypeKind k)
{
    switch (k) {
    case TYPE_UCHAR:
        return TYPE_SCHAR;
    case TYPE_USHORT:
        return TYPE_SHORT;
    case TYPE_UINT:
        return TYPE_INT;
    case TYPE_ULONG:
        return TYPE_LONG;
    case TYPE_ULONG_LONG:
        return TYPE_LONG_LONG;
    default:
        return k;
    }
}

const BitField *access_bitfield(const Expr *e)
{
    const BitField *bf = NULL;
    if (e->kind == EXPR_FIELD_ACCESS)
        bf = &e->u.field_access.bf;
    else if (e->kind == EXPR_PTR_ACCESS)
        bf = &e->u.ptr_access.bf;
    return bf && bf->width ? bf : NULL;
}

bool is_signed(const Type *t)
{
    t = unalias(t);
    switch (t->kind) {
    case TYPE_CHAR:
        // Plain `char` has target-defined signedness (default signed).  This is a
        // value-signedness query only; TYPE_CHAR stays a distinct type from
        // signed/unsigned char for _Generic and pointer compatibility.
        return !target_config || target_config->char_signed;
    case TYPE_SCHAR:
    case TYPE_SHORT:
    case TYPE_INT:
    case TYPE_LONG:
    case TYPE_LONG_LONG:
    case TYPE_ENUM:
        return true;
    case TYPE_BOOL:
    case TYPE_UCHAR:
    case TYPE_USHORT:
    case TYPE_UINT:
    case TYPE_ULONG:
    case TYPE_ULONG_LONG:
    case TYPE_POINTER:
        return false;
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
    case TYPE_LONG_DOUBLE:
    case TYPE_FUNCTION:
    case TYPE_ARRAY:
    case TYPE_VOID:
    case TYPE_STRUCT:
    case TYPE_UNION:
    default:
        internal_error("is_signed: Signedness doesn't make sense for non-integral type %s",
                       type_kind_str[t->kind]);
    }
    return false; // Unreachable
}

// Return true if a qualifier list contains the volatile qualifier.
static bool has_volatile_qualifier(const TypeQualifier *q)
{
    for (; q; q = q->next)
        if (q->kind == TYPE_QUALIFIER_VOLATILE)
            return true;
    return false;
}

// Return true if accessing an object of this type is a volatile access.
// Checks the type's own qualifier list, plus the pointer/array object's own
// qualifiers (the `int * volatile p` case). Volatility of a pointee is *not*
// considered here: it surfaces at the dereference site via that expression's
// own (pointee) type. Local typedefs are resolved before lowering; a global
// typedef name is looked through after first honoring the node's own qualifiers.
bool type_is_volatile(const Type *t)
{
    if (!t)
        return false;
    if (has_volatile_qualifier(t->qualifiers))
        return true;
    if (t->kind == TYPE_TYPEDEF_NAME)
        return type_is_volatile(typetab_resolve(t->u.typedef_name.name));
    if (t->kind == TYPE_POINTER)
        return has_volatile_qualifier(t->u.pointer.qualifiers);
    if (t->kind == TYPE_ARRAY)
        return has_volatile_qualifier(t->u.array.qualifiers);
    return false;
}

bool is_pointer(const Type *t)
{
    return unalias(t)->kind == TYPE_POINTER;
}

bool is_integer(const Type *t)
{
    t = unalias(t);
    switch (t->kind) {
    case TYPE_BOOL:
    case TYPE_CHAR:
    case TYPE_UCHAR:
    case TYPE_SCHAR:
    case TYPE_SHORT:
    case TYPE_USHORT:
    case TYPE_INT:
    case TYPE_UINT:
    case TYPE_LONG:
    case TYPE_ULONG:
    case TYPE_LONG_LONG:
    case TYPE_ULONG_LONG:
    case TYPE_ENUM:
        return true;
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
    case TYPE_LONG_DOUBLE:
    case TYPE_ARRAY:
    case TYPE_POINTER:
    case TYPE_FUNCTION:
    case TYPE_VOID:
    case TYPE_STRUCT:
    case TYPE_UNION:
    default:
        return false;
    }
}

bool is_array(const Type *t)
{
    return unalias(t)->kind == TYPE_ARRAY;
}

bool is_character(const Type *t)
{
    t = unalias(t);
    switch (t->kind) {
    case TYPE_CHAR:
    case TYPE_SCHAR:
    case TYPE_UCHAR:
        return true;
    default:
        return false;
    }
}

// True for an integer type the integer promotions (C11 §6.3.1.1p2) widen to int:
// _Bool, the three character types, and short/unsigned short.  (`unsigned short` fills
// a BESM-6 word, so promoting it to signed int is the deliberate simplification
// get_common_type and promote_kind both make; the two must agree.)  _Bool is narrow by
// *value* width — one bit — not by storage: get_size may give it a whole word, but the
// promotion is still what makes `b << 1`, `~b`, `-b` and `b op= x` compute in int and
// re-normalise on the way back into the _Bool.
bool is_promotable_narrow(const Type *t)
{
    t = unalias(t);
    return t->kind == TYPE_BOOL || is_character(t) || t->kind == TYPE_SHORT ||
           t->kind == TYPE_USHORT;
}

// C11 §6.3.1.1p2: unsigned short promotes to unsigned int where int cannot represent
// all its values, i.e. where short is as wide as int (AVR).  BESM-6 keeps the
// simplification described above: its unsigned short fills the word, and promotes to int.
bool ushort_promotes_unsigned(void)
{
    return target_config && !target_word_addressed() &&
           (int)target_config->short_size * 8 >= target_config->int_bits;
}

// The type a promotable narrow integer type promotes to.
TypeKind promoted_kind(const Type *t)
{
    t = unalias(t);
    return t->kind == TYPE_USHORT && ushort_promotes_unsigned() ? TYPE_UINT : TYPE_INT;
}

// ptrdiff_t, the type of a pointer difference and of a pointer's index: the signed
// integer type as wide as a pointer, long where it is (every target but AVR, where a
// pointer is the size of int).
TypeKind ptrdiff_kind(void)
{
    if (target_config->long_size == target_config->pointer_size)
        return TYPE_LONG;
    if (target_config->int_size == target_config->pointer_size)
        return TYPE_INT;
    return TYPE_LONG_LONG;
}

// size_t, the type of sizeof and _Alignof: the unsigned ptrdiff_t.
TypeKind size_kind(void)
{
    switch (ptrdiff_kind()) {
    case TYPE_LONG:
        return TYPE_ULONG;
    case TYPE_INT:
        return TYPE_UINT;
    default:
        return TYPE_ULONG_LONG;
    }
}

bool is_arithmetic(const Type *t)
{
    t = unalias(t);
    switch (t->kind) {
    case TYPE_BOOL:
    case TYPE_CHAR:
    case TYPE_UCHAR:
    case TYPE_SCHAR:
    case TYPE_SHORT:
    case TYPE_USHORT:
    case TYPE_INT:
    case TYPE_UINT:
    case TYPE_LONG:
    case TYPE_ULONG:
    case TYPE_LONG_LONG:
    case TYPE_ULONG_LONG:
    case TYPE_ENUM:
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
    case TYPE_LONG_DOUBLE:
        return true;
    case TYPE_FUNCTION:
    case TYPE_POINTER:
    case TYPE_ARRAY:
    case TYPE_VOID:
    case TYPE_STRUCT:
    case TYPE_UNION:
    default:
        return false;
    }
}

bool is_scalar(const Type *t)
{
    t = unalias(t);
    switch (t->kind) {
    case TYPE_ARRAY:
    case TYPE_VOID:
    case TYPE_FUNCTION:
    case TYPE_STRUCT:
    case TYPE_UNION:
        return false;
    case TYPE_BOOL:
    case TYPE_CHAR:
    case TYPE_UCHAR:
    case TYPE_SCHAR:
    case TYPE_SHORT:
    case TYPE_USHORT:
    case TYPE_INT:
    case TYPE_UINT:
    case TYPE_LONG:
    case TYPE_ULONG:
    case TYPE_LONG_LONG:
    case TYPE_ULONG_LONG:
    case TYPE_ENUM:
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
    case TYPE_LONG_DOUBLE:
    case TYPE_POINTER:
        return true;
    default:
        return false;
    }
}

bool is_complete(const Type *t)
{
    t = unalias(t);
    switch (t->kind) {
    case TYPE_VOID:
        return false;
    case TYPE_STRUCT:
    case TYPE_UNION: {
        // A forward-declared tag lives in structtab but is not yet a complete type.
        const StructDef *d = structtab_find_opt(t->u.struct_t.name);
        return d != NULL && d->complete;
    }
    default:
        return true;
    }
}

bool is_complete_pointer(const Type *t)
{
    t = unalias(t);
    if (t->kind == TYPE_POINTER) {
        return is_complete(t->u.pointer.target);
    }
    return false;
}
