//
// AST to TAC lowering: shared helpers, type conversion, and top-level entry points.
//

#include "translate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "string_map.h"
#include "structtab.h"
#include "target.h"
#include "typecheck.h"
#include "xalloc.h"

// Enable debug output
int translator_debug;

#ifdef NDEBUG
int translate_verify;
#else
int translate_verify = 1;
#endif

// A while or for loop is tested at its bottom, behind a guard at its top: see gen_stmt.
int translate_rotate_loops;

//
// Low-level TAC-building helpers
//

void tac_append(TacCtx *ctx, Tac_Instruction *instr)
{
    if (!ctx->head) {
        ctx->head = ctx->tail = instr;
    } else {
        ctx->tail->next = instr;
        ctx->tail       = instr;
    }
    instr->next = NULL;
}

char *new_temp(TacCtx *ctx)
{
    return xstruniq("%", &ctx->temp_id);
}

// Append {name, type} to the function's symbol list (takes ownership of `type`).
static void record_symbol(TacCtx *ctx, const char *name, Tac_Type *type)
{
    Tac_Param *p = tac_new_param();
    p->name      = xstrdup(name);
    p->type      = type;
    p->next      = NULL;
    if (!ctx->locals)
        ctx->locals = ctx->locals_tail = p;
    else {
        ctx->locals_tail->next = p;
        ctx->locals_tail       = p;
    }
}

char *new_typed_temp(TacCtx *ctx, Tac_Type *type)
{
    char *name = new_temp(ctx);
    record_symbol(ctx, name, type);
    return name;
}

// Record an automatic local variable on the function being lowered.  Besides typing
// it, the list tells the optimizer private locals (whose dead stores may be removed)
// from observable globals (whose stores must be preserved).
void tac_record_local(TacCtx *ctx, const char *name, const Type *type)
{
    // A deferred statement is lowered once per exit, so its locals are met again.
    if (ctx->defer_depth > 0) {
        for (const Tac_Param *p = ctx->locals; p; p = p->next)
            if (strcmp(p->name, name) == 0)
                return;
    }
    record_symbol(ctx, name, ast_type_to_tac_type(type));
}

// Record a local array name so a later value use (decay) can be lowered to a GET_ADDRESS
// (or fat-pointer GET_ADDRESS_DECAY for char/void).  Local symbols are purged from the
// symbol table before lowering, so the translator tracks arrays itself (globals/strings
// stay queryable via symtab).
void tac_record_array_local(TacCtx *ctx, const char *name)
{
    Tac_Param *p      = tac_new_param();
    p->name           = xstrdup(name);
    p->next           = ctx->array_locals;
    ctx->array_locals = p;
}

void tac_record_extern(TacCtx *ctx, const char *name, const Type *type)
{
    Tac_TopLevel **tail = &ctx->externs;
    for (; *tail; tail = &(*tail)->next)
        if (strcmp((*tail)->u.extern_.name, name) == 0)
            return;
    Tac_TopLevel *ext   = tac_new_toplevel(TAC_TOPLEVEL_EXTERN);
    ext->u.extern_.name = xstrdup(name);
    ext->u.extern_.type = ast_type_to_tac_type(type);
    *tail               = ext;
}

void tac_record_extern_tac(TacCtx *ctx, const char *name, Tac_Type *type)
{
    Tac_TopLevel **tail = &ctx->externs;
    for (; *tail; tail = &(*tail)->next)
        if (strcmp((*tail)->u.extern_.name, name) == 0) {
            tac_free_type(type);
            return;
        }
    Tac_TopLevel *ext   = tac_new_toplevel(TAC_TOPLEVEL_EXTERN);
    ext->u.extern_.name = xstrdup(name);
    ext->u.extern_.type = type;
    *tail               = ext;
}

bool tac_is_array_local(const TacCtx *ctx, const char *name)
{
    for (const Tac_Param *p = ctx->array_locals; p; p = p->next)
        if (strcmp(p->name, name) == 0)
            return true;
    return false;
}

Tac_Val *val_int(int64_t v)
{
    Tac_Val *tv    = tac_new_val(TAC_VAL_CONSTANT);
    Tac_Const *c   = tac_new_const(TAC_CONST_INT);
    c->u.int_val   = v;
    tv->u.constant = c;
    return tv;
}

Tac_Val *val_long(long v)
{
    Tac_Val *tv    = tac_new_val(TAC_VAL_CONSTANT);
    Tac_Const *c   = tac_new_const(TAC_CONST_LONG);
    c->u.long_val  = v;
    tv->u.constant = c;
    return tv;
}

Tac_Val *val_long_long(long long v)
{
    Tac_Val *tv        = tac_new_val(TAC_VAL_CONSTANT);
    Tac_Const *c       = tac_new_const(TAC_CONST_LONG_LONG);
    c->u.long_long_val = v;
    tv->u.constant     = c;
    return tv;
}

Tac_Val *val_uint(uint64_t v)
{
    Tac_Val *tv    = tac_new_val(TAC_VAL_CONSTANT);
    Tac_Const *c   = tac_new_const(TAC_CONST_UINT);
    c->u.uint_val  = v;
    tv->u.constant = c;
    return tv;
}

Tac_Val *val_ulong(unsigned long v)
{
    Tac_Val *tv    = tac_new_val(TAC_VAL_CONSTANT);
    Tac_Const *c   = tac_new_const(TAC_CONST_ULONG);
    c->u.ulong_val = v;
    tv->u.constant = c;
    return tv;
}

Tac_Val *val_ulong_long(unsigned long long v)
{
    Tac_Val *tv         = tac_new_val(TAC_VAL_CONSTANT);
    Tac_Const *c        = tac_new_const(TAC_CONST_ULONG_LONG);
    c->u.ulong_long_val = v;
    tv->u.constant      = c;
    return tv;
}

Tac_Val *val_float(float v)
{
    Tac_Val *tv    = tac_new_val(TAC_VAL_CONSTANT);
    Tac_Const *c   = tac_new_const(TAC_CONST_FLOAT);
    c->u.float_val = v;
    tv->u.constant = c;
    return tv;
}

Tac_Val *val_double(double v)
{
    Tac_Val *tv     = tac_new_val(TAC_VAL_CONSTANT);
    Tac_Const *c    = tac_new_const(TAC_CONST_DOUBLE);
    c->u.double_val = v;
    tv->u.constant  = c;
    return tv;
}

Tac_Val *val_long_double(Float128 v)
{
    Tac_Val *tv          = tac_new_val(TAC_VAL_CONSTANT);
    Tac_Const *c         = tac_new_const(TAC_CONST_LONG_DOUBLE);
    c->u.long_double_val = v;
    tv->u.constant       = c;
    return tv;
}

Tac_Val *val_var(const char *name)
{
    Tac_Val *tv    = tac_new_val(TAC_VAL_VAR);
    tv->u.var_name = xstrdup(name);
    return tv;
}

Tac_Val *new_var_val(TacCtx *ctx, Tac_Type *type)
{
    char *d       = new_typed_temp(ctx, type);
    Tac_Val *v    = tac_new_val(TAC_VAL_VAR);
    v->u.var_name = d;
    return v;
}

//
// A second, independently owned reference to the same value.
// A constant of the target's size_t, the type of sizeof and _Alignof. It must
// carry that kind, not int's: folded, `p - sizeof(T)` negates it, and a 32-bit
// negative would then be zero-extended into a 64-bit pointer offset.
Tac_Val *val_size(uint64_t v)
{
    switch (size_kind()) {
    case TYPE_UINT:
        return val_uint(v);
    case TYPE_ULONG:
        return val_ulong((unsigned long)v);
    default:
        return val_ulong_long((unsigned long long)v);
    }
}

//
// Every Tac_Val belongs to the one instruction it is attached to, so a value handed both
// to an instruction and back to the caller has to be duplicated -- returning the pointer
// itself would have tac_free() release it twice.  Tac_Const is plain data (no pointers,
// see tac.h), so a constant copies by value.
//
Tac_Val *val_zero(const Type *t)
{
    switch (unalias(t)->kind) {
    case TYPE_FLOAT:
        return val_float(0.0f);
    case TYPE_DOUBLE:
        return val_double(0.0);
    case TYPE_LONG_DOUBLE:
        return val_long_double(f128_from_i64(0));
    default:
        return val_int(0); // integers, enums and word pointers: a zero word
    }
}

