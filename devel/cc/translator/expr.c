//
// Expression lowering: AST Expr → TAC instructions.
//

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "c_escape.h"
#include "semantic.h"
#include "target.h"
#include "translate.h"
#include "typecheck.h"
#include "xalloc.h"

// The declared type of the member named by a FIELD_ACCESS/PTR_ACCESS node.  After
// typecheck an array member used as a value has been decayed to a pointer (so
// e->type no longer says "array"); the backend recovers the member's true type
// here to decide whether to load it or decay it to its address.  Typecheck
// resolved it while the tag was live and stashed it on the node beside the member
// offset, so this works for a block-scope tag that structtab has since purged.
// Returns NULL for a synthesized access node that never went through typecheck,
// in which case the caller falls back to a plain load.
static const Type *field_member_type(const Expr *e)
{
    return e->kind == EXPR_FIELD_ACCESS ? e->u.field_access.member_type
                                        : e->u.ptr_access.member_type;
}

// True when a struct member is addressed by byte (a char scalar or a character
// array), so its address is a fat pointer built with ADD_PTR scale 1.  Every other
// member (a word scalar, pointer, struct/union, or word-element array) is addressed
// by word, so its member offset is added as a plain word offset — keeping the
// pointer a plain word address that later array indexing / loads can use.
static bool member_is_byte_addressed(const Type *mt)
{
    if (!mt)
        return false;
    mt = unalias(mt);
    while (mt->kind == TYPE_ARRAY)
        mt = unalias(mt->u.array.element);
    return is_character(mt);
}

// True when a member at `byte_offset` of declared type `mt` is addressed as a fat byte
// pointer (ADD_PTR scale 1): a char/char-array member, an unknown-tag member (mt NULL),
// or a (pathological) misaligned member.  The complement is a word-aligned word member,
// added as a plain word offset that keeps the pointer a plain word address.  gen_lval and
// emit_member_offset both consult this so they agree on the addressing mode.
static bool member_is_byte_offset(const Type *mt, int byte_offset)
{
    int w = target_word_bytes();
    return !(mt && !member_is_byte_addressed(mt) && byte_offset % w == 0);
}

// Fill in an ADD_PTR's index/scale for a struct member at `byte_offset` whose
// declared type is `mt` (NULL if the tag is out of scope at lowering time).  A
// word-addressed member at a word-aligned offset is added as a plain word offset
// (scale = word) so the result stays a plain word pointer that later subscripts
// and loads can chain off; a char member (or an unknown member) keeps the byte
// (scale 1, fat-pointer) form it has always used.
static void emit_member_offset(Tac_Instruction *ap, int byte_offset, const Type *mt)
{
    if (member_is_byte_offset(mt, byte_offset)) {
        ap->u.add_ptr.index = val_int(byte_offset);
        ap->u.add_ptr.scale = 1;
    } else {
        int w               = target_word_bytes();
        ap->u.add_ptr.index = val_int(byte_offset / w);
        ap->u.add_ptr.scale = w;
    }
}

// Reinterpret a plain word address (a struct base / &struct / struct-pointer value) as a
// fat char pointer at byte #0 of its first word, so a scale-1 ADD_PTR's byte member offset
// addresses the member byte directly.  Packed char data is MSB-first (byte #0 = offset 0),
// but the byte-pointer helpers (b/padd, b/pinc) read a bare word address as byte #5
// (offset_enc 0).  Mirror the int*->char* cast (PTR_TO_CHAR_PTR sets the fat marker with
// offset_enc 5 = byte #0) so the member offset lands on the right byte.
static Tac_Val *member_byte_base(TacCtx *ctx, Tac_Val *word_ptr)
{
    Tac_Val *dst              = new_var_val(ctx, tac_type_ptr(tac_type_char()));
    Tac_Instruction *in       = tac_new_instruction(TAC_INSTRUCTION_PTR_TO_CHAR_PTR);
    in->u.ptr_to_char_ptr.src = word_ptr;
    in->u.ptr_to_char_ptr.dst = dst;
    tac_append(ctx, in);
    return val_var(dst->u.var_name);
}

static bool is_unsigned_type(const Type *t)
{
    t = unalias(t);
    return t->kind == TYPE_UCHAR || t->kind == TYPE_UINT || t->kind == TYPE_ULONG ||
           t->kind == TYPE_ULONG_LONG;
}

// 1 when an object/pointee occupies a single byte (the char types; _Bool is a whole
// word on a word-addressed target — see get_size), so a
// load/store through it is a byte access and its address is a fat pointer.  Selects the
// byte variant of LOAD/STORE/GET_ADDRESS/COPY_*_OFFSET for the BESM-6 backend.
static int byte_access_for(const Type *t)
{
    return get_size(t) == 1;
}

// True for a char*/void* — a fat pointer whose arithmetic adjusts the 3-bit byte
// offset (scale 1) rather than the word address.  Mirrors is_fat_pointer in translate.c.
// A pointer to a char-innermost array (e.g. char(*)[4], produced by decaying a
// multi-dimensional char array) is also a fat byte pointer over the array's contiguous
// byte storage, so look through array element types to the innermost scalar.
static bool is_byte_pointer(const Type *t)
{
    t = unalias(t);
    if (t->kind != TYPE_POINTER)
        return false;
    const Type *target = unalias(t->u.pointer.target);
    while (target->kind == TYPE_ARRAY)
        target = unalias(target->u.array.element);
    return is_character(target) || target->kind == TYPE_VOID;
}

static bool is_floating_type(const Type *t)
{
    return is_arithmetic(t) && !is_integer(t);
}

// For a non-byte pointer whose arithmetic must be scaled, return the element size in
// bytes (the ADD_PTR scale); 0 when a plain BINARY add/subtract suffices.  On a
// word-addressed target that is a pointee of one word or less, which already advances
// by exactly one word; only a wider one (pointer-to-array / -struct) scales.  On a
// byte-addressed target every pointee scales.
static int wide_ptr_scale(const Type *t)
{
    t = unalias(t);
    if (t->kind != TYPE_POINTER || is_byte_pointer(t))
        return 0;
    int sz = (int)get_size(t->u.pointer.target);
    if (!target_word_addressed())
        return sz;
    return sz > target_word_bytes() ? sz : 0;
}

// Byte stride of a fat (byte) pointer — the size of its pointee: 1 for char*/void*, or the
// row size for a pointer to a char-innermost array (char(*)[N], from decaying a
// multi-dimensional char array).  BESM-6 has no sub-word word-scale, so fat-pointer
// arithmetic runs at scale 1 with the index pre-multiplied by this stride.
static int fat_ptr_byte_scale(const Type *ptr_type)
{
    const Type *target = unalias(unalias(ptr_type)->u.pointer.target);
    return target->kind == TYPE_VOID ? 1 : (int)get_size(target);
}

// Multiply a fat-pointer index by its byte stride (a no-op for stride 1).  A constant index
// folds away in the optimizer.
static Tac_Val *scale_byte_index(TacCtx *ctx, Tac_Val *idx, const Type *idx_type, int scale)
{
    if (scale == 1)
        return idx;
    Tac_Val *scaled      = new_var_val(ctx, ast_type_to_tac_type(idx_type));
    Tac_Instruction *mul = tac_new_instruction(TAC_INSTRUCTION_BINARY);
    mul->u.binary.op     = TAC_BINARY_MULTIPLY;
    mul->u.binary.src1   = idx;
    mul->u.binary.src2   = val_int(scale);
    mul->u.binary.dst    = scaled;
    tac_append(ctx, mul);
    return val_var(scaled->u.var_name);
}

// Emit dst = val / divisor (signed) for a positive constant divisor; converts a raw
// pointer difference into a C element count.  A divisor that is a power of two folds to
// a shift in the backend; otherwise it calls b/div.
static Tac_Val *gen_div_const(TacCtx *ctx, Tac_Val *val, int divisor)
{
    Tac_Val *vd             = new_var_val(ctx, tac_type_ptrdiff());
    Tac_Instruction *divide = tac_new_instruction(TAC_INSTRUCTION_BINARY);
    divide->u.binary.op     = TAC_BINARY_DIVIDE;
    divide->u.binary.src1   = val;
    divide->u.binary.src2   = val_int(divisor);
    divide->u.binary.dst    = vd;
    tac_append(ctx, divide);
    return val_var(vd->u.var_name);
}

static Tac_BinaryOperator map_binary_op(BinaryOp op, const Type *operand_type)
{
    bool is_unsigned = is_unsigned_type(operand_type);
    bool is_float    = is_floating_type(operand_type);
    // Addresses are ordered unsigned, so a pointer compare folds as one too; the BESM-6
    // word pointers keep the plain operators.
    if ((op == BINARY_LT || op == BINARY_GT || op == BINARY_LE || op == BINARY_GE) &&
        unalias(operand_type)->kind == TYPE_POINTER && !target_word_addressed())
        is_unsigned = true;
    switch (op) {
    case BINARY_ADD:
        return is_float      ? TAC_BINARY_ADD_DOUBLE
               : is_unsigned ? TAC_BINARY_ADD_UNSIGNED
                             : TAC_BINARY_ADD;
    case BINARY_SUB:
        return is_float      ? TAC_BINARY_SUBTRACT_DOUBLE
               : is_unsigned ? TAC_BINARY_SUBTRACT_UNSIGNED
                             : TAC_BINARY_SUBTRACT;
    case BINARY_MUL:
        return is_float      ? TAC_BINARY_MULTIPLY_DOUBLE
               : is_unsigned ? TAC_BINARY_MULTIPLY_UNSIGNED
                             : TAC_BINARY_MULTIPLY;
    case BINARY_DIV:
        return is_float      ? TAC_BINARY_DIVIDE_DOUBLE
               : is_unsigned ? TAC_BINARY_DIVIDE_UNSIGNED
                             : TAC_BINARY_DIVIDE;
    case BINARY_MOD:
        return is_unsigned ? TAC_BINARY_REMAINDER_UNSIGNED : TAC_BINARY_REMAINDER;
    case BINARY_LT:
        return is_float      ? TAC_BINARY_LESS_THAN_DOUBLE
               : is_unsigned ? TAC_BINARY_LESS_THAN_UNSIGNED
                             : TAC_BINARY_LESS_THAN;
    case BINARY_GT:
        return is_float      ? TAC_BINARY_GREATER_THAN_DOUBLE
               : is_unsigned ? TAC_BINARY_GREATER_THAN_UNSIGNED
                             : TAC_BINARY_GREATER_THAN;
    case BINARY_LE:
        return is_float      ? TAC_BINARY_LESS_OR_EQUAL_DOUBLE
               : is_unsigned ? TAC_BINARY_LESS_OR_EQUAL_UNSIGNED
                             : TAC_BINARY_LESS_OR_EQUAL;
    case BINARY_GE:
        return is_float      ? TAC_BINARY_GREATER_OR_EQUAL_DOUBLE
               : is_unsigned ? TAC_BINARY_GREATER_OR_EQUAL_UNSIGNED
                             : TAC_BINARY_GREATER_OR_EQUAL;
    case BINARY_EQ:
        return TAC_BINARY_EQUAL;
    case BINARY_NE:
        return TAC_BINARY_NOT_EQUAL;
    case BINARY_BIT_AND:
        return TAC_BINARY_BITWISE_AND;
    case BINARY_BIT_OR:
        return TAC_BINARY_BITWISE_OR;
    case BINARY_BIT_XOR:
        return TAC_BINARY_BITWISE_XOR;
    case BINARY_LEFT_SHIFT:
        return TAC_BINARY_LEFT_SHIFT;
    case BINARY_RIGHT_SHIFT:
        return is_unsigned ? TAC_BINARY_RIGHT_SHIFT_LOGICAL : TAC_BINARY_RIGHT_SHIFT;
    default:
        internal_error("unsupported binary operator in TAC lowering");
    }
}

static Tac_UnaryOperator map_unary_op(UnaryOp op, const Type *operand_type)
{
    switch (op) {
    case UNARY_BIT_NOT:
        // Signed ~ flips the value bits but must preserve the INT-format exponent
        // (the result is still a canonical signed integer, so ~1 == -2).  Unsigned
        // ~ is the exact 48-bit complement of the plain-integer word.
        return is_unsigned_type(operand_type) ? TAC_UNARY_COMPLEMENT_UNSIGNED
                                              : TAC_UNARY_COMPLEMENT;
    case UNARY_NEG:
        if (is_floating_type(operand_type))
            return TAC_UNARY_NEGATE_DOUBLE;
        if (is_unsigned_type(operand_type))
            return TAC_UNARY_NEGATE_UNSIGNED;
        return TAC_UNARY_NEGATE;
    case UNARY_LOG_NOT:
        return TAC_UNARY_NOT;
    default:
        internal_error("unsupported unary operator in TAC lowering");
    }
}

