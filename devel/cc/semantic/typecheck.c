//
// Core type-checking utilities and entry points.
//
#include "typecheck.h"

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantic.h"
#include "structtab.h"
#include "symtab.h"
#include "target.h"
#include "typetab.h"

// Enable debug output
int semantic_debug;

// Level of scope for nested compound operators.
int scope_level;

void scope_increment(void)
{
    scope_level++;
}

void scope_decrement(void)
{
    coro_lint_scope_exit(scope_level);
    scope_level--;
    symtab_purge(scope_level);
    structtab_purge(scope_level);
    typetab_purge(scope_level);
}

void semantic_recover(const ExternalDecl *d)
{
    if (d && d->kind == EXTERNAL_DECL_FUNCTION) {
        symtab_poison(d->u.function.name);
    } else if (d && d->u.declaration->kind == DECL_VAR) {
        for (const InitDeclarator *v = d->u.declaration->u.var.declarators; v; v = v->next)
            symtab_poison(v->name);
    }
    coro_recover();
    declarations_recover();
    statements_recover();
    static_locals_set_function(NULL);
    scope_level = 0;
    symtab_purge(0);
    structtab_purge(0);
    typetab_purge(0);
}

int round_away_from_zero(int alignment, int size)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    if (size % alignment == 0) {
        return size;
    }

    if (size < 0) {
        return size - alignment - (size % alignment);
    } else {
        return size + alignment - (size % alignment);
    }
}

size_t get_array_size(const Type *t)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    t = unalias(t);
    if (t->kind != TYPE_ARRAY) {
        internal_error("get_array_size: Array is expected");
    }
    if (!t->u.array.size) {
        return 0;
    }
    if (t->u.array.size->kind != EXPR_LITERAL) {
        internal_error("get_array_size: Size is not a literal");
    }
    if (!t->u.array.size->u.literal) {
        internal_error("get_array_size: No literal in size");
    }
    assert(t->u.array.size->u.literal);
    if (t->u.array.size->u.literal->kind != LITERAL_INT) {
        internal_error("get_array_size: Non-integer size");
    }
    return t->u.array.size->u.literal->u.int_val;
}

void set_array_size(Type *t, size_t size)
{
    t->u.array.size                       = new_expression(EXPR_LITERAL);
    t->u.array.size->u.literal            = new_literal(LITERAL_INT);
    t->u.array.size->u.literal->u.int_val = size;
}

// Validate a type (recursive).
void validate_type(const Type *t)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
        print_type(stdout, t, 4);
    }
    if (!t)
        return;
    switch (t->kind) {
    case TYPE_ARRAY:
        if (!is_complete(t->u.array.element)) {
            fatal_error("array has incomplete element type '%s'", type_to_c(t->u.array.element));
        }
        if (unalias(t->u.array.element)->kind == TYPE_FUNCTION) {
            fatal_error("array of functions '%s' is not allowed", type_to_c(t));
        }
        validate_type(t->u.array.element);
        break;
    case TYPE_POINTER:
        validate_type(t->u.pointer.target);
        break;
    case TYPE_FUNCTION:
        if (t->u.function.return_type->kind == TYPE_FUNCTION) {
            fatal_error("function cannot return function type '%s'",
                        type_to_c(t->u.function.return_type));
        }
        if (t->u.function.return_type->kind == TYPE_ARRAY) {
            fatal_error("function cannot return array type '%s'",
                        type_to_c(t->u.function.return_type));
        }
        validate_type(t->u.function.return_type);
        for (const Param *p = t->u.function.params; p; p = p->next) {
            if (p->type->kind == TYPE_VOID && p->name) {
                fatal_error("parameter '%s' has type 'void'", p->name);
            }
            validate_type(p->type);
        }
        break;
    case TYPE_VOID:
    case TYPE_BOOL:
    case TYPE_CHAR:
    case TYPE_SCHAR:
    case TYPE_UCHAR:
    case TYPE_SHORT:
    case TYPE_USHORT:
    case TYPE_INT:
    case TYPE_UINT:
    case TYPE_LONG:
    case TYPE_ULONG:
    case TYPE_LONG_LONG:
    case TYPE_ULONG_LONG:
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
    case TYPE_LONG_DOUBLE:
    case TYPE_ENUM:
        break;
    case TYPE_STRUCT:
    case TYPE_UNION: {
        if (is_coro_struct(t)) {
            check_frame_type(t); // _Coro_frame(Y, T), _Coro_ptr(Y, T): never defined
            break;
        }
        // A reference such as `union x foo;` must agree with the tag's declared keyword.
        check_tag_kind(t);
        // Cache the size/alignment on the AST node while the tag is still live in
        // structtab.  A block-local tag is purged on block exit, so the translator
        // (get_size/get_alignment) can no longer resolve it; the cache lets it fall
        // back without re-querying structtab.  Mirrors field_access.offset, which is
        // likewise resolved and stashed on the AST during typecheck.
        const StructDef *d = structtab_find_opt(t->u.struct_t.name);
        if (d && d->complete) {
            Type *mut                    = (Type *)t;
            mut->u.struct_t.cached_size  = (int)d->size;
            mut->u.struct_t.cached_align = (int)d->alignment;
            mut->u.struct_t.cached_def   = d;
        }
        break;
    }
    case TYPE_TYPEDEF_NAME:
        // Global typedef names survive resolve_typedef_names as references.
        if (!typetab_exists(t->u.typedef_name.name)) {
            fatal_error("unknown type name '%s'", t->u.typedef_name.name);
        }
        validate_type(typetab_resolve(t->u.typedef_name.name));
        break;
    default:
        internal_error("unsupported type kind %d", t->kind);
    }
}