Tac_Val *dup_val(const Tac_Val *v)
{
    Tac_Val *tv;

    if (v->kind == TAC_VAL_VAR)
        return val_var(v->u.var_name);

    tv              = tac_new_val(TAC_VAL_CONSTANT);
    tv->u.constant  = tac_new_const(v->u.constant->kind);
    *tv->u.constant = *v->u.constant;
    return tv;
}

// A "fat pointer" on byte-addressed targets (BESM-6: char*/void*) carries a byte
// offset and a marker bit, so it has a different bit layout from a plain word
// pointer (int*, etc.).  True when t is a pointer whose pointee is a character type
// or void.  Used to choose the pointer-representation conversion in emit_cast.
static bool is_fat_pointer(const Type *t)
{
    t = unalias(t);
    if (t->kind != TYPE_POINTER)
        return false;
    const Type *target = unalias(t->u.pointer.target);
    // A pointer to a char-innermost array (char(*)[N], from decaying a multi-dimensional
    // char array) is a fat byte pointer too: look through array element types.
    while (target->kind == TYPE_ARRAY)
        target = unalias(target->u.array.element);
    return is_character(target) || target->kind == TYPE_VOID;
}

// Map an integer destination type to the Tac_ConstKind a folded conversion result
// should carry, or -1 when the type has no direct Tac_ConstKind (short and enum; also
// _Bool, though a conversion *to* _Bool never reaches a width conversion — see
// emit_cast) so the folder keeps its legacy source-derived kind.  Threaded into the integer-width
// conversions by emit_cast: it is what lets the folder tell a promotion `unsigned char →
// int` (signed result) from a cast `unsigned char → unsigned int`, which lower to the
// same ZERO_EXTEND.  Plain `char` follows the target's signedness via is_signed.
static int const_kind_of_int_type(const Type *t)
{
    t = unalias(t);
    switch (t->kind) {
    case TYPE_INT:
        return TAC_CONST_INT;
    case TYPE_LONG:
        return TAC_CONST_LONG;
    case TYPE_LONG_LONG:
        return TAC_CONST_LONG_LONG;
    case TYPE_UINT:
        return TAC_CONST_UINT;
    case TYPE_ULONG:
        return TAC_CONST_ULONG;
    case TYPE_ULONG_LONG:
        return TAC_CONST_ULONG_LONG;
    case TYPE_SCHAR:
        return TAC_CONST_SCHAR;
    case TYPE_UCHAR:
        return TAC_CONST_UCHAR;
    case TYPE_CHAR:
        return is_signed(t) ? TAC_CONST_SCHAR : TAC_CONST_UCHAR;
    default:
        return -1;
    }
}

//
// Emit "dst = (src != 0)" — the C11 §6.3.1.2 conversion of a scalar value to _Bool.
// A char*/void* is a *fat* pointer: even a null one carries a byte-offset marker, so the
// raw word is not zero and must be reduced to its bare address first, exactly as
// gen_cond_val does for `if (p)`.  A floating-point source is tested against a zero
// constant of its own kind: NOT_EQUAL on FP operands is the same bit comparison `if (d)`
// already lowers to, and matching kinds is what lets the constant folder fold it.
//
// Returns the destination Val *owned by the emitted instruction* (the gen_step
// convention); a caller that hands the result on as its own value re-wraps it with
// val_var, as emit_cast does.
//
Tac_Val *emit_bool_normalize(TacCtx *ctx, Tac_Val *src, const Type *from, const Type *to)
{
    const Type *f = unalias(from);
    if (is_fat_pointer(f)) {
        Tac_Val *addr             = new_var_val(ctx, tac_type_ptr(tac_new_type(TAC_TYPE_VOID)));
        Tac_Instruction *in       = tac_new_instruction(TAC_INSTRUCTION_CHAR_PTR_TO_PTR);
        in->u.char_ptr_to_ptr.src = src;
        in->u.char_ptr_to_ptr.dst = addr;
        tac_append(ctx, in);
        src = val_var(addr->u.var_name);
    }

    Tac_Val *dst        = new_var_val(ctx, ast_type_to_tac_type(to));
    Tac_Instruction *ne = tac_new_instruction(TAC_INSTRUCTION_BINARY);
    ne->u.binary.op     = TAC_BINARY_NOT_EQUAL;
    ne->u.binary.src1   = src;
    ne->u.binary.src2   = val_zero(f);
    ne->u.binary.dst    = dst;
    tac_append(ctx, ne);
    return dst;
}

