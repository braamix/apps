//
// Instruction selection: each TAC instruction becomes stack code that pushes its
// operands, computes, and pops the result into its destination.  A destination in
// memory takes its address first, under the value: begin_dst before the operands,
// end_dst after them.  A narrow integer is kept extended in its i32 (signed or zero
// by its type), so a load of one needs no fixing and a truncation does it.
//
#include <string.h>

#include "internal.h"
#include "xalloc.h"

// The value of an integer constant, sign- or zero-extended from its own width.
static int64_t const_int(const Tac_Const *c)
{
    switch (c->kind) {
    case TAC_CONST_INT:
        return (int32_t)c->u.int_val;
    case TAC_CONST_LONG:
        return (int32_t)c->u.long_val;
    case TAC_CONST_LONG_LONG:
        return c->u.long_long_val;
    case TAC_CONST_UINT:
        return (uint32_t)c->u.uint_val;
    case TAC_CONST_ULONG:
        return (uint32_t)c->u.ulong_val;
    case TAC_CONST_ULONG_LONG:
        return (int64_t)c->u.ulong_long_val;
    case TAC_CONST_SCHAR:
        return (signed char)c->u.char_val;
    case TAC_CONST_UCHAR:
        return c->u.uchar_val;
    case TAC_CONST_FLOAT:
        return (int64_t)c->u.float_val;
    case TAC_CONST_DOUBLE:
        return (int64_t)c->u.double_val;
    case TAC_CONST_LONG_DOUBLE:
        break;
    }
    internal_error("wasm: a long double constant as an integer");
}

static double const_double(const Tac_Const *c)
{
    switch (c->kind) {
    case TAC_CONST_FLOAT:
        return c->u.float_val;
    case TAC_CONST_DOUBLE:
        return c->u.double_val;
    case TAC_CONST_LONG_DOUBLE:
        internal_error("wasm: a long double constant as a double");
    case TAC_CONST_ULONG_LONG:
        return (double)c->u.ulong_long_val;
    case TAC_CONST_UINT:
    case TAC_CONST_ULONG:
    case TAC_CONST_UCHAR:
        return (double)(uint64_t)const_int(c);
    default:
        return (double)const_int(c);
    }
}

// The value type a constant of this kind is held in.
static Wasm_ValType const_valtype(const Tac_Const *c)
{
    switch (c->kind) {
    case TAC_CONST_LONG_LONG:
    case TAC_CONST_ULONG_LONG:
        return WASM_I64;
    case TAC_CONST_FLOAT:
        return WASM_F32;
    case TAC_CONST_DOUBLE:
        return WASM_F64;
    default:
        return WASM_I32;
    }
}

Wasm_Instr *emit(Gen *g, Wasm_Op op)
{
    Wasm_Instr *in = wasm_append(g->fn, op);
    in->barrier    = g->barrier;
    return in;
}

void emit_imm(Gen *g, Wasm_Op op, int64_t imm)
{
    emit(g, op)->imm = imm;
}

void push_int(Gen *g, Wasm_ValType t, int64_t v)
{
    if (t == WASM_I64)
        emit_imm(g, WASM_I64_CONST, v);
    else
        emit_imm(g, WASM_I32_CONST, (int32_t)v);
}

// Push constant c as a value of type t.
static void push_const(Gen *g, const Tac_Const *c, Wasm_ValType t)
{
    switch (t) {
    case WASM_I32:
    case WASM_I64:
        push_int(g, t, const_int(c));
        return;
    case WASM_F32: {
        float f = (float)const_double(c);
        uint32_t bits;
        memcpy(&bits, &f, sizeof(bits));
        emit_imm(g, WASM_F32_CONST, bits);
        return;
    }
    case WASM_F64: {
        double d = const_double(c);
        uint64_t bits;
        memcpy(&bits, &d, sizeof(bits));
        emit_imm(g, WASM_F64_CONST, (int64_t)bits);
        return;
    }
    case WASM_VOID:
        break;
    }
    internal_error("wasm: %s: a constant of no type", g->fn->name);
}

//
// Variables: a local, a frame slot, or a static object in memory.
//
// The type of variable v, wherever it lives.
const Tac_Type *type_of(const Gen *g, const char *name)
{
    const Tac_Type *t = var_type(g, name);
    if (!t)
        t = global_type(g, name);
    if (!t)
        internal_error("wasm: %s: %s has no type", g->fn->name, name);
    return t;
}

// Push the base of the address of named object `name`, which *sym (a symbol or NULL)
// and *off complete in the access's immediate: the frame's address and the slot's
// offset, or 0 and the static object's symbol.
void push_base(Gen *g, const char *name, const char **sym, int64_t *off)
{
    int ref = var_ref(g, name);
    if (ref >= 0) {
        emit_imm(g, WASM_LOCAL_GET, ref);
        *sym = NULL;
        *off = 0;
        return;
    }
    int slot = var_slot(g, name);
    if (slot >= 0) {
        emit_imm(g, WASM_LOCAL_GET, g->fp);
        *sym = NULL;
        *off = slot;
    } else {
        emit_imm(g, WASM_I32_CONST, 0);
        *sym = wasm_symbol(g->program, name);
        *off = 0;
    }
}

// A load or store at the address push_base began.
void emit_access(Gen *g, Wasm_Op op, const char *sym, int64_t off)
{
    Wasm_Instr *in = emit(g, op);
    in->sym        = sym ? xstrdup(sym) : NULL;
    in->imm        = off;
}

// Push value v, of type t.
void push_val(Gen *g, const Tac_Val *v, Wasm_ValType t)
{
    if (v->kind == TAC_VAL_CONSTANT) {
        push_const(g, v->u.constant, t);
        return;
    }
    const char *name = v->u.var_name;
    int local        = find_local(g, name);
    if (local >= 0) {
        emit_imm(g, WASM_LOCAL_GET, local);
        return;
    }
    const char *sym;
    int64_t off;
    push_base(g, name, &sym, &off);
    emit_access(g, wasm_load_op(type_of(g, name)), sym, off);
}

// The value type of value v; a constant's is its kind's.
Wasm_ValType val_valtype(const Gen *g, const Tac_Val *v)
{
    if (v->kind == TAC_VAL_CONSTANT)
        return const_valtype(v->u.constant);
    return wasm_valtype(type_of(g, v->u.var_name));
}