// Convert an expression to a target type.
Expr *convert_to_type(Expr *e, const Type *target_type)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    const Type *src = unalias(e->type);
    const Type *tgt = unalias(target_type);
    if (src->kind == tgt->kind &&
        (!is_pointer(src) || src->u.pointer.target->kind == tgt->u.pointer.target->kind))
        return e; // Avoid unnecessary casts

    Expr *cast        = new_expression(EXPR_CAST);
    cast->u.cast.type = clone_type(target_type, __func__, __FILE__, __LINE__);
    cast->u.cast.expr = e;
    cast->type        = clone_type(target_type, __func__, __FILE__, __LINE__);
    return cast;
}

Expr *convert_to_kind(Expr *e, TypeKind target_kind)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    if (e->type->kind == target_kind)
        return e; // Avoid unnecessary casts

    Expr *cast        = new_expression(EXPR_CAST);
    cast->u.cast.type = new_type(target_kind, __func__, __FILE__, __LINE__);
    cast->u.cast.expr = e;
    cast->type        = new_type(target_kind, __func__, __FILE__, __LINE__);
    return cast;
}

// Get common type for arithmetic operations.
const Type *get_common_type(const Type *t1, const Type *t2)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    static const Type int_type         = { .kind = TYPE_INT };
    static const Type uint_type        = { .kind = TYPE_UINT };
    static const Type double_type      = { .kind = TYPE_DOUBLE };
    static const Type float_type       = { .kind = TYPE_FLOAT };
    static const Type long_double_type = { .kind = TYPE_LONG_DOUBLE };
    t1                                 = unalias(t1);
    t2                                 = unalias(t2);
    // Integer promotions (C11 §6.3.1.1p2) first: every type narrower than int becomes
    // int.  _Bool must be in that set — otherwise `b + 1` falls through to the size
    // comparison below, which on a target where _Bool is int-sized finds the two equal
    // and picks the *unsigned* one, i.e. _Bool, as the common type.
    if (is_promotable_narrow(t1))
        t1 = promoted_kind(t1) == TYPE_UINT ? &uint_type : &int_type;
    if (is_promotable_narrow(t2))
        t2 = promoted_kind(t2) == TYPE_UINT ? &uint_type : &int_type;
    if (t1->kind == t2->kind)
        return t1;
    if (t1->kind == TYPE_LONG_DOUBLE || t2->kind == TYPE_LONG_DOUBLE)
        return &long_double_type;
    if (t1->kind == TYPE_DOUBLE || t2->kind == TYPE_DOUBLE)
        return &double_type;
    if (t1->kind == TYPE_FLOAT || t2->kind == TYPE_FLOAT)
        return &float_type;
    if (get_size(t1) == get_size(t2))
        return is_signed(t1) ? t2 : t1;
    return get_size(t1) > get_size(t2) ? t1 : t2;
}

// Check if a constant is a zero integer.
bool is_zero_int(const Literal *c)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    switch (c->kind) {
    case LITERAL_INT:
        return c->u.int_val == 0;
    case LITERAL_LONG:
        return c->u.long_val == 0;
    case LITERAL_LONG_LONG:
        return c->u.long_long_val == 0;
    case LITERAL_UINT:
        return c->u.uint_val == 0;
    case LITERAL_ULONG:
        return c->u.ulong_val == 0;
    case LITERAL_ULONG_LONG:
        return c->u.ulong_long_val == 0;
    case LITERAL_CHAR:
        return c->u.char_val == 0;
    case LITERAL_ENUM:
        return false; // Conservative
    default:
        return false;
    }
}

// Check if an expression is a null pointer constant.
bool is_null_pointer_constant(const Expr *e)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    return e->kind == EXPR_LITERAL && is_zero_int(e->u.literal);
}

// Get common pointer type for pointer-involved binary operations.
Type *common_pointer_type(const Expr *e1, const Expr *e2)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    const Type *p1 = unalias(e1->type), *p2 = unalias(e2->type);
    if (p1->kind == p2->kind && is_pointer(p1) &&
        unalias(p1->u.pointer.target)->kind == unalias(p2->u.pointer.target)->kind) {
        // Pointers to struct/union are compatible only if the tags match.
        const Type *t1 = unalias(p1->u.pointer.target), *t2 = unalias(p2->u.pointer.target);
        if ((t1->kind == TYPE_STRUCT || t1->kind == TYPE_UNION) &&
            (strcmp(t1->u.struct_t.name, t2->u.struct_t.name) != 0 ||
             (is_coro_struct(t1) && !same_frame_type(t1, t2))))
            fatal_error("incompatible pointer types ('%s' and '%s')", type_to_c(e1->type),
                        type_to_c(e2->type));
        return e1->type;
    }
    if (is_null_pointer_constant(e1))
        return e2->type;
    if (is_null_pointer_constant(e2))
        return e1->type;

    // void* is the common type with any other object pointer — but only when both
    // operands are actually pointers (a void* vs. a non-null integer is invalid).
    bool e1_void_ptr = p1->kind == TYPE_POINTER && unalias(p1->u.pointer.target)->kind == TYPE_VOID;
    bool e2_void_ptr = p2->kind == TYPE_POINTER && unalias(p2->u.pointer.target)->kind == TYPE_VOID;
    if (e1_void_ptr && p2->kind == TYPE_POINTER)
        return e1->type; // already void* — return it borrowed (no allocation)
    if (e2_void_ptr && p1->kind == TYPE_POINTER)
        return e2->type;
    fatal_error("incompatible pointer types ('%s' and '%s')", type_to_c(e1->type),
                type_to_c(e2->type));
}