Tac_Val *emit_cast(TacCtx *ctx, Tac_Val *src, const Type *from, const Type *to)
{
    // A cast to void discards the value: the operand has already been evaluated
    // for its side effects, and a void expression can never be used as a value.
    // Emit no conversion and hand the source back unchanged (the caller drops it).
    // This must precede the pointer/size logic below, which would otherwise call
    // get_size(void) — a fatal error — for a pointer cast to void, e.g. va_end's
    // ((void)ap).
    if (to->kind == TYPE_VOID)
        return src;

    // C11 §6.3.1.2: "When any scalar value is converted to _Bool, the result is 0 if the
    // value compares equal to 0; otherwise the result is 1."  That is a zero *test*, not a
    // width conversion, so it must pre-empt the size-driven TRUNCATE/EXTEND/COPY logic
    // below — which, _Bool being int-sized here, would emit a bare COPY and store the raw
    // integer.  Every runtime conversion to _Bool funnels through this one function:
    // assignment, initialization of an automatic object, argument passing, return, and the
    // explicit cast all become an EXPR_CAST.  (A static initializer normalizes in
    // semantic/const_convert.c instead; ++/-- in gen_step, which never reaches here.)
    // A _Bool *source* needs nothing — it already holds 0 or 1.  This precedes the `dst`
    // temporary below so the pass-through burns no temporary name.
    if (unalias(to)->kind == TYPE_BOOL) {
        if (unalias(from)->kind == TYPE_BOOL)
            return src;
        return val_var(emit_bool_normalize(ctx, src, from, to)->u.var_name);
    }

    bool from_int = is_integer(from);
    bool to_int   = is_integer(to);
    bool from_ptr = is_pointer(from);
    bool to_ptr   = is_pointer(to);
    Tac_Val *dst  = new_var_val(ctx, ast_type_to_tac_type(to));

    if (from_ptr || to_ptr) {
        size_t from_size = get_size(from);
        size_t to_size   = get_size(to);
        if (from_ptr && to_ptr) {
            bool from_fat = is_fat_pointer(from);
            bool to_fat   = is_fat_pointer(to);
            if (!from_fat && to_fat) {
                // word pointer → char*/void*: set the fat marker and byte offset.
                Tac_Instruction *in       = tac_new_instruction(TAC_INSTRUCTION_PTR_TO_CHAR_PTR);
                in->u.ptr_to_char_ptr.src = src;
                in->u.ptr_to_char_ptr.dst = dst;
                tac_append(ctx, in);
            } else if (from_fat && !to_fat) {
                // char*/void* → word pointer: clear the fat marker and offset.
                Tac_Instruction *in       = tac_new_instruction(TAC_INSTRUCTION_CHAR_PTR_TO_PTR);
                in->u.char_ptr_to_ptr.src = src;
                in->u.char_ptr_to_ptr.dst = dst;
                tac_append(ctx, in);
            } else {
                // word↔word or fat↔fat (incl. char*↔void*): identical representation.
                Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_COPY);
                in->u.copy.src      = src;
                in->u.copy.dst      = dst;
                tac_append(ctx, in);
            }
        } else if (to_ptr) {
            // integer → pointer
            if (from_size < to_size) {
                if (is_signed(from)) {
                    Tac_Instruction *in   = tac_new_instruction(TAC_INSTRUCTION_SIGN_EXTEND);
                    in->u.sign_extend.src = src;
                    in->u.sign_extend.dst = dst;
                    tac_append(ctx, in);
                } else {
                    Tac_Instruction *in   = tac_new_instruction(TAC_INSTRUCTION_ZERO_EXTEND);
                    in->u.zero_extend.src = src;
                    in->u.zero_extend.dst = dst;
                    tac_append(ctx, in);
                }
            } else if (from_size > to_size) {
                // long → a 2-byte pointer (AVR)
                Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_TRUNCATE);
                in->u.truncate.src  = src;
                in->u.truncate.dst  = dst;
                tac_append(ctx, in);
            } else {
                Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_COPY);
                in->u.copy.src      = src;
                in->u.copy.dst      = dst;
                tac_append(ctx, in);
            }
        } else {
            // pointer → integer
            if (from_size > to_size) {
                Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_TRUNCATE);
                in->u.truncate.src  = src;
                in->u.truncate.dst  = dst;
                tac_append(ctx, in);
            } else if (from_size < to_size) {
                // a 2-byte pointer → long (AVR): zero-extended, as clang's ptrtoint
                Tac_Instruction *in   = tac_new_instruction(TAC_INSTRUCTION_ZERO_EXTEND);
                in->u.zero_extend.src = src;
                in->u.zero_extend.dst = dst;
                tac_append(ctx, in);
            } else {
                Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_COPY);
                in->u.copy.src      = src;
                in->u.copy.dst      = dst;
                tac_append(ctx, in);
            }
        }
        return val_var(dst->u.var_name);
    }

    if (from_int && to_int) {
        size_t from_size = get_size(from);
        size_t to_size   = get_size(to);
        if (to_size < from_size) {
            Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_TRUNCATE);
            in->u.truncate.src  = src;
            in->u.truncate.dst  = dst;
            // A truncation to a type with no constant kind (a short narrower than
            // int) must not fold as the folder's legacy int→char.
            int kind                = const_kind_of_int_type(to);
            in->u.truncate.dst_kind = kind >= 0 ? kind : TAC_DST_KIND_NO_FOLD;
            tac_append(ctx, in);
        } else if (to_size == from_size) {
            // Same C size (e.g. int↔unsigned, long↔unsigned long): a bare COPY.
            // NB: on BESM-6 this makes a signed→unsigned conversion deviate from
            // C11 §6.3.1.3p2.  Signed int/long is a 41-bit type (bits 42-48 forced
            // to zero) while unsigned uses all 48 bits, yet the COPY reuses the
            // 41-bit pattern verbatim with no sign extension — so a negative source
            // is not adjusted to `value + 2^48`.  E.g. `(unsigned long)-1` is
            // 2^41-1, not ULONG_MAX (2^48-1).  This is an intentional deviation
            // (matching how the target keeps signed integers in 41 bits); see
            // backend/besm6/Besm6_Data_Representation.md.
            Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_COPY);
            in->u.copy.src      = src;
            in->u.copy.dst      = dst;
            tac_append(ctx, in);
        } else if (is_signed(from)) {
            Tac_Instruction *in        = tac_new_instruction(TAC_INSTRUCTION_SIGN_EXTEND);
            in->u.sign_extend.src      = src;
            in->u.sign_extend.dst      = dst;
            in->u.sign_extend.dst_kind = const_kind_of_int_type(to);
            tac_append(ctx, in);
        } else {
            Tac_Instruction *in        = tac_new_instruction(TAC_INSTRUCTION_ZERO_EXTEND);
            in->u.zero_extend.src      = src;
            in->u.zero_extend.dst      = dst;
            in->u.zero_extend.dst_kind = const_kind_of_int_type(to);
            tac_append(ctx, in);
        }
    } else if (!from_int && to_int) {
        // float/double/long double → integer
        bool from_float       = (from->kind == TYPE_FLOAT);
        bool from_long_double = (from->kind == TYPE_LONG_DOUBLE);
        if (is_signed(to)) {
            Tac_InstructionKind op       = from_float         ? TAC_INSTRUCTION_FLOAT_TO_INT
                                           : from_long_double ? TAC_INSTRUCTION_LONG_DOUBLE_TO_INT
                                                              : TAC_INSTRUCTION_DOUBLE_TO_INT;
            Tac_Instruction *in          = tac_new_instruction(op);
            in->u.double_to_int.src      = src;
            in->u.double_to_int.dst      = dst;
            in->u.double_to_int.dst_kind = const_kind_of_int_type(to);
            tac_append(ctx, in);
        } else {
            Tac_InstructionKind op        = from_float         ? TAC_INSTRUCTION_FLOAT_TO_UINT
                                            : from_long_double ? TAC_INSTRUCTION_LONG_DOUBLE_TO_UINT
                                                               : TAC_INSTRUCTION_DOUBLE_TO_UINT;
            Tac_Instruction *in           = tac_new_instruction(op);
            in->u.double_to_uint.src      = src;
            in->u.double_to_uint.dst      = dst;
            in->u.double_to_uint.dst_kind = const_kind_of_int_type(to);
            tac_append(ctx, in);
        }
    } else if (from_int && !to_int) {
        // integer → float/double/long double
        //
        // A sub-int integer source (char on BESM-6) must be promoted to a full word
        // before the FP conversion: b/utod and the inline INT-format path both assume a
        // full-width operand, so an unwidened unsigned char >= 128 carries garbage in the
        // high bits (task #30).  Mirror the integer promotion the int->int path performs.
        // A normalized _Bool holds 0 or 1, so it converts correctly through either path;
        // take the signed one, which the BESM-6 backend lowers inline (INT format +
        // normalize) instead of through the b/utod helper call.
        bool from_signed = is_signed(from) || unalias(from)->kind == TYPE_BOOL;
        if (get_size(from) < target_config->int_size) {
            Tac_Val *ext = new_var_val(ctx, tac_new_type(TAC_TYPE_INT));
            // The widening promotes to `int`, so label the folded result signed.
            if (from_signed) {
                Tac_Instruction *e        = tac_new_instruction(TAC_INSTRUCTION_SIGN_EXTEND);
                e->u.sign_extend.src      = src;
                e->u.sign_extend.dst      = ext;
                e->u.sign_extend.dst_kind = TAC_CONST_INT;
                tac_append(ctx, e);
            } else {
                Tac_Instruction *e        = tac_new_instruction(TAC_INSTRUCTION_ZERO_EXTEND);
                e->u.zero_extend.src      = src;
                e->u.zero_extend.dst      = ext;
                e->u.zero_extend.dst_kind = TAC_CONST_INT;
                tac_append(ctx, e);
            }
            src = val_var(ext->u.var_name);
        }
        bool to_float       = (to->kind == TYPE_FLOAT);
        bool to_long_double = (to->kind == TYPE_LONG_DOUBLE);
        if (from_signed) {
            Tac_InstructionKind op  = to_float         ? TAC_INSTRUCTION_INT_TO_FLOAT
                                      : to_long_double ? TAC_INSTRUCTION_INT_TO_LONG_DOUBLE
                                                       : TAC_INSTRUCTION_INT_TO_DOUBLE;
            Tac_Instruction *in     = tac_new_instruction(op);
            in->u.int_to_double.src = src;
            in->u.int_to_double.dst = dst;
            tac_append(ctx, in);
        } else {
            Tac_InstructionKind op   = to_float         ? TAC_INSTRUCTION_UINT_TO_FLOAT
                                       : to_long_double ? TAC_INSTRUCTION_UINT_TO_LONG_DOUBLE
                                                        : TAC_INSTRUCTION_UINT_TO_DOUBLE;
            Tac_Instruction *in      = tac_new_instruction(op);
            in->u.uint_to_double.src = src;
            in->u.uint_to_double.dst = dst;
            tac_append(ctx, in);
        }
    } else {
        // float ↔ double ↔ long double, or same-type copy
        if (from->kind == TYPE_FLOAT && to->kind == TYPE_DOUBLE) {
            Tac_Instruction *in       = tac_new_instruction(TAC_INSTRUCTION_FLOAT_TO_DOUBLE);
            in->u.float_to_double.src = src;
            in->u.float_to_double.dst = dst;
            tac_append(ctx, in);
        } else if (from->kind == TYPE_DOUBLE && to->kind == TYPE_FLOAT) {
            Tac_Instruction *in       = tac_new_instruction(TAC_INSTRUCTION_DOUBLE_TO_FLOAT);
            in->u.double_to_float.src = src;
            in->u.double_to_float.dst = dst;
            tac_append(ctx, in);
        } else if (from->kind == TYPE_LONG_DOUBLE && to->kind == TYPE_DOUBLE) {
            Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_LONG_DOUBLE_TO_DOUBLE);
            in->u.long_double_to_double.src = src;
            in->u.long_double_to_double.dst = dst;
            tac_append(ctx, in);
        } else if (from->kind == TYPE_DOUBLE && to->kind == TYPE_LONG_DOUBLE) {
            Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_DOUBLE_TO_LONG_DOUBLE);
            in->u.double_to_long_double.src = src;
            in->u.double_to_long_double.dst = dst;
            tac_append(ctx, in);
        } else if (from->kind == TYPE_LONG_DOUBLE && to->kind == TYPE_FLOAT) {
            Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_LONG_DOUBLE_TO_FLOAT);
            in->u.long_double_to_float.src = src;
            in->u.long_double_to_float.dst = dst;
            tac_append(ctx, in);
        } else if (from->kind == TYPE_FLOAT && to->kind == TYPE_LONG_DOUBLE) {
            Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_FLOAT_TO_LONG_DOUBLE);
            in->u.float_to_long_double.src = src;
            in->u.float_to_long_double.dst = dst;
            tac_append(ctx, in);
        } else {
            Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_COPY);
            in->u.copy.src      = src;
            in->u.copy.dst      = dst;
            tac_append(ctx, in);
        }
    }
    return val_var(dst->u.var_name);
}