// The type of variable v, or NULL for a constant.
static const Tac_Type *val_type(const Gen *g, const Tac_Val *v)
{
    return v->kind == TAC_VAL_VAR ? type_of(g, v->u.var_name) : NULL;
}

// The type of value v: a constant's by its kind.
const Tac_Type *any_type(const Gen *g, const Tac_Val *v)
{
    static const Tac_Type types[] = {
        [TAC_CONST_INT]         = { .kind = TAC_TYPE_INT },
        [TAC_CONST_LONG]        = { .kind = TAC_TYPE_LONG },
        [TAC_CONST_LONG_LONG]   = { .kind = TAC_TYPE_LONG_LONG },
        [TAC_CONST_UINT]        = { .kind = TAC_TYPE_UINT },
        [TAC_CONST_ULONG]       = { .kind = TAC_TYPE_ULONG },
        [TAC_CONST_ULONG_LONG]  = { .kind = TAC_TYPE_ULONG_LONG },
        [TAC_CONST_FLOAT]       = { .kind = TAC_TYPE_FLOAT },
        [TAC_CONST_DOUBLE]      = { .kind = TAC_TYPE_DOUBLE },
        [TAC_CONST_LONG_DOUBLE] = { .kind = TAC_TYPE_LONG_DOUBLE },
        [TAC_CONST_SCHAR]       = { .kind = TAC_TYPE_SCHAR },
        [TAC_CONST_UCHAR]       = { .kind = TAC_TYPE_UCHAR },
    };
    if (v->kind == TAC_VAL_CONSTANT)
        return &types[v->u.constant->kind];
    return type_of(g, v->u.var_name);
}

static void check_dst(const Gen *g, const Tac_Val *dst)
{
    if (dst->kind != TAC_VAL_VAR)
        internal_error("wasm: %s: a constant destination", g->fn->name);
}

// Before the value of `dst` is computed: the address of a destination in memory.
void begin_dst(Gen *g, const Tac_Val *dst)
{
    check_dst(g, dst);
    if (find_local(g, dst->u.var_name) >= 0)
        return;
    const char *sym;
    int64_t off;
    push_base(g, dst->u.var_name, &sym, &off);
}

// Pop the value on top of the stack into `dst`.
void end_dst(Gen *g, const Tac_Val *dst)
{
    const char *name = dst->u.var_name;
    int local        = find_local(g, name);
    if (local >= 0) {
        emit_imm(g, WASM_LOCAL_SET, local);
        return;
    }
    int slot = var_slot(g, name);
    if (var_ref(g, name) >= 0)
        emit_access(g, wasm_store_op(type_of(g, name)), NULL, 0);
    else if (slot >= 0)
        emit_access(g, wasm_store_op(type_of(g, name)), NULL, slot);
    else
        emit_access(g, wasm_store_op(type_of(g, name)), wasm_symbol(g->program, name), 0);
}

static bool is_signed_type(const Tac_Type *t)
{
    switch (t->kind) {
    case TAC_TYPE_SCHAR:
    case TAC_TYPE_SHORT:
    case TAC_TYPE_INT:
    case TAC_TYPE_LONG:
    case TAC_TYPE_LONG_LONG:
        return true;
    default:
        return false;
    }
}

// Bring an i32 holding a value of type t to its canonical form: a narrow type
// extended from its width, by its signedness.
void narrow(Gen *g, const Tac_Type *t)
{
    int size = wasm_type_size(t);
    if (size >= 4 || t->kind == TAC_TYPE_VOID)
        return;
    if (is_signed_type(t)) {
        emit(g, size == 1 ? WASM_I32_EXTEND8_S : WASM_I32_EXTEND16_S);
    } else {
        emit_imm(g, WASM_I32_CONST, size == 1 ? 0xff : 0xffff);
        emit(g, WASM_I32_AND);
    }
}

// The canonical value of constant v as a value of type t.
static int64_t narrow_const(int64_t v, const Tac_Type *t)
{
    switch (wasm_type_size(t)) {
    case 1:
        return is_signed_type(t) ? (int8_t)v : (uint8_t)v;
    case 2:
        return is_signed_type(t) ? (int16_t)v : (uint16_t)v;
    case 4:
        return is_signed_type(t) ? (int32_t)v : (uint32_t)v;
    default:
        return v;
    }
}

// Push value v as a value of type t.  TAC leaves out a conversion between narrow types
// of one width (signed char and unsigned char), so a narrow value whose own type is
// not t is extended again by t's signedness.
void push_val_as(Gen *g, const Tac_Val *v, const Tac_Type *t)
{
    Wasm_ValType vt = wasm_valtype(t);
    if (v->kind == TAC_VAL_CONSTANT && (vt == WASM_I32 || vt == WASM_I64)) {
        push_int(g, vt, narrow_const(const_int(v->u.constant), t));
        return;
    }
    push_val(g, v, vt);
    if (v->kind == TAC_VAL_VAR && wasm_type_size(t) < 4 &&
        type_of(g, v->u.var_name)->kind != t->kind)
        narrow(g, t);
}

//
// Integer width conversions: truncation, sign and zero extension.
//
static void gen_int_convert(Gen *g, const Tac_Val *src, const Tac_Val *dst, bool sign)
{
    begin_dst(g, dst);
    const Tac_Type *dt = type_of(g, dst->u.var_name);
    Wasm_ValType dvt   = wasm_valtype(dt);
    if (src->kind == TAC_VAL_CONSTANT) {
        push_int(g, dvt, narrow_const(const_int(src->u.constant), dt));
        end_dst(g, dst);
        return;
    }
    const Tac_Type *st = val_type(g, src);
    Wasm_ValType svt   = wasm_valtype(st);
    push_val(g, src, svt);
    // A zero extension of a signed narrow value drops its sign bits first.
    if (!sign && is_signed_type(st) && wasm_type_size(st) < 4) {
        emit_imm(g, WASM_I32_CONST, wasm_type_size(st) == 1 ? 0xff : 0xffff);
        emit(g, WASM_I32_AND);
    }
    if (svt == WASM_I64 && dvt == WASM_I32)
        emit(g, WASM_I32_WRAP_I64);
    else if (svt == WASM_I32 && dvt == WASM_I64)
        emit(g, sign ? WASM_I64_EXTEND_I32_S : WASM_I64_EXTEND_I32_U);
    if (wasm_type_size(dt) < wasm_type_size(st) || svt != dvt)
        narrow(g, dt);
    end_dst(g, dst);
}