static Tac_BinaryOperator map_assign_op(AssignOp op, const Type *operand_type)
{
    bool is_unsigned = is_unsigned_type(operand_type);
    bool is_float    = is_floating_type(operand_type);
    switch (op) {
    case ASSIGN_ADD:
        return is_float      ? TAC_BINARY_ADD_DOUBLE
               : is_unsigned ? TAC_BINARY_ADD_UNSIGNED
                             : TAC_BINARY_ADD;
    case ASSIGN_SUB:
        return is_float      ? TAC_BINARY_SUBTRACT_DOUBLE
               : is_unsigned ? TAC_BINARY_SUBTRACT_UNSIGNED
                             : TAC_BINARY_SUBTRACT;
    case ASSIGN_MUL:
        return is_float      ? TAC_BINARY_MULTIPLY_DOUBLE
               : is_unsigned ? TAC_BINARY_MULTIPLY_UNSIGNED
                             : TAC_BINARY_MULTIPLY;
    case ASSIGN_DIV:
        return is_float      ? TAC_BINARY_DIVIDE_DOUBLE
               : is_unsigned ? TAC_BINARY_DIVIDE_UNSIGNED
                             : TAC_BINARY_DIVIDE;
    case ASSIGN_MOD:
        return is_unsigned ? TAC_BINARY_REMAINDER_UNSIGNED : TAC_BINARY_REMAINDER;
    case ASSIGN_LEFT:
        return TAC_BINARY_LEFT_SHIFT;
    case ASSIGN_RIGHT:
        return is_unsigned ? TAC_BINARY_RIGHT_SHIFT_LOGICAL : TAC_BINARY_RIGHT_SHIFT;
    case ASSIGN_AND:
        return TAC_BINARY_BITWISE_AND;
    case ASSIGN_XOR:
        return TAC_BINARY_BITWISE_XOR;
    case ASSIGN_OR:
        return TAC_BINARY_BITWISE_OR;
    default:
        internal_error("unsupported compound assignment operator in TAC lowering");
    }
}

static bool is_aggregate_type(const Type *t)
{
    return t->kind == TYPE_ARRAY || t->kind == TYPE_STRUCT || t->kind == TYPE_UNION;
}

// Materialize a compound literal in its own frame slot; returns the slot name.  A scalar
// gets a slot too, so a store through its address is never dropped as a dead temporary.
static char *gen_compound_literal(TacCtx *ctx, const Expr *e)
{
    const Type *lit_type = unalias(e->u.compound_literal.type);
    char *slot           = new_typed_temp(ctx, ast_type_to_tac_type(lit_type));

    Tac_Instruction *al            = tac_new_instruction(TAC_INSTRUCTION_ALLOCATE_LOCAL);
    al->u.allocate_local.name      = xstrdup(slot);
    al->u.allocate_local.size      = (int)get_size(lit_type);
    al->u.allocate_local.alignment = (int)get_alignment(lit_type);
    tac_append(ctx, al);

    const Initializer *first = e->u.compound_literal.init->init;
    if (!is_aggregate_type(lit_type)) {
        gen_compound_init(ctx, slot, 0, first);
        return slot;
    }
    if (lit_type->kind == TYPE_ARRAY && first->kind == INITIALIZER_SINGLE &&
        first->u.expr->kind == EXPR_LITERAL && first->u.expr->u.literal->kind == LITERAL_STRING &&
        is_integer(lit_type->u.array.element)) {
        // A char array from a string (the only item; see typecheck of EXPR_COMPOUND).
        gen_string_array_init(ctx, slot, first->u.expr, (int)get_size(lit_type));
        return slot;
    }
    Initializer wrap;
    memset(&wrap, 0, sizeof wrap);
    wrap.kind    = INITIALIZER_COMPOUND;
    wrap.u.items = e->u.compound_literal.init;
    wrap.type    = (Type *)lit_type;
    gen_aggregate_init(ctx, slot, &wrap, (int)get_size(lit_type));
    return slot;
}

static Tac_Val *gen_aggregate_assign(TacCtx *ctx, Expr *target, Expr *value, Tac_Val **addr_out);

static Tac_Val *gen_lval(TacCtx *ctx, Expr *e)
{
    switch (e->kind) {
    case EXPR_VAR: {
        Tac_Val *dst = new_var_val(ctx, tac_type_ptr_to(e->type));
        Tac_Instruction *in =
            tac_new_instruction(byte_access_for(e->type) ? TAC_INSTRUCTION_GET_ADDRESS_BYTE
                                                         : TAC_INSTRUCTION_GET_ADDRESS);
        in->u.get_address.src = val_var(e->u.var);
        in->u.get_address.dst = dst;
        tac_append(ctx, in);
        return val_var(dst->u.var_name);
    }
    case EXPR_UNARY_OP:
        if (e->u.unary_op.op == UNARY_DEREF)
            return gen_expr(ctx, e->u.unary_op.expr);
        internal_error("lvalue not yet supported in gen_lval: expression kind %d", (int)e->kind);
    case EXPR_SUBSCRIPT: {
        // After typecheck exactly one operand is a (decayed) pointer; the other is
        // the integer index.  Scale by the size of the pointee: for a multi-
        // dimensional array the pointee is itself the row array, so this is the
        // row size, not the decayed element-pointer size.
        Expr *lexp          = e->u.subscript.left;
        Expr *rexp          = e->u.subscript.right;
        const Expr *ptr_exp = is_pointer(lexp->type) ? lexp : rexp;
        int scale           = (int)get_size(unalias(ptr_exp->type)->u.pointer.target);
        Tac_Val *lval       = gen_expr(ctx, lexp);
        Tac_Val *rval       = gen_expr(ctx, rexp);
        Tac_Val *idx        = ptr_exp == lexp ? rval : lval;
        // Indexing a row of a multi-dimensional char array: the base is a fat byte pointer
        // over contiguous byte storage, so advance it by index*rowsize BYTES at scale 1
        // rather than by a whole-word scale (BESM-6 has no sub-word word-scale).
        if (is_byte_pointer(ptr_exp->type)) {
            idx   = scale_byte_index(ctx, idx, (ptr_exp == lexp ? rexp : lexp)->type, scale);
            scale = 1;
        }
        Tac_Val *dst        = new_var_val(ctx, ast_type_to_tac_type(ptr_exp->type));
        Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_ADD_PTR);
        in->u.add_ptr.ptr   = ptr_exp == lexp ? lval : rval;
        in->u.add_ptr.index = idx;
        in->u.add_ptr.scale = scale;
        in->u.add_ptr.dst   = dst;
        tac_append(ctx, in);
        return val_var(dst->u.var_name);
    }
    case EXPR_FIELD_ACCESS: {
        Expr *base = e->u.field_access.expr;
        int offset = e->u.field_access.offset;
        Tac_Val *base_addr;
        if (base->kind == EXPR_VAR) {
            Tac_Val *tmp          = new_var_val(ctx, tac_type_ptr_to(base->type));
            Tac_Instruction *ga   = tac_new_instruction(TAC_INSTRUCTION_GET_ADDRESS);
            ga->u.get_address.src = val_var(base->u.var);
            ga->u.get_address.dst = tmp;
            tac_append(ctx, ga);
            base_addr = val_var(tmp->u.var_name);
        } else {
            base_addr = gen_lval(ctx, base);
        }
        // A byte-addressed member (char/char-array) is reached at scale 1; the byte
        // helpers read a bare word base as byte #5, so convert it to a fat byte-#0 pointer.
        // Only a member known to be char-typed needs this — an out-of-scope tag (mt NULL)
        // keeps the legacy scale-1 form with no conversion, as before.
        const Type *mt = field_member_type(e);
        if (member_is_byte_addressed(mt))
            base_addr = member_byte_base(ctx, base_addr);
        Tac_Val *dst        = new_var_val(ctx, tac_type_ptr_to(mt ? mt : e->type));
        Tac_Instruction *ap = tac_new_instruction(TAC_INSTRUCTION_ADD_PTR);
        ap->u.add_ptr.ptr   = base_addr;
        emit_member_offset(ap, offset, mt);
        ap->u.add_ptr.dst = dst;
        tac_append(ctx, ap);
        return val_var(dst->u.var_name);
    }
    case EXPR_PTR_ACCESS: {
        Expr *ptr_expr   = e->u.ptr_access.expr;
        Tac_Val *ptr_val = gen_expr(ctx, ptr_expr);
        int offset       = e->u.ptr_access.offset;
        // Same fat-byte-#0 base conversion as FIELD_ACCESS for a byte-addressed member.
        const Type *mt = field_member_type(e);
        if (member_is_byte_addressed(mt))
            ptr_val = member_byte_base(ctx, ptr_val);
        Tac_Val *dst        = new_var_val(ctx, tac_type_ptr_to(mt ? mt : e->type));
        Tac_Instruction *ap = tac_new_instruction(TAC_INSTRUCTION_ADD_PTR);
        ap->u.add_ptr.ptr   = ptr_val;
        emit_member_offset(ap, offset, mt);
        ap->u.add_ptr.dst = dst;
        tac_append(ctx, ap);
        return val_var(dst->u.var_name);
    }
    case EXPR_COMPOUND: {
        char *T               = gen_compound_literal(ctx, e);
        Tac_Val *ptr          = new_var_val(ctx, tac_type_ptr_to(e->u.compound_literal.type));
        Tac_Instruction *ga   = tac_new_instruction(TAC_INSTRUCTION_GET_ADDRESS);
        ga->u.get_address.src = val_var(T);
        ga->u.get_address.dst = ptr;
        tac_append(ctx, ga);
        xfree(T);
        // A char literal is stored as byte #0 of its slot, like a char member.
        if (byte_access_for(e->type))
            return member_byte_base(ctx, val_var(ptr->u.var_name));
        return val_var(ptr->u.var_name);
    }
    case EXPR_ASSIGN: {
        // Member / subscript access on an assignment result, e.g. (x = y).member.  The C
        // result of an assignment is the value of its left operand; for an aggregate that
        // is the target object itself.  Perform the assignment for its side effect, then
        // return the target's address so the outer access reads the just-stored value.
        // An aggregate target is evaluated once: a compound literal must not be re-created.
        Expr *target = e->u.assign.target;
        TypeKind tk  = unalias(target->type)->kind;
        if (e->u.assign.op == ASSIGN_SIMPLE && (tk == TYPE_STRUCT || tk == TYPE_UNION)) {
            Tac_Val *addr = NULL;
            tac_free_val(gen_aggregate_assign(ctx, target, e->u.assign.value, &addr));
            return addr ? addr : gen_lval(ctx, target);
        }
        tac_free_val(gen_expr(ctx, e)); // discard the assignment's (unused) rvalue result
        return gen_lval(ctx, target);
    }
    case EXPR_CALL:
    case EXPR_CO_OP:
    case EXPR_AWAIT:
    case EXPR_COND: {
        // An aggregate temporary (struct/union returned by value, or selected by a
        // conditional) is already materialized into a frame slot by gen_expr — it
        // returns val_var(slot).  Implicitly take that slot's address so a member /
        // subscript lvalue (e.g. f().arr[i], (c ? u1 : u2).s.arr[0]) can reach it.
        Tac_Val *agg          = gen_expr(ctx, e);
        Tac_Val *ptr          = new_var_val(ctx, tac_type_ptr_to(e->type));
        Tac_Instruction *ga   = tac_new_instruction(TAC_INSTRUCTION_GET_ADDRESS);
        ga->u.get_address.src = agg; // transfer ownership to the GET_ADDRESS
        ga->u.get_address.dst = ptr;
        tac_append(ctx, ga);
        return val_var(ptr->u.var_name);
    }
    default:
        internal_error("lvalue not yet supported in gen_lval: expression kind %d", (int)e->kind);
    }
}

// An aggregate (struct/union) lvalue that names a frame/global base directly, so its words
// are reached by COPY_*_OFFSET: a plain variable, or a `var.member` at a word-aligned byte
// offset.  Other lvalues (through a pointer, a subscript, a nested member) are reached
// through their address instead.  On a hit, *name/*off receive the base name and byte offset.
static bool aggregate_named_base(const Expr *e, const char **name, int *off)
{
    if (e->kind == EXPR_VAR) {
        *name = e->u.var;
        *off  = 0;
        return true;
    }
    if (e->kind == EXPR_FIELD_ACCESS && e->u.field_access.expr->kind == EXPR_VAR &&
        e->u.field_access.offset % target_word_bytes() == 0) {
        *name = e->u.field_access.expr->u.var;
        *off  = e->u.field_access.offset;
        return true;
    }
    return false;
}

// Lower a whole-aggregate assignment `target = value` (struct/union, simple assignment) by
// copying it chunk by chunk (gen_aggregate_copy).  Each side is either a named base
// (COPY_FROM_OFFSET / COPY_TO_OFFSET) or, for a pointer/subscript/nested lvalue, an address
// reached by ADD_PTR + LOAD / STORE.  A non-lvalue source (a function-call return or compound
// literal) is first materialised into a named temporary via gen_expr.  If addr_out is given,
// it gets the destination's address when one was computed, else NULL (a named base).
static Tac_Val *gen_aggregate_assign(TacCtx *ctx, Expr *target, Expr *value, Tac_Val **addr_out)
{
    AggPlace dst  = { NULL, 0, NULL };
    Tac_Val *dptr = NULL;
    if (!aggregate_named_base(target, &dst.name, &dst.offset)) {
        dptr    = gen_lval(ctx, target);
        dst.ptr = dptr->u.var_name;
    }

    AggPlace src          = { NULL, 0, NULL };
    Tac_Val *sptr         = NULL;
    Tac_Val *src_material = NULL; // owned materialised rvalue (freed below)
    if (!aggregate_named_base(value, &src.name, &src.offset)) {
        if (value->kind == EXPR_CALL || value->kind == EXPR_COMPOUND || value->kind == EXPR_CO_OP ||
            value->kind == EXPR_AWAIT) {
            // An rvalue aggregate: gen_expr leaves it in a named temporary.
            src_material = gen_expr(ctx, value);
            src.name     = src_material->u.var_name;
        } else {
            sptr    = gen_lval(ctx, value); // through a pointer / subscript / nested member
            src.ptr = sptr->u.var_name;
        }
    }

    gen_aggregate_copy(ctx, &dst, &src, target->type);

    // The address/materialised-value Tac_Vals are consumed only by name above; free them.
    if (addr_out)
        *addr_out = dptr;
    else
        tac_free_val(dptr);
    tac_free_val(sptr);
    tac_free_val(src_material);
    return new_var_val(ctx, ast_type_to_tac_type(target->type));
}

