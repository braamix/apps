//
// Type-checking for initializers.
//
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "c_escape.h"
#include "semantic.h"
#include "structtab.h"
#include "symtab.h"
#include "target.h"
#include "typecheck.h"
#include "xalloc.h"

// Create a zero initializer for a type.
static Initializer *make_zero_init(Type *t);

// The zeros for the rest of a union's storage past its initialized member, or NULL: a
// second item, an array at the member's end, of int where the sizes allow.  The static
// path zero-pads the same way, and GCC and clang zero the whole union (§6.7.9p10 zeroes
// only the first member): code reads another member of `T x = { .kind = K };`.
static InitItem *union_rest(const Type *u, const Type *member)
{
    int size = (int)structtab_find(u->u.struct_t.name)->size;
    int used = (int)get_size(member);
    if (size <= used)
        return NULL;
    int unit             = (int)target_config->int_size;
    bool word            = used % unit == 0 && size % unit == 0;
    Type *pad            = new_type(TYPE_ARRAY, __func__, __FILE__, __LINE__);
    pad->u.array.element = new_type(word ? TYPE_INT : TYPE_UCHAR, __func__, __FILE__, __LINE__);
    set_array_size(pad, word ? (size - used) / unit : size - used);
    InitItem *rest = new_init_item(NULL, make_zero_init(pad));
    rest->offset   = used;
    free_type(pad);
    return rest;
}

static Initializer *make_zero_init(Type *t)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    t = (Type *)unalias(t); // look through global typedef references (read-only use)
    if (t->kind == TYPE_ARRAY) {
        Initializer *init = new_initializer(INITIALIZER_COMPOUND);
        init->type        = clone_type(t, __func__, __FILE__, __LINE__);

        InitItem **tail = &init->u.items;
        size_t size     = get_array_size(t);
        for (size_t i = 0; i < size; i++) {
            InitItem *item = new_init_item(NULL, make_zero_init(t->u.array.element));
            *tail          = item;
            tail           = &item->next;
        }
        return init;
    }
    if (t->kind == TYPE_STRUCT) {
        Initializer *init = new_initializer(INITIALIZER_COMPOUND);
        init->type        = clone_type(t, __func__, __FILE__, __LINE__);

        InitItem **tail   = &init->u.items;
        FieldDef *members = structtab_find(t->u.struct_t.name)->members;
        for (; members; members = members->next) {
            InitItem *item = new_init_item(NULL, make_zero_init(members->type));
            // Stash the member's byte offset, as typecheck_init does for an explicit
            // initializer.  gen_compound_init's struct branch addresses each member at
            // base_offset + item->offset; without this, every member of a zero-filled
            // nested struct collapses onto offset 0.
            item->offset = members->offset;
            item->bf     = members->bf;
            *tail        = item;
            tail         = &item->next;
        }
        return init;
    }
    if (t->kind == TYPE_UNION) {
        // Zero the first member, then the rest of the union's storage.
        Initializer *init   = new_initializer(INITIALIZER_COMPOUND);
        init->type          = clone_type(t, __func__, __FILE__, __LINE__);
        FieldDef *first     = structtab_find(t->u.struct_t.name)->members;
        init->u.items       = new_init_item(NULL, make_zero_init(first->type));
        init->u.items->next = union_rest(t, first->type);
        return init;
    }

    Initializer *init  = new_initializer(INITIALIZER_SINGLE);
    init->type         = clone_type(t, __func__, __FILE__, __LINE__);
    init->u.expr       = new_expression(EXPR_LITERAL);
    init->u.expr->type = clone_type(t, __func__, __FILE__, __LINE__);
    switch (t->kind) {
    case TYPE_CHAR:
    case TYPE_SCHAR:
    case TYPE_UCHAR:
        init->u.expr->u.literal = new_literal(LITERAL_CHAR);
        break;
    case TYPE_BOOL:
    case TYPE_SHORT:
    case TYPE_USHORT:
    case TYPE_INT:
    case TYPE_ENUM:
        init->u.expr->u.literal = new_literal(LITERAL_INT);
        break;
    case TYPE_UINT:
        init->u.expr->u.literal = new_literal(LITERAL_UINT);
        break;
    case TYPE_LONG:
        init->u.expr->u.literal = new_literal(LITERAL_LONG);
        break;
    case TYPE_LONG_LONG:
        init->u.expr->u.literal = new_literal(LITERAL_LONG_LONG);
        break;
    case TYPE_ULONG:
        init->u.expr->u.literal = new_literal(LITERAL_ULONG);
        break;
    case TYPE_ULONG_LONG:
        init->u.expr->u.literal = new_literal(LITERAL_ULONG_LONG);
        break;
    case TYPE_FLOAT:
        init->u.expr->u.literal = new_literal(LITERAL_FLOAT);
        break;
    case TYPE_DOUBLE:
        init->u.expr->u.literal = new_literal(LITERAL_DOUBLE);
        break;
    case TYPE_LONG_DOUBLE:
        init->u.expr->u.literal = new_literal(LITERAL_LONG_DOUBLE);
        break;
    case TYPE_POINTER:
        init->u.expr->u.literal = new_literal(LITERAL_INT); // Null pointer
        break;
    default:
        internal_error("unsupported type for zero init: %d", t->kind);
    }
    return init;
}