//
// Conversions between integers and floating point.
//
static bool is_fp_valtype(Wasm_ValType t)
{
    return t == WASM_F32 || t == WASM_F64;
}

static bool is_unsigned_conversion(Tac_InstructionKind k)
{
    return k == TAC_INSTRUCTION_DOUBLE_TO_UINT || k == TAC_INSTRUCTION_FLOAT_TO_UINT ||
           k == TAC_INSTRUCTION_UINT_TO_DOUBLE || k == TAC_INSTRUCTION_UINT_TO_FLOAT;
}

// The conversion of a value of type `from` to one of type `to`, both value types.
static Wasm_Op convert_op(Wasm_ValType from, Wasm_ValType to, bool is_unsigned)
{
    if (from == WASM_F32 && to == WASM_F64)
        return WASM_F64_PROMOTE_F32;
    if (from == WASM_F64 && to == WASM_F32)
        return WASM_F32_DEMOTE_F64;
    if (to == WASM_I32)
        return from == WASM_F64
                   ? (is_unsigned ? WASM_I32_TRUNC_SAT_F64_U : WASM_I32_TRUNC_SAT_F64_S)
                   : (is_unsigned ? WASM_I32_TRUNC_SAT_F32_U : WASM_I32_TRUNC_SAT_F32_S);
    if (to == WASM_I64)
        return from == WASM_F64
                   ? (is_unsigned ? WASM_I64_TRUNC_SAT_F64_U : WASM_I64_TRUNC_SAT_F64_S)
                   : (is_unsigned ? WASM_I64_TRUNC_SAT_F32_U : WASM_I64_TRUNC_SAT_F32_S);
    if (to == WASM_F64)
        return from == WASM_I64 ? (is_unsigned ? WASM_F64_CONVERT_I64_U : WASM_F64_CONVERT_I64_S)
                                : (is_unsigned ? WASM_F64_CONVERT_I32_U : WASM_F64_CONVERT_I32_S);
    return from == WASM_I64 ? (is_unsigned ? WASM_F32_CONVERT_I64_U : WASM_F32_CONVERT_I64_S)
                            : (is_unsigned ? WASM_F32_CONVERT_I32_U : WASM_F32_CONVERT_I32_S);
}

static void gen_fp_convert(Gen *g, const Tac_Instruction *in)
{
    const Tac_Val *src = in->u.int_to_double.src, *dst = in->u.int_to_double.dst;
    begin_dst(g, dst);
    const Tac_Type *dt = type_of(g, dst->u.var_name);
    Wasm_ValType from = val_valtype(g, src), to = wasm_valtype(dt);
    push_val(g, src, from);
    if (from != to)
        emit(g, convert_op(from, to, is_unsigned_conversion(in->kind)));
    narrow(g, dt);
    end_dst(g, dst);
}

//
// Unary and binary operators on integers.
//
static void gen_unary(Gen *g, const Tac_Instruction *in)
{
    const Tac_Val *src = in->u.unary.src, *dst = in->u.unary.dst;
    begin_dst(g, dst);
    Wasm_ValType t = val_valtype(g, src);
    bool i64       = t == WASM_I64;
    if (is_fp_valtype(t)) {
        bool f64 = t == WASM_F64;
        push_val(g, src, t);
        switch (in->u.unary.op) {
        case TAC_UNARY_NEGATE:
        case TAC_UNARY_NEGATE_DOUBLE:
            emit(g, f64 ? WASM_F64_NEG : WASM_F32_NEG);
            break;
        case TAC_UNARY_SQRT_DOUBLE:
            emit(g, f64 ? WASM_F64_SQRT : WASM_F32_SQRT);
            break;
        case TAC_UNARY_NOT: {
            Tac_Const zero = { .kind = TAC_CONST_DOUBLE };
            push_const(g, &zero, t);
            emit(g, f64 ? WASM_F64_EQ : WASM_F32_EQ);
            break;
        }
        default:
            internal_error("wasm: %s: unary operator %d on floating point", g->fn->name,
                           in->u.unary.op);
        }
        end_dst(g, dst);
        return;
    }
    switch (in->u.unary.op) {
    case TAC_UNARY_COMPLEMENT:
    case TAC_UNARY_COMPLEMENT_UNSIGNED:
        push_val(g, src, t);
        push_int(g, t, -1);
        emit(g, i64 ? WASM_I64_XOR : WASM_I32_XOR);
        break;
    case TAC_UNARY_NEGATE:
    case TAC_UNARY_NEGATE_UNSIGNED:
        push_int(g, t, 0);
        push_val(g, src, t);
        emit(g, i64 ? WASM_I64_SUB : WASM_I32_SUB);
        break;
    case TAC_UNARY_NOT:
        push_val(g, src, t);
        emit(g, i64 ? WASM_I64_EQZ : WASM_I32_EQZ);
        break;
    default:
        internal_error("wasm: %s: unary operator %d is not implemented", g->fn->name,
                       in->u.unary.op);
    }
    narrow(g, type_of(g, dst->u.var_name));
    end_dst(g, dst);
}

// The i32 and i64 opcodes of an integer binary operator; false when it has none.
static bool int_binop(Tac_BinaryOperator op, Wasm_Op *op32, Wasm_Op *op64)
{
    switch (op) {
#define OP(tac, w)            \
    case TAC_BINARY_##tac:    \
        *op32 = WASM_I32_##w; \
        *op64 = WASM_I64_##w; \
        return true;
        OP(ADD, ADD)
        OP(ADD_UNSIGNED, ADD)
        OP(SUBTRACT, SUB)
        OP(SUBTRACT_UNSIGNED, SUB)
        OP(MULTIPLY, MUL)
        OP(MULTIPLY_UNSIGNED, MUL)
        OP(DIVIDE, DIV_S)
        OP(DIVIDE_UNSIGNED, DIV_U)
        OP(REMAINDER, REM_S)
        OP(REMAINDER_UNSIGNED, REM_U)
        OP(EQUAL, EQ)
        OP(NOT_EQUAL, NE)
        OP(LESS_THAN, LT_S)
        OP(LESS_OR_EQUAL, LE_S)
        OP(GREATER_THAN, GT_S)
        OP(GREATER_OR_EQUAL, GE_S)
        OP(LESS_THAN_UNSIGNED, LT_U)
        OP(LESS_OR_EQUAL_UNSIGNED, LE_U)
        OP(GREATER_THAN_UNSIGNED, GT_U)
        OP(GREATER_OR_EQUAL_UNSIGNED, GE_U)
        OP(BITWISE_AND, AND)
        OP(BITWISE_OR, OR)
        OP(BITWISE_XOR, XOR)
        OP(LEFT_SHIFT, SHL)
        OP(RIGHT_SHIFT, SHR_S)
        OP(RIGHT_SHIFT_LOGICAL, SHR_U)
#undef OP
    default:
        return false;
    }
}