void emit_jump(TacCtx *ctx, const char *target)
{
    Tac_Instruction *j = tac_new_instruction(TAC_INSTRUCTION_JUMP);
    j->u.jump.target   = xstrdup(target);
    tac_append(ctx, j);
}

void emit_label(TacCtx *ctx, const char *name)
{
    Tac_Instruction *l = tac_new_instruction(TAC_INSTRUCTION_LABEL);
    l->u.label.name    = xstrdup(name);
    tac_append(ctx, l);
}

static void free_label_name(intptr_t v)
{
    xfree((void *)v);
}

void free_user_labels(StringMap *labels)
{
    map_destroy_free(labels, free_label_name);
}

// Map a user C label to a unique per-function TAC name (%L<n>) so identically
// named labels in different functions never collide once the Unix backend drops
// Madlen's per-function module framing (b6as emits into one flat namespace).
// Reuses the unit-wide counter that %L loop labels use, so the name is unique
// across the whole translation unit; the Unix sanitizer renders it as .L<n>.
const char *user_label_name(TacCtx *ctx, const char *src)
{
    intptr_t v;
    if (map_get(&ctx->user_labels, src, &v))
        return (const char *)v;
    char *uniq = xstruniq("%L", &ctx->temp_id);
    map_insert(&ctx->user_labels, src, (intptr_t)uniq, 0);
    return uniq;
}

//
// Struct-by-value support
//

int target_word_bytes(void)
{
    return (int)target_config->pointer_size;
}

static bool is_struct_or_union(const Type *t)
{
    t = unalias(t);
    return t && (t->kind == TYPE_STRUCT || t->kind == TYPE_UNION);
}

bool type_is_byval_sret(const Type *t)
{
    if (!is_struct_or_union(t))
        return false;
    return get_size(t) > target_config->struct_return_max;
}

bool type_is_split_arg(const Type *t)
{
    return target_config->struct_args_split && is_struct_or_union(t) &&
           (int)get_size(t) > target_word_bytes();
}

bool type_needs_slot(const Type *t)
{
    return is_struct_or_union(t) &&
           ((int)get_size(t) > target_word_bytes() || !target_word_addressed());
}

int aggregate_chunk(const Type *t)
{
    int align = (int)get_alignment(t);
    int w     = target_word_bytes();
    return align < w ? align : w;
}

// The unsigned integer type of `bytes` bytes, the carrier of one copy chunk.
static Tac_Type *tac_type_unsigned(int bytes)
{
    if (bytes == (int)target_config->long_size)
        return tac_new_type(TAC_TYPE_ULONG);
    if (bytes == (int)target_config->int_size)
        return tac_new_type(TAC_TYPE_UINT);
    if (bytes == (int)target_config->short_size)
        return tac_new_type(TAC_TYPE_USHORT);
    if (bytes == 1)
        return tac_new_type(TAC_TYPE_UCHAR);
    return tac_new_type(TAC_TYPE_ULONG_LONG);
}

// Address of chunk `index` (of `chunk` bytes) of the object `ptr` points to.
static Tac_Val *chunk_address(TacCtx *ctx, const char *ptr, int index, int chunk)
{
    Tac_Val *p          = new_var_val(ctx, tac_type_ptr(tac_type_unsigned(chunk)));
    Tac_Instruction *ap = tac_new_instruction(TAC_INSTRUCTION_ADD_PTR);
    ap->u.add_ptr.ptr   = val_var(ptr);
    ap->u.add_ptr.index = val_int(index);
    ap->u.add_ptr.scale = chunk;
    ap->u.add_ptr.dst   = p;
    tac_append(ctx, ap);
    return val_var(p->u.var_name);
}

void gen_aggregate_copy(TacCtx *ctx, const AggPlace *dst, const AggPlace *src, const Type *type)
{
    int chunk = aggregate_chunk(type);
    int size  = (int)get_size(type);
    bool byte = chunk == 1;
    for (int i = 0; i * chunk < size; i++) {
        Tac_Val *t = new_var_val(ctx, tac_type_unsigned(chunk));
        if (src->name) {
            Tac_Instruction *ld = tac_new_instruction(byte ? TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET
                                                           : TAC_INSTRUCTION_COPY_FROM_OFFSET);
            ld->u.copy_from_offset.src    = xstrdup(src->name);
            ld->u.copy_from_offset.offset = src->offset + i * chunk;
            ld->u.copy_from_offset.dst    = t;
            tac_append(ctx, ld);
        } else {
            Tac_Instruction *ld =
                tac_new_instruction(byte ? TAC_INSTRUCTION_LOAD_BYTE : TAC_INSTRUCTION_LOAD);
            ld->u.load.src_ptr = chunk_address(ctx, src->ptr, i, chunk);
            ld->u.load.dst     = t;
            tac_append(ctx, ld);
        }
        if (dst->name) {
            Tac_Instruction *st = tac_new_instruction(byte ? TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET
                                                           : TAC_INSTRUCTION_COPY_TO_OFFSET);
            st->u.copy_to_offset.src    = val_var(t->u.var_name);
            st->u.copy_to_offset.dst    = xstrdup(dst->name);
            st->u.copy_to_offset.offset = dst->offset + i * chunk;
            tac_append(ctx, st);
        } else {
            Tac_Instruction *st =
                tac_new_instruction(byte ? TAC_INSTRUCTION_STORE_BYTE : TAC_INSTRUCTION_STORE);
            st->u.store.src     = val_var(t->u.var_name);
            st->u.store.dst_ptr = chunk_address(ctx, dst->ptr, i, chunk);
            tac_append(ctx, st);
        }
    }
}

void gen_struct_assign(TacCtx *ctx, const char *dst_name, int dst_off, const char *src_name,
                       const Type *type)
{
    AggPlace dst = { dst_name, dst_off, NULL };
    AggPlace src = { src_name, 0, NULL };
    gen_aggregate_copy(ctx, &dst, &src, type);
}

//
// Type conversion: AST Type → TAC Type
//

static Tac_Param *params_from_type(const Type *fun_type)
{
    if (!fun_type || fun_type->kind != TYPE_FUNCTION) {
        return NULL;
    }
    Tac_Param *head  = NULL;
    Tac_Param **tail = &head;
    for (const Param *p = fun_type->u.function.params; p; p = p->next) {
        if (!p->name)
            continue; // skip void sentinel and unnamed params
        Tac_Param *tp = tac_new_param();
        tp->name      = xstrdup(p->name);
        tp->type      = ast_type_to_tac_type(p->type);
        *tail         = tp;
        tail          = &tp->next;

        // On a struct_args_split target a struct parameter wider than a word is passed
        // as N consecutive machine words (see the call-site decomposition in expr.c).  The real
        // param above is the struct's base slot; append N-1 filler params so frame_build reserves N
        // contiguous slots and the body's `base + i*word` member accesses resolve
        // correctly.  The fillers are never referenced by name.
        if (type_is_split_arg(p->type)) {
            int w      = target_word_bytes();
            int nwords = ((int)get_size(p->type) + w - 1) / w;
            for (int i = 1; i < nwords; i++) {
                Tac_Param *fill = tac_new_param();
                char buf[64];
                snprintf(buf, sizeof(buf), "%s$w%d", p->name, i);
                fill->name = xstrdup(buf);
                fill->type = tac_type_word();
                *tail      = fill;
                tail       = &fill->next;
            }
        }
    }
    return head;
}