// Parser represents f(void) as a single unnamed TYPE_VOID param; treat as no params.
static const Param *params_for_compat(const Type *fn_type)
{
    const Param *params = fn_type->u.function.params;
    if (params && !params->next && unalias(params->type)->kind == TYPE_VOID && !params->name)
        return NULL;
    return params;
}

static bool compatible_params(const Param *a, const Param *b)
{
    while (a && b) {
        if (!compatible_type(a->type, b->type))
            return false;
        a = a->next;
        b = b->next;
    }
    return a == NULL && b == NULL;
}

// Return true if src may initialize or be assigned to target (target is the lhs type).
bool compatible_type(const Type *target, const Type *src)
{
    if (!target && !src)
        return true;
    if (!target || !src)
        return false;
    // Look through global typedef references (kept by resolve_typedef_names) so a
    // nested `MyInt *` compares equal to `int *`.
    target = unalias(target);
    src    = unalias(src);
    if (target->kind != src->kind)
        return false;
    switch (target->kind) {
    case TYPE_FUNCTION: {
        if (target->u.function.variadic != src->u.function.variadic)
            return false;
        if (!compatible_type(target->u.function.return_type, src->u.function.return_type))
            return false;
        const Param *tp = params_for_compat(target);
        const Param *sp = params_for_compat(src);
        if (!tp || !sp)
            return true; // old-style or f(void): no prototype to compare
        return compatible_params(tp, sp);
    }
    case TYPE_POINTER:
        return compatible_type(target->u.pointer.target, src->u.pointer.target);
    case TYPE_ARRAY: {
        if (!compatible_type(target->u.array.element, src->u.array.element))
            return false;
        // Arrays are incompatible when both dimensions are known and differ
        // (int[3] vs int[4]). An unknown (incomplete) dimension on either side
        // stays compatible — get_array_size() returns 0 for an incomplete array.
        size_t ts = get_array_size(target), ss = get_array_size(src);
        return !(ts && ss && ts != ss);
    }
    case TYPE_COMPLEX:
    case TYPE_IMAGINARY:
        return compatible_type(target->u.complex.base, src->u.complex.base);
    case TYPE_ATOMIC:
        return compatible_type(target->u.atomic.base, src->u.atomic.base);
    case TYPE_STRUCT:
    case TYPE_UNION:
        if (is_coro_struct(target) || is_coro_struct(src))
            return is_coro_struct(target) && is_coro_struct(src) && same_frame_type(target, src);
        return strcmp(target->u.struct_t.name, src->u.struct_t.name) == 0;
    default:
        // Leaf types (scalars, enum, void, typedef name): the kind equality
        // checked above is sufficient. Type qualifiers (const/volatile/...) are
        // deliberately ignored: C assignment and argument compatibility operate
        // on the unqualified value type, so e.g. `char *` is compatible with
        // `const char *`. (compare_type is qualifier-strict and unsuitable here.)
        return true;
    }
}

// Convert an expression for assignment to target_type.
Expr *coerce_for_assignment(Expr *e, const Type *target_type, const char *context)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    const Type *e_type = unalias(e->type);
    target_type        = unalias(target_type);
    if (e_type->kind == target_type->kind && compatible_type(target_type, e_type))
        return e;
    if (is_arithmetic(e_type) && is_arithmetic(target_type))
        return convert_to_type(e, target_type);
    if (is_null_pointer_constant(e) && is_pointer(target_type))
        return convert_to_type(e, target_type);
    if ((target_type->kind == TYPE_POINTER && target_type->u.pointer.target->kind == TYPE_VOID &&
         is_pointer(e_type)) ||
        (is_pointer(target_type) && e_type->kind == TYPE_POINTER &&
         e_type->u.pointer.target->kind == TYPE_VOID)) {
        return convert_to_type(e, target_type);
    }
    // C11 §6.7.6.3p7: array param adjusts to pointer; int* arg matches int[N] param
    if (e_type->kind == TYPE_POINTER && target_type->kind == TYPE_ARRAY &&
        e_type->u.pointer.target->kind == target_type->u.array.element->kind)
        return e;
    fatal_error("cannot convert '%s' to '%s' when %s", type_to_c(e->type), type_to_c(target_type),
                context);
}