static bool is_shift(Tac_BinaryOperator op)
{
    return op == TAC_BINARY_LEFT_SHIFT || op == TAC_BINARY_RIGHT_SHIFT ||
           op == TAC_BINARY_RIGHT_SHIFT_LOGICAL;
}

// The f32 and f64 opcodes of a floating-point binary operator; false when it has none.
static bool fp_binop(Tac_BinaryOperator op, Wasm_Op *op32, Wasm_Op *op64)
{
    switch (op) {
#define OP(tac, w)            \
    case TAC_BINARY_##tac:    \
        *op32 = WASM_F32_##w; \
        *op64 = WASM_F64_##w; \
        return true;
        OP(ADD, ADD)
        OP(ADD_DOUBLE, ADD)
        OP(SUBTRACT, SUB)
        OP(SUBTRACT_DOUBLE, SUB)
        OP(MULTIPLY, MUL)
        OP(MULTIPLY_DOUBLE, MUL)
        OP(DIVIDE, DIV)
        OP(DIVIDE_DOUBLE, DIV)
        OP(EQUAL, EQ)
        OP(NOT_EQUAL, NE)
        OP(LESS_THAN, LT)
        OP(LESS_THAN_DOUBLE, LT)
        OP(LESS_OR_EQUAL, LE)
        OP(LESS_OR_EQUAL_DOUBLE, LE)
        OP(GREATER_THAN, GT)
        OP(GREATER_THAN_DOUBLE, GT)
        OP(GREATER_OR_EQUAL, GE)
        OP(GREATER_OR_EQUAL_DOUBLE, GE)
#undef OP
    default:
        return false;
    }
}

static void gen_fp_binary(Gen *g, const Tac_Instruction *in, Wasm_ValType t)
{
    const Tac_Val *a = in->u.binary.src1, *b = in->u.binary.src2, *dst = in->u.binary.dst;
    Wasm_Op op32, op64;
    if (!fp_binop(in->u.binary.op, &op32, &op64))
        internal_error("wasm: %s: binary operator %d on floating point", g->fn->name,
                       in->u.binary.op);
    begin_dst(g, dst);
    push_val(g, a, t);
    push_val(g, b, t);
    emit(g, t == WASM_F64 ? op64 : op32);
    end_dst(g, dst);
}

static void gen_binary(Gen *g, const Tac_Instruction *in)
{
    const Tac_Val *a = in->u.binary.src1, *b = in->u.binary.src2, *dst = in->u.binary.dst;
    Tac_BinaryOperator op = in->u.binary.op;
    Wasm_ValType ft       = a->kind == TAC_VAL_VAR ? val_valtype(g, a) : val_valtype(g, b);
    if (is_fp_valtype(ft)) {
        gen_fp_binary(g, in, ft);
        return;
    }
    Wasm_Op op32, op64;
    if (!int_binop(op, &op32, &op64))
        internal_error("wasm: %s: binary operator %d is not implemented", g->fn->name, op);
    begin_dst(g, dst);
    // The operands' type: the left one's, but the right one's when only it is a
    // variable; a shift's count has a type of its own.
    Wasm_ValType t = val_valtype(g, a);
    if (a->kind == TAC_VAL_CONSTANT && b->kind == TAC_VAL_VAR && !is_shift(op))
        t = val_valtype(g, b);
    push_val(g, a, t);
    if (is_shift(op) && b->kind == TAC_VAL_VAR) {
        // The count has a type of its own.
        Wasm_ValType ct = val_valtype(g, b);
        push_val(g, b, ct);
        if (ct == WASM_I64 && t == WASM_I32)
            emit(g, WASM_I32_WRAP_I64);
        else if (ct == WASM_I32 && t == WASM_I64)
            emit(g, WASM_I64_EXTEND_I32_U);
    } else {
        push_val(g, b, t);
    }
    emit(g, t == WASM_I64 ? op64 : op32);
    narrow(g, type_of(g, dst->u.var_name));
    end_dst(g, dst);
}

//
// Long double: binary128 in memory, its operations calls of the runtime (float128.c,
// with libgcc's names), each operand two i64 and an arithmetic result through memory.
//
static const Tac_Type ld_type  = { .kind = TAC_TYPE_LONG_DOUBLE },
                      int_type = { .kind = TAC_TYPE_INT };

static bool is_ld(const Tac_Type *t)
{
    return t && t->kind == TAC_TYPE_LONG_DOUBLE;
}

static void check_ld_operand(const Gen *g, const Tac_Val *v)
{
    if (v->kind == TAC_VAL_CONSTANT && v->u.constant->kind != TAC_CONST_LONG_DOUBLE)
        internal_error("wasm: %s: a long double operand of constant kind %d", g->fn->name,
                       v->u.constant->kind);
}

// A comparison routine's int on the stack, against 0 by operator op.
static void ld_compare(Gen *g, Tac_BinaryOperator op)
{
    if (op == TAC_BINARY_EQUAL) {
        emit(g, WASM_I32_EQZ);
        return;
    }
    emit_imm(g, WASM_I32_CONST, 0);
    switch (op) {
    case TAC_BINARY_NOT_EQUAL:
        emit(g, WASM_I32_NE);
        break;
    case TAC_BINARY_LESS_THAN:
    case TAC_BINARY_LESS_THAN_DOUBLE:
        emit(g, WASM_I32_LT_S);
        break;
    case TAC_BINARY_LESS_OR_EQUAL:
    case TAC_BINARY_LESS_OR_EQUAL_DOUBLE:
        emit(g, WASM_I32_LE_S);
        break;
    case TAC_BINARY_GREATER_THAN:
    case TAC_BINARY_GREATER_THAN_DOUBLE:
        emit(g, WASM_I32_GT_S);
        break;
    default:
        emit(g, WASM_I32_GE_S);
        break;
    }
}