// Initialize a whole aggregate, named destination base `dname`+`doff`, from a value
// expression.  Unlike gen_struct_assign (which assumes the source is itself a named
// aggregate base), this reads the source the same way gen_aggregate_assign does: a named
// base via COPY_FROM_OFFSET, a call/compound rvalue materialised by gen_expr, or — the case
// gen_struct_assign got wrong for `agg = *ptr` — a pointer/subscript lvalue loaded through
// ADD_PTR + LOAD.
void gen_aggregate_init_from_expr(TacCtx *ctx, const char *dname, int doff, Expr *value,
                                  const Type *type)
{
    AggPlace dst          = { dname, doff, NULL };
    AggPlace src          = { NULL, 0, NULL };
    Tac_Val *sptr         = NULL;
    Tac_Val *src_material = NULL;
    if (!aggregate_named_base(value, &src.name, &src.offset)) {
        if (value->kind == EXPR_CALL || value->kind == EXPR_COMPOUND || value->kind == EXPR_CO_OP ||
            value->kind == EXPR_AWAIT) {
            src_material = gen_expr(ctx, value);
            src.name     = src_material->u.var_name;
        } else {
            sptr    = gen_lval(ctx, value); // through a pointer / subscript / nested member
            src.ptr = sptr->u.var_name;
        }
    }
    gen_aggregate_copy(ctx, &dst, &src, type);
    tac_free_val(sptr);
    tac_free_val(src_material);
}

// A struct/union value read from memory (*p, a[i], s.m, p->m) that does not fit a scalar
// temporary (type_needs_slot): copy it into a fresh frame slot and yield the slot, the
// way a call result or a compound literal is held.  A LOAD would carry one word only.
static Tac_Val *gen_aggregate_rvalue(TacCtx *ctx, Expr *e)
{
    char *slot                     = new_typed_temp(ctx, ast_type_to_tac_type(e->type));
    Tac_Instruction *al            = tac_new_instruction(TAC_INSTRUCTION_ALLOCATE_LOCAL);
    al->u.allocate_local.name      = xstrdup(slot);
    al->u.allocate_local.size      = (int)get_size(e->type);
    al->u.allocate_local.alignment = (int)get_alignment(e->type);
    tac_append(ctx, al);
    gen_aggregate_init_from_expr(ctx, slot, 0, e, e->type);
    Tac_Val *val = val_var(slot);
    xfree(slot);
    return val;
}

static Tac_Val *gen_logical_and(TacCtx *ctx, Expr *l, Expr *r)
{
    Tac_Val *left  = gen_cond_val(ctx, l);
    char *false_l  = new_temp(ctx);
    char *end_l    = new_temp(ctx);
    char *dst_name = new_typed_temp(ctx, tac_new_type(TAC_TYPE_INT));

    Tac_Instruction *jz          = tac_new_instruction(TAC_INSTRUCTION_JUMP_IF_ZERO);
    jz->u.jump_if_zero.condition = left;
    jz->u.jump_if_zero.target    = false_l; // instruction takes ownership
    tac_append(ctx, jz);

    Tac_Val *right       = gen_cond_val(ctx, r);
    Tac_Instruction *bin = tac_new_instruction(TAC_INSTRUCTION_BINARY);
    bin->u.binary.op     = TAC_BINARY_NOT_EQUAL;
    bin->u.binary.src1   = right;
    bin->u.binary.src2   = val_zero(r->type);
    bin->u.binary.dst    = val_var(dst_name);
    tac_append(ctx, bin);
    emit_jump(ctx, end_l);

    emit_label(ctx, false_l); // false_l still valid; owned by jz
    Tac_Instruction *cp = tac_new_instruction(TAC_INSTRUCTION_COPY);
    cp->u.copy.src      = val_int(0);
    cp->u.copy.dst      = val_var(dst_name);
    tac_append(ctx, cp);

    emit_label(ctx, end_l);
    xfree(end_l);
    Tac_Val *result = val_var(dst_name);
    xfree(dst_name);
    return result;
}

static Tac_Val *gen_logical_or(TacCtx *ctx, Expr *l, Expr *r)
{
    Tac_Val *left  = gen_cond_val(ctx, l);
    char *true_l   = new_temp(ctx);
    char *end_l    = new_temp(ctx);
    char *dst_name = new_typed_temp(ctx, tac_new_type(TAC_TYPE_INT));

    Tac_Instruction *jnz              = tac_new_instruction(TAC_INSTRUCTION_JUMP_IF_NOT_ZERO);
    jnz->u.jump_if_not_zero.condition = left;
    jnz->u.jump_if_not_zero.target    = true_l; // instruction takes ownership
    tac_append(ctx, jnz);

    Tac_Val *right       = gen_cond_val(ctx, r);
    Tac_Instruction *bin = tac_new_instruction(TAC_INSTRUCTION_BINARY);
    bin->u.binary.op     = TAC_BINARY_NOT_EQUAL;
    bin->u.binary.src1   = right;
    bin->u.binary.src2   = val_zero(r->type);
    bin->u.binary.dst    = val_var(dst_name);
    tac_append(ctx, bin);
    emit_jump(ctx, end_l);

    emit_label(ctx, true_l); // true_l still valid; owned by jnz
    Tac_Instruction *cp = tac_new_instruction(TAC_INSTRUCTION_COPY);
    cp->u.copy.src      = val_int(1);
    cp->u.copy.dst      = val_var(dst_name);
    tac_append(ctx, cp);

    emit_label(ctx, end_l);
    xfree(end_l);
    Tac_Val *result = val_var(dst_name);
    xfree(dst_name);
    return result;
}

// Whether call `e` is of the C library's sqrt on a target where square root is an
// instruction: then it is lowered to the unary sqrt_double, which is no call at all.  C11
// §7.1.3 reserves the name with external linkage, so a program cannot mean another
// function by it; still, it must be the external function by that name (a static one, a
// definition seen in this unit, or a variable holding a function pointer is left alone),
// with a double result and one double argument (as converted by the prototype).  The result is the
// correctly rounded IEEE square root, as sqrt's is, and sqrt sets no errno here.
static bool is_hw_sqrt(const Expr *e)
{
    const Expr *func = e->u.call.func, *arg = e->u.call.args;
    if (!target_config->hw_sqrt || func->kind != EXPR_VAR || strcmp(func->u.var, "sqrt") != 0 ||
        !func->type || unalias(func->type)->kind != TYPE_FUNCTION)
        return false;
    if (!arg || arg->next || !arg->type || unalias(arg->type)->kind != TYPE_DOUBLE ||
        unalias(e->type)->kind != TYPE_DOUBLE)
        return false;
    const Symbol *sym = symtab_get_opt(func->u.var);
    return sym && sym->kind == SYM_FUNC && sym->u.func.global && !sym->u.func.defined;
}

static Tac_Val *gen_unary(TacCtx *ctx, UnaryOp op, Expr *inner, const Type *type)
{
    // Logical NOT of a char*/void* is a null test; gen_cond_val reduces a fat pointer to
    // its word address so a marker-tagged null still reads as zero (no effect otherwise).
    Tac_Val *src = op == UNARY_LOG_NOT ? gen_cond_val(ctx, inner) : gen_expr(ctx, inner);
    Tac_Val *vd  = new_var_val(ctx, ast_type_to_tac_type(type));

    Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_UNARY);
    in->u.unary.op      = map_unary_op(op, inner->type);
    in->u.unary.src     = src;
    in->u.unary.dst     = vd;
    tac_append(ctx, in);
    // Return a fresh val so callers can store it in a second instruction
    // without aliasing vd (which is already owned by this instruction).
    return val_var(vd->u.var_name);
}

// Emit "dst = ptr (+/-) idx" as ADD_PTR with the given byte scale.  For a char*/void*
// fat pointer the scale is 1 (the delta adjusts the 3-bit offset); for a pointer-to-array
// it is the element size.  `vptr`/`vidx` are consumed; returns the result val.
static Tac_Val *gen_ptr_add(TacCtx *ctx, Tac_Val *vptr, Tac_Val *vidx, bool subtract, int scale,
                            const Type *ptr_type, const Type *idx_type)
{
    if (subtract) {
        // ptr - n : negate the index (ADD_PTR / b/padd take a signed count).
        Tac_Val *neg        = new_var_val(ctx, ast_type_to_tac_type(idx_type));
        Tac_Instruction *un = tac_new_instruction(TAC_INSTRUCTION_UNARY);
        un->u.unary.op      = TAC_UNARY_NEGATE;
        un->u.unary.src     = vidx;
        un->u.unary.dst     = neg;
        tac_append(ctx, un);
        vidx = val_var(neg->u.var_name);
    }
    Tac_Val *vd         = new_var_val(ctx, ast_type_to_tac_type(ptr_type));
    Tac_Instruction *ap = tac_new_instruction(TAC_INSTRUCTION_ADD_PTR);
    ap->u.add_ptr.ptr   = vptr;
    ap->u.add_ptr.index = vidx;
    ap->u.add_ptr.scale = scale;
    ap->u.add_ptr.dst   = vd;
    tac_append(ctx, ap);
    return val_var(vd->u.var_name);
}

// Peel EXPR_CAST wrappers and report whether the underlying expression is a null
// pointer constant (an integer literal 0).  `p == NULL` arrives as a CAST of the
// literal 0 to the pointer type, and a coerced `p == 0` likewise, so the bare
// is_null_pointer_constant check must see through the casts.
static bool is_null_ptr_operand(const Expr *e)
{
    while (e->kind == EXPR_CAST)
        e = e->u.cast.expr;
    return is_null_pointer_constant(e);
}

// Reduce a char*/void* fat pointer to its bare word address (marker bit 48 and the
// 3-bit byte offset cleared) for null-testing.  Reuses CHAR_PTR_TO_PTR, which the
// BESM-6 backend already lowers to an AAX address mask (and which is an identity copy
// on byte-addressed targets).  A fat pointer is null iff this address word is 0,
// regardless of its marker/offset, so every "== NULL" / "if(p)" test runs the pointer
// through this first.
static Tac_Val *gen_ptr_addr_word(TacCtx *ctx, Tac_Val *fat)
{
    Tac_Val *vd               = new_var_val(ctx, tac_type_ptr(tac_new_type(TAC_TYPE_VOID)));
    Tac_Instruction *in       = tac_new_instruction(TAC_INSTRUCTION_CHAR_PTR_TO_PTR);
    in->u.char_ptr_to_ptr.src = fat;
    in->u.char_ptr_to_ptr.dst = vd;
    tac_append(ctx, in);
    return val_var(vd->u.var_name);
}

// Evaluate a controlling expression (an if/while/for/do condition, or a &&/||/?:
// operand) to a value suitable for a zero test.  A char*/void* fat pointer is first
// reduced to its word address so a marker-tagged null still tests as zero.
Tac_Val *gen_cond_val(TacCtx *ctx, Expr *cond)
{
    Tac_Val *v = gen_expr(ctx, cond);
    if (is_byte_pointer(cond->type))
        v = gen_ptr_addr_word(ctx, v);
    return v;
}

bool translate_cond_jumps = true;

// Whether a condition's `&&`, `||` and `!` are jumps: not on BESM-6, whose code must not
// change.
bool cond_jumps(void)
{
    return translate_cond_jumps && !target_config->no_loop_opt;
}

// Jump to `target` when `cond` is true (`if_true`) or false, else fall through.  `&&`,
// `||` and `!` become jumps, never a 0/1 value tested again (when cond_jumps()).
void gen_cond_jump(TacCtx *ctx, Expr *cond, bool if_true, const char *target)
{
    if (cond_jumps()) {
        if (cond->kind == EXPR_UNARY_OP && cond->u.unary_op.op == UNARY_LOG_NOT) {
            gen_cond_jump(ctx, cond->u.unary_op.expr, !if_true, target);
            return;
        }
        if (cond->kind == EXPR_BINARY_OP &&
            (cond->u.binary_op.op == BINARY_LOG_AND || cond->u.binary_op.op == BINARY_LOG_OR)) {
            bool is_and = cond->u.binary_op.op == BINARY_LOG_AND;
            Expr *l = cond->u.binary_op.left, *r = cond->u.binary_op.right;
            if (is_and != if_true) {
                // && jumping when false, || when true: either operand decides alone.
                gen_cond_jump(ctx, l, if_true, target);
                gen_cond_jump(ctx, r, if_true, target);
            } else {
                // && jumping when true, || when false: the left operand can only skip
                // the right one.
                char *skip = new_temp(ctx);
                gen_cond_jump(ctx, l, !if_true, skip);
                gen_cond_jump(ctx, r, if_true, target);
                emit_label(ctx, skip);
                xfree(skip);
            }
            return;
        }
    }
    Tac_Val *v = gen_cond_val(ctx, cond);
    Tac_Instruction *j;
    if (if_true) {
        j                               = tac_new_instruction(TAC_INSTRUCTION_JUMP_IF_NOT_ZERO);
        j->u.jump_if_not_zero.condition = v;
        j->u.jump_if_not_zero.target    = xstrdup(target);
    } else {
        j                           = tac_new_instruction(TAC_INSTRUCTION_JUMP_IF_ZERO);
        j->u.jump_if_zero.condition = v;
        j->u.jump_if_zero.target    = xstrdup(target);
    }
    tac_append(ctx, j);
}