Tac_Type *tac_type_ptr(Tac_Type *target)
{
    Tac_Type *tp              = tac_new_type(TAC_TYPE_POINTER);
    tp->u.pointer.target_type = target;
    return tp;
}

Tac_Type *tac_type_ptr_to(const Type *t)
{
    return tac_type_ptr(ast_type_to_tac_type(t));
}

void tac_layout_of_target(Tac_Layout *l)
{
    const Target *t                 = target_config;
    l->scalar[TAC_TYPE_SCHAR]       = 1;
    l->scalar[TAC_TYPE_UCHAR]       = 1;
    l->scalar[TAC_TYPE_SHORT]       = (int)t->short_size;
    l->scalar[TAC_TYPE_USHORT]      = (int)t->short_size;
    l->scalar[TAC_TYPE_INT]         = (int)t->int_size;
    l->scalar[TAC_TYPE_UINT]        = (int)t->int_size;
    l->scalar[TAC_TYPE_LONG]        = (int)t->long_size;
    l->scalar[TAC_TYPE_ULONG]       = (int)t->long_size;
    l->scalar[TAC_TYPE_LONG_LONG]   = (int)t->llong_size;
    l->scalar[TAC_TYPE_ULONG_LONG]  = (int)t->llong_size;
    l->scalar[TAC_TYPE_FLOAT]       = (int)t->float_size;
    l->scalar[TAC_TYPE_DOUBLE]      = (int)t->double_size;
    l->scalar[TAC_TYPE_LONG_DOUBLE] = (int)t->ldouble_size;
    l->pointer                      = (int)t->pointer_size;
}

Tac_Type *tac_type_char(void)
{
    return tac_new_type(target_config->char_signed ? TAC_TYPE_SCHAR : TAC_TYPE_UCHAR);
}

Tac_Type *tac_type_ptrdiff(void)
{
    Type t = { .kind = ptrdiff_kind() };
    return ast_type_to_tac_type(&t);
}

// The unsigned integer type as wide as a pointer.
Tac_Type *tac_type_word(void)
{
    if (target_config->long_size == target_config->pointer_size)
        return tac_new_type(TAC_TYPE_ULONG);
    if (target_config->int_size == target_config->pointer_size)
        return tac_new_type(TAC_TYPE_UINT); // AVR
    return tac_new_type(TAC_TYPE_ULONG_LONG);
}

//
// Top-level translation
//

// For each POINTER / FAT_POINTER entry in `inits` that references a SYM_CONST string
// literal whose data has not yet been emitted, build a TAC_TOPLEVEL_STATIC_CONSTANT and
// append it through `*ctailp`.  Ownership of the const init transfers to the new toplevel,
// so a string referenced more than once (by several statics or by the body) emits once.
// A file-scope compound literal's object (_clN) is emitted the same way, as a non-global
// static variable, followed by whatever its own data references.
static void emit_referenced_string_constants(const Tac_StaticInit *inits, Tac_TopLevel ***ctailp)
{
    for (const Tac_StaticInit *init = inits; init; init = init->next) {
        if (init->kind != TAC_STATIC_INIT_POINTER && init->kind != TAC_STATIC_INIT_FAT_POINTER)
            continue;
        const char *sname = init->u.pointer.name;
        Symbol *sym       = symtab_get_opt(sname); // f$co has none
        if (sym && sym->kind == SYM_STATIC && sym->u.static_var.literal &&
            sym->u.static_var.init_list) {
            Tac_TopLevel *sv                = tac_new_toplevel(TAC_TOPLEVEL_STATIC_VARIABLE);
            sv->u.static_variable.name      = xstrdup(sname);
            sv->u.static_variable.global    = false;
            sv->u.static_variable.type      = ast_type_to_tac_type(sym->type);
            sv->u.static_variable.init_list = sym->u.static_var.init_list;
            sv->u.static_variable.alignment = sym->u.static_var.alignment;
            sym->u.static_var.init_list     = NULL; // transfer ownership to TAC
            **ctailp                        = sv;
            *ctailp                         = &sv->next;
            emit_referenced_string_constants(sv->u.static_variable.init_list, ctailp);
            continue;
        }
        if (!sym || sym->kind != SYM_CONST || !sym->u.const_init)
            continue;
        Tac_TopLevel *sc           = tac_new_toplevel(TAC_TOPLEVEL_STATIC_CONSTANT);
        sc->u.static_constant.name = xstrdup(sname);
        sc->u.static_constant.type = ast_type_to_tac_type(sym->type);
        sc->u.static_constant.init = sym->u.const_init;
        sym->u.const_init          = NULL; // transfer ownership to TAC
        **ctailp                   = sc;
        *ctailp                    = &sc->next;
    }
}

// The coroutine translate_fn lowered last, which translate() splits after the
// optimizer (coro.c).
static Tac_TopLevel *coro_pending_fn;
static CoroSplit coro_pending;