static void gen_ld_binary(Gen *g, const Tac_Instruction *in)
{
    static const struct {
        Tac_BinaryOperator op;
        const char *name;
    } ops[] = {
        { TAC_BINARY_ADD, "__addtf3" },
        { TAC_BINARY_ADD_DOUBLE, "__addtf3" },
        { TAC_BINARY_SUBTRACT, "__subtf3" },
        { TAC_BINARY_SUBTRACT_DOUBLE, "__subtf3" },
        { TAC_BINARY_MULTIPLY, "__multf3" },
        { TAC_BINARY_MULTIPLY_DOUBLE, "__multf3" },
        { TAC_BINARY_DIVIDE, "__divtf3" },
        { TAC_BINARY_DIVIDE_DOUBLE, "__divtf3" },
        { TAC_BINARY_EQUAL, "__eqtf2" },
        { TAC_BINARY_NOT_EQUAL, "__netf2" },
        { TAC_BINARY_LESS_THAN, "__lttf2" },
        { TAC_BINARY_LESS_THAN_DOUBLE, "__lttf2" },
        { TAC_BINARY_LESS_OR_EQUAL, "__letf2" },
        { TAC_BINARY_LESS_OR_EQUAL_DOUBLE, "__letf2" },
        { TAC_BINARY_GREATER_THAN, "__gttf2" },
        { TAC_BINARY_GREATER_THAN_DOUBLE, "__gttf2" },
        { TAC_BINARY_GREATER_OR_EQUAL, "__getf2" },
        { TAC_BINARY_GREATER_OR_EQUAL_DOUBLE, "__getf2" },
    };
    Tac_BinaryOperator op = in->u.binary.op;
    const char *name      = NULL;
    for (size_t i = 0; i < sizeof(ops) / sizeof(ops[0]) && !name; i++)
        if (ops[i].op == op)
            name = ops[i].name;
    if (!name)
        internal_error("wasm: %s: long double operator %d", g->fn->name, op);
    const Tac_Val *dst             = in->u.binary.dst;
    const Tac_Val *const args[2]   = { in->u.binary.src1, in->u.binary.src2 };
    const Tac_Type *const types[2] = { &ld_type, &ld_type };
    check_ld_operand(g, args[0]);
    check_ld_operand(g, args[1]);
    if (is_ld(type_of(g, dst->u.var_name))) {
        gen_runtime(g, name, &ld_type, args, types, 2, dst);
        return;
    }
    begin_dst(g, dst);
    gen_runtime(g, name, &int_type, args, types, 2, NULL);
    ld_compare(g, op);
    end_dst(g, dst);
}

static void gen_ld_unary(Gen *g, const Tac_Instruction *in)
{
    const Tac_Val *src = in->u.unary.src, *dst = in->u.unary.dst;
    check_ld_operand(g, src);
    switch (in->u.unary.op) {
    case TAC_UNARY_NEGATE:
    case TAC_UNARY_NEGATE_DOUBLE: {
        // A copy with the sign flipped.
        Place p = place_named(dst->u.var_name, 0);
        store_value(g, &p, src, &ld_type);
        const char *sym;
        int64_t off;
        push_base(g, dst->u.var_name, &sym, &off);
        push_base(g, dst->u.var_name, &sym, &off);
        emit_access(g, WASM_I64_LOAD, sym, off + 8);
        emit_imm(g, WASM_I64_CONST, INT64_MIN);
        emit(g, WASM_I64_XOR);
        emit_access(g, WASM_I64_STORE, sym, off + 8);
        return;
    }
    case TAC_UNARY_NOT: {
        Tac_Const zero                 = { .kind = TAC_CONST_LONG_DOUBLE };
        Tac_Val z                      = { .kind = TAC_VAL_CONSTANT, .u.constant = &zero };
        const Tac_Val *const args[2]   = { src, &z };
        const Tac_Type *const types[2] = { &ld_type, &ld_type };
        begin_dst(g, dst);
        gen_runtime(g, "__eqtf2", &int_type, args, types, 2, NULL);
        emit(g, WASM_I32_EQZ);
        end_dst(g, dst);
        return;
    }
    default:
        internal_error("wasm: %s: unary operator %d on long double", g->fn->name, in->u.unary.op);
    }
}

static void gen_ld_convert(Gen *g, const Tac_Instruction *in)
{
    static const Tac_Type uint_type = { .kind = TAC_TYPE_UINT },
                          ll_type   = { .kind = TAC_TYPE_LONG_LONG },
                          ull_type  = { .kind = TAC_TYPE_ULONG_LONG },
                          dbl_type  = { .kind = TAC_TYPE_DOUBLE },
                          flt_type  = { .kind = TAC_TYPE_FLOAT };
    const Tac_Val *src = in->u.int_to_double.src, *dst = in->u.int_to_double.dst;
    const Tac_Type *dt = type_of(g, dst->u.var_name), *st = any_type(g, src);
    const Tac_Type *arg = st, *ret = &ld_type;
    const char *name;
    bool w = wasm_type_size(st) <= 4;
    switch (in->kind) {
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_UINT: {
        bool un = in->kind == TAC_INSTRUCTION_LONG_DOUBLE_TO_UINT;
        arg     = &ld_type;
        if (wasm_type_size(dt) <= 4) {
            name = un ? "__fixunstfsi" : "__fixtfsi";
            ret  = un ? &uint_type : &int_type;
        } else {
            name = un ? "__fixunstfdi" : "__fixtfdi";
            ret  = un ? &ull_type : &ll_type;
        }
        break;
    }
    case TAC_INSTRUCTION_INT_TO_LONG_DOUBLE:
        name = w ? "__floatsitf" : "__floatditf";
        arg  = w ? &int_type : &ll_type;
        break;
    case TAC_INSTRUCTION_UINT_TO_LONG_DOUBLE:
        name = w ? "__floatunsitf" : "__floatunditf";
        arg  = w ? &uint_type : &ull_type;
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_LONG_DOUBLE:
        name = "__extenddftf2";
        arg  = &dbl_type;
        break;
    case TAC_INSTRUCTION_FLOAT_TO_LONG_DOUBLE:
        name = "__extendsftf2";
        arg  = &flt_type;
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_DOUBLE:
        name = "__trunctfdf2";
        arg  = &ld_type;
        ret  = &dbl_type;
        break;
    default: // LONG_DOUBLE_TO_FLOAT
        name = "__trunctfsf2";
        arg  = &ld_type;
        ret  = &flt_type;
        break;
    }
    if (is_ld(arg))
        check_ld_operand(g, src);
    const Tac_Val *const args[1]   = { src };
    const Tac_Type *const types[1] = { arg };
    if (wasm_type_size(dt) < 4 && !is_ld(dt)) {
        // A narrow result: the routine's int, brought to the destination's form.
        begin_dst(g, dst);
        gen_runtime(g, name, ret, args, types, 1, NULL);
        narrow(g, dt);
        end_dst(g, dst);
        return;
    }
    gen_runtime(g, name, ret, args, types, 1, dst);
}