// Whether `e` is `&&`, `||` or `!`, which gen_cond_jump makes jumps.
bool is_logical(const Expr *e)
{
    return (e->kind == EXPR_UNARY_OP && e->u.unary_op.op == UNARY_LOG_NOT) ||
           (e->kind == EXPR_BINARY_OP &&
            (e->u.binary_op.op == BINARY_LOG_AND || e->u.binary_op.op == BINARY_LOG_OR));
}

// Whether `e` is cheap to evaluate twice and has no side effects: names, constants and
// operators on them, but no `&&`, `||`, `?:`, call or assignment.
bool is_simple_cond(const Expr *e)
{
    switch (e->kind) {
    case EXPR_LITERAL:
    case EXPR_VAR:
    case EXPR_SIZEOF_EXPR:
    case EXPR_SIZEOF_TYPE:
    case EXPR_ALIGNOF:
        return true;
    case EXPR_UNARY_OP:
        return e->u.unary_op.op != UNARY_PRE_INC && e->u.unary_op.op != UNARY_PRE_DEC &&
               is_simple_cond(e->u.unary_op.expr);
    case EXPR_BINARY_OP:
        return e->u.binary_op.op != BINARY_LOG_AND && e->u.binary_op.op != BINARY_LOG_OR &&
               e->u.binary_op.op != BINARY_COMMA && is_simple_cond(e->u.binary_op.left) &&
               is_simple_cond(e->u.binary_op.right);
    case EXPR_SUBSCRIPT:
        return is_simple_cond(e->u.subscript.left) && is_simple_cond(e->u.subscript.right);
    case EXPR_CAST:
        return is_simple_cond(e->u.cast.expr);
    case EXPR_FIELD_ACCESS:
        return is_simple_cond(e->u.field_access.expr);
    case EXPR_PTR_ACCESS:
        return is_simple_cond(e->u.ptr_access.expr);
    default:
        return false;
    }
}

static Tac_Val *gen_binary(TacCtx *ctx, BinaryOp op, Expr *l, Expr *r, const Type *type)
{
    // char*/void* arithmetic: pointer ± integer adjusts the 3-bit byte offset of a fat
    // pointer, so lower it to ADD_PTR (scale 1) rather than a raw word add/subtract.
    // (Word pointers fall through to the plain BINARY path below — correct because the
    // machine is word-addressed and a 1-word element advances by one word.)
    if (op == BINARY_ADD || op == BINARY_SUB) {
        bool l_fat = is_byte_pointer(l->type);
        bool r_fat = is_byte_pointer(r->type);
        if (op == BINARY_SUB && l_fat && r_fat) {
            // char* - char* : the difference is a ptrdiff_t element count.  Decode
            // both fat pointers to absolute byte positions and subtract (the runtime
            // helper b/pdiff), then divide by the pointee byte size to get the element
            // count.  sizeof(char) == 1, so plain char*/void* needs no divide; a pointer
            // to a char-innermost array (char(*)[N]) divides by the row size N.
            Tac_Val *vl          = gen_expr(ctx, l);
            Tac_Val *vr          = gen_expr(ctx, r);
            Tac_Val *vd          = new_var_val(ctx, tac_type_ptrdiff());
            Tac_Instruction *pd  = tac_new_instruction(TAC_INSTRUCTION_PTR_DIFF);
            pd->u.ptr_diff.ptr_a = vl;
            pd->u.ptr_diff.ptr_b = vr;
            pd->u.ptr_diff.dst   = vd;
            tac_append(ctx, pd);
            int elem_bytes = fat_ptr_byte_scale(l->type);
            Tac_Val *diff  = val_var(vd->u.var_name);
            return elem_bytes > 1 ? gen_div_const(ctx, diff, elem_bytes) : diff;
        }
        bool ptr_left  = l_fat && is_integer(r->type);
        bool ptr_right = op == BINARY_ADD && r_fat && is_integer(l->type);
        if (ptr_left || ptr_right) {
            Tac_Val *vl   = gen_expr(ctx, l); // keep source evaluation order
            Tac_Val *vr   = gen_expr(ctx, r);
            Tac_Val *vptr = ptr_left ? vl : vr;
            Tac_Val *vidx = ptr_left ? vr : vl;
            // Scale the index to bytes by the pointee size (1 for char*/void*, the row size
            // for a pointer to a char-innermost array), then advance at scale 1.
            vidx = scale_byte_index(ctx, vidx, ptr_left ? r->type : l->type,
                                    fat_ptr_byte_scale(ptr_left ? l->type : r->type));
            return gen_ptr_add(ctx, vptr, vidx, op == BINARY_SUB, 1, ptr_left ? l->type : r->type,
                               ptr_left ? r->type : l->type);
        }
        // Word pointer minus word pointer, both pointing to a multi-word element
        // (pointer-to-array / -struct): the raw word-address difference must be divided
        // by the element word size to yield a C element count.  Detect before the ptr ±
        // int scale block below, which would otherwise misread q as a scaled index.
        if (op == BINARY_SUB && wide_ptr_scale(l->type) && wide_ptr_scale(r->type)) {
            Tac_Val *vl          = gen_expr(ctx, l);
            Tac_Val *vr          = gen_expr(ctx, r);
            Tac_Val *vd          = new_var_val(ctx, tac_type_ptrdiff());
            Tac_Instruction *sub = tac_new_instruction(TAC_INSTRUCTION_BINARY);
            sub->u.binary.op     = TAC_BINARY_SUBTRACT; // raw word-address difference
            sub->u.binary.src1   = vl;
            sub->u.binary.src2   = vr;
            sub->u.binary.dst    = vd;
            tac_append(ctx, sub);
            // The difference is in addressing units: words, or bytes.
            int unit  = target_word_addressed() ? target_word_bytes() : 1;
            int elems = wide_ptr_scale(l->type) / unit;
            if (elems == 1)
                return val_var(vd->u.var_name);
            return gen_div_const(ctx, val_var(vd->u.var_name), elems);
        }
        // Word pointer with a multi-word element (pointer-to-array / -struct):
        // ptr ± int must scale by the element size, not advance a single word.
        int l_scale = wide_ptr_scale(l->type);
        int r_scale = op == BINARY_ADD ? wide_ptr_scale(r->type) : 0;
        if (l_scale || r_scale) {
            Tac_Val *vl   = gen_expr(ctx, l); // keep source evaluation order
            Tac_Val *vr   = gen_expr(ctx, r);
            Tac_Val *vptr = l_scale ? vl : vr;
            Tac_Val *vidx = l_scale ? vr : vl;
            return gen_ptr_add(ctx, vptr, vidx, op == BINARY_SUB, l_scale ? l_scale : r_scale,
                               l_scale ? l->type : r->type, l_scale ? r->type : l->type);
        }
    }

    // Relational comparison of two char*/void* fat pointers.  The fat-pointer encoding
    // carries the byte offset in the high (exponent) bits and the word address in the low
    // bits, so a raw-word ordering compare is wrong.  Decode both to absolute byte positions
    // and subtract (PTR_DIFF / b/pdiff), then compare the signed difference against 0:
    // a >= b  iff  bytepos(a) - bytepos(b) >= 0, and likewise for <, >, <=.  (==/!=
    // between two real pointers fall through to a full-word compare — identical positions
    // have identical words — but ==/!= against a null constant is handled just below.)
    // Only a word-addressed target has fat pointers: elsewhere the difference could
    // overflow (rv32 pointers 2 GB apart), so the pointers are compared themselves.
    if ((op == BINARY_LT || op == BINARY_GT || op == BINARY_LE || op == BINARY_GE) &&
        target_word_addressed() && is_byte_pointer(l->type) && is_byte_pointer(r->type)) {
        Tac_Val *vl          = gen_expr(ctx, l);
        Tac_Val *vr          = gen_expr(ctx, r);
        Tac_Val *vdiff       = new_var_val(ctx, tac_type_ptrdiff());
        Tac_Instruction *pd  = tac_new_instruction(TAC_INSTRUCTION_PTR_DIFF);
        pd->u.ptr_diff.ptr_a = vl;
        pd->u.ptr_diff.ptr_b = vr;
        pd->u.ptr_diff.dst   = vdiff;
        tac_append(ctx, pd);

        Tac_BinaryOperator cmp;
        switch (op) {
        case BINARY_LT:
            cmp = TAC_BINARY_LESS_THAN;
            break;
        case BINARY_GT:
            cmp = TAC_BINARY_GREATER_THAN;
            break;
        case BINARY_LE:
            cmp = TAC_BINARY_LESS_OR_EQUAL;
            break;
        default: // BINARY_GE
            cmp = TAC_BINARY_GREATER_OR_EQUAL;
            break;
        }
        Tac_Val *vd         = new_var_val(ctx, tac_new_type(TAC_TYPE_INT));
        Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_BINARY);
        in->u.binary.op     = cmp;
        in->u.binary.src1   = val_var(vdiff->u.var_name);
        in->u.binary.src2   = val_int(0);
        in->u.binary.dst    = vd;
        tac_append(ctx, in);
        return val_var(vd->u.var_name);
    }

    // Equality of a char*/void* fat pointer against a null constant (p == NULL, p != 0).
    // A null fat pointer may carry a marker/offset (e.g. a null word pointer cast to
    // char*), so a full-word compare would wrongly read it as non-null.  Reduce the
    // pointer to its word address and compare that against 0; a fat pointer is null iff
    // its address part is 0.  (Two real pointers fall through to the full-word compare,
    // which correctly distinguishes distinct byte positions.)
    if ((op == BINARY_EQ || op == BINARY_NE) &&
        (is_byte_pointer(l->type) || is_byte_pointer(r->type)) &&
        (is_null_ptr_operand(l) || is_null_ptr_operand(r))) {
        Tac_Val *vl       = gen_expr(ctx, l); // keep source evaluation order
        Tac_Val *vr       = gen_expr(ctx, r);
        bool r_is_null    = is_null_ptr_operand(r);
        Tac_Val *ptr_addr = gen_ptr_addr_word(ctx, r_is_null ? vl : vr);
        tac_free_val(r_is_null ? vr : vl); // unused null-constant side
        Tac_Val *vd         = new_var_val(ctx, tac_new_type(TAC_TYPE_INT));
        Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_BINARY);
        in->u.binary.op     = op == BINARY_EQ ? TAC_BINARY_EQUAL : TAC_BINARY_NOT_EQUAL;
        in->u.binary.src1   = ptr_addr;
        in->u.binary.src2   = val_int(0);
        in->u.binary.dst    = vd;
        tac_append(ctx, in);
        return val_var(vd->u.var_name);
    }

    Tac_Val *vl = gen_expr(ctx, l);
    Tac_Val *vr = gen_expr(ctx, r);
    Tac_Val *vd = new_var_val(ctx, ast_type_to_tac_type(type));

    Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_BINARY);
    in->u.binary.op     = map_binary_op(op, l->type);
    in->u.binary.src1   = vl;
    in->u.binary.src2   = vr;
    in->u.binary.dst    = vd;
    tac_append(ctx, in);
    // Return a fresh val so callers can store it in a second instruction
    // without aliasing vd (which is already owned by this instruction).
    return val_var(vd->u.var_name);
}

// The value of scalar variable `name` of type `type` as an operand: the variable itself,
// or, when it is volatile, a fresh read of it through a volatile COPY, so the optimizer
// can neither fold nor propagate the value away.
static Tac_Val *read_var(TacCtx *ctx, const char *name, const Type *type)
{
    if (!type_is_volatile(type) || !is_scalar(type))
        return val_var(name);
    Tac_Val *dst        = new_var_val(ctx, ast_type_to_tac_type(type));
    Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_COPY);
    in->is_volatile     = true;
    in->u.copy.src      = val_var(name);
    in->u.copy.dst      = dst;
    tac_append(ctx, in);
    return val_var(dst->u.var_name);
}

// Emit "dst = src (+/-) 1" for the inc/dec operators.  For a char*/void* the step adjusts
// the 3-bit byte offset of the fat pointer (ADD_PTR scale 1, byte index +1/-1); for any
// other scalar it is a plain BINARY add/subtract by 1 (word-addressed for word pointers).
// Returns the dst Val owned by the emitted instruction; callers re-wrap with val_var.
static Tac_Val *gen_step(TacCtx *ctx, const Type *type, Tac_Val *src, bool inc)
{
    Tac_Val *dst = new_var_val(ctx, ast_type_to_tac_type(type));
    int wscale   = wide_ptr_scale(type);
    if (is_byte_pointer(type)) {
        // Fat pointer step: ±(pointee byte size) at scale 1 — ±1 for char*/void*, ±row
        // size for a pointer to a char-innermost array (char(*)[N]).
        int bscale          = fat_ptr_byte_scale(type);
        Tac_Instruction *ap = tac_new_instruction(TAC_INSTRUCTION_ADD_PTR);
        ap->u.add_ptr.ptr   = src;
        ap->u.add_ptr.index = val_int(inc ? bscale : -bscale);
        ap->u.add_ptr.scale = 1;
        ap->u.add_ptr.dst   = dst;
        tac_append(ctx, ap);
    } else if (wscale) {
        // Pointer-to-array/-struct (word pointer): a one-element step by the element size.
        Tac_Instruction *ap = tac_new_instruction(TAC_INSTRUCTION_ADD_PTR);
        ap->u.add_ptr.ptr   = src;
        ap->u.add_ptr.index = val_int(inc ? 1 : -1);
        ap->u.add_ptr.scale = wscale;
        ap->u.add_ptr.dst   = dst;
        tac_append(ctx, ap);
    } else {
        // The step is `1` in the operand's own type: a floating-point ++/-- must
        // add a floating-point 1.0, not an integer 1 (TAC binary ops are typed by
        // their operands, so an int 1 would be FP-added as a tiny denormal value).
        Tac_Val *step;
        if (is_floating_type(type)) {
            switch (unalias(type)->kind) {
            case TYPE_FLOAT:
                step = val_float(1.0f);
                break;
            case TYPE_LONG_DOUBLE:
                step = val_long_double(f128_from_i64(1));
                break;
            default:
                step = val_double(1.0);
                break;
            }
        } else {
            step = val_int(1);
        }
        Tac_Instruction *bin = tac_new_instruction(TAC_INSTRUCTION_BINARY);
        bin->u.binary.op     = map_binary_op(inc ? BINARY_ADD : BINARY_SUB, type);
        bin->u.binary.src1   = src;
        bin->u.binary.src2   = step;
        bin->u.binary.dst    = dst;
        tac_append(ctx, bin);
    }
    // C11 §6.5.2.4p2 / §6.5.3.1p2: the stepped value is converted back to the operand's
    // type, so `b++` on a true _Bool ends at 1 rather than 2 (and `b--` from 0 at 1, not
    // -1).  ++/-- computes the step in the operand's own type and stores it directly, so
    // it never passes through emit_cast: normalize here instead.
    if (unalias(type)->kind == TYPE_BOOL)
        return emit_bool_normalize(ctx, val_var(dst->u.var_name), type, type);
    return dst;
}