//
// A folded arithmetic constant: either an integer or a real.  C's usual arithmetic
// conversions let an operator's operands disagree, and an operator's result kind need
// not match its operands' (comparing two reals yields an int; casting a real to int
// yields an int), so the folder carries the value's kind along with it rather than
// committing to one up front.  try_eval_const_int/try_eval_const_real project the result.
//
// An integer value carries its C type kind and its canonical 64-bit representation:
// a signed kind is sign-extended to 64 bits, an unsigned kind is zero-extended (masked
// to its target width).  The kind decides whether an operator uses signed or unsigned
// arithmetic, so "(unsigned long)-1 > 0" folds to 1 even when unsigned long fills the
// full 64 bits.  This mirrors the TAC folder in optimize/const_fold.c
// (const_to_int64/const_to_uint64/make_int_const_val); the two folders must agree.
//
typedef struct {
    bool is_real;
    TypeKind kind; // integer type kind of the value, valid when !is_real
    uint64_t u;    // canonical bits (see above),      valid when !is_real
    double d;      //                                  valid when  is_real
    bool is_ld;    // a long double wider than double:  valid when  is_real
    Float128 q;    // its exact value (d is it rounded), valid when  is_ld
} ConstVal;

// Value width in bits of an integer type kind on the active target.  A signed kind uses
// the target's <type>_bits (a BESM-6 signed int is 41-bit inside a 48-bit word); an
// unsigned kind uses the full storage width, size*8.  Mirrors
// target_signed_bits/target_unsigned_bits in optimize/const_fold.c.  Returns 0 when no
// target is configured, and the caller then keeps the host's 64-bit arithmetic.
static int kind_value_bits(TypeKind k)
{
    if (!target_config)
        return 0;
    switch (k) {
    case TYPE_BOOL:
        return 1; // one *value* bit, whatever storage width get_size gives it
    case TYPE_CHAR:
    case TYPE_SCHAR:
    case TYPE_UCHAR:
        return 8;
    case TYPE_SHORT:
        return target_config->short_bits;
    case TYPE_USHORT:
        return (int)target_config->short_size * 8;
    case TYPE_INT:
    case TYPE_ENUM:
        return target_config->int_bits;
    case TYPE_UINT:
        return (int)target_config->int_size * 8;
    case TYPE_LONG:
        return target_config->long_bits;
    case TYPE_ULONG:
        return (int)target_config->long_size * 8;
    case TYPE_LONG_LONG:
        return target_config->llong_bits;
    case TYPE_ULONG_LONG:
        return (int)target_config->llong_size * 8;
    default:
        return 0;
    }
}

int integer_value_bits(const Type *t)
{
    return kind_value_bits(unalias(t)->kind);
}

// Value-signedness of an integer type kind; plain char follows the target (see is_signed).
static bool kind_is_unsigned(TypeKind k)
{
    switch (k) {
    case TYPE_BOOL:
    case TYPE_UCHAR:
    case TYPE_USHORT:
    case TYPE_UINT:
    case TYPE_ULONG:
    case TYPE_ULONG_LONG:
        return true;
    case TYPE_CHAR:
        return target_config && !target_config->char_signed;
    default:
        return false;
    }
}

long narrow_const_int(long val, const Type *t)
{
    TypeKind k = unalias(t)->kind;
    int width  = kind_value_bits(k);
    if (width <= 0)
        return val;
    if (kind_is_unsigned(k))
        return (long)unsigned_narrow((uint64_t)val, width);
    return (long)sign_narrow((uint64_t)val, width);
}

// Integer promotion (C11 §6.3.1.1p2) on a type kind: every sub-int kind promotes to int,
// except an unsigned short as wide as int (AVR), which promotes to unsigned int.  On
// BESM-6 USHORT->INT deliberately matches get_common_type's rule rather than a stricter
// reading of the standard (its unsigned short fills the word), so the folder and the
// typechecker always agree.
static TypeKind promote_kind(TypeKind k)
{
    switch (k) {
    case TYPE_BOOL:
    case TYPE_CHAR:
    case TYPE_SCHAR:
    case TYPE_UCHAR:
    case TYPE_SHORT:
    case TYPE_ENUM:
        return TYPE_INT;
    case TYPE_USHORT:
        return ushort_promotes_unsigned() ? TYPE_UINT : TYPE_INT;
    default:
        return k;
    }
}

// A borrowed Type singleton for a promoted integer kind, so the folder can reuse
// get_common_type and is guaranteed the same conversion rules as the typechecker.
static const Type *kind_type(TypeKind k)
{
    static const Type types[] = {
        [TYPE_INT]        = { .kind = TYPE_INT },
        [TYPE_UINT]       = { .kind = TYPE_UINT },
        [TYPE_LONG]       = { .kind = TYPE_LONG },
        [TYPE_ULONG]      = { .kind = TYPE_ULONG },
        [TYPE_LONG_LONG]  = { .kind = TYPE_LONG_LONG },
        [TYPE_ULONG_LONG] = { .kind = TYPE_ULONG_LONG },
    };
    return &types[k];
}

// The usual-arithmetic-conversions result kind for two promoted integer kinds.
static TypeKind common_kind(TypeKind k1, TypeKind k2)
{
    return get_common_type(kind_type(k1), kind_type(k2))->kind;
}

// The folded integer as a signed 64-bit value (its canonical form).
static int64_t cv_int64(const ConstVal *v)
{
    return (int64_t)v->u;
}