// Whether a long double is zero, by __netf2(x, 0): its int, 0 when zero.
static void push_ld_nonzero(Gen *g, const Tac_Val *cond)
{
    Tac_Const zero                 = { .kind = TAC_CONST_LONG_DOUBLE };
    Tac_Val z                      = { .kind = TAC_VAL_CONSTANT, .u.constant = &zero };
    const Tac_Val *const args[2]   = { cond, &z };
    const Tac_Type *const types[2] = { &ld_type, &ld_type };
    check_ld_operand(g, cond);
    gen_runtime(g, "__netf2", &int_type, args, types, 2, NULL);
}

bool is_ld_op(const Gen *g, const Tac_Instruction *in)
{
    switch (in->kind) {
    case TAC_INSTRUCTION_BINARY:
        return is_ld(any_type(g, in->u.binary.src1)) || is_ld(any_type(g, in->u.binary.src2));
    case TAC_INSTRUCTION_UNARY:
        return is_ld(any_type(g, in->u.unary.src));
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_UINT:
    case TAC_INSTRUCTION_INT_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_UINT_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_DOUBLE:
    case TAC_INSTRUCTION_DOUBLE_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_FLOAT:
    case TAC_INSTRUCTION_FLOAT_TO_LONG_DOUBLE:
        return true;
    default:
        return false;
    }
}

//
// Control: jumps by the skeleton's rules (structure.c).
//
void push_condition(Gen *g, const Tac_Val *cond, bool if_zero)
{
    if (is_ld(any_type(g, cond))) {
        push_ld_nonzero(g, cond);
        if (if_zero)
            emit(g, WASM_I32_EQZ);
        return;
    }
    Wasm_ValType t = val_valtype(g, cond);
    push_val(g, cond, t);
    if (is_fp_valtype(t)) {
        Tac_Const zero = { .kind = TAC_CONST_DOUBLE };
        push_const(g, &zero, t);
        emit(g, if_zero ? (t == WASM_F64 ? WASM_F64_EQ : WASM_F32_EQ)
                        : (t == WASM_F64 ? WASM_F64_NE : WASM_F32_NE));
    } else if (t == WASM_I64) {
        emit(g, WASM_I64_EQZ);
        if (!if_zero)
            emit(g, WASM_I32_EQZ);
    } else if (if_zero) {
        emit(g, WASM_I32_EQZ);
    }
}

static void gen_cond_jump(Gen *g, const Tac_Val *cond, const char *target, bool if_zero)
{
    gen_branch_setup(g, target);
    push_condition(g, cond, if_zero);
    gen_branch(g, target, true);
}

//
// Memory: addresses, loads and stores through pointers, members of aggregates.
//
bool is_aggregate(const Tac_Type *t)
{
    return t->kind == TAC_TYPE_STRUCTURE || t->kind == TAC_TYPE_ARRAY;
}

// Push the address of named object `name`: a slot's, a static object's, a function's
// (its index in the table, which the relocation gives).
void push_addr(Gen *g, const char *name)
{
    int ref = var_ref(g, name);
    if (ref >= 0) {
        emit_imm(g, WASM_LOCAL_GET, ref);
        return;
    }
    int slot = var_slot(g, name);
    if (slot >= 0) {
        emit_imm(g, WASM_LOCAL_GET, g->fp);
        if (slot) {
            emit_imm(g, WASM_I32_CONST, slot);
            emit(g, WASM_I32_ADD);
        }
        return;
    }
    if (find_local(g, name) >= 0)
        internal_error("wasm: %s: the address of %s, which is not in memory", g->fn->name, name);
    emit(g, WASM_I32_CONST)->sym = xstrdup(wasm_symbol(g->program, name));
}

// Copy `size` bytes between the two addresses on the stack, the destination's under
// the source's.
static void copy_bytes(Gen *g, int size)
{
    emit_imm(g, WASM_I32_CONST, size);
    emit(g, WASM_MEMORY_COPY);
}

bool is_memory_type(const Tac_Type *t)
{
    return is_aggregate(t) || t->kind == TAC_TYPE_LONG_DOUBLE;
}

static void add_offset(Gen *g, int offset)
{
    if (offset) {
        emit_imm(g, WASM_I32_CONST, offset);
        emit(g, WASM_I32_ADD);
    }
}

// The base of an access to place p, which *sym and *off complete in its immediate.
static void place_base(Gen *g, const Place *p, const char **sym, int64_t *off)
{
    if (p->name) {
        push_base(g, p->name, sym, off);
    } else {
        if (p->ptr)
            push_val(g, p->ptr, WASM_I32);
        else
            emit_imm(g, WASM_LOCAL_GET, p->local);
        *sym = NULL;
        *off = 0;
    }
    *off += p->offset;
}

void place_addr(Gen *g, const Place *p)
{
    if (p->name)
        push_addr(g, p->name);
    else if (p->ptr)
        push_val(g, p->ptr, WASM_I32);
    else
        emit_imm(g, WASM_LOCAL_GET, p->local);
    add_offset(g, p->offset);
}

Place place_named(const char *name, int offset)
{
    return (Place){ .name = name, .offset = offset };
}