//
// Bit-fields.  A bit-field is reached through its storage unit (BitField, ast.h): the
// unit is loaded as an unsigned integer, the field's bits are shifted and masked out of
// it, and a store merges the new bits into the unit and stores it whole.  A unit that
// cannot be loaded in one piece is loaded byte by byte and assembled in a wider integer.
//

// Where a bit-field's storage unit lies: in a named aggregate `var`, reached by
// COPY_*_OFFSET, or else at `offset` from the struct address `addr`.
typedef struct {
    const char *var;
    Tac_Val *addr; // owned; NULL with var
    int offset;    // of the unit
    bool vol;
    const BitField *bf;
    const Type *type; // the member's declared type
    Type unit;        // the unsigned integer the unit is loaded as
    Type work;        // the unsigned integer, at least an int, it is worked on in: the
                      // backends shift and extend nothing narrower, as C never does
} BfPlace;

// Choose the unit and work types of `p`: a unit loaded byte by byte is assembled in the
// next power-of-two size.
static void bf_types(BfPlace *p)
{
    int size = p->bf->unit_size;
    while (p->bf->bytewise && (size & (size - 1)))
        size++;
    p->unit.kind = unsigned_kind_of_size(size);
    if (size < (int)target_config->int_size)
        size = (int)target_config->int_size;
    p->work.kind = unsigned_kind_of_size(size);
}

// The storage unit of bit-field access `e`, its struct base evaluated once.
static BfPlace bf_place(TacCtx *ctx, Expr *e)
{
    BfPlace p = { 0 };
    p.bf      = access_bitfield(e);
    p.type    = e->type;
    p.vol     = type_is_volatile(e->type);
    if (e->kind == EXPR_FIELD_ACCESS) {
        Expr *base = e->u.field_access.expr;
        p.offset   = e->u.field_access.offset;
        p.vol      = p.vol || type_is_volatile(base->type);
        if (base->kind == EXPR_VAR)
            p.var = base->u.var;
        else
            p.addr = gen_lval(ctx, base);
    } else {
        p.offset = e->u.ptr_access.offset;
        p.addr   = gen_expr(ctx, e->u.ptr_access.expr);
    }
    bf_types(&p);
    return p;
}

// A binary operation of type `t` on two owned values.
static Tac_Val *bf_binary(TacCtx *ctx, Tac_BinaryOperator op, Tac_Val *a, Tac_Val *b, const Type *t)
{
    Tac_Val *dst         = new_var_val(ctx, ast_type_to_tac_type(t));
    Tac_Instruction *bin = tac_new_instruction(TAC_INSTRUCTION_BINARY);
    bin->u.binary.op     = op;
    bin->u.binary.src1   = a;
    bin->u.binary.src2   = b;
    bin->u.binary.dst    = dst;
    tac_append(ctx, bin);
    return val_var(dst->u.var_name);
}

// The unsigned constant `v` of integer type `t`.
static Tac_Val *bf_const(const Type *t, uint64_t v)
{
    switch (unalias(t)->kind) {
    case TYPE_ULONG_LONG:
    case TYPE_LONG_LONG:
        return val_ulong_long(v);
    case TYPE_ULONG:
    case TYPE_LONG:
        return val_ulong((unsigned long)v);
    default:
        return val_uint(v);
    }
}

// The low `width` bits set.
static uint64_t bf_mask(int width)
{
    return width >= 64 ? ~(uint64_t)0 : ((uint64_t)1 << width) - 1;
}

// The address of the part of `p`'s unit at `offset`, of type `t`.
static Tac_Val *bf_part_address(TacCtx *ctx, const BfPlace *p, int offset, const Type *t)
{
    Tac_Val *base = dup_val(p->addr);
    if (member_is_byte_addressed(t))
        base = member_byte_base(ctx, base);
    Tac_Val *dst        = new_var_val(ctx, tac_type_ptr_to(t));
    Tac_Instruction *ap = tac_new_instruction(TAC_INSTRUCTION_ADD_PTR);
    ap->u.add_ptr.ptr   = base;
    emit_member_offset(ap, offset, t);
    ap->u.add_ptr.dst = dst;
    tac_append(ctx, ap);
    return val_var(dst->u.var_name);
}

// Load the part of `p`'s unit at `offset`, of type `t`.
static Tac_Val *bf_load_part(TacCtx *ctx, const BfPlace *p, int offset, const Type *t)
{
    bool byte    = byte_access_for(t);
    Tac_Val *dst = new_var_val(ctx, ast_type_to_tac_type(t));
    Tac_Instruction *in;
    if (p->var) {
        in = tac_new_instruction(byte ? TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET
                                      : TAC_INSTRUCTION_COPY_FROM_OFFSET);
        in->u.copy_from_offset.src    = xstrdup(p->var);
        in->u.copy_from_offset.offset = offset;
        in->u.copy_from_offset.dst    = dst;
    } else {
        Tac_Val *ptr = bf_part_address(ctx, p, offset, t);
        in           = tac_new_instruction(byte ? TAC_INSTRUCTION_LOAD_BYTE : TAC_INSTRUCTION_LOAD);
        in->u.load.src_ptr = ptr;
        in->u.load.dst     = dst;
    }
    in->is_volatile = p->vol;
    tac_append(ctx, in);
    return val_var(dst->u.var_name);
}

// Store owned value `v` of type `t` into the part of `p`'s unit at `offset`.
static void bf_store_part(TacCtx *ctx, const BfPlace *p, int offset, const Type *t, Tac_Val *v)
{
    bool byte = byte_access_for(t);
    Tac_Instruction *in;
    if (p->var) {
        in                          = tac_new_instruction(byte ? TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET
                                                               : TAC_INSTRUCTION_COPY_TO_OFFSET);
        in->u.copy_to_offset.src    = v;
        in->u.copy_to_offset.dst    = xstrdup(p->var);
        in->u.copy_to_offset.offset = offset;
    } else {
        Tac_Val *ptr = bf_part_address(ctx, p, offset, t);
        in = tac_new_instruction(byte ? TAC_INSTRUCTION_STORE_BYTE : TAC_INSTRUCTION_STORE);
        in->u.store.src     = v;
        in->u.store.dst_ptr = ptr;
    }
    in->is_volatile = p->vol;
    tac_append(ctx, in);
}

// The shift that places byte `i` of an `n`-byte unit in its value.
static int bf_byte_shift(int i, int n)
{
    return 8 * (target_config->big_endian ? n - 1 - i : i);
}

// Load `p`'s storage unit, as its work type.
static Tac_Val *bf_load_unit(TacCtx *ctx, const BfPlace *p)
{
    if (!p->bf->bytewise)
        return emit_cast(ctx, bf_load_part(ctx, p, p->offset, &p->unit), &p->unit, &p->work);
    static const Type uchar = { .kind = TYPE_UCHAR };
    int n                   = p->bf->unit_size;
    Tac_Val *acc            = NULL;
    for (int i = 0; i < n; i++) {
        Tac_Val *v = emit_cast(ctx, bf_load_part(ctx, p, p->offset + i, &uchar), &uchar, &p->work);
        if (bf_byte_shift(i, n))
            v = bf_binary(ctx, TAC_BINARY_LEFT_SHIFT, v, val_int(bf_byte_shift(i, n)), &p->work);
        acc = acc ? bf_binary(ctx, TAC_BINARY_BITWISE_OR, acc, v, &p->work) : v;
    }
    return acc;
}

// Store owned unit value `u`, of the work type, into `p`'s storage unit.
static void bf_store_unit(TacCtx *ctx, const BfPlace *p, Tac_Val *u)
{
    if (!p->bf->bytewise) {
        bf_store_part(ctx, p, p->offset, &p->unit, emit_cast(ctx, u, &p->work, &p->unit));
        return;
    }
    static const Type uchar = { .kind = TYPE_UCHAR };
    int n                   = p->bf->unit_size;
    for (int i = 0; i < n; i++) {
        Tac_Val *v = dup_val(u);
        if (bf_byte_shift(i, n))
            v = bf_binary(ctx, TAC_BINARY_RIGHT_SHIFT_LOGICAL, v, val_int(bf_byte_shift(i, n)),
                          &p->work);
        bf_store_part(ctx, p, p->offset + i, &uchar, emit_cast(ctx, v, &p->work, &uchar));
    }
    tac_free_val(u);
}

// The `width`-bit field at bit `pos` of owned unit value `u` (of unsigned type `ut`),
// converted to type `t`: zero-extended, or sign-extended when `t` is signed.
static Tac_Val *bf_extract(TacCtx *ctx, Tac_Val *u, const Type *ut, int pos, int width,
                           const Type *t)
{
    int ubits = (int)get_size(ut) * 8;
    bool sign = is_signed(t);
    if (sign && !target_config->right_shift_is_logical) {
        // Shift the field to the top of the unit, then arithmetically down to the bottom.
        Type st = { .kind = signed_kind_of(unalias(ut)->kind) };
        if (ubits - pos - width)
            u = bf_binary(ctx, TAC_BINARY_LEFT_SHIFT, u, val_int(ubits - pos - width), ut);
        u = emit_cast(ctx, u, ut, &st);
        if (ubits - width)
            u = bf_binary(ctx, TAC_BINARY_RIGHT_SHIFT, u, val_int(ubits - width), &st);
        return emit_cast(ctx, u, &st, t);
    }
    if (pos)
        u = bf_binary(ctx, TAC_BINARY_RIGHT_SHIFT_LOGICAL, u, val_int(pos), ut);
    if (pos + width < ubits)
        u = bf_binary(ctx, TAC_BINARY_BITWISE_AND, u, bf_const(ut, bf_mask(width)), ut);
    if (!sign)
        return emit_cast(ctx, u, ut, t);
    // No arithmetic shift (BESM-6): extend the sign as (u ^ m) - m, m the field's sign
    // bit, in the signed type, at least an int.  The field's bits fit it as a value: a
    // BESM-6 int has 41 value bits in a 48-bit word, and a negative one leaves the top 7
    // clear, so the unsigned word cannot be reinterpreted as one.
    Type st    = { .kind = get_size(t) < target_config->int_size ? TYPE_INT : unalias(t)->kind };
    uint64_t m = (uint64_t)1 << (width - 1);
    u          = emit_cast(ctx, u, ut, &st);
    u          = bf_binary(ctx, TAC_BINARY_BITWISE_XOR, u, bf_const(&st, m), &st);
    u          = bf_binary(ctx, TAC_BINARY_SUBTRACT, u, bf_const(&st, m), &st);
    return emit_cast(ctx, u, &st, t);
}

// Owned unit value `u` with `p`'s field replaced by owned value `v` of the field's type.
static Tac_Val *bf_insert(TacCtx *ctx, const BfPlace *p, Tac_Val *u, Tac_Val *v)
{
    const Type *ut = &p->work;
    int ubits      = (int)get_size(ut) * 8;
    int pos        = p->bf->pos;
    int width      = p->bf->width;
    v              = emit_cast(ctx, v, p->type, ut);
    if (width < ubits)
        v = bf_binary(ctx, TAC_BINARY_BITWISE_AND, v, bf_const(ut, bf_mask(width)), ut);
    if (pos)
        v = bf_binary(ctx, TAC_BINARY_LEFT_SHIFT, v, val_int(pos), ut);
    if (width == ubits) {
        tac_free_val(u);
        return v;
    }
    uint64_t keep = ~(bf_mask(width) << pos) & bf_mask(ubits);
    u             = bf_binary(ctx, TAC_BINARY_BITWISE_AND, u, bf_const(ut, keep), ut);
    return bf_binary(ctx, TAC_BINARY_BITWISE_OR, u, v, ut);
}

// The value `v` (owned, of the field's type) reads back as from `p`'s field: the value of
// an assignment to it (C11 §6.5.16p3).
static Tac_Val *bf_truncate(TacCtx *ctx, const BfPlace *p, Tac_Val *v)
{
    v = emit_cast(ctx, v, p->type, &p->work);
    return bf_extract(ctx, v, &p->work, 0, p->bf->width, p->type);
}

// Initialize bit-field `bf` of type `type`, in the storage unit at `offset` of named
// aggregate `var`, to owned value `v` (an automatic initializer).
void gen_bitfield_init(TacCtx *ctx, const char *var, int offset, const BitField *bf,
                       const Type *type, Tac_Val *v)
{
    BfPlace p = { .var = var, .offset = offset, .bf = bf, .type = type };
    bf_types(&p);
    bf_store_unit(ctx, &p, bf_insert(ctx, &p, bf_load_unit(ctx, &p), v));
}