// Storage width in bits of an integer type kind on the active target: always size*8,
// even for a signed kind whose *value* width is narrower (BESM-6 signed int: 41 value
// bits inside 48 storage bits).
static int kind_storage_bits(TypeKind k)
{
    if (!target_config)
        return 0;
    switch (k) {
    case TYPE_SHORT:
        return (int)target_config->short_size * 8;
    case TYPE_INT:
    case TYPE_ENUM:
        return (int)target_config->int_size * 8;
    case TYPE_LONG:
        return (int)target_config->long_size * 8;
    case TYPE_LONG_LONG:
        return (int)target_config->llong_size * 8;
    default:
        return kind_value_bits(k); // unsigned/char/bool kinds: value width == storage
    }
}

// The folded integer as a 64-bit bit pattern for unsigned arithmetic.  On a target whose
// signed value width is narrower than the storage word (BESM-6: 41 bits inside 48), a
// *negative* signed value reduces to the unsigned view of its word — its two's-complement
// pattern zero-extended from the signed width, because the word's upper bits physically
// hold zeros and a signed→unsigned conversion is a plain word copy: (unsigned long)-1
// folds to 2^41-1 there, matching const_to_uint64 in optimize/const_fold.c and the code
// the backend emits.  On a conventional target (signed width == storage width) the
// sign-extended pattern is kept, so the C modulo conversion falls out when the caller
// re-narrows: (unsigned long long)-1 on x86_64 is 2^64-1.
static uint64_t cv_uint64(const ConstVal *v)
{
    if (!kind_is_unsigned(v->kind) && (int64_t)v->u < 0) {
        int width = kind_value_bits(v->kind);
        if (width > 0 && width < kind_storage_bits(v->kind))
            return unsigned_narrow(v->u, width);
    }
    return v->u;
}

// Store `bits` as a folded integer of kind `k`, wrapping to the kind's target value
// width and signedness (C11 §6.3.1.3), so "(char)300" folds to 44 rather than staying
// 300.  _Bool yields 0 or 1 (§6.3.1.2).  This is the one place results re-narrow;
// mirrors make_int_const_val in optimize/const_fold.c.
static void cv_set_int(ConstVal *out, TypeKind k, uint64_t bits)
{
    out->is_real = false;
    out->kind    = k;
    if (k == TYPE_BOOL) {
        out->u = bits != 0;
        return;
    }
    int width = kind_value_bits(k);
    out->u =
        kind_is_unsigned(k) ? unsigned_narrow(bits, width) : (uint64_t)sign_narrow(bits, width);
}

// The folded integer converted to integer kind `k` (C11 §6.3.1.3), as canonical bits.
static uint64_t cv_convert(const ConstVal *v, TypeKind k)
{
    ConstVal t;
    cv_set_int(&t, k, kind_is_unsigned(k) ? cv_uint64(v) : (uint64_t)cv_int64(v));
    return t.u;
}

// A folded real, rounded to the target's double (IEEE single on AVR).
static void cv_set_real(ConstVal *out, double d)
{
    out->is_real = true;
    out->is_ld   = false;
    out->d       = target_double_round(d);
}

// Whether long double is wider than double on the target, and so folds in binary128.
static bool wide_ld(void)
{
    return !target_config || target_config->ldouble_size > target_config->double_size;
}

static void cv_set_ld(ConstVal *out, Float128 q)
{
    if (!wide_ld()) {
        cv_set_real(out, f128_to_double(target_ld_round(q)));
        return;
    }
    q            = target_ld_round(q);
    out->is_real = true;
    out->is_ld   = true;
    out->q       = q;
    out->d       = f128_to_double(q);
}

// The value of a folded constant as a long double, whichever kind it holds.
static Float128 const_as_ld(const ConstVal *v)
{
    if (v->is_real)
        return v->is_ld ? v->q : f128_from_double(v->d);
    return kind_is_unsigned(v->kind) ? f128_from_u64(v->u) : f128_from_i64(cv_int64(v));
}

// Whether a folded constant is zero.
static bool cv_is_zero(const ConstVal *v)
{
    if (v->is_real)
        return v->is_ld ? f128_is_zero(v->q) : v->d == 0.0;
    return v->u == 0;
}

// The value of a folded constant as a real, whichever kind it holds.
static double const_as_real(const ConstVal *v)
{
    if (v->is_real)
        return v->d;
    return kind_is_unsigned(v->kind) ? target_double_from_u64(v->u)
                                     : target_double_from_i64(cv_int64(v));
}

// Round a folded real of float type to float precision where float is narrower than
// double, as the target computes it (FLT_EVAL_METHOD 0); cv_set_real has already done
// it where double is single too (AVR).  BESM-6 float is the double format and keeps
// the value.
static void round_float(TypeKind kind, ConstVal *v)
{
    if (v->is_real && kind == TYPE_FLOAT && target_config &&
        target_config->float_size < target_config->double_size)
        // cppcheck-suppress suspiciousFloatingPointCast ; rounding to float is the point
        v->d = (float)v->d;
}