Place place_local(int local, int offset)
{
    return (Place){ .local = local, .offset = offset };
}

void store_value(Gen *g, const Place *dst, const Tac_Val *v, const Tac_Type *t)
{
    const char *sym;
    int64_t off;
    if (!is_memory_type(t)) {
        place_base(g, dst, &sym, &off);
        push_val_as(g, v, t);
        emit_access(g, wasm_store_op(t), sym, off);
        return;
    }
    if (v->kind == TAC_VAL_CONSTANT) {
        if (v->u.constant->kind != TAC_CONST_LONG_DOUBLE)
            internal_error("wasm: %s: a constant of an aggregate type", g->fn->name);
        Float128 bits = v->u.constant->u.long_double_val;
        for (int half = 0; half < 2; half++) {
            place_base(g, dst, &sym, &off);
            emit_imm(g, WASM_I64_CONST, (int64_t)(half ? bits.hi : bits.lo));
            emit_access(g, WASM_I64_STORE, sym, off + 8 * half);
        }
        return;
    }
    place_addr(g, dst);
    push_addr(g, v->u.var_name);
    copy_bytes(g, wasm_type_size(t));
}

void load_value(Gen *g, const Place *src, const Tac_Type *t)
{
    const char *sym;
    int64_t off;
    place_base(g, src, &sym, &off);
    emit_access(g, wasm_load_op(t), sym, off);
}

// dst = the value of type t at place src.
static void copy_from_place(Gen *g, const Place *src, const Tac_Val *dst, const Tac_Type *t)
{
    if (is_memory_type(t)) {
        push_addr(g, dst->u.var_name);
        place_addr(g, src);
        copy_bytes(g, wasm_type_size(t));
        return;
    }
    begin_dst(g, dst);
    load_value(g, src, t);
    end_dst(g, dst);
}

// The scalar of `size` bytes at byte `offset` of an object of type t, or NULL.
static const Tac_Type *scalar_at(const Tac_Type *t, int offset, int size)
{
    if (!t)
        return NULL;
    if (t->kind == TAC_TYPE_ARRAY) {
        int esize = wasm_type_size(t->u.array.elem_type);
        return esize > 0 ? scalar_at(t->u.array.elem_type, offset % esize, size) : NULL;
    }
    if (t->kind != TAC_TYPE_STRUCTURE)
        return offset == 0 ? t : NULL;
    const Tac_Type *first = NULL;
    for (const Tac_Member *m = t->u.structure.members; m; m = m->next) {
        if (offset < m->offset || offset >= m->offset + wasm_type_size(m->type))
            continue;
        const Tac_Type *sc = scalar_at(m->type, offset - m->offset, size);
        if (sc && wasm_type_size(sc) == size)
            return sc;
        if (!first)
            first = sc;
    }
    return first;
}

static bool is_float_type(const Tac_Type *t)
{
    return t->kind == TAC_TYPE_FLOAT || t->kind == TAC_TYPE_DOUBLE ||
           t->kind == TAC_TYPE_LONG_DOUBLE;
}

// Member store: aggregate `dst` at byte `offset` = src.  A constant takes the type of
// the member there (its own kind may be wider); a byte copy is one byte.
static void gen_copy_to_offset(Gen *g, const Tac_Val *src, const char *dst, int offset, bool byte)
{
    static const Tac_Type uchar = { .kind = TAC_TYPE_UCHAR };
    const Tac_Type *t           = any_type(g, src);
    if (byte) {
        t = &uchar;
    } else if (src->kind == TAC_VAL_CONSTANT) {
        const Tac_Type *m = scalar_at(type_of(g, dst), offset, wasm_type_size(t));
        if (m && is_float_type(m) == is_float_type(t) && !is_aggregate(m))
            t = m;
    }
    Place p = place_named(dst, offset);
    store_value(g, &p, src, t);
}

// Member load: dst = aggregate `src` at byte `offset`.
static void gen_copy_from_offset(Gen *g, const char *src, int offset, const Tac_Val *dst)
{
    Place p = place_named(src, offset);
    copy_from_place(g, &p, dst, type_of(g, dst->u.var_name));
}

// dst = *ptr.
static void gen_load(Gen *g, const Tac_Val *ptr, const Tac_Val *dst)
{
    Place p = { .ptr = ptr };
    copy_from_place(g, &p, dst, type_of(g, dst->u.var_name));
}

// *ptr = src, in the type pointed to (a constant's own kind may be wider).
static void gen_store(Gen *g, const Tac_Val *src, const Tac_Val *ptr)
{
    const Tac_Type *t = any_type(g, src);
    if (ptr->kind == TAC_VAL_VAR) {
        const Tac_Type *pt = type_of(g, ptr->u.var_name);
        if (pt->kind == TAC_TYPE_POINTER && pt->u.pointer.target_type &&
            pt->u.pointer.target_type->kind != TAC_TYPE_VOID &&
            is_memory_type(pt->u.pointer.target_type) == is_memory_type(t))
            t = pt->u.pointer.target_type;
    }
    Place p = { .ptr = ptr };
    store_value(g, &p, src, t);
}

// dst = ptr + index * scale, in bytes.
static void gen_add_ptr(Gen *g, const Tac_Instruction *in)
{
    const Tac_Val *index = in->u.add_ptr.index;
    int scale            = in->u.add_ptr.scale;
    begin_dst(g, in->u.add_ptr.dst);
    push_val(g, in->u.add_ptr.ptr, WASM_I32);
    if (index->kind == TAC_VAL_CONSTANT) {
        int32_t delta = (int32_t)(const_int(index->u.constant) * scale);
        if (delta) {
            emit_imm(g, WASM_I32_CONST, delta);
            emit(g, WASM_I32_ADD);
        }
    } else {
        Wasm_ValType t = val_valtype(g, index);
        push_val(g, index, t);
        if (t == WASM_I64)
            emit(g, WASM_I32_WRAP_I64);
        if (scale != 1) {
            emit_imm(g, WASM_I32_CONST, scale);
            emit(g, WASM_I32_MUL);
        }
        emit(g, WASM_I32_ADD);
    }
    end_dst(g, in->u.add_ptr.dst);
}