// --- static address-constant evaluation --------------------------------------
//
// A static pointer initializer must be an address constant (C11 §6.6): the address of a
// static-storage object, optionally displaced by constant subscripting, member selection, and
// integer pointer arithmetic, composed in any order (`&s.v[2]`, `&arr[1].b`, `arr + 2`, ...).
// The two helpers fold such an expression to the triple (base symbol name, linear byte offset
// from that symbol, type), so build_static_init can emit a single POINTER/FAT_POINTER relocation
// instead of pattern-matching each syntactic shape.  Both return false for anything that is not
// a compile-time address constant, letting the caller fall back to null-pointer / integer-cast
// handling; a malformed address constant (non-constant subscript, unknown member) is a
// fatal_error, since build_static_init is the validation point for static initializers.

// Materialize a file-scope compound literal as an anonymous static object (C11 §6.5.2.5p5)
// and return its name.  A block-scope literal has automatic storage: not a constant.
static const char *static_compound_literal(Expr *e)
{
    if (scope_level > 0)
        fatal_error("initializer element is not a constant expression");
    Type *t                    = check_type_name(e->u.compound_literal.type);
    Initializer *init          = new_initializer(INITIALIZER_COMPOUND);
    init->u.items              = e->u.compound_literal.init;
    e->u.compound_literal.type = t;
    e->u.compound_literal.init = NULL;
    Tac_StaticInit *data       = build_static_init(t, &init);
    free_initializer(init);
    char *name      = symtab_add_compound_literal(t, data);
    const char *ret = symtab_get(name)->name;
    xfree(name);
    return ret;
}

// Fold an lvalue expression to the storage it names: (base symbol, byte offset, object type).
static bool eval_lvalue_addr(const Expr *e, const char **name, long *off, const Type **type)
{
    switch (e->kind) {
    case EXPR_VAR: {
        const Symbol *sym = symtab_get(e->u.var);
        if (!sym)
            return false;
        *name = e->u.var;
        *off  = 0;
        *type = unalias(sym->type);
        return true;
    }
    case EXPR_COMPOUND:
        *name = static_compound_literal((Expr *)e);
        *off  = 0;
        *type = unalias(symtab_get(*name)->type);
        return true;
    case EXPR_FIELD_ACCESS: {
        const Type *base_type;
        if (!eval_lvalue_addr(e->u.field_access.expr, name, off, &base_type))
            return false;
        if (base_type->kind != TYPE_STRUCT && base_type->kind != TYPE_UNION)
            fatal_error("member reference base type '%s' is not a structure or union",
                        type_to_c(base_type));
        const FieldDef *member = structtab_find(base_type->u.struct_t.name)->members;
        for (; member; member = member->next) {
            if (strcmp(member->name, e->u.field_access.field) == 0)
                break;
        }
        if (!member)
            fatal_error("no member named '%s' in '%s'", e->u.field_access.field,
                        type_to_c(base_type));
        assert(member);
        *off += member->offset; // field offsets are byte offsets within the struct
        *type = unalias(member->type);
        return true;
    }
    case EXPR_SUBSCRIPT: {
        const Type *base_type;
        if (!eval_lvalue_addr(e->u.subscript.left, name, off, &base_type))
            return false;
        // Only an array has a constant element address; subscripting a runtime pointer
        // (its value is a separate object) is not an address constant.
        if (base_type->kind != TYPE_ARRAY)
            return false;
        long index;
        if (!try_eval_const_int(e->u.subscript.right, &index))
            fatal_error("array subscript in an initializer is not a constant expression");
        const Type *element = unalias(base_type->u.array.element);
        *off += index * (long)get_size(element);
        *type = element;
        return true;
    }
    default:
        return false;
    }
}