// Read bit-field `e`.
static Tac_Val *gen_bitfield_read(TacCtx *ctx, Expr *e)
{
    BfPlace p  = bf_place(ctx, e);
    Tac_Val *r = bf_extract(ctx, bf_load_unit(ctx, &p), &p.work, p.bf->pos, p.bf->width, p.type);
    if (p.addr)
        tac_free_val(p.addr);
    return r;
}

// Assignment `e` to a bit-field, simple or compound, of the already evaluated `src`.
static Tac_Val *gen_bitfield_assign(TacCtx *ctx, Expr *e, Tac_Val *src)
{
    BfPlace p  = bf_place(ctx, e->u.assign.target);
    Tac_Val *u = bf_load_unit(ctx, &p);
    Tac_Val *v = src;
    if (e->u.assign.op != ASSIGN_SIMPLE) {
        // Computed in the common type (e->u.assign.value->type, after typecheck's
        // promotions) and converted back to the field's type.
        const Type *op_type = e->u.assign.value->type;
        bool widen          = unalias(op_type)->kind != unalias(p.type)->kind;
        Tac_Val *cur        = bf_extract(ctx, dup_val(u), &p.work, p.bf->pos, p.bf->width, p.type);
        if (widen)
            cur = emit_cast(ctx, cur, p.type, op_type);
        v = bf_binary(ctx, map_assign_op(e->u.assign.op, op_type), cur, src, op_type);
        if (widen)
            v = emit_cast(ctx, v, op_type, p.type);
    }
    bf_store_unit(ctx, &p, bf_insert(ctx, &p, u, dup_val(v)));
    Tac_Val *r = bf_truncate(ctx, &p, v);
    if (p.addr)
        tac_free_val(p.addr);
    return r;
}

// ++/-- of bit-field `e`: the new value, or with `post` the old one.
static Tac_Val *gen_bitfield_step(TacCtx *ctx, Expr *e, bool inc, bool post)
{
    BfPlace p     = bf_place(ctx, e);
    Tac_Val *u    = bf_load_unit(ctx, &p);
    Tac_Val *old  = bf_extract(ctx, dup_val(u), &p.work, p.bf->pos, p.bf->width, p.type);
    Tac_Val *next = val_var(gen_step(ctx, p.type, dup_val(old), inc)->u.var_name);
    bf_store_unit(ctx, &p, bf_insert(ctx, &p, u, dup_val(next)));
    Tac_Val *r;
    if (post) {
        r = old;
        tac_free_val(next);
    } else {
        r = bf_truncate(ctx, &p, next);
        tac_free_val(old);
    }
    if (p.addr)
        tac_free_val(p.addr);
    return r;
}

// The address of a string constant of `len` bytes at `s`, NUL-terminated.
Tac_Val *gen_string_constant(TacCtx *ctx, const char *s, size_t len)
{
    const char *sname = symtab_add_string(s, len);
    Symbol *sym       = symtab_get(sname);

    Tac_TopLevel *sc           = tac_new_toplevel(TAC_TOPLEVEL_STATIC_CONSTANT);
    sc->u.static_constant.name = xstrdup(sname);
    sc->u.static_constant.type = ast_type_to_tac_type(sym->type);
    sc->u.static_constant.init = sym->u.const_init;
    sym->u.const_init          = NULL; // transfer ownership to TAC node

    sc->next              = ctx->static_constants;
    ctx->static_constants = sc;

    Tac_Val *dst = new_var_val(ctx, tac_type_ptr(tac_type_char()));
    // A string literal decays to a char* at its first byte (byte#0 = MSB):
    // a fat pointer at offset_enc 5.
    Tac_Instruction *in   = tac_new_instruction(TAC_INSTRUCTION_GET_ADDRESS_DECAY);
    in->u.get_address.src = val_var(sname);
    in->u.get_address.dst = dst;
    tac_append(ctx, in);

    xfree((char *)sname);
    return val_var(dst->u.var_name);
}

Tac_Val *gen_expr(TacCtx *ctx, Expr *e);