static Tac_TopLevel *translate_fn(const ExternalDecl *ast, int *label_seq)
{
    const char *name  = ast->u.function.name;
    const Symbol *sym = symtab_get(name);

    Tac_TopLevel *tl        = tac_new_toplevel(TAC_TOPLEVEL_FUNCTION);
    tl->u.function.name     = xstrdup(name);
    tl->u.function.global   = sym->u.func.global;
    tl->u.function.params   = params_from_type(ast->u.function.type);
    tl->u.function.variadic = ast->u.function.type && ast->u.function.type->kind == TYPE_FUNCTION &&
                              ast->u.function.type->u.function.variadic;
    tl->u.function.noret    = sym->u.func.noret;
    if (ast->u.function.type && ast->u.function.type->kind == TYPE_FUNCTION)
        tl->u.function.type = ast_type_to_tac_type(ast->u.function.type);

    // A coroutine is first f$resume(fp, params) -> int, over a frame at fp (coro.c).
    TacCoro coro;
    if (sym->u.func.coro) {
        const Type *ret = ast->u.function.type->u.function.return_type;
        int end, align;
        coro_layout(sym->u.func.yield_type, ret, &coro.value_off, &coro.result_off, &end, &align);
        coro.fp     = ".fp";
        coro.yield  = sym->u.func.yield_type;
        coro.result = ret;
        xfree(tl->u.function.name);
        size_t n            = strlen(name);
        tl->u.function.name = xalloc(n + sizeof "$resume", __func__, __FILE__, __LINE__);
        memcpy(tl->u.function.name, name, n);
        memcpy(tl->u.function.name + n, "$resume", sizeof "$resume");
        Tac_Param *fp              = tac_new_param();
        fp->name                   = xstrdup(coro.fp);
        fp->type                   = tac_type_ptr(tac_type_char());
        fp->next                   = tl->u.function.params;
        tl->u.function.params      = fp;
        Tac_Type *ft               = tl->u.function.type;
        Tac_Type *fpt              = tac_type_ptr(tac_type_char());
        fpt->next                  = ft->u.fun_type.param_types;
        ft->u.fun_type.param_types = fpt;
        tac_free_type(ft->u.fun_type.ret_type);
        ft->u.fun_type.ret_type = tac_new_type(TAC_TYPE_INT);
        coro_pending_fn         = tl;
        coro_pending =
            (CoroSplit){ name, sym->u.func.global, end, align, coroutine_has_coro_ptr(sym->type) };
    }

    // A struct return too wide to return by value (type_is_byval_sret) uses the
    // hidden-pointer (sret) ABI: the caller passes
    // the address of the result slot as an implicit first argument.  Prepend it to the
    // param list so it lands in frame slot 0 (this shifts the user params' slots by one,
    // which body references pick up automatically by name).
    const char *sret_name = NULL;
    if (ast->u.function.type && ast->u.function.type->kind == TYPE_FUNCTION && !sym->u.func.coro &&
        type_is_byval_sret(ast->u.function.type->u.function.return_type)) {
        sret_name             = ".ret";
        Tac_Param *hp         = tac_new_param();
        hp->name              = xstrdup(sret_name);
        hp->type              = tac_type_ptr_to(ast->u.function.type->u.function.return_type);
        hp->next              = tl->u.function.params;
        tl->u.function.params = hp;
    }

    if (ast->u.function.body) {
        // Seed this function's temp/label counter from the unit-wide sequence so
        // its `%N` names never collide with another function's in a single-file
        // backend (see translate.h); write the advanced value back afterwards.
        TacCtx ctx    = { NULL, NULL, *label_seq, NULL, NULL, NULL, NULL, NULL };
        ctx.sret_name = sret_name;
        ctx.body      = ast->u.function.body;
        ctx.coro      = sym->u.func.coro ? &coro : NULL;
        map_init(&ctx.user_labels);
        gen_stmt(&ctx, ast->u.function.body);
        // The end of a coroutine's body: done.  (A non-void one cannot reach it.)
        if (ctx.coro && unalias(coro.result)->kind == TYPE_VOID)
            gen_coro_return(&ctx, NULL, NULL);
        *label_seq            = ctx.temp_id;
        tl->u.function.body   = ctx.head;
        tl->u.function.locals = ctx.locals;
        tac_free_param(ctx.array_locals);
        free_user_labels(&ctx.user_labels);
        xfree(ctx.scopes);
        xfree(ctx.where);
        xfree(ctx.ret_var);
        xfree(ctx.breaks);
        if (ctx.label_pos_ready)
            defer_free_labels(&ctx.label_pos);

        // Attach this function's block-scope statics, captured during typecheck.  The
        // capture list is newest-first; prepend each as we walk it so the result is in
        // declaration order.  Ownership of each init list transfers into the TAC node.
        for (StaticLocalRec *r = static_locals_head(); r; r = r->next) {
            if (strcmp(r->func, name) != 0)
                continue;
            Tac_StaticLocal *sl          = tac_new_static_local();
            sl->name                     = xstrdup(r->name);
            sl->type                     = ast_type_to_tac_type(r->type);
            sl->init_list                = r->init_list;
            sl->alignment                = r->alignment;
            r->init_list                 = NULL; // transferred
            sl->next                     = tl->u.function.static_locals;
            tl->u.function.static_locals = sl;
        }

        // String literals used to initialize a static local (e.g. `static char *p = "ABC";`)
        // are referenced only from the static-local init list, not the body, so emit their
        // data constants here.  Append after the body's expression constants
        // (ctx.static_constants).
        Tac_TopLevel **ctail = &ctx.static_constants;
        while (*ctail)
            ctail = &(*ctail)->next;
        for (const Tac_StaticLocal *sl = tl->u.function.static_locals; sl; sl = sl->next)
            emit_referenced_string_constants(sl->init_list, &ctail);

        // Emit order: block-scope externs, string constants, the function.
        *ctail = tl;
        if (ctx.externs) {
            Tac_TopLevel *last = ctx.externs;
            while (last->next)
                last = last->next;
            last->next = ctx.static_constants;
            return ctx.externs;
        }
        return ctx.static_constants;
    }
    return tl;
}

static Tac_TopLevel *translate_decl(const Declaration *decl)
{
    if (decl->kind != DECL_VAR)
        return NULL;
    if (decl->u.var.specifiers && decl->u.var.specifiers->storage == STORAGE_CLASS_TYPEDEF)
        return NULL;

    Tac_TopLevel *head  = NULL;
    Tac_TopLevel **tail = &head;
    for (const InitDeclarator *id = decl->u.var.declarators; id; id = id->next) {
        Symbol *sym = symtab_get(id->name);
        Tac_TopLevel *tl;

        if (unalias(id->type)->kind == TYPE_FUNCTION) {
            continue; // prototype — no TAC needed
        } else if (sym->u.static_var.init_kind == INIT_NONE) {
            // extern-only declaration — no storage to allocate, no TAC emitted.  A later
            // reference decays the array to its address via GET_ADDRESS, which self-declares
            // the external name (SUBP), so the backend needs no array-ness record here.
            continue;
        } else if (sym->u.static_var.init_kind == INIT_INITIALIZED &&
                   sym->u.static_var.init_list == NULL) {
            // The initializer was already emitted by an earlier declaration of this symbol
            // (ownership of the init list was transferred to that prior TAC top-level, see
            // below).  A later tentative ("int x;") or extern redeclaration must NOT re-emit:
            // each Madlen module silently replaces any prior module with the same ,NAME,, so a
            // second zero-init module would clobber the value back to 0.
            continue;
        } else {
            tl                              = tac_new_toplevel(TAC_TOPLEVEL_STATIC_VARIABLE);
            tl->u.static_variable.name      = xstrdup(id->name);
            tl->u.static_variable.global    = sym->u.static_var.global;
            tl->u.static_variable.type      = ast_type_to_tac_type(sym->type);
            tl->u.static_variable.init_list = sym->u.static_var.init_list;
            tl->u.static_variable.alignment = sym->u.static_var.alignment;
            sym->u.static_var.init_list     = NULL; // transfer ownership to TAC
        }
        *tail = tl;
        tail  = &tl->next;
    }

    // Scan each static variable's init list for POINTER / FAT_POINTER entries that
    // reference SYM_CONST symbols (string literals). Emit a TAC_TOPLEVEL_STATIC_CONSTANT
    // for each and prepend it before the variable that uses it.  (A char* initializer is a
    // fat pointer, so the string it points at arrives as FAT_POINTER, not POINTER.)
    Tac_TopLevel *constants_head = NULL;
    Tac_TopLevel **ctail         = &constants_head;
    for (const Tac_TopLevel *cur = head; cur; cur = cur->next)
        emit_referenced_string_constants(cur->u.static_variable.init_list, &ctail);
    if (constants_head) {
        *ctail = head;
        return constants_head;
    }
    return head;
}

static Tac_TopLevel *translate_external_decl(const ExternalDecl *ast, int *label_seq)
{
    if (!ast)
        return NULL;
    switch (ast->kind) {
    case EXTERNAL_DECL_FUNCTION:
        return translate_fn(ast, label_seq);
    case EXTERNAL_DECL_DECLARATION:
        return translate_decl(ast->u.declaration);
    }
    return NULL;
}

// Return a freshly allocated copy of `name` with a leading '%' prepended.
static char *percent_name(const char *name)
{
    size_t n  = strlen(name);
    char *out = xalloc(n + 2, __func__, __FILE__, __LINE__);
    out[0]    = '%';
    memcpy(out + 1, name, n + 1);
    return out;
}

// If `v` (or any value in its ->next chain) names a frame-resident variable
// (member of `autos`), rewrite its name with a leading '%' in place.
static void percent_vals(Tac_Val *v, const StringMap *autos)
{
    intptr_t dummy;
    for (; v; v = v->next) {
        if (v->kind != TAC_VAL_VAR)
            continue;
        if (!map_get(autos, v->u.var_name, &dummy))
            continue;
        char *renamed = percent_name(v->u.var_name);
        xfree(v->u.var_name);
        v->u.var_name = renamed;
    }
}

// Same, for a bare char* name field (copy_to_offset.dst / copy_from_offset.src).
static void percent_name_field(char **name, const StringMap *autos)
{
    intptr_t dummy;
    if (!*name || !map_get(autos, *name, &dummy))
        return;
    char *renamed = percent_name(*name);
    xfree(*name);
    *name = renamed;
}