// dst = a - b, in bytes.
static void gen_ptr_diff(Gen *g, const Tac_Instruction *in)
{
    begin_dst(g, in->u.ptr_diff.dst);
    push_val(g, in->u.ptr_diff.ptr_a, WASM_I32);
    push_val(g, in->u.ptr_diff.ptr_b, WASM_I32);
    emit(g, WASM_I32_SUB);
    end_dst(g, in->u.ptr_diff.dst);
}

// dst = src: an aggregate or a long double by its bytes.
static void gen_copy(Gen *g, const Tac_Val *src, const Tac_Val *dst)
{
    const Tac_Type *t = type_of(g, dst->u.var_name);
    if (is_memory_type(t)) {
        Place p = place_named(dst->u.var_name, 0);
        store_value(g, &p, src, t);
        return;
    }
    begin_dst(g, dst);
    push_val_as(g, src, t);
    end_dst(g, dst);
}

static void select_instr(Gen *g, const Tac_Instruction *in)
{
    switch (in->kind) {
    case TAC_INSTRUCTION_RETURN:
        gen_return(g, in->u.return_.src);
        return;
    case TAC_INSTRUCTION_COPY:
    case TAC_INSTRUCTION_PTR_TO_CHAR_PTR:
    case TAC_INSTRUCTION_CHAR_PTR_TO_PTR:
        check_dst(g, in->u.copy.dst);
        gen_copy(g, in->u.copy.src, in->u.copy.dst);
        return;
    case TAC_INSTRUCTION_GET_ADDRESS:
    case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
    case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
        begin_dst(g, in->u.get_address.dst);
        push_addr(g, in->u.get_address.src->u.var_name);
        end_dst(g, in->u.get_address.dst);
        return;
    case TAC_INSTRUCTION_LOAD:
    case TAC_INSTRUCTION_LOAD_BYTE:
        gen_load(g, in->u.load.src_ptr, in->u.load.dst);
        return;
    case TAC_INSTRUCTION_STORE:
    case TAC_INSTRUCTION_STORE_BYTE:
        gen_store(g, in->u.store.src, in->u.store.dst_ptr);
        return;
    case TAC_INSTRUCTION_ADD_PTR:
        gen_add_ptr(g, in);
        return;
    case TAC_INSTRUCTION_PTR_DIFF:
        gen_ptr_diff(g, in);
        return;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET:
        gen_copy_to_offset(g, in->u.copy_to_offset.src, in->u.copy_to_offset.dst,
                           in->u.copy_to_offset.offset,
                           in->kind == TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET);
        return;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
        gen_copy_from_offset(g, in->u.copy_from_offset.src, in->u.copy_from_offset.offset,
                             in->u.copy_from_offset.dst);
        return;
    case TAC_INSTRUCTION_ALLOCATE_LOCAL:
        return; // the slot is laid out with the frame
    case TAC_INSTRUCTION_SIGN_EXTEND:
        gen_int_convert(g, in->u.sign_extend.src, in->u.sign_extend.dst, true);
        return;
    case TAC_INSTRUCTION_ZERO_EXTEND:
        gen_int_convert(g, in->u.zero_extend.src, in->u.zero_extend.dst, false);
        return;
    case TAC_INSTRUCTION_TRUNCATE:
        gen_int_convert(g, in->u.truncate.src, in->u.truncate.dst, true);
        return;
    case TAC_INSTRUCTION_UNARY:
        if (is_ld(any_type(g, in->u.unary.src)))
            gen_ld_unary(g, in);
        else
            gen_unary(g, in);
        return;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_UINT:
    case TAC_INSTRUCTION_INT_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_UINT_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_DOUBLE:
    case TAC_INSTRUCTION_DOUBLE_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_FLOAT:
    case TAC_INSTRUCTION_FLOAT_TO_LONG_DOUBLE:
        gen_ld_convert(g, in);
        return;
    case TAC_INSTRUCTION_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_DOUBLE_TO_UINT:
    case TAC_INSTRUCTION_INT_TO_DOUBLE:
    case TAC_INSTRUCTION_UINT_TO_DOUBLE:
    case TAC_INSTRUCTION_FLOAT_TO_DOUBLE:
    case TAC_INSTRUCTION_DOUBLE_TO_FLOAT:
    case TAC_INSTRUCTION_INT_TO_FLOAT:
    case TAC_INSTRUCTION_UINT_TO_FLOAT:
    case TAC_INSTRUCTION_FLOAT_TO_INT:
    case TAC_INSTRUCTION_FLOAT_TO_UINT:
        gen_fp_convert(g, in);
        return;
    case TAC_INSTRUCTION_BINARY:
        if (is_ld_op(g, in))
            gen_ld_binary(g, in);
        else
            gen_binary(g, in);
        return;
    case TAC_INSTRUCTION_LABEL:
        return; // a block boundary: structure.c
    case TAC_INSTRUCTION_JUMP:
        gen_branch_setup(g, in->u.jump.target);
        gen_branch(g, in->u.jump.target, false);
        return;
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
        gen_cond_jump(g, in->u.jump_if_zero.condition, in->u.jump_if_zero.target, true);
        return;
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
        gen_cond_jump(g, in->u.jump_if_not_zero.condition, in->u.jump_if_not_zero.target, false);
        return;
    case TAC_INSTRUCTION_JUMP_TABLE: // the dispatch skeleton's: compares, then the default
        for (int i = 0; i < in->u.jump_table.count; i++) {
            const char *t = in->u.jump_table.targets[i];
            gen_branch_setup(g, t);
            push_val(g, in->u.jump_table.index, WASM_I32);
            emit_imm(g, WASM_I32_CONST, i);
            emit(g, WASM_I32_EQ);
            gen_branch(g, t, true);
        }
        gen_branch_setup(g, in->u.jump_table.default_target);
        gen_branch(g, in->u.jump_table.default_target, false);
        return;
    case TAC_INSTRUCTION_FUN_CALL:
        gen_call(g, in, false);
        return;
    case TAC_INSTRUCTION_FUN_CALL_NORETURN:
        gen_call(g, in, true);
        return;
    default:
        break;
    }
    internal_error("wasm: %s: TAC %s is not implemented", g->fn->name,
                   tac_instruction_name(in->kind));
}

void gen_instr(Gen *g, const Tac_Instruction *in)
{
    g->barrier = in->is_volatile;
    select_instr(g, in);
    g->barrier = false;
}