// Fold a binary operator whose operands underwent the usual arithmetic conversions to a
// real type.  Arithmetic yields a real; the comparisons and the logical operators yield
// an int.  The remaining operators require integer operands and do not fold.
static bool fold_real_binop(BinaryOp op, double left, double right, ConstVal *out)
{
    switch (op) {
    case BINARY_MUL:
        cv_set_real(out, left * right);
        return true;
    case BINARY_DIV:
        // Division by zero is not a constant expression; reject rather than fold an
        // infinity into a static initializer.  Mirrors the integer guard below.
        if (right == 0.0)
            return false;
        cv_set_real(out, left / right);
        return true;
    case BINARY_ADD:
        cv_set_real(out, left + right);
        return true;
    case BINARY_SUB:
        cv_set_real(out, left - right);
        return true;
    case BINARY_LT:
        cv_set_int(out, TYPE_INT, left < right);
        return true;
    case BINARY_GT:
        cv_set_int(out, TYPE_INT, left > right);
        return true;
    case BINARY_LE:
        cv_set_int(out, TYPE_INT, left <= right);
        return true;
    case BINARY_GE:
        cv_set_int(out, TYPE_INT, left >= right);
        return true;
    case BINARY_EQ:
        cv_set_int(out, TYPE_INT, left == right);
        return true;
    case BINARY_NE:
        cv_set_int(out, TYPE_INT, left != right);
        return true;
    case BINARY_LOG_AND:
        cv_set_int(out, TYPE_INT, left != 0.0 && right != 0.0);
        return true;
    case BINARY_LOG_OR:
        cv_set_int(out, TYPE_INT, left != 0.0 || right != 0.0);
        return true;
    default:
        // BINARY_MOD, the shifts and the bitwise operators need integer operands.
        return false;
    }
}

// fold_real_binop for long double operands, in binary128.
static bool fold_ld_binop(BinaryOp op, const ConstVal *l, const ConstVal *r, ConstVal *out)
{
    Float128 a = const_as_ld(l), b = const_as_ld(r);
    int c = f128_cmp(a, b); // 2 when unordered
    switch (op) {
    case BINARY_MUL:
        cv_set_ld(out, f128_mul(a, b));
        return true;
    case BINARY_DIV:
        if (f128_is_zero(b))
            return false;
        cv_set_ld(out, f128_div(a, b));
        return true;
    case BINARY_ADD:
        cv_set_ld(out, f128_add(a, b));
        return true;
    case BINARY_SUB:
        cv_set_ld(out, f128_sub(a, b));
        return true;
    case BINARY_LT:
        cv_set_int(out, TYPE_INT, c == -1);
        return true;
    case BINARY_GT:
        cv_set_int(out, TYPE_INT, c == 1);
        return true;
    case BINARY_LE:
        cv_set_int(out, TYPE_INT, c == -1 || c == 0);
        return true;
    case BINARY_GE:
        cv_set_int(out, TYPE_INT, c == 1 || c == 0);
        return true;
    case BINARY_EQ:
        cv_set_int(out, TYPE_INT, c == 0);
        return true;
    case BINARY_NE:
        cv_set_int(out, TYPE_INT, c != 0);
        return true;
    case BINARY_LOG_AND:
        cv_set_int(out, TYPE_INT, !cv_is_zero(l) && !cv_is_zero(r));
        return true;
    case BINARY_LOG_OR:
        cv_set_int(out, TYPE_INT, !cv_is_zero(l) || !cv_is_zero(r));
        return true;
    default:
        return false;
    }
}