// Visit every operand of one instruction and prefix frame-resident names with '%'.
// Mirrors backend/besm6/frame.c collect_instr's operand coverage.
static void percent_instr(Tac_Instruction *in, const StringMap *autos)
{
    switch (in->kind) {
    case TAC_INSTRUCTION_RETURN:
        percent_vals(in->u.return_.src, autos);
        break;
    case TAC_INSTRUCTION_SIGN_EXTEND:
        percent_vals(in->u.sign_extend.src, autos);
        percent_vals(in->u.sign_extend.dst, autos);
        break;
    case TAC_INSTRUCTION_TRUNCATE:
        percent_vals(in->u.truncate.src, autos);
        percent_vals(in->u.truncate.dst, autos);
        break;
    case TAC_INSTRUCTION_ZERO_EXTEND:
        percent_vals(in->u.zero_extend.src, autos);
        percent_vals(in->u.zero_extend.dst, autos);
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_INT:
        percent_vals(in->u.double_to_int.src, autos);
        percent_vals(in->u.double_to_int.dst, autos);
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_UINT:
        percent_vals(in->u.double_to_uint.src, autos);
        percent_vals(in->u.double_to_uint.dst, autos);
        break;
    case TAC_INSTRUCTION_INT_TO_DOUBLE:
        percent_vals(in->u.int_to_double.src, autos);
        percent_vals(in->u.int_to_double.dst, autos);
        break;
    case TAC_INSTRUCTION_UINT_TO_DOUBLE:
        percent_vals(in->u.uint_to_double.src, autos);
        percent_vals(in->u.uint_to_double.dst, autos);
        break;
    case TAC_INSTRUCTION_FLOAT_TO_DOUBLE:
        percent_vals(in->u.float_to_double.src, autos);
        percent_vals(in->u.float_to_double.dst, autos);
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_FLOAT:
        percent_vals(in->u.double_to_float.src, autos);
        percent_vals(in->u.double_to_float.dst, autos);
        break;
    case TAC_INSTRUCTION_INT_TO_FLOAT:
        percent_vals(in->u.int_to_float.src, autos);
        percent_vals(in->u.int_to_float.dst, autos);
        break;
    case TAC_INSTRUCTION_UINT_TO_FLOAT:
        percent_vals(in->u.uint_to_float.src, autos);
        percent_vals(in->u.uint_to_float.dst, autos);
        break;
    case TAC_INSTRUCTION_FLOAT_TO_INT:
        percent_vals(in->u.float_to_int.src, autos);
        percent_vals(in->u.float_to_int.dst, autos);
        break;
    case TAC_INSTRUCTION_FLOAT_TO_UINT:
        percent_vals(in->u.float_to_uint.src, autos);
        percent_vals(in->u.float_to_uint.dst, autos);
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_INT:
        percent_vals(in->u.long_double_to_int.src, autos);
        percent_vals(in->u.long_double_to_int.dst, autos);
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_UINT:
        percent_vals(in->u.long_double_to_uint.src, autos);
        percent_vals(in->u.long_double_to_uint.dst, autos);
        break;
    case TAC_INSTRUCTION_INT_TO_LONG_DOUBLE:
        percent_vals(in->u.int_to_long_double.src, autos);
        percent_vals(in->u.int_to_long_double.dst, autos);
        break;
    case TAC_INSTRUCTION_UINT_TO_LONG_DOUBLE:
        percent_vals(in->u.uint_to_long_double.src, autos);
        percent_vals(in->u.uint_to_long_double.dst, autos);
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_DOUBLE:
        percent_vals(in->u.long_double_to_double.src, autos);
        percent_vals(in->u.long_double_to_double.dst, autos);
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_LONG_DOUBLE:
        percent_vals(in->u.double_to_long_double.src, autos);
        percent_vals(in->u.double_to_long_double.dst, autos);
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_FLOAT:
        percent_vals(in->u.long_double_to_float.src, autos);
        percent_vals(in->u.long_double_to_float.dst, autos);
        break;
    case TAC_INSTRUCTION_FLOAT_TO_LONG_DOUBLE:
        percent_vals(in->u.float_to_long_double.src, autos);
        percent_vals(in->u.float_to_long_double.dst, autos);
        break;
    case TAC_INSTRUCTION_PTR_TO_CHAR_PTR:
        percent_vals(in->u.ptr_to_char_ptr.src, autos);
        percent_vals(in->u.ptr_to_char_ptr.dst, autos);
        break;
    case TAC_INSTRUCTION_CHAR_PTR_TO_PTR:
        percent_vals(in->u.char_ptr_to_ptr.src, autos);
        percent_vals(in->u.char_ptr_to_ptr.dst, autos);
        break;
    case TAC_INSTRUCTION_UNARY:
        percent_vals(in->u.unary.src, autos);
        percent_vals(in->u.unary.dst, autos);
        break;
    case TAC_INSTRUCTION_BINARY:
        percent_vals(in->u.binary.src1, autos);
        percent_vals(in->u.binary.src2, autos);
        percent_vals(in->u.binary.dst, autos);
        break;
    case TAC_INSTRUCTION_COPY:
        percent_vals(in->u.copy.src, autos);
        percent_vals(in->u.copy.dst, autos);
        break;
    case TAC_INSTRUCTION_GET_ADDRESS:
    case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
    case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
        percent_vals(in->u.get_address.src, autos);
        percent_vals(in->u.get_address.dst, autos);
        break;
    case TAC_INSTRUCTION_LOAD:
    case TAC_INSTRUCTION_LOAD_BYTE:
        percent_vals(in->u.load.src_ptr, autos);
        percent_vals(in->u.load.dst, autos);
        break;
    case TAC_INSTRUCTION_STORE:
    case TAC_INSTRUCTION_STORE_BYTE:
        percent_vals(in->u.store.src, autos);
        percent_vals(in->u.store.dst_ptr, autos);
        break;
    case TAC_INSTRUCTION_ADD_PTR:
        percent_vals(in->u.add_ptr.ptr, autos);
        percent_vals(in->u.add_ptr.index, autos);
        percent_vals(in->u.add_ptr.dst, autos);
        break;
    case TAC_INSTRUCTION_PTR_DIFF:
        percent_vals(in->u.ptr_diff.ptr_a, autos);
        percent_vals(in->u.ptr_diff.ptr_b, autos);
        percent_vals(in->u.ptr_diff.dst, autos);
        break;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET:
        percent_vals(in->u.copy_to_offset.src, autos);
        percent_name_field(&in->u.copy_to_offset.dst, autos);
        break;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
        percent_name_field(&in->u.copy_from_offset.src, autos);
        percent_vals(in->u.copy_from_offset.dst, autos);
        break;
    case TAC_INSTRUCTION_JUMP:
    case TAC_INSTRUCTION_LABEL:
        break;
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
        percent_vals(in->u.jump_if_zero.condition, autos);
        break;
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
        percent_vals(in->u.jump_if_not_zero.condition, autos);
        break;
    case TAC_INSTRUCTION_JUMP_TABLE: // made by the coroutine split, after this
        percent_vals(in->u.jump_table.index, autos);
        break;
    case TAC_INSTRUCTION_FUN_CALL:
    case TAC_INSTRUCTION_FUN_CALL_NORETURN:
        // An indirect call's callee is a frame-resident pointer (param/local/temp); rename
        // it so the backend recognizes it as a frame slot.  A module-level function name is
        // not in `autos` and is left untouched, so the backend emits a direct CALL.
        percent_name_field(&in->u.fun_call.fun_name, autos);
        percent_vals(in->u.fun_call.args, autos);
        percent_vals(in->u.fun_call.dst, autos);
        break;
    case TAC_INSTRUCTION_ALLOCATE_LOCAL:
        percent_name_field(&in->u.allocate_local.name, autos);
        break;
    }
}

// Rewrite parameter and automatic-local names with a leading '%', so the backend
// can tell frame-resident names from module-level globals by name alone. Globals
// and compiler temporaries (already '%'-prefixed by new_temp) are left untouched.
// Run before optimize_function so the optimizer's private-name analysis, the
// stored params/locals lists, and the body all agree on the percent-prefixed spelling.
static void percent_locals_in_function(const Tac_TopLevel *fn)
{
    StringMap autos;
    map_init(&autos);
    for (const Tac_Param *p = fn->u.function.params; p; p = p->next)
        if (p->name && p->name[0] != '%')
            map_insert(&autos, p->name, 1, 0);
    for (const Tac_Param *p = fn->u.function.locals; p; p = p->next)
        if (p->name && p->name[0] != '%')
            map_insert(&autos, p->name, 1, 0);

    // Rewrite the body first (matching the still-raw names), then the lists.
    for (Tac_Instruction *in = fn->u.function.body; in; in = in->next)
        percent_instr(in, &autos);

    for (Tac_Param *p = fn->u.function.params; p; p = p->next)
        if (p->name && p->name[0] != '%') {
            char *d = percent_name(p->name);
            xfree(p->name);
            p->name = d;
        }
    for (Tac_Param *p = fn->u.function.locals; p; p = p->next)
        if (p->name && p->name[0] != '%') {
            char *d = percent_name(p->name);
            xfree(p->name);
            p->name = d;
        }

    map_destroy(&autos);
}