// Fold a pointer-valued address constant to (base symbol, byte offset, pointee type).
static bool eval_addr_const(const Expr *e, const char **name, long *off, const Type **pointee)
{
    switch (e->kind) {
    case EXPR_UNARY_OP:
        // &lvalue: the pointer points at the addressed object.
        if (e->u.unary_op.op != UNARY_ADDRESS)
            return false;
        return eval_lvalue_addr(e->u.unary_op.expr, name, off, pointee);
    case EXPR_VAR:
    case EXPR_COMPOUND: {
        // An array or function name, or an array literal, decays to a pointer to its first
        // element / to the function.  A scalar's value is not an address constant.
        const Type *t;
        if (!eval_lvalue_addr(e, name, off, &t))
            return false;
        if (t->kind == TYPE_ARRAY) {
            *pointee = unalias(t->u.array.element);
            return true;
        }
        if (t->kind == TYPE_FUNCTION) {
            *pointee = t;
            return true;
        }
        return false;
    }
    case EXPR_BINARY_OP: {
        BinaryOp op = e->u.binary_op.op;
        if (op != BINARY_ADD && op != BINARY_SUB)
            return false;
        // (address ± constant); '+' also commutes as (constant + address), '-' does not.
        const Expr *addr_side = e->u.binary_op.left;
        const Expr *int_side  = e->u.binary_op.right;
        if (!eval_addr_const(addr_side, name, off, pointee)) {
            if (op != BINARY_ADD)
                return false;
            addr_side = e->u.binary_op.right;
            int_side  = e->u.binary_op.left;
            if (!eval_addr_const(addr_side, name, off, pointee))
                return false;
        }
        long delta;
        if (!try_eval_const_int(int_side, &delta))
            return false;
        *off += (op == BINARY_SUB ? -delta : delta) * (long)get_size(*pointee);
        return true;
    }
    default:
        return false;
    }
}

// The member a canonical union item initializes: the one its designator names, or the
// first (see init_normalize.c).
static const FieldDef *union_member(const Type *t, const InitItem *item)
{
    const FieldDef *member = structtab_find(t->u.struct_t.name)->members;
    if (item->designators) {
        while (strcmp(member->name, item->designators->u.name) != 0) {
            member = member->next;
        }
    }
    return member;
}

// Append list to *current and return the new tail.
static Tac_StaticInit **append_static_init(Tac_StaticInit **current, Tac_StaticInit *list)
{
    *current = list;
    while (*current) {
        current = &(*current)->next;
    }
    return current;
}

// Append a ZERO run of n bytes (if any) and return the new tail.
static Tac_StaticInit **append_zero(Tac_StaticInit **current, size_t n)
{
    if (n == 0) {
        return current;
    }
    Tac_StaticInit *zero = tac_new_static_init(TAC_STATIC_INIT_ZERO);
    zero->u.zero_bytes   = n;
    *current             = zero;
    return &zero->next;
}

// Convert a canonical initializer (see init_normalize.c) to a Tac_StaticInit list.
// The bytes of a run of initialized bit-fields: bytes[lo, hi) of the struct hold bits
// of them; lo is -1 while there are none.
typedef struct {
    unsigned char *bytes; // as many as the struct has, zeroed
    int lo, hi;
} BitImage;

// The integer value of a scalar static initializer, which it frees.
static uint64_t static_int_value(Tac_StaticInit *si)
{
    uint64_t v;
    switch (si->kind) {
    case TAC_STATIC_INIT_I8:
        v = (uint64_t)si->u.char_val;
        break;
    case TAC_STATIC_INIT_I16:
        v = (uint64_t)si->u.short_val;
        break;
    case TAC_STATIC_INIT_I32:
        v = (uint64_t)si->u.int_val;
        break;
    case TAC_STATIC_INIT_I64:
        v = (uint64_t)si->u.long_val;
        break;
    case TAC_STATIC_INIT_U8:
        v = si->u.uchar_val;
        break;
    case TAC_STATIC_INIT_U16:
        v = si->u.ushort_val;
        break;
    case TAC_STATIC_INIT_U32:
        v = si->u.uint_val;
        break;
    case TAC_STATIC_INIT_U64:
        v = si->u.ulong_val;
        break;
    case TAC_STATIC_INIT_ZERO:
        v = 0;
        break;
    default:
        fatal_error("initializer of a bit-field is not an integer constant expression");
    }
    tac_free_static_init(si);
    return v;
}

// Put the low bits of `v` into bit-field `f` of a struct of `size` bytes, in memory order:
// from the least significant bit of the unit up on a little-endian target, from the most
// significant one down on a big-endian one (BitField.pos counts in the unit's value).
static void put_bitfield(BitImage *img, int size, const FieldDef *f, uint64_t v)
{
    if (!img->bytes) {
        img->bytes = xalloc((size_t)size + 1, __func__, __FILE__, __LINE__);
        memset(img->bytes, 0, (size_t)size + 1);
    }
    int unit_bits = f->bf.unit_size * 8;
    bool big      = target_config && target_config->big_endian;
    for (int k = 0; k < f->bf.width; k++) {
        int bit = f->bf.pos + k; // in the unit's value, from its least significant bit
        // The byte of the unit holding it, and the bit in that byte.
        int byte = big ? (unit_bits - 1 - bit) / 8 : bit / 8;
        int at   = f->offset + byte;
        if ((v >> k) & 1)
            img->bytes[at] |= (unsigned char)(1u << (bit % 8));
        if (img->lo < 0 || at < img->lo)
            img->lo = at;
        if (at + 1 > img->hi)
            img->hi = at + 1;
    }
}