// Evaluate a constant arithmetic expression; return false if not constant.
static bool eval_const(const Expr *e, ConstVal *out)
{
    switch (e->kind) {
    case EXPR_LITERAL:
        // A literal loads unmasked (an out-of-target-range positive literal keeps its
        // value, as in optimize/const_fold.c); results re-narrow through cv_set_int.
        out->is_real = false;
        switch (e->u.literal->kind) {
        case LITERAL_INT:
            out->kind = TYPE_INT;
            out->u    = (uint64_t)e->u.literal->u.int_val;
            return true;
        case LITERAL_LONG:
            out->kind = TYPE_LONG;
            out->u    = (uint64_t)(int64_t)e->u.literal->u.long_val;
            return true;
        case LITERAL_LONG_LONG:
            out->kind = TYPE_LONG_LONG;
            out->u    = (uint64_t)(int64_t)e->u.literal->u.long_long_val;
            return true;
        case LITERAL_UINT:
            out->kind = TYPE_UINT;
            out->u    = e->u.literal->u.uint_val;
            return true;
        case LITERAL_ULONG:
            out->kind = TYPE_ULONG;
            out->u    = e->u.literal->u.ulong_val;
            return true;
        case LITERAL_ULONG_LONG:
            out->kind = TYPE_ULONG_LONG;
            out->u    = e->u.literal->u.ulong_long_val;
            return true;
        case LITERAL_CHAR:
            // A character constant has type int (C11 §6.4.4.4p10).
            out->kind = TYPE_INT;
            out->u    = (unsigned char)e->u.literal->u.char_val;
            return true;
        case LITERAL_ENUM: {
            const Symbol *sym = symtab_get_opt(e->u.literal->u.enum_const);
            if (!sym)
                return false;
            // An enum constant has type int (C11 §6.7.2.2p3).
            out->kind = TYPE_INT;
            out->u    = (uint64_t)(int64_t)sym->u.enum_val;
            return true;
        }
        case LITERAL_LONG_DOUBLE:
            cv_set_ld(out, e->u.literal->u.long_double_val);
            return true;
        case LITERAL_FLOAT:
        case LITERAL_DOUBLE:
            cv_set_real(out, literal_to_double(e->u.literal));
            if (e->u.literal->kind == LITERAL_FLOAT)
                round_float(TYPE_FLOAT, out);
            return true;
        default:
            return false;
        }
    case EXPR_CAST: {
        ConstVal v;
        if (!eval_const(e->u.cast.expr, &v))
            return false;
        const Type *ct = unalias(e->u.cast.type);
        if (is_integer(ct)) {
            uint64_t bits;
            if (v.is_real && v.is_ld)
                bits = v.q.hi >> 63 ? (uint64_t)f128_to_i64(v.q, 64) : f128_to_u64(v.q, 64);
            else if (v.is_real)
                // C11 §6.3.1.4: a real converts to an integer by truncation toward zero
                // (via int64_t when negative — a direct uint64_t conversion would be UB).
                bits = v.d < 0 ? (uint64_t)(int64_t)v.d : (uint64_t)v.d;
            else
                bits = kind_is_unsigned(ct->kind) ? cv_uint64(&v) : (uint64_t)cv_int64(&v);
            cv_set_int(out, ct->kind, bits);
            return true;
        }
        if (ct->kind == TYPE_FLOAT) {
            // A cast to float rounds to float precision, and the folded value keeps it.
            float rounded = v.is_real && v.is_ld ? f128_to_float(v.q) : (float)const_as_real(&v);
            cv_set_real(out, rounded);
            return true;
        }
        if (ct->kind == TYPE_LONG_DOUBLE) {
            cv_set_ld(out, const_as_ld(&v));
            return true;
        }
        if (ct->kind == TYPE_DOUBLE) {
            cv_set_real(out, const_as_real(&v));
            return true;
        }
        // A cast to a non-arithmetic type passes an integer operand through unchanged:
        // build_static_init folds an address constant such as "(char *)0x4000" this way
        // and then re-inspects the cast node itself.
        if (v.is_real)
            return false;
        *out = v;
        return true;
    }
    case EXPR_UNARY_OP: {
        ConstVal v;
        if (!eval_const(e->u.unary_op.expr, &v))
            return false;
        switch (e->u.unary_op.op) {
        case UNARY_NEG: {
            if (v.is_real && v.is_ld) {
                cv_set_ld(out, f128_neg(v.q));
                return true;
            }
            if (v.is_real) {
                cv_set_real(out, -v.d);
                return true;
            }
            // The result has the promoted operand type; a negated unsigned wraps
            // (C11 §6.2.5p9): -1u folds to UINT_MAX, not -1.
            TypeKind k = promote_kind(v.kind);
            cv_set_int(out, k, 0 - cv_convert(&v, k));
            return true;
        }
        case UNARY_PLUS:
            *out = v;
            if (!v.is_real) {
                TypeKind k = promote_kind(v.kind);
                cv_set_int(out, k, cv_convert(&v, k));
            }
            return true;
        case UNARY_BIT_NOT: {
            if (v.is_real)
                return false; // ~ requires an integer operand
            TypeKind k = promote_kind(v.kind);
            cv_set_int(out, k, ~cv_convert(&v, k));
            return true;
        }
        case UNARY_LOG_NOT:
            // ! yields an int whatever the operand's type is (C11 §6.5.3.3p5).
            cv_set_int(out, TYPE_INT, cv_is_zero(&v));
            return true;
        default:
            return false;
        }
    }
    case EXPR_BINARY_OP: {
        ConstVal l, r;
        if (!eval_const(e->u.binary_op.left, &l) || !eval_const(e->u.binary_op.right, &r))
            return false;
        if ((l.is_real && l.is_ld) || (r.is_real && r.is_ld))
            return fold_ld_binop(e->u.binary_op.op, &l, &r, out);
        if (l.is_real || r.is_real) {
            if (!fold_real_binop(e->u.binary_op.op, const_as_real(&l), const_as_real(&r), out))
                return false;
            if (e->type)
                round_float(unalias(e->type)->kind, out);
            return true;
        }

        // Both operands convert to the usual-arithmetic-conversions result kind; that
        // kind's signedness picks signed or unsigned division, remainder and comparison
        // (so "-1 < 1u" folds to 0).  The wrapping operators compute on the 64-bit
        // pattern either way and re-narrow through cv_set_int.  The shifts are the
        // exception: each operand converts independently and the result has the
        // promoted *left* operand's kind (C11 §6.5.7p3).
        TypeKind k  = common_kind(promote_kind(l.kind), promote_kind(r.kind));
        bool uns    = kind_is_unsigned(k);
        uint64_t ul = cv_convert(&l, k);
        uint64_t ur = cv_convert(&r, k);
        int64_t sl  = (int64_t)ul;
        int64_t sr  = (int64_t)ur;
        switch (e->u.binary_op.op) {
        case BINARY_MUL:
            cv_set_int(out, k, ul * ur);
            return true;
        case BINARY_DIV:
        case BINARY_MOD: {
            bool is_div = e->u.binary_op.op == BINARY_DIV;
            if (ur == 0)
                return false;
            uint64_t bits;
            if (uns)
                bits = is_div ? ul / ur : ul % ur;
            else if (sr == -1)
                // x/-1 wraps as negation and x%-1 is 0; dividing INT64_MIN by -1
                // directly would trap on the host.
                bits = is_div ? 0 - ul : 0;
            else
                bits = (uint64_t)(is_div ? sl / sr : sl % sr);
            cv_set_int(out, k, bits);
            return true;
        }
        case BINARY_ADD:
            cv_set_int(out, k, ul + ur);
            return true;
        case BINARY_SUB:
            cv_set_int(out, k, ul - ur);
            return true;
        case BINARY_LEFT_SHIFT:
        case BINARY_RIGHT_SHIFT: {
            TypeKind lk   = promote_kind(l.kind);
            int64_t count = cv_int64(&r);
            if (count < 0 || count >= 64)
                return false; // shift out of range: not a constant expression
            uint64_t lbits = cv_convert(&l, lk);
            uint64_t bits;
            if (e->u.binary_op.op == BINARY_LEFT_SHIFT) {
                bits = lbits << count;
            } else if (kind_is_unsigned(lk)) {
                bits = lbits >> count; // canonical unsigned bits are already masked
            } else if (target_config && target_config->right_shift_is_logical) {
                // The target (BESM-6) shifts right logically even for signed operands:
                // zero-fill the operand's target-width bit pattern, matching the shift
                // the backend emits.  Mirrors optimize/const_fold.c.
                bits = unsigned_narrow(lbits, kind_value_bits(lk)) >> count;
            } else {
                bits = (uint64_t)((int64_t)lbits >> count); // arithmetic, sign-preserving
            }
            cv_set_int(out, lk, bits);
            return true;
        }
        case BINARY_LT:
            cv_set_int(out, TYPE_INT, uns ? ul < ur : sl < sr);
            return true;
        case BINARY_GT:
            cv_set_int(out, TYPE_INT, uns ? ul > ur : sl > sr);
            return true;
        case BINARY_LE:
            cv_set_int(out, TYPE_INT, uns ? ul <= ur : sl <= sr);
            return true;
        case BINARY_GE:
            cv_set_int(out, TYPE_INT, uns ? ul >= ur : sl >= sr);
            return true;
        case BINARY_EQ:
            cv_set_int(out, TYPE_INT, ul == ur);
            return true;
        case BINARY_NE:
            cv_set_int(out, TYPE_INT, ul != ur);
            return true;
        case BINARY_BIT_AND:
            cv_set_int(out, k, ul & ur);
            return true;
        case BINARY_BIT_XOR:
            cv_set_int(out, k, ul ^ ur);
            return true;
        case BINARY_BIT_OR:
            cv_set_int(out, k, ul | ur);
            return true;
        case BINARY_LOG_AND:
            cv_set_int(out, TYPE_INT, ul != 0 && ur != 0);
            return true;
        case BINARY_LOG_OR:
            cv_set_int(out, TYPE_INT, ul != 0 || ur != 0);
            return true;
        default:
            return false;
        }
    }
    // sizeof and _Alignof yield size_t, an unsigned type; size_kind() matches the type
    // the typechecker annotates on these expressions (see semantic/expressions.c).
    case EXPR_SIZEOF_TYPE:
        cv_set_int(out, size_kind(), get_size(e->u.sizeof_type));
        return true;
    case EXPR_SIZEOF_EXPR:
        if (e->u.sizeof_expr->type) {
            cv_set_int(out, size_kind(), get_size(e->u.sizeof_expr->type));
            return true;
        }
        return false;
    case EXPR_ALIGNOF:
        cv_set_int(out, size_kind(), get_alignment(e->u.align_of));
        return true;
    case EXPR_VA_CLASS:
        cv_set_int(out, TYPE_INT, va_class_of(e->u.va_class));
        return true;
    default:
        return false;
    }
}