static Tac_Val *gen_expr_at(TacCtx *ctx, Expr *e)
{
    if (!e) {
        internal_error("NULL expression in TAC lowering");
    }
    assert(e);
    switch (e->kind) {
    case EXPR_LITERAL:
        switch (e->u.literal->kind) {
        case LITERAL_INT:
            return val_int(e->u.literal->u.int_val);
        case LITERAL_LONG:
            return val_long(e->u.literal->u.long_val);
        case LITERAL_LONG_LONG:
            return val_long_long(e->u.literal->u.long_long_val);
        case LITERAL_UINT:
            return val_uint(e->u.literal->u.uint_val);
        case LITERAL_ULONG:
            return val_ulong(e->u.literal->u.ulong_val);
        case LITERAL_ULONG_LONG:
            return val_ulong_long(e->u.literal->u.ulong_long_val);
        case LITERAL_FLOAT:
            return val_float((float)e->u.literal->u.real_val);
        case LITERAL_DOUBLE:
            return val_double(e->u.literal->u.real_val);
        case LITERAL_LONG_DOUBLE:
            return val_long_double(e->u.literal->u.long_double_val);
        case LITERAL_CHAR:
            return val_int(e->u.literal->u.char_val);
        case LITERAL_STRING: {
            size_t decoded_len;
            char *decoded_str = c_decode_string_literal(e->u.literal->u.string_val, &decoded_len);
            Tac_Val *v        = gen_string_constant(ctx, decoded_str, decoded_len);
            xfree(decoded_str);
            return v;
        }
        default:
            internal_error("unsupported literal in TAC lowering");
        }
    case EXPR_VAR:
        // An array used as a value decays to a pointer to its first element.  Materialize
        // the address explicitly so the backend never has to disambiguate an array name
        // (whose value is its label address) from a pointer name (whose value is stored):
        // the bare array name would otherwise load the array's first word, not its address.
        // After typecheck a decayed array has pointer type, while its symbol is an array.
        // A char/void array decays to a fat pointer at its first byte (byte#0 = MSB,
        // offset_enc 5) via GET_ADDRESS_DECAY; any other array uses a plain GET_ADDRESS.
        if (unalias(e->type)->kind == TYPE_POINTER) {
            const Symbol *sym = symtab_get_opt(e->u.var);
            bool sym_is_array = (sym && unalias(sym->type)->kind == TYPE_ARRAY) ||
                                tac_is_array_local(ctx, e->u.var);
            if (sym_is_array) {
                Tac_Val *dst = new_var_val(ctx, ast_type_to_tac_type(e->type));
                Tac_Instruction *in =
                    tac_new_instruction(is_byte_pointer(e->type) ? TAC_INSTRUCTION_GET_ADDRESS_DECAY
                                                                 : TAC_INSTRUCTION_GET_ADDRESS);
                in->u.get_address.src = val_var(e->u.var);
                in->u.get_address.dst = dst;
                tac_append(ctx, in);
                return val_var(dst->u.var_name);
            }
            // A coroutine's name used as a value is its coro_ptr: the descriptor's address.
            if (sym && sym->kind == SYM_FUNC && sym->u.func.coro)
                return gen_coro_ptr(ctx, e->u.var, e->type);
            // A function designator used as a value decays to a pointer-to-function
            // (C11 §6.3.2.1p4).  Its symbol has function type while a function-pointer
            // *variable*'s symbol has pointer type, so the symbol kind disambiguates.
            // Materialize the function's label address explicitly — the bare name would
            // otherwise make the backend load mem[name] (the first code word).
            if (sym && unalias(sym->type)->kind == TYPE_FUNCTION) {
                Tac_Val *dst          = new_var_val(ctx, ast_type_to_tac_type(e->type));
                Tac_Instruction *in   = tac_new_instruction(TAC_INSTRUCTION_GET_ADDRESS);
                in->u.get_address.src = val_var(e->u.var);
                in->u.get_address.dst = dst;
                tac_append(ctx, in);
                return val_var(dst->u.var_name);
            }
        }
        // A read of a volatile scalar variable must re-read memory on every use.
        // Materialize it into a volatile COPY so the optimizer cannot fold or
        // propagate the value away. Aggregates are read via field access instead.
        return read_var(ctx, e->u.var, e->type);
    case EXPR_UNARY_OP:
        if (e->u.unary_op.op == UNARY_PLUS)
            return gen_expr(ctx, e->u.unary_op.expr);
        if (e->u.unary_op.op == UNARY_ADDRESS) {
            // &"string literal": the address of the string's static storage is exactly
            // the fat pointer its decayed-value path already materializes.  gen_lval has
            // no string-literal case, so route it through gen_expr.
            Expr *operand = e->u.unary_op.expr;
            if (operand->kind == EXPR_LITERAL && operand->u.literal->kind == LITERAL_STRING)
                return gen_expr(ctx, operand);
            return gen_lval(ctx, operand);
        }
        if (e->u.unary_op.op == UNARY_DEREF) {
            if (type_needs_slot(e->type))
                return gen_aggregate_rvalue(ctx, e);
            Tac_Val *addr = gen_lval(ctx, e);
            // Dereferencing a pointer-to-array yields a sub-array whose value is
            // its own address (array-to-pointer decay), not a scalar to load.
            const Type *opnd = unalias(e->u.unary_op.expr->type);
            if (opnd->kind == TYPE_POINTER && unalias(opnd->u.pointer.target)->kind == TYPE_ARRAY)
                return addr;
            Tac_Val *dst        = new_var_val(ctx, ast_type_to_tac_type(e->type));
            Tac_Instruction *in = tac_new_instruction(
                byte_access_for(e->type) ? TAC_INSTRUCTION_LOAD_BYTE : TAC_INSTRUCTION_LOAD);
            in->is_volatile    = type_is_volatile(e->type);
            in->u.load.src_ptr = addr;
            in->u.load.dst     = dst;
            tac_append(ctx, in);
            return val_var(dst->u.var_name);
        }
        if (e->u.unary_op.op == UNARY_PRE_INC || e->u.unary_op.op == UNARY_PRE_DEC) {
            Expr *inner = e->u.unary_op.expr;
            bool inc    = (e->u.unary_op.op == UNARY_PRE_INC);
            if (access_bitfield(inner))
                return gen_bitfield_step(ctx, inner, inc, false);
            if (inner->kind == EXPR_VAR) {
                const char *var = inner->u.var;
                const Tac_Val *vd =
                    gen_step(ctx, inner->type, read_var(ctx, var, inner->type), inc);
                Tac_Instruction *cp = tac_new_instruction(TAC_INSTRUCTION_COPY);
                cp->is_volatile     = type_is_volatile(inner->type);
                cp->u.copy.src      = val_var(vd->u.var_name);
                cp->u.copy.dst      = val_var(var);
                tac_append(ctx, cp);
                return val_var(vd->u.var_name);
            } else {
                bool vol          = type_is_volatile(inner->type);
                Tac_Val *addr_raw = gen_lval(ctx, inner);
                Tac_Val *loaded   = new_var_val(ctx, ast_type_to_tac_type(inner->type));
                Tac_Instruction *ld =
                    tac_new_instruction(byte_access_for(inner->type) ? TAC_INSTRUCTION_LOAD_BYTE
                                                                     : TAC_INSTRUCTION_LOAD);
                ld->is_volatile    = vol;
                ld->u.load.src_ptr = addr_raw;
                ld->u.load.dst     = loaded;
                tac_append(ctx, ld);
                const Tac_Val *result =
                    gen_step(ctx, inner->type, val_var(loaded->u.var_name), inc);
                Tac_Instruction *st =
                    tac_new_instruction(byte_access_for(inner->type) ? TAC_INSTRUCTION_STORE_BYTE
                                                                     : TAC_INSTRUCTION_STORE);
                st->is_volatile     = vol;
                st->u.store.src     = val_var(result->u.var_name);
                st->u.store.dst_ptr = val_var(addr_raw->u.var_name);
                tac_append(ctx, st);
                return val_var(result->u.var_name);
            }
        }
        return gen_unary(ctx, e->u.unary_op.op, e->u.unary_op.expr, e->type);
    case EXPR_BINARY_OP:
        if (e->u.binary_op.op == BINARY_LOG_AND)
            return gen_logical_and(ctx, e->u.binary_op.left, e->u.binary_op.right);
        if (e->u.binary_op.op == BINARY_LOG_OR)
            return gen_logical_or(ctx, e->u.binary_op.left, e->u.binary_op.right);
        if (e->u.binary_op.op == BINARY_COMMA) {
            // C11 6.5.17p2: evaluate the left operand for its side effects and throw
            // the value away (as an expression statement does), then yield the right.
            tac_free_val(gen_expr(ctx, e->u.binary_op.left));
            return gen_expr(ctx, e->u.binary_op.right);
        }
        return gen_binary(ctx, e->u.binary_op.op, e->u.binary_op.left, e->u.binary_op.right,
                          e->type);
    case EXPR_ASSIGN: {
        Expr *target = e->u.assign.target;
        // Whole-aggregate assignment (struct/union, simple `=`): copy word by word, with
        // each side reached by name or through its address — handles struct members and
        // pointer/subscript lvalues, not just named-base to named-base.
        if (e->u.assign.op == ASSIGN_SIMPLE && (unalias(target->type)->kind == TYPE_STRUCT ||
                                                unalias(target->type)->kind == TYPE_UNION))
            return gen_aggregate_assign(ctx, target, e->u.assign.value, NULL);
        Tac_Val *src = gen_expr(ctx, e->u.assign.value);
        if (access_bitfield(target))
            return gen_bitfield_assign(ctx, e, src);
        if (target->kind == EXPR_VAR) {
            const char *dst = target->u.var;
            bool vol        = type_is_volatile(target->type);
            if (e->u.assign.op == ASSIGN_SIMPLE) {
                Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_COPY);
                in->is_volatile     = vol;
                in->u.copy.src      = src;
                in->u.copy.dst      = val_var(dst);
                tac_append(ctx, in);
                // The value of an assignment to a volatile variable is the value
                // stored, not a second read of the variable.
                if (vol)
                    return dup_val(src);
            } else if ((is_byte_pointer(target->type) || wide_ptr_scale(target->type)) &&
                       (e->u.assign.op == ASSIGN_ADD || e->u.assign.op == ASSIGN_SUB)) {
                // char* += n (fat-pointer byte arithmetic) or pointer-to-array/-struct
                // += n (element-scaled), not a raw word add.
                int pscale   = is_byte_pointer(target->type) ? 1 : wide_ptr_scale(target->type);
                Tac_Val *res = gen_ptr_add(ctx, read_var(ctx, dst, target->type), src,
                                           e->u.assign.op == ASSIGN_SUB, pscale, target->type,
                                           e->u.assign.value->type);
                Tac_Instruction *cp = tac_new_instruction(TAC_INSTRUCTION_COPY);
                cp->is_volatile     = vol;
                cp->u.copy.src      = res;
                cp->u.copy.dst      = val_var(dst);
                tac_append(ctx, cp);
                return vol ? dup_val(res) : val_var(dst);
            } else {
                // Compound op computed in the common type (== e->u.assign.value->type
                // after typecheck's promotions): widen the lvalue, operate, narrow the
                // result back to the lvalue type.  For shift/bitwise (op_type ==
                // target type) `widen` is false and this is byte-identical to before.
                const Type *op_type = e->u.assign.value->type;
                bool widen          = unalias(op_type)->kind != unalias(target->type)->kind;
                Tac_Val *cur        = read_var(ctx, dst, target->type);
                Tac_Val *opnd       = widen ? emit_cast(ctx, cur, target->type, op_type) : cur;
                Tac_Val *vd         = new_var_val(
                    ctx, ast_type_to_tac_type(is_pointer(target->type) ? target->type : op_type));
                Tac_Instruction *bin = tac_new_instruction(TAC_INSTRUCTION_BINARY);
                bin->u.binary.op     = map_assign_op(e->u.assign.op, op_type);
                bin->u.binary.src1   = opnd;
                bin->u.binary.src2   = src;
                bin->u.binary.dst    = vd;
                tac_append(ctx, bin);
                Tac_Val *result =
                    widen ? emit_cast(ctx, val_var(vd->u.var_name), op_type, target->type)
                          : val_var(vd->u.var_name);
                Tac_Instruction *cp = tac_new_instruction(TAC_INSTRUCTION_COPY);
                cp->is_volatile     = vol;
                cp->u.copy.src      = result;
                cp->u.copy.dst      = val_var(dst);
                tac_append(ctx, cp);
                if (vol)
                    return dup_val(result);
            }
            return val_var(dst);
        } else if (target->kind == EXPR_FIELD_ACCESS &&
                   target->u.field_access.expr->kind == EXPR_VAR &&
                   e->u.assign.op == ASSIGN_SIMPLE) {
            const char *var_name = target->u.field_access.expr->u.var;
            int offset           = target->u.field_access.offset;
            // The value of `s.f = v' is the value stored (C11 6.5.16p3), NOT the aggregate
            // it went into: returning the base would name field 0 whatever `offset' says.
            Tac_Val *result          = dup_val(src);
            Tac_Instruction *in      = tac_new_instruction(byte_access_for(target->type)
                                                               ? TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET
                                                               : TAC_INSTRUCTION_COPY_TO_OFFSET);
            in->is_volatile          = type_is_volatile(target->type) ||
                                       type_is_volatile(target->u.field_access.expr->type);
            in->u.copy_to_offset.src = src;
            in->u.copy_to_offset.dst = xstrdup(var_name);
            in->u.copy_to_offset.offset = offset;
            tac_append(ctx, in);
            return result;
        } else {
            bool vol          = type_is_volatile(target->type);
            Tac_Val *addr_raw = gen_lval(ctx, target);
            if (e->u.assign.op == ASSIGN_SIMPLE) {
                // The value of `*p = v' is the value stored (C11 6.5.16p3) -- hand back a
                // second reference to it.  A fresh temp would be one nothing ever defines,
                // so `while ((*p = *q))' would branch on whatever the frame slot held.
                // Not a re-read through `p' either: that would be wrong for a volatile
                // lvalue, and would cost a load.
                Tac_Val *result = dup_val(src);
                Tac_Instruction *st =
                    tac_new_instruction(byte_access_for(target->type) ? TAC_INSTRUCTION_STORE_BYTE
                                                                      : TAC_INSTRUCTION_STORE);
                st->is_volatile     = vol;
                st->u.store.src     = src;
                st->u.store.dst_ptr = addr_raw;
                tac_append(ctx, st);
                return result;
            } else {
                Tac_Val *loaded = new_var_val(ctx, ast_type_to_tac_type(target->type));
                Tac_Instruction *ld =
                    tac_new_instruction(byte_access_for(target->type) ? TAC_INSTRUCTION_LOAD_BYTE
                                                                      : TAC_INSTRUCTION_LOAD);
                ld->is_volatile    = vol;
                ld->u.load.src_ptr = addr_raw;
                ld->u.load.dst     = loaded;
                tac_append(ctx, ld);
                Tac_Val *result;
                if ((is_byte_pointer(target->type) || wide_ptr_scale(target->type)) &&
                    (e->u.assign.op == ASSIGN_ADD || e->u.assign.op == ASSIGN_SUB)) {
                    // char* lvalue += n (fat-pointer byte arithmetic) or pointer-to-
                    // array/-struct += n (element-scaled).
                    int pscale = is_byte_pointer(target->type) ? 1 : wide_ptr_scale(target->type);
                    result     = gen_ptr_add(ctx, val_var(loaded->u.var_name), src,
                                             e->u.assign.op == ASSIGN_SUB, pscale, target->type,
                                             e->u.assign.value->type);
                } else {
                    // Compound op in the common type (== e->u.assign.value->type): widen
                    // the loaded lvalue, operate, narrow back.  No-op when op_type ==
                    // target type (shift/bitwise, same-type arithmetic).
                    const Type *op_type = e->u.assign.value->type;
                    bool widen          = unalias(op_type)->kind != unalias(target->type)->kind;
                    Tac_Val *opnd =
                        widen ? emit_cast(ctx, val_var(loaded->u.var_name), target->type, op_type)
                              : val_var(loaded->u.var_name);
                    Tac_Val *vd = new_var_val(
                        ctx,
                        ast_type_to_tac_type(is_pointer(target->type) ? target->type : op_type));
                    Tac_Instruction *bin = tac_new_instruction(TAC_INSTRUCTION_BINARY);
                    bin->u.binary.op     = map_assign_op(e->u.assign.op, op_type);
                    bin->u.binary.src1   = opnd;
                    bin->u.binary.src2   = src;
                    bin->u.binary.dst    = vd;
                    tac_append(ctx, bin);
                    result = widen ? emit_cast(ctx, val_var(vd->u.var_name), op_type, target->type)
                                   : val_var(vd->u.var_name);
                }
                Tac_Instruction *st =
                    tac_new_instruction(byte_access_for(target->type) ? TAC_INSTRUCTION_STORE_BYTE
                                                                      : TAC_INSTRUCTION_STORE);
                st->is_volatile     = vol;
                st->u.store.src     = result;
                st->u.store.dst_ptr = val_var(addr_raw->u.var_name);
                tac_append(ctx, st);
                return val_var(result->u.var_name);
            }
        }
    }
    case EXPR_COND: {
        // A struct/union conditional whose value spans multiple machine words cannot be
        // merged with a single-word COPY (that would carry only the first word, leaving
        // the rest of the aggregate — e.g. a packed char-array member — uninitialised).
        // Allocate a result slot and copy the whole aggregate out of each branch, the
        // same way the by-value/sret paths do.
        if (type_needs_slot(e->type)) {
            Tac_Val *cond_val = gen_cond_val(ctx, e->u.cond.condition);
            char *else_l      = new_temp(ctx);
            char *end_l       = new_temp(ctx);
            char *slot        = new_typed_temp(ctx, ast_type_to_tac_type(e->type));
            int size          = (int)get_size(e->type);

            Tac_Instruction *al            = tac_new_instruction(TAC_INSTRUCTION_ALLOCATE_LOCAL);
            al->u.allocate_local.name      = xstrdup(slot);
            al->u.allocate_local.size      = size;
            al->u.allocate_local.alignment = (int)get_alignment(e->type);
            tac_append(ctx, al);

            Tac_Instruction *jz          = tac_new_instruction(TAC_INSTRUCTION_JUMP_IF_ZERO);
            jz->u.jump_if_zero.condition = cond_val;
            jz->u.jump_if_zero.target    = else_l; // instruction takes ownership
            tac_append(ctx, jz);

            Tac_Val *then_val = gen_expr(ctx, e->u.cond.then_expr);
            gen_struct_assign(ctx, slot, 0, then_val->u.var_name, e->type);
            tac_free_val(then_val);
            emit_jump(ctx, end_l);

            emit_label(ctx, else_l);
            Tac_Val *else_val = gen_expr(ctx, e->u.cond.else_expr);
            gen_struct_assign(ctx, slot, 0, else_val->u.var_name, e->type);
            tac_free_val(else_val);

            emit_label(ctx, end_l);
            xfree(end_l);
            Tac_Val *result = val_var(slot);
            xfree(slot);
            return result;
        }
        Tac_Val *cond_val = gen_cond_val(ctx, e->u.cond.condition);
        char *else_l      = new_temp(ctx);
        char *end_l       = new_temp(ctx);
        char *dst_name    = new_typed_temp(ctx, ast_type_to_tac_type(e->type));

        Tac_Instruction *jz          = tac_new_instruction(TAC_INSTRUCTION_JUMP_IF_ZERO);
        jz->u.jump_if_zero.condition = cond_val;
        jz->u.jump_if_zero.target    = else_l; // instruction takes ownership
        tac_append(ctx, jz);

        Tac_Val *then_val        = gen_expr(ctx, e->u.cond.then_expr);
        Tac_Instruction *cp_then = tac_new_instruction(TAC_INSTRUCTION_COPY);
        cp_then->u.copy.src      = then_val;
        cp_then->u.copy.dst      = val_var(dst_name);
        tac_append(ctx, cp_then);
        emit_jump(ctx, end_l);

        emit_label(ctx, else_l);
        Tac_Val *else_val        = gen_expr(ctx, e->u.cond.else_expr);
        Tac_Instruction *cp_else = tac_new_instruction(TAC_INSTRUCTION_COPY);
        cp_else->u.copy.src      = else_val;
        cp_else->u.copy.dst      = val_var(dst_name);
        tac_append(ctx, cp_else);

        emit_label(ctx, end_l);
        xfree(end_l);
        Tac_Val *result = val_var(dst_name);
        xfree(dst_name);
        return result;
    }
    case EXPR_CAST: {
        Tac_Val *inner = gen_expr(ctx, e->u.cast.expr);
        return emit_cast(ctx, inner, e->u.cast.expr->type, e->u.cast.type);
    }
    case EXPR_CALL: {
        if (is_hw_sqrt(e)) {
            Tac_Val *src        = gen_expr(ctx, e->u.call.args);
            Tac_Val *vd         = new_var_val(ctx, ast_type_to_tac_type(e->type));
            Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_UNARY);
            in->u.unary.op      = TAC_UNARY_SQRT_DOUBLE;
            in->u.unary.src     = src;
            in->u.unary.dst     = vd;
            tac_append(ctx, in);
            return val_var(vd->u.var_name);
        }
        if (e->u.call.func->kind == EXPR_VAR &&
            strcmp(e->u.call.func->u.var, "__builtin_alloca") == 0)
            return gen_alloca(ctx, e);
        Tac_Val *args_head  = NULL;
        Tac_Val **args_tail = &args_head;
        for (Expr *arg = e->u.call.args; arg; arg = arg->next) {
            // On a struct_args_split target a struct wider than a word is marshalled as N
            // consecutive machine-word arguments (true by-value): read each word out of the struct
            // slot and append it as its own call argument.  The callee reserves N
            // contiguous param slots (see params_from_type), so the words line up.
            if (type_is_split_arg(arg->type)) {
                Tac_Val *sv = gen_expr(ctx, arg);
                int w       = target_word_bytes();
                int nwords  = ((int)get_size(arg->type) + w - 1) / w;
                for (int i = 0; i < nwords; i++) {
                    Tac_Val *t          = new_var_val(ctx, tac_type_word());
                    Tac_Instruction *ld = tac_new_instruction(TAC_INSTRUCTION_COPY_FROM_OFFSET);
                    ld->u.copy_from_offset.src    = xstrdup(sv->u.var_name);
                    ld->u.copy_from_offset.offset = i * w;
                    ld->u.copy_from_offset.dst    = t; // owned by the COPY_FROM_OFFSET
                    tac_append(ctx, ld);

                    *args_tail = val_var(t->u.var_name);
                    args_tail  = &(*args_tail)->next;
                }
                tac_free_val(sv);
                continue;
            }
            Tac_Val *av = gen_expr(ctx, arg);
            *args_tail  = av;
            args_tail   = &av->next;
        }

        // A struct return too wide to return by value: allocate a result slot, pass its
        // address as a hidden
        // first argument (sret ABI), and let the call write the struct into that slot.
        // The call expression's value is then the slot itself.
        char *sret_slot = NULL;
        if (type_is_byval_sret(e->type)) {
            char *slot                     = new_typed_temp(ctx, ast_type_to_tac_type(e->type));
            Tac_Instruction *al            = tac_new_instruction(TAC_INSTRUCTION_ALLOCATE_LOCAL);
            al->u.allocate_local.name      = xstrdup(slot);
            al->u.allocate_local.size      = (int)get_size(e->type);
            al->u.allocate_local.alignment = (int)get_alignment(e->type);
            tac_append(ctx, al);

            Tac_Val *addr         = new_var_val(ctx, tac_type_ptr_to(e->type));
            Tac_Instruction *ga   = tac_new_instruction(TAC_INSTRUCTION_GET_ADDRESS);
            ga->u.get_address.src = val_var(slot);
            ga->u.get_address.dst = addr; // owned by the GET_ADDRESS instruction
            tac_append(ctx, ga);

            // Prepend a *copy* of the address as argument #1 (the operand above is owned by
            // the GET_ADDRESS; the args list owns its own values).
            Tac_Val *arg0 = val_var(addr->u.var_name);
            arg0->next    = args_head;
            args_head     = arg0;
            sret_slot     = slot; // owns `slot`; freed below
        }

        Expr *func = e->u.call.func;
        const char *fun_name;
        Tac_Val *fn_ptr = NULL;
        bool indirect   = true;
        if (func->kind == EXPR_VAR) {
            // A bare name: the callee itself, or a *variable* holding its address.  Either
            // way the name is what the call names, so this arm is shared; what differs is
            // how the backend must read it, which is what `indirect` records.  The node's
            // own type tells them apart — a function designator has function type, a
            // function-pointer variable has pointer type — and typecheck stamped it there
            // from the symbol precisely because the symbol is gone by now: locals and
            // parameters are purged from the symbol table when their block ends.  (Do not
            // gen_expr the node either: a bare-name callee is not decayed, so its value is
            // the name itself.)
            indirect = func->type && unalias(func->type)->kind != TYPE_FUNCTION;
            fun_name = func->u.var;
        } else {
            // Indirect call. C11 §6.3.2.1p4: dereferencing a function pointer yields a
            // function designator that immediately decays back to the same pointer, so
            // (*fp)(...) is equivalent to fp(...).  Strip the DEREF whenever its operand
            // has function-pointer type and call through the pointer value directly,
            // rather than emitting a LOAD from it.  (The DEREF node's own type is the
            // re-decayed pointer type, so the operand's type is the reliable signal.)
            Expr *callee = func;
            if (callee->kind == EXPR_UNARY_OP && callee->u.unary_op.op == UNARY_DEREF) {
                const Type *opnd = unalias(callee->u.unary_op.expr->type);
                if (opnd && opnd->kind == TYPE_POINTER &&
                    unalias(opnd->u.pointer.target)->kind == TYPE_FUNCTION)
                    callee = callee->u.unary_op.expr;
            }
            fn_ptr   = gen_expr(ctx, callee);
            fun_name = fn_ptr->u.var_name;
        }

        // With the sret ABI the struct result lives in the slot we allocated, not in a
        // scalar destination register, so the call has no scalar `dst`.
        Tac_Val *dst = (unalias(e->type)->kind != TYPE_VOID && !sret_slot)
                           ? new_var_val(ctx, ast_type_to_tac_type(e->type))
                           : NULL;
        // A direct call to a _Noreturn function never returns, so emit the dedicated kind:
        // the backend tail-jumps to it and drops the dead post-call path.  (Indirect calls
        // through a function pointer stay a plain FUN_CALL — the callee is not known here,
        // which is also why FUN_CALL_NORETURN never carries the `indirect` flag.)
        Tac_InstructionKind call_kind = TAC_INSTRUCTION_FUN_CALL;
        if (!indirect) {
            const Symbol *sym = symtab_get_opt(func->u.var);
            if (sym && sym->kind == SYM_FUNC && sym->u.func.noret)
                call_kind = TAC_INSTRUCTION_FUN_CALL_NORETURN;
        }
        Tac_Instruction *in     = tac_new_instruction(call_kind);
        in->u.fun_call.fun_name = xstrdup(fun_name);
        in->u.fun_call.indirect = indirect;
        in->u.fun_call.args     = args_head;
        in->u.fun_call.dst      = dst;
        // The callee's type: the designator's own, or the pointee of a function pointer.
        const Type *ft = func->type ? unalias(func->type) : NULL;
        if (ft && ft->kind == TYPE_POINTER)
            ft = unalias(ft->u.pointer.target);
        if (ft && ft->kind == TYPE_FUNCTION)
            in->u.fun_call.fun_type = ast_type_to_tac_type(ft);
        tac_append(ctx, in);

        if (fn_ptr)
            tac_free_val(fn_ptr);

        if (sret_slot) {
            Tac_Val *result = val_var(sret_slot);
            xfree(sret_slot);
            return result;
        }
        return dst ? val_var(dst->u.var_name) : val_int(0);
    }
    case EXPR_POST_INC:
    case EXPR_POST_DEC: {
        Expr *inner = (e->kind == EXPR_POST_INC) ? e->u.post_inc : e->u.post_dec;
        bool inc    = (e->kind == EXPR_POST_INC);
        if (access_bitfield(inner))
            return gen_bitfield_step(ctx, inner, inc, true);
        if (inner->kind == EXPR_VAR) {
            bool vol             = type_is_volatile(inner->type);
            const char *var      = inner->u.var;
            Tac_Val *old         = new_var_val(ctx, ast_type_to_tac_type(inner->type));
            Tac_Instruction *cp1 = tac_new_instruction(TAC_INSTRUCTION_COPY);
            cp1->is_volatile     = vol;
            cp1->u.copy.src      = val_var(var);
            cp1->u.copy.dst      = old;
            tac_append(ctx, cp1);
            // A volatile variable is read once, into `old`.
            const Tac_Val *vd =
                gen_step(ctx, inner->type, vol ? val_var(old->u.var_name) : val_var(var), inc);
            Tac_Instruction *cp2 = tac_new_instruction(TAC_INSTRUCTION_COPY);
            cp2->is_volatile     = vol;
            cp2->u.copy.src      = val_var(vd->u.var_name);
            cp2->u.copy.dst      = val_var(var);
            tac_append(ctx, cp2);
            return val_var(old->u.var_name);
        } else {
            bool vol            = type_is_volatile(inner->type);
            Tac_Val *addr_raw   = gen_lval(ctx, inner);
            Tac_Val *old        = new_var_val(ctx, ast_type_to_tac_type(inner->type));
            Tac_Instruction *ld = tac_new_instruction(
                byte_access_for(inner->type) ? TAC_INSTRUCTION_LOAD_BYTE : TAC_INSTRUCTION_LOAD);
            ld->is_volatile    = vol;
            ld->u.load.src_ptr = addr_raw;
            ld->u.load.dst     = old;
            tac_append(ctx, ld);
            const Tac_Val *result = gen_step(ctx, inner->type, val_var(old->u.var_name), inc);
            Tac_Instruction *st   = tac_new_instruction(
                byte_access_for(inner->type) ? TAC_INSTRUCTION_STORE_BYTE : TAC_INSTRUCTION_STORE);
            st->is_volatile     = vol;
            st->u.store.src     = val_var(result->u.var_name);
            st->u.store.dst_ptr = val_var(addr_raw->u.var_name);
            tac_append(ctx, st);
            return val_var(old->u.var_name);
        }
    }
    case EXPR_SUBSCRIPT: {
        if (type_needs_slot(e->type))
            return gen_aggregate_rvalue(ctx, e);
        Tac_Val *addr = gen_lval(ctx, e);
        // If the subscript selects a sub-array of a multi-dimensional array, its
        // value is the address of that sub-array (array-to-pointer decay), not a
        // load of a scalar.
        const Expr *ptr_exp =
            is_pointer(e->u.subscript.left->type) ? e->u.subscript.left : e->u.subscript.right;
        if (unalias(unalias(ptr_exp->type)->u.pointer.target)->kind == TYPE_ARRAY)
            return addr;
        Tac_Val *dst        = new_var_val(ctx, ast_type_to_tac_type(e->type));
        Tac_Instruction *in = tac_new_instruction(
            byte_access_for(e->type) ? TAC_INSTRUCTION_LOAD_BYTE : TAC_INSTRUCTION_LOAD);
        in->is_volatile    = type_is_volatile(e->type);
        in->u.load.src_ptr = addr;
        in->u.load.dst     = dst;
        tac_append(ctx, in);
        return val_var(dst->u.var_name);
    }
    case EXPR_SIZEOF_EXPR:
        return val_size(get_size(e->u.sizeof_expr->type));
    case EXPR_SIZEOF_TYPE:
        return val_size(get_size(e->u.sizeof_type));
    case EXPR_ALIGNOF:
        return val_size(get_alignment(e->u.align_of));
    case EXPR_VA_CLASS:
        return val_int(va_class_of(e->u.va_class));
    case EXPR_FIELD_ACCESS: {
        if (access_bitfield(e))
            return gen_bitfield_read(ctx, e);
        const Expr *base = e->u.field_access.expr;
        int offset       = e->u.field_access.offset;
        // An array-typed member is not loaded: it decays to the address of its
        // first element (e.g. `s.arr` / `x.b.inner_arr`), just like a subscript
        // selecting a sub-array.  The member's array type is recovered from the
        // node's cached member type because e->type was decayed at typecheck.
        {
            const Type *mt = unalias(field_member_type(e));
            if (mt && mt->kind == TYPE_ARRAY)
                return gen_lval(ctx, e);
        }
        if (type_needs_slot(e->type))
            return gen_aggregate_rvalue(ctx, e);
        if (base->kind == EXPR_VAR) {
            Tac_Val *dst = new_var_val(ctx, ast_type_to_tac_type(e->type));
            Tac_Instruction *in =
                tac_new_instruction(byte_access_for(e->type) ? TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET
                                                             : TAC_INSTRUCTION_COPY_FROM_OFFSET);
            in->is_volatile            = type_is_volatile(e->type) || type_is_volatile(base->type);
            in->u.copy_from_offset.src = xstrdup(base->u.var);
            in->u.copy_from_offset.offset = offset;
            in->u.copy_from_offset.dst    = dst;
            tac_append(ctx, in);
            return val_var(dst->u.var_name);
        } else {
            Tac_Val *addr       = gen_lval(ctx, e);
            Tac_Val *dst        = new_var_val(ctx, ast_type_to_tac_type(e->type));
            Tac_Instruction *in = tac_new_instruction(
                byte_access_for(e->type) ? TAC_INSTRUCTION_LOAD_BYTE : TAC_INSTRUCTION_LOAD);
            in->is_volatile    = type_is_volatile(e->type);
            in->u.load.src_ptr = addr;
            in->u.load.dst     = dst;
            tac_append(ctx, in);
            return val_var(dst->u.var_name);
        }
    }
    case EXPR_PTR_ACCESS: {
        if (access_bitfield(e))
            return gen_bitfield_read(ctx, e);
        // An array-typed member decays to the address of its first element.
        {
            const Type *mt = unalias(field_member_type(e));
            if (mt && mt->kind == TYPE_ARRAY)
                return gen_lval(ctx, e);
        }
        if (type_needs_slot(e->type))
            return gen_aggregate_rvalue(ctx, e);
        Tac_Val *addr       = gen_lval(ctx, e);
        Tac_Val *dst        = new_var_val(ctx, ast_type_to_tac_type(e->type));
        Tac_Instruction *in = tac_new_instruction(
            byte_access_for(e->type) ? TAC_INSTRUCTION_LOAD_BYTE : TAC_INSTRUCTION_LOAD);
        in->is_volatile    = type_is_volatile(e->type);
        in->u.load.src_ptr = addr;
        in->u.load.dst     = dst;
        tac_append(ctx, in);
        return val_var(dst->u.var_name);
    }
    case EXPR_GENERIC: {
        GenericAssoc *match = e->u.generic.associations;
        Expr *match_expr =
            (match->kind == GENERIC_ASSOC_TYPE) ? match->u.type_assoc.expr : match->u.default_assoc;
        return gen_expr(ctx, match_expr);
    }
    case EXPR_COMPOUND: {
        const Type *lit_type = unalias(e->u.compound_literal.type);
        if (lit_type->kind == TYPE_ARRAY) {
            return gen_lval(ctx, e); // decays to its address
        }
        if (is_aggregate_type(lit_type)) {
            // Like an sret call, the value of a struct/union literal is its slot.
            char *slot   = gen_compound_literal(ctx, e);
            Tac_Val *val = val_var(slot);
            xfree(slot);
            return val;
        }
        return gen_expr(ctx, e->u.compound_literal.init->init->u.expr);
    }
    case EXPR_YIELD:
        return gen_yield(ctx, e);
    case EXPR_CO_OP:
        return gen_co_op(ctx, e);
    case EXPR_AWAIT:
        return gen_await(ctx, e);
    default:
        internal_error("unsupported expression kind %d in TAC lowering", (int)e->kind);
    }
}

// gen_expr with diag_loc at the node, for the errors found in it.
Tac_Val *gen_expr(TacCtx *ctx, Expr *e)
{
    SrcLoc saved    = diag_enter(e ? e->loc : diag_loc);
    Tac_Val *result = gen_expr_at(ctx, e);
    diag_loc        = saved;
    return result;
}