// Emit the pending bit-field bytes after `*offset`, and advance it past them.
static Tac_StaticInit **flush_bits(BitImage *img, Tac_StaticInit **current, int *offset)
{
    if (img->lo < 0)
        return current;
    current = append_zero(current, img->lo - *offset);
    for (int i = img->lo; i < img->hi; i++) {
        Tac_StaticInit *b = tac_new_static_init(TAC_STATIC_INIT_U8);
        b->u.uchar_val    = img->bytes[i];
        img->bytes[i]     = 0;
        current           = append_static_init(current, b);
    }
    *offset = img->hi;
    img->lo = img->hi = -1;
    return current;
}

static Tac_StaticInit *static_init(Type *var_type, const Initializer *init);

static Tac_StaticInit *static_init_at(Type *var_type, const Initializer *init)
{
    // Look through a global typedef reference. Reads use the resolved type; the only
    // in-place mutation (set_array_size, below) is reached solely for a genuine
    // unspecified-size array, never for a (complete) typedef'd array, so it never
    // touches a shared typetab entry.
    var_type = (Type *)unalias(var_type);

    // Handle null initializer: initialize with zeros.
    if (!init) {
        Tac_StaticInit *zero_init = tac_new_static_init(TAC_STATIC_INIT_ZERO);
        zero_init->u.zero_bytes   = get_size(var_type);
        return zero_init;
    }

    // Handle array initialized with a string literal.
    if (var_type->kind == TYPE_ARRAY && init->kind == INITIALIZER_SINGLE &&
        init->u.expr->kind == EXPR_LITERAL && init->u.expr->u.literal->kind == LITERAL_STRING) {
        const Type *element_type = unalias(var_type->u.array.element);
        if (element_type->kind != TYPE_CHAR && element_type->kind != TYPE_SCHAR &&
            element_type->kind != TYPE_UCHAR) {
            fatal_error("cannot initialize array of type '%s' with a string literal",
                        type_to_c(var_type));
        }
        size_t string_length;
        char *decoded =
            c_decode_string_literal(init->u.expr->u.literal->u.string_val, &string_length);
        size_t array_size;
        if (!var_type->u.array.size) {
            // Array size is not specified - use string size.
            array_size = string_length + 1;
            set_array_size(var_type, array_size);
        } else {
            array_size = get_array_size(var_type);
            if (string_length > array_size) {
                xfree(decoded);
                fatal_error("initializer string for array of type '%s' is too long",
                            type_to_c(var_type));
            }
        }

        Tac_StaticInit *string_init           = tac_new_static_init(TAC_STATIC_INIT_STRING);
        string_init->u.string.val             = decoded;
        string_init->u.string.len             = string_length;
        string_init->u.string.null_terminated = (array_size >= string_length + 1);
        if (array_size > string_length + 1) {
            Tac_StaticInit *zero_padding = tac_new_static_init(TAC_STATIC_INIT_ZERO);
            zero_padding->u.zero_bytes =
                (array_size - (string_length + 1)) * get_size(element_type);
            string_init->next = zero_padding;
        }
        return string_init;
    }

    // Handle pointer initialized with a string literal: char * or void *, as on the
    // automatic path.
    if (var_type->kind == TYPE_POINTER && init->kind == INITIALIZER_SINGLE &&
        init->u.expr->kind == EXPR_LITERAL && init->u.expr->u.literal->kind == LITERAL_STRING) {
        TypeKind target_kind = unalias(var_type->u.pointer.target)->kind;
        if (target_kind != TYPE_CHAR && target_kind != TYPE_VOID) {
            fatal_error("cannot initialize '%s' with a string literal", type_to_c(var_type));
        }
        size_t decoded_length;
        char *decoded =
            c_decode_string_literal(init->u.expr->u.literal->u.string_val, &decoded_length);
        char *string_id = symtab_add_string(decoded, decoded_length);
        xfree(decoded);
        // A char*/void* is a fat pointer.  A string decays to its first byte, which is
        // packed in the MSB (byte#0), so byte_offset 0 yields offset_enc 5.
        Tac_StaticInit *pointer_init        = tac_new_static_init(TAC_STATIC_INIT_FAT_POINTER);
        pointer_init->u.pointer.name        = string_id;
        pointer_init->u.pointer.byte_offset = 0;
        return pointer_init;
    }

    // A coro_ptr initialized with a coroutine's name: the address of its descriptor.
    if (var_type->kind == TYPE_POINTER && init->kind == INITIALIZER_SINGLE &&
        init->u.expr->kind == EXPR_VAR && coro_desc_target(var_type)) {
        const Symbol *sym = symtab_get(init->u.expr->u.var);
        if (sym->kind == SYM_FUNC && sym->u.func.coro) {
            Expr probe = { .kind = EXPR_VAR, .u.var = init->u.expr->u.var };
            coroutine_value(&probe, sym); // checks it has a coro_ptr, and types it
            bool same = compatible_type(var_type, probe.type);
            free_type(probe.type);
            if (!same)
                fatal_error("incompatible pointer types when initializing '%s'",
                            type_to_c(var_type));
            size_t n                     = strlen(sym->name);
            Tac_StaticInit *pointer_init = tac_new_static_init(TAC_STATIC_INIT_POINTER);
            pointer_init->u.pointer.name = xalloc(n + sizeof "$co", __func__, __FILE__, __LINE__);
            memcpy(pointer_init->u.pointer.name, sym->name, n);
            memcpy(pointer_init->u.pointer.name + n, "$co", sizeof "$co");
            return pointer_init;
        }
    }

    // Handle a pointer initialized with a constant address expression (C11 §6.6): an array or
    // function name (decay), &lvalue, and constant pointer arithmetic, composed in any order —
    // e.g. `arr + 2`, `&arr[1] + 1`, `&s.v[2]`, `&o.in.y`, `&arr[1].b`.  eval_addr_const folds
    // the whole expression to a base symbol and a linear byte offset from it.
    if (var_type->kind == TYPE_POINTER && init->kind == INITIALIZER_SINGLE) {
        const char *base;
        long off;
        const Type *pointee;
        if (eval_addr_const(init->u.expr, &base, &off, &pointee)) {
            const Type *target = unalias(var_type->u.pointer.target);
            // A void pointer is compatible with any object address, and any pointer accepts a
            // void address; otherwise the pointee types must match (the checks the per-shape
            // blocks used to make, unified here).
            if (target->kind != TYPE_VOID && pointee->kind != TYPE_VOID &&
                !compatible_type(var_type->u.pointer.target, pointee)) {
                fatal_error("incompatible pointer types when initializing '%s'",
                            type_to_c(var_type));
            }
            bool is_fat = (target->kind == TYPE_CHAR || target->kind == TYPE_SCHAR ||
                           target->kind == TYPE_UCHAR || target->kind == TYPE_VOID);
            if (is_fat) {
                // A char*/void* addresses a byte.  eval_addr_const already yields the packed
                // byte position for a char-array element/member and for a string/array decay;
                // on a word-addressed target a directly-addressed scalar char keeps its value
                // in the low byte of its one-word cell, so &c is byte#5 (offset_enc 5).
                // Sub-word char addressing beyond these forms is the known char-in-struct
                // limitation.
                const Expr *operand = init->u.expr->kind == EXPR_UNARY_OP &&
                                              init->u.expr->u.unary_op.op == UNARY_ADDRESS
                                          ? init->u.expr->u.unary_op.expr
                                          : NULL;
                if (target_word_addressed() && operand &&
                    (operand->kind == EXPR_VAR || operand->kind == EXPR_COMPOUND)) {
                    const Type *ot = unalias(symtab_get(base)->type);
                    if (ot->kind == TYPE_CHAR || ot->kind == TYPE_SCHAR || ot->kind == TYPE_UCHAR)
                        off += (long)target_config->aggregate_align - 1;
                }
                Tac_StaticInit *fi        = tac_new_static_init(TAC_STATIC_INIT_FAT_POINTER);
                fi->u.pointer.name        = xstrdup(base);
                fi->u.pointer.byte_offset = (int)off;
                return fi;
            }
            Tac_StaticInit *pointer_init        = tac_new_static_init(TAC_STATIC_INIT_POINTER);
            pointer_init->u.pointer.name        = xstrdup(base);
            pointer_init->u.pointer.byte_offset = (int)off;
            return pointer_init;
        }
    }

    // Handle pointer initialized with an integer constant expression (e.g. cast from integer).
    if (var_type->kind == TYPE_POINTER && init->kind == INITIALIZER_SINGLE) {
        long val;
        if (try_eval_const_int(init->u.expr, &val)) {
            if (val == 0) {
                Tac_StaticInit *zero_init = tac_new_static_init(TAC_STATIC_INIT_ZERO);
                zero_init->u.zero_bytes   = get_size(var_type);
                return zero_init;
            }
            // A non-zero integer initializes a pointer only when it is explicitly
            // cast to a pointer type (an address constant, e.g.
            // "(char *)0x4000").  A bare integer is not a null pointer constant
            // and is rejected (C11 §6.7.9p4 / §6.3.2.3p3).
            if (init->u.expr->kind != EXPR_CAST ||
                unalias(init->u.expr->u.cast.type)->kind != TYPE_POINTER) {
                fatal_error("initializer of '%s' is neither an address nor a null pointer constant",
                            type_to_c(var_type));
            }
            return new_static_init_int(get_size(var_type), true, (uint64_t)val);
        }
    }

    // An aggregate (array/struct/union) cannot be initialized by a scalar literal
    // (e.g. "static int a[1] = 0;", "struct s x = 0;").  The only valid single-expression
    // aggregate initializer is a string literal for a char array, handled above; a
    // non-constant single expression (e.g. "struct s y = other;") falls through to the
    // catch-all below.  (Pointers are scalar and handled by the blocks above.)
    if (init->kind == INITIALIZER_SINGLE && init->u.expr->kind == EXPR_LITERAL &&
        (var_type->kind == TYPE_ARRAY || var_type->kind == TYPE_STRUCT ||
         var_type->kind == TYPE_UNION)) {
        fatal_error("cannot initialize '%s' with a scalar value", type_to_c(var_type));
    }

    // Handle scalar initialized with a literal.  An enum constant is an EXPR_LITERAL too,
    // but its Literal holds only the enumerator's identifier — resolving it to a value needs
    // the symbol table, which new_static_init_from_literal has no access to.  So it falls
    // through to the constant-expression branch below, whose typecheck_and_decay rewrites
    // the node to a LITERAL_INT and whose try_eval_const_int folds enumerators natively.
    if (init->kind == INITIALIZER_SINGLE && init->u.expr->kind == EXPR_LITERAL &&
        init->u.expr->u.literal->kind != LITERAL_ENUM) {
        Literal *literal = init->u.expr->u.literal;
        type_char_literal(literal);
        check_int_literal_width(literal);
        if (is_zero_int(literal)) {
            Tac_StaticInit *zero_init = tac_new_static_init(TAC_STATIC_INIT_ZERO);
            zero_init->u.zero_bytes   = get_size(var_type);
            return zero_init;
        }
        if (!is_arithmetic(var_type)) {
            fatal_error("cannot initialize '%s' with this initializer", type_to_c(var_type));
        }
        return new_static_init_from_literal(var_type, literal);
    }

    // Handle integer scalar initialized with a constant expression (e.g. -1, ~0, sizeof(T)).
    if (init->kind == INITIALIZER_SINGLE && is_integer(var_type)) {
        const Expr *expr = typecheck_and_decay(init->u.expr);
        long val;
        if (try_eval_const_int(expr, &val)) {
            Literal lit;
            if (get_size(var_type) == 8 && is_signed(var_type)) {
                lit = (Literal){ .kind = LITERAL_LONG_LONG, .u.long_long_val = (long long)val };
            } else if (get_size(var_type) == 8) {
                lit = (Literal){ .kind             = LITERAL_ULONG_LONG,
                                 .u.ulong_long_val = (unsigned long long)(unsigned long)val };
            } else {
                lit = (Literal){ .kind = LITERAL_INT, .u.int_val = (int64_t)val };
            }
            return new_static_init_from_literal(var_type, &lit);
        }
        // A real constant expression converts to the integer target (C11 §6.3.1.4):
        // "int m = -1.5;".  new_static_init_from_literal truncates via literal_to_int64.
        double real_val;
        if (try_eval_const_real(expr, &real_val)) {
            Literal lit = { .kind = LITERAL_DOUBLE, .u.real_val = real_val };
            return new_static_init_from_literal(var_type, &lit);
        }
        // A variable with static storage duration must have a constant
        // initializer (C11 §6.7.9p4): "int b = 1 + a;" / "static int b = a * 2;".
        fatal_error("initializer element is not a constant expression");
    }

    // Handle floating scalar initialized with a constant expression (e.g. -0.5, 1.0 / 4).
    // The integer block above always returns or fails, so an arithmetic type reaching
    // here is necessarily a real one.
    if (init->kind == INITIALIZER_SINGLE && is_arithmetic(var_type)) {
        const Expr *expr = typecheck_and_decay(init->u.expr);
        Float128 q;
        if (unalias(var_type)->kind == TYPE_LONG_DOUBLE && try_eval_const_ld(expr, &q)) {
            Literal lit = { .kind = LITERAL_LONG_DOUBLE, .u.long_double_val = q };
            return new_static_init_from_literal(var_type, &lit);
        }
        double val;
        if (try_eval_const_real(expr, &val)) {
            Literal lit = { .kind = LITERAL_DOUBLE, .u.real_val = val };
            return new_static_init_from_literal(var_type, &lit);
        }
        fatal_error("initializer element is not a constant expression");
    }

    // Handle array with compound initializer: exactly one item per element.  A run of
    // uninitialized elements becomes a single ZERO run.
    if (var_type->kind == TYPE_ARRAY && init->kind == INITIALIZER_COMPOUND) {
        Type *element_type         = var_type->u.array.element;
        size_t element_size        = get_size(element_type);
        Tac_StaticInit *array_init = NULL;
        Tac_StaticInit **current   = &array_init;
        size_t pending_zero        = 0;

        for (const InitItem *item = init->u.items; item; item = item->next) {
            if (!item->init) {
                pending_zero += element_size;
                continue;
            }
            current      = append_zero(current, pending_zero);
            pending_zero = 0;
            current      = append_static_init(current, static_init(element_type, item->init));
        }
        append_zero(current, pending_zero);
        return array_init;
    }

    // Handle struct with compound initializer.
    if (var_type->kind == TYPE_STRUCT && init->kind == INITIALIZER_COMPOUND) {
        const StructDef *struct_def = structtab_find(var_type->u.struct_t.name);
        const FieldDef *field       = struct_def->members;
        Tac_StaticInit *struct_init = NULL;
        Tac_StaticInit **current    = &struct_init;
        int current_offset          = 0;

        // Exactly one item per member.  An uninitialized member emits nothing: the
        // padding before the next initialized member (or the tail) zero-fills it.
        // Bit-fields share bytes, so a run of them is gathered into a byte image first
        // and emitted as bytes once a member that is not one follows.
        BitImage img = { .bytes = NULL, .lo = -1, .hi = -1 };
        for (const InitItem *item = init->u.items; item; item = item->next, field = field->next) {
            assert(field);
            if (field->bf.width) {
                if (item->init)
                    put_bitfield(&img, struct_def->size, field,
                                 static_int_value(static_init(field->type, item->init)));
                continue;
            }
            current = flush_bits(&img, current, &current_offset);
            if (!item->init) {
                continue;
            }
            current        = append_zero(current, field->offset - current_offset);
            current        = append_static_init(current, static_init(field->type, item->init));
            current_offset = field->offset + get_size(field->type);
        }
        current = flush_bits(&img, current, &current_offset);
        xfree(img.bytes);
        append_zero(current, struct_def->size - current_offset);
        return struct_init;
    }

    // Handle union with compound initializer: initialize the chosen member, then zero-pad
    // the remaining union storage to its full size.
    if (var_type->kind == TYPE_UNION && init->kind == INITIALIZER_COMPOUND) {
        const StructDef *union_def = structtab_find(var_type->u.struct_t.name);
        const FieldDef *field      = union_member(var_type, init->u.items);
        const Initializer *member  = init->u.items->init;
        // An uninitialized union zeroes its whole storage.
        if (!member) {
            Tac_StaticInit *zero_init = tac_new_static_init(TAC_STATIC_INIT_ZERO);
            zero_init->u.zero_bytes   = union_def->size;
            return zero_init;
        }
        Tac_StaticInit *u_init   = NULL;
        Tac_StaticInit **current = &u_init;
        if (field->bf.width) {
            BitImage img = { .bytes = NULL, .lo = -1, .hi = -1 };
            int offset   = 0;
            put_bitfield(&img, union_def->size, field,
                         static_int_value(static_init(field->type, member)));
            current = flush_bits(&img, current, &offset);
            xfree(img.bytes);
            append_zero(current, union_def->size - offset);
            return u_init;
        }
        current = append_static_init(current, static_init(field->type, member));
        append_zero(current, union_def->size - get_size(field->type));
        return u_init;
    }

    // Handle invalid cases.
    fatal_error("cannot initialize '%s' with this initializer", type_to_c(var_type));
}