// Evaluate a constant integer expression; return false if not constant.  A folded real
// value is not an integer constant expression (C11 §6.6p6) — "int a[1.5];" stays an
// error; only an explicit cast, "(int)1.5", makes it integral.
bool try_eval_const_int(const Expr *e, long *out)
{
    ConstVal v;
    if (!eval_const(e, &v) || v.is_real)
        return false;
    // The canonical bit pattern converts to the caller's long; an unsigned value at the
    // host's full width comes out sign-flipped, exactly as the C conversion would do it.
    *out = (long)v.u;
    return true;
}

// Evaluate a constant arithmetic expression as a real; an integral result converts.
bool try_eval_const_real(const Expr *e, double *out)
{
    ConstVal v;
    if (!eval_const(e, &v))
        return false;
    *out = const_as_real(&v);
    return true;
}

// Evaluate a constant arithmetic expression as a long double.
bool try_eval_const_ld(const Expr *e, Float128 *out)
{
    ConstVal v;
    if (!eval_const(e, &v))
        return false;
    *out = const_as_ld(&v);
    return true;
}

// Type-check a global declaration and label its loops.  Loop labels draw from the
// caller-owned unit-wide counter *seq (shared with the translator's temporaries).
void typecheck_decl(ExternalDecl *d, int *seq)
{
    typecheck_global_decl(d);
    label_loops(d, seq);
    resolve_labels(d);
    check_defers(d);
}

// Type-check an entire program.
void typecheck_program(const Program *p)
{
    int seq = 0;
    for (ExternalDecl *d = p->decls; d; d = d->next) {
        typecheck_decl(d, &seq);
    }
}