//
// Names defined and referenced in the current translation unit, for its extern list.
//
static bool unit_active;
static StringMap unit_defined;
static StringMap unit_referenced;
static StringMap unit_externs; // names already given an EXTERN toplevel -> its type (owned)

static void free_type_value(intptr_t v)
{
    tac_free_type((Tac_Type *)v);
}

static void note_referenced(const char *name, void *arg)
{
    (void)arg;
    if (name[0] != '%')
        map_insert(&unit_referenced, name, 1, 0);
}

static void note_init_refs(const Tac_StaticInit *init)
{
    for (; init; init = init->next)
        if ((init->kind == TAC_STATIC_INIT_POINTER || init->kind == TAC_STATIC_INIT_FAT_POINTER) &&
            init->u.pointer.name)
            note_referenced(init->u.pointer.name, NULL);
}

// A name the coroutine split defines (f$resume, f$init, f$co) has no symbol: its
// type goes with the unit's externs, for the verifier and against a second EXTERN.
static void note_own_type(const char *name, const Tac_Type *type)
{
    if (type && !symtab_get_opt(name) && !map_get(&unit_externs, name, NULL))
        map_insert_free(&unit_externs, name, (intptr_t)tac_clone_type(type), 0, free_type_value);
}

static void note_toplevel(const Tac_TopLevel *t)
{
    switch (t->kind) {
    case TAC_TOPLEVEL_FUNCTION:
        note_own_type(t->u.function.name, t->u.function.type);
        map_insert(&unit_defined, t->u.function.name, 1, 0);
        for (const Tac_StaticLocal *sl = t->u.function.static_locals; sl; sl = sl->next) {
            map_insert(&unit_defined, sl->name, 1, 0);
            note_init_refs(sl->init_list);
        }
        for (const Tac_Instruction *in = t->u.function.body; in; in = in->next)
            tac_visit_names(in, note_referenced, NULL);
        break;
    case TAC_TOPLEVEL_STATIC_VARIABLE:
        note_own_type(t->u.static_variable.name, t->u.static_variable.type);
        map_insert(&unit_defined, t->u.static_variable.name, 1, 0);
        note_init_refs(t->u.static_variable.init_list);
        break;
    case TAC_TOPLEVEL_STATIC_CONSTANT:
        map_insert(&unit_defined, t->u.static_constant.name, 1, 0);
        break;
    case TAC_TOPLEVEL_EXTERN:
        map_insert_free(&unit_externs, t->u.extern_.name,
                        (intptr_t)tac_clone_type(t->u.extern_.type), 0, free_type_value);
        break;
    }
}

void translate_unit_begin(void)
{
    map_init(&unit_defined);
    map_init(&unit_referenced);
    map_init(&unit_externs);
    unit_active = true;
}

// map_iterate callback: append an EXTERN for a referenced name the unit does not define.
static void add_extern(const char *name, intptr_t value, const void *arg)
{
    (void)value;
    Tac_TopLevel ***tailp = (Tac_TopLevel ***)arg;
    const Symbol *sym     = symtab_get_opt(name);
    if (map_get(&unit_defined, name, NULL) || map_get(&unit_externs, name, NULL) || !sym)
        return;
    Tac_TopLevel *ext   = tac_new_toplevel(TAC_TOPLEVEL_EXTERN);
    ext->u.extern_.name = xstrdup(name);
    ext->u.extern_.type = ast_type_to_tac_type(sym->type);
    **tailp             = ext;
    *tailp              = &ext->next;
}

Tac_TopLevel *translate_unit_end(void)
{
    Tac_TopLevel *head = NULL, **tail = &head;
    map_iterate(&unit_referenced, add_extern, &tail); // ascending name order
    map_destroy(&unit_defined);
    map_destroy(&unit_referenced);
    map_destroy_free(&unit_externs, free_type_value);
    unit_active = false;
    return head;
}

// tac_verify resolver for global names: the EXTERN toplevels translated with the
// function (its block-scope declarations), else the file-scope symbol table.
typedef struct {
    const Tac_TopLevel *chain;
    Tac_Type *owned; // types built from symbols, freed after the check
} GlobalTypes;

static const Tac_Type *symtab_global_type(const char *name, void *arg)
{
    GlobalTypes *g = arg;
    for (const Tac_TopLevel *t = g->chain; t; t = t->next)
        if (t->kind == TAC_TOPLEVEL_EXTERN && strcmp(t->u.extern_.name, name) == 0)
            return t->u.extern_.type;
    intptr_t v;
    if (unit_active && map_get(&unit_externs, name, &v))
        return (const Tac_Type *)v;
    const Symbol *sym = symtab_get_opt(name);
    if (!sym)
        return NULL;
    Tac_Type *t = ast_type_to_tac_type(sym->type);
    t->next     = g->owned;
    g->owned    = t;
    return t;
}

static void verify_function(const Tac_TopLevel *chain, const Tac_TopLevel *fn)
{
    Tac_Layout layout;
    tac_layout_of_target(&layout);
    GlobalTypes g = { chain, NULL };
    int errors    = tac_verify_function(fn, &layout, symtab_global_type, &g, stderr);
    tac_free_type(g.owned);
    if (errors)
        internal_error("TAC of %s fails verification (%d problems)", fn->u.function.name, errors);
}

//
// Convert the AST to TAC.
//
Tac_TopLevel *translate(const ExternalDecl *ast, OptFlags flags, int *label_seq)
{
    translate_rotate_loops = flags.loop_rotate && !target_config->no_loop_opt;
    Tac_TopLevel *tac      = translate_external_decl(ast, label_seq);
    if (unit_active) {
        // One EXTERN per name in a unit, and none for a name it defines.
        for (Tac_TopLevel **pp = &tac; *pp;) {
            Tac_TopLevel *t = *pp;
            if (t->kind == TAC_TOPLEVEL_EXTERN &&
                (map_get(&unit_externs, t->u.extern_.name, NULL) ||
                 map_get(&unit_defined, t->u.extern_.name, NULL))) {
                *pp     = t->next;
                t->next = NULL;
                tac_free_toplevel(t);
                continue;
            }
            pp = &t->next;
        }
    }
    for (Tac_TopLevel *t = tac; t; t = t->next) {
        // Each function is optimized against its own toplevel, which carries the
        // params + automatic locals needed to tell private locals from globals.
        if (t->kind == TAC_TOPLEVEL_FUNCTION) {
            percent_locals_in_function(t);
            t->u.function.body = optimize_function(t->u.function.body, flags, t);
            optimize_prune_locals(t);
            if (t == coro_pending_fn) {
                // A coroutine: split, then optimized once more over its frame.
                Tac_TopLevel *made = coro_split(t, &coro_pending);
                Tac_TopLevel *last = made;
                while (last->next)
                    last = last->next;
                last->next         = t->next;
                t->next            = made;
                t->u.function.body = optimize_function(t->u.function.body, flags, t);
                optimize_prune_locals(t);
                coro_pending_fn = NULL;
            }
            if (translate_verify)
                verify_function(tac, t);
        }
        if (unit_active)
            note_toplevel(t);
    }
    // The suspension was a call to the optimizer only; the split removed every one.
    for (Tac_TopLevel **pp = &tac; *pp;) {
        Tac_TopLevel *t = *pp;
        if (t->kind == TAC_TOPLEVEL_EXTERN && strcmp(t->u.extern_.name, "__coro_suspend") == 0) {
            *pp     = t->next;
            t->next = NULL;
            tac_free_toplevel(t);
            continue;
        }
        pp = &t->next;
    }
    return tac;
}