// static_init with diag_loc at the node, for the errors found in it.
static Tac_StaticInit *static_init(Type *var_type, const Initializer *init)
{
    SrcLoc saved           = diag_enter(init ? init->loc : diag_loc);
    Tac_StaticInit *result = static_init_at(var_type, init);
    diag_loc               = saved;
    return result;
}

// Convert an initializer to a Tac_StaticInit list for global/static variables.
// *init is normalized in place; the caller still owns it.
// Merge adjacent ZERO runs, e.g. a zeroed member followed by padding.
static void merge_zero_runs(Tac_StaticInit *list)
{
    for (Tac_StaticInit *cur = list; cur; cur = cur->next) {
        while (cur->kind == TAC_STATIC_INIT_ZERO && cur->next &&
               cur->next->kind == TAC_STATIC_INIT_ZERO) {
            Tac_StaticInit *next = cur->next;
            cur->u.zero_bytes += next->u.zero_bytes;
            cur->next  = next->next;
            next->next = NULL;
            tac_free_static_init(next);
        }
    }
}

Tac_StaticInit *build_static_init(Type *var_type, Initializer **init)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    if (*init) {
        *init = normalize_init(var_type, *init, INIT_STATIC);
    }
    Tac_StaticInit *list = static_init(var_type, *init);
    merge_zero_runs(list);
    return list;
}

// Type-check a canonical initializer (see init_normalize.c) against a target type.
// Leaf expressions are already typechecked; an item with a NULL init becomes zero.
static Initializer *check_init(Type *target_type, Initializer *init);

static Initializer *check_init_at(Type *target_type, Initializer *init)
{
    if (!init) {
        return make_zero_init(target_type);
    }
    // Look through a global typedef reference (reads + recursion only).
    target_type = (Type *)unalias(target_type);

    // Update initializer type.
    free_type(init->type);
    init->type = clone_type(target_type, __func__, __FILE__, __LINE__);

    // Handle array initialized with a string literal.
    if (target_type->kind == TYPE_ARRAY && init->kind == INITIALIZER_SINGLE &&
        init->u.expr->kind == EXPR_LITERAL && init->u.expr->u.literal->kind == LITERAL_STRING) {
        const Type *element_type = unalias(target_type->u.array.element);
        if (element_type->kind != TYPE_CHAR && element_type->kind != TYPE_SCHAR &&
            element_type->kind != TYPE_UCHAR) {
            fatal_error("cannot initialize array of type '%s' with a string literal",
                        type_to_c(target_type));
        }
        size_t string_length;
        char *decoded =
            c_decode_string_literal(init->u.expr->u.literal->u.string_val, &string_length);
        xfree(decoded);
        if (!target_type->u.array.size) {
            set_array_size(target_type, string_length + 1);
        } else {
            size_t array_size = get_array_size(target_type);
            if (string_length > array_size) {
                fatal_error("initializer string for array of type '%s' is too long",
                            type_to_c(target_type));
            }
        }
        init->u.expr = typecheck_string(init->u.expr);
        return init;
    }

    // Handle a single (already typechecked) expression.
    if (init->kind == INITIALIZER_SINGLE) {
        init->u.expr = coerce_for_assignment(init->u.expr, target_type, "initializing");
        return init;
    }

    // Handle array with compound initializer: exactly one item per element.
    if (target_type->kind == TYPE_ARRAY) {
        Type *element_type = target_type->u.array.element;
        for (InitItem *item = init->u.items; item; item = item->next) {
            item->init = check_init(element_type, item->init);
        }
        return init;
    }

    // Handle struct with compound initializer: exactly one item per member.
    if (target_type->kind == TYPE_STRUCT) {
        const FieldDef *field = structtab_find(target_type->u.struct_t.name)->members;
        for (InitItem *item = init->u.items; item; item = item->next, field = field->next) {
            assert(field);
            item->init = check_init(field->type, item->init);
            // Stash the member's byte offset on the AST node while the tag is still
            // live; a block-local tag is purged on block exit, so the translator's
            // gen_compound_init can no longer resolve it.  Mirrors field_access.offset.
            item->offset = field->offset;
            item->bf     = field->bf;
        }
        return init;
    }

    // Handle union with compound initializer: a single item, for the chosen member.
    if (target_type->kind == TYPE_UNION) {
        const FieldDef *member = union_member(target_type, init->u.items);
        init->u.items->init    = check_init(member->type, init->u.items->init);
        init->u.items->offset  = member->offset;
        init->u.items->bf      = member->bf;
        init->u.items->next    = union_rest(target_type, member->type);
        return init;
    }

    fatal_error("cannot initialize scalar type '%s' with a braced list", type_to_c(target_type));
}

// check_init with diag_loc at the node, for the errors found in it.
static Initializer *check_init(Type *target_type, Initializer *init)
{
    SrcLoc saved        = diag_enter(init ? init->loc : diag_loc);
    Initializer *result = check_init_at(target_type, init);
    diag_loc            = saved;
    return result;
}

// Type-check an initializer against a target type.
Initializer *typecheck_init(Type *target_type, Initializer *init)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    if (!init) {
        return NULL;
    }
    return check_init(target_type, normalize_init(target_type, init, INIT_AUTOMATIC));
}
