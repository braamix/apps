#include <math.h>
#include <stdbool.h>
#include <string.h>

#include "tac.h"

// Treat two floating-point constants as equal when they compare equal OR both
// are NaN. Raw == makes NaN != NaN, which would keep the optimizer's fixed-point
// loop from ever converging on a folded NaN constant (e.g. 0.0/0.0).
//
// 0.0 and -0.0 compare equal with ==, but they are different constants (1/x tells
// them apart): copy propagation must not merge `r = 0.0` on one path with
// `r = -0.0` on another.
static bool fp_equal(long double x, long double y)
{
    return (x == y && signbit(x) == signbit(y)) || (isnan(x) && isnan(y));
}

// Binary128 constants are equal when their bits are, or both are NaN.
static bool f128_equal(Float128 x, Float128 y)
{
    return (x.lo == y.lo && x.hi == y.hi) || (f128_is_nan(x) && f128_is_nan(y));
}

// Compare two Tac_Const structures
bool tac_compare_const(const Tac_Const *a, const Tac_Const *b)
{
    if (a == b)
        return true;
    if (!a || !b)
        return false;
    if (a->kind != b->kind)
        return false;
    switch (a->kind) {
    case TAC_CONST_INT:
        return a->u.int_val == b->u.int_val;
    case TAC_CONST_LONG:
        return a->u.long_val == b->u.long_val;
    case TAC_CONST_LONG_LONG:
        return a->u.long_long_val == b->u.long_long_val;
    case TAC_CONST_UINT:
        return a->u.uint_val == b->u.uint_val;
    case TAC_CONST_ULONG:
        return a->u.ulong_val == b->u.ulong_val;
    case TAC_CONST_ULONG_LONG:
        return a->u.ulong_long_val == b->u.ulong_long_val;
    case TAC_CONST_FLOAT:
        return fp_equal(a->u.float_val, b->u.float_val);
    case TAC_CONST_DOUBLE:
        return fp_equal(a->u.double_val, b->u.double_val);
    case TAC_CONST_LONG_DOUBLE:
        return f128_equal(a->u.long_double_val, b->u.long_double_val);
    case TAC_CONST_SCHAR:
        return a->u.char_val == b->u.char_val;
    case TAC_CONST_UCHAR:
        return a->u.uchar_val == b->u.uchar_val;
    }
    return false;
}

// Compare two Tac_Val structures recursively
bool tac_compare_val(const Tac_Val *a, const Tac_Val *b)
{
    if (a == b)
        return true;
    if (!a || !b)
        return false;
    if (a->kind != b->kind)
        return false;
    if (a->kind == TAC_VAL_CONSTANT) {
        if (!tac_compare_const(a->u.constant, b->u.constant))
            return false;
    } else {
        if ((a->u.var_name == NULL) != (b->u.var_name == NULL))
            return false;
        if (a->u.var_name && strcmp(a->u.var_name, b->u.var_name) != 0)
            return false;
    }
    return tac_compare_val(a->next, b->next);
}

// Compare two Tac_Type structures recursively
bool tac_compare_type(const Tac_Type *a, const Tac_Type *b)
{
    if (a == b)
        return true;
    if (!a || !b)
        return false;
    if (a->kind != b->kind)
        return false;
    switch (a->kind) {
    case TAC_TYPE_FUN_TYPE:
        if (!tac_compare_type(a->u.fun_type.param_types, b->u.fun_type.param_types))
            return false;
        if (!tac_compare_type(a->u.fun_type.ret_type, b->u.fun_type.ret_type))
            return false;
        if (a->u.fun_type.variadic != b->u.fun_type.variadic)
            return false;
        break;
    case TAC_TYPE_POINTER:
        if (!tac_compare_type(a->u.pointer.target_type, b->u.pointer.target_type))
            return false;
        break;
    case TAC_TYPE_ARRAY:
        if (!tac_compare_type(a->u.array.elem_type, b->u.array.elem_type))
            return false;
        if (a->u.array.size != b->u.array.size)
            return false;
        break;
    case TAC_TYPE_STRUCTURE:
        if ((a->u.structure.tag == NULL) != (b->u.structure.tag == NULL))
            return false;
        if (a->u.structure.tag && strcmp(a->u.structure.tag, b->u.structure.tag) != 0)
            return false;
        if (a->u.structure.size != b->u.structure.size ||
            a->u.structure.alignment != b->u.structure.alignment ||
            a->u.structure.is_union != b->u.structure.is_union)
            return false;
        {
            const Tac_Member *ma = a->u.structure.members, *mb = b->u.structure.members;
            for (; ma && mb; ma = ma->next, mb = mb->next) {
                if ((ma->name == NULL) != (mb->name == NULL) ||
                    (ma->name && strcmp(ma->name, mb->name) != 0) || ma->offset != mb->offset ||
                    !tac_compare_type(ma->type, mb->type))
                    return false;
            }
            if (ma || mb)
                return false;
        }
        break;
    default:
        break;
    }
    return tac_compare_type(a->next, b->next);
}

// Compare two Tac_Param structures recursively
bool tac_compare_param(const Tac_Param *a, const Tac_Param *b)
{
    if (a == b)
        return true;
    if (!a || !b)
        return false;
    if ((a->name == NULL) != (b->name == NULL))
        return false;
    if (a->name && strcmp(a->name, b->name) != 0)
        return false;
    if (!tac_compare_type(a->type, b->type))
        return false;
    return tac_compare_param(a->next, b->next);
}

// Compare two Tac_StaticLocal lists recursively
static bool tac_compare_static_local(const Tac_StaticLocal *a, const Tac_StaticLocal *b)
{
    if (a == b)
        return true;
    if (!a || !b)
        return false;
    if ((a->name == NULL) != (b->name == NULL))
        return false;
    if (a->name && strcmp(a->name, b->name) != 0)
        return false;
    if (!tac_compare_type(a->type, b->type))
        return false;
    if (!tac_compare_static_init(a->init_list, b->init_list))
        return false;
    if (a->alignment != b->alignment)
        return false;
    return tac_compare_static_local(a->next, b->next);
}

// Compare two Tac_StaticInit structures recursively
bool tac_compare_static_init(const Tac_StaticInit *a, const Tac_StaticInit *b)
{
    if (a == b)
        return true;
    if (!a || !b)
        return false;
    if (a->kind != b->kind)
        return false;
    switch (a->kind) {
    case TAC_STATIC_INIT_I8:
        return a->u.char_val == b->u.char_val;
    case TAC_STATIC_INIT_I16:
        return a->u.short_val == b->u.short_val;
    case TAC_STATIC_INIT_I32:
        return a->u.int_val == b->u.int_val;
    case TAC_STATIC_INIT_I64:
        return a->u.long_val == b->u.long_val;
    case TAC_STATIC_INIT_U8:
        return a->u.uchar_val == b->u.uchar_val;
    case TAC_STATIC_INIT_U16:
        return a->u.ushort_val == b->u.ushort_val;
    case TAC_STATIC_INIT_U32:
        return a->u.uint_val == b->u.uint_val;
    case TAC_STATIC_INIT_U64:
        return a->u.ulong_val == b->u.ulong_val;
    case TAC_STATIC_INIT_FLOAT:
        return a->u.float_val == b->u.float_val;
    case TAC_STATIC_INIT_DOUBLE:
        return a->u.double_val == b->u.double_val;
    case TAC_STATIC_INIT_LONG_DOUBLE:
        return f128_equal(a->u.long_double_val, b->u.long_double_val);
    case TAC_STATIC_INIT_ZERO:
        return a->u.zero_bytes == b->u.zero_bytes;
    case TAC_STATIC_INIT_STRING:
        if ((a->u.string.val == NULL) != (b->u.string.val == NULL))
            return false;
        if (a->u.string.len != b->u.string.len)
            return false;
        // memcmp, not strcmp: the bytes may include embedded NULs.
        if (a->u.string.val && memcmp(a->u.string.val, b->u.string.val, a->u.string.len) != 0)
            return false;
        return a->u.string.null_terminated == b->u.string.null_terminated;
    case TAC_STATIC_INIT_POINTER:
        if ((a->u.pointer.name == NULL) != (b->u.pointer.name == NULL))
            return false;
        if (a->u.pointer.name && strcmp(a->u.pointer.name, b->u.pointer.name) != 0)
            return false;
        if (a->u.pointer.byte_offset != b->u.pointer.byte_offset)
            return false;
        break;
    case TAC_STATIC_INIT_FAT_POINTER:
        if ((a->u.pointer.name == NULL) != (b->u.pointer.name == NULL))
            return false;
        if (a->u.pointer.name && strcmp(a->u.pointer.name, b->u.pointer.name) != 0)
            return false;
        if (a->u.pointer.byte_offset != b->u.pointer.byte_offset)
            return false;
        break;
    }
    return tac_compare_static_init(a->next, b->next);
}

// Compare two Tac_Instruction structures recursively
bool tac_compare_instruction(const Tac_Instruction *a, const Tac_Instruction *b)
{
    if (a == b)
        return true;
    if (!a || !b)
        return false;
    if (a->kind != b->kind)
        return false;
    if (a->is_volatile != b->is_volatile)
        return false;
    switch (a->kind) {
    case TAC_INSTRUCTION_RETURN:
        return tac_compare_val(a->u.return_.src, b->u.return_.src);
    case TAC_INSTRUCTION_SIGN_EXTEND:
    case TAC_INSTRUCTION_TRUNCATE:
    case TAC_INSTRUCTION_ZERO_EXTEND:
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
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_UINT:
    case TAC_INSTRUCTION_INT_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_UINT_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_DOUBLE:
    case TAC_INSTRUCTION_DOUBLE_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_FLOAT:
    case TAC_INSTRUCTION_FLOAT_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_PTR_TO_CHAR_PTR:
    case TAC_INSTRUCTION_CHAR_PTR_TO_PTR:
        return tac_compare_val(a->u.sign_extend.src, b->u.sign_extend.src) &&
               tac_compare_val(a->u.sign_extend.dst, b->u.sign_extend.dst);
    case TAC_INSTRUCTION_UNARY:
        return a->u.unary.op == b->u.unary.op && tac_compare_val(a->u.unary.src, b->u.unary.src) &&
               tac_compare_val(a->u.unary.dst, b->u.unary.dst);
    case TAC_INSTRUCTION_BINARY:
        return a->u.binary.op == b->u.binary.op &&
               tac_compare_val(a->u.binary.src1, b->u.binary.src1) &&
               tac_compare_val(a->u.binary.src2, b->u.binary.src2) &&
               tac_compare_val(a->u.binary.dst, b->u.binary.dst);
    case TAC_INSTRUCTION_COPY:
        return tac_compare_val(a->u.copy.src, b->u.copy.src) &&
               tac_compare_val(a->u.copy.dst, b->u.copy.dst);
    case TAC_INSTRUCTION_GET_ADDRESS:
    case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
    case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
        return tac_compare_val(a->u.get_address.src, b->u.get_address.src) &&
               tac_compare_val(a->u.get_address.dst, b->u.get_address.dst);
    case TAC_INSTRUCTION_LOAD:
    case TAC_INSTRUCTION_LOAD_BYTE:
        return tac_compare_val(a->u.load.src_ptr, b->u.load.src_ptr) &&
               tac_compare_val(a->u.load.dst, b->u.load.dst);
    case TAC_INSTRUCTION_STORE:
    case TAC_INSTRUCTION_STORE_BYTE:
        return tac_compare_val(a->u.store.src, b->u.store.src) &&
               tac_compare_val(a->u.store.dst_ptr, b->u.store.dst_ptr);
    case TAC_INSTRUCTION_ADD_PTR:
        return tac_compare_val(a->u.add_ptr.ptr, b->u.add_ptr.ptr) &&
               tac_compare_val(a->u.add_ptr.index, b->u.add_ptr.index) &&
               a->u.add_ptr.scale == b->u.add_ptr.scale &&
               tac_compare_val(a->u.add_ptr.dst, b->u.add_ptr.dst);
    case TAC_INSTRUCTION_PTR_DIFF:
        return tac_compare_val(a->u.ptr_diff.ptr_a, b->u.ptr_diff.ptr_a) &&
               tac_compare_val(a->u.ptr_diff.ptr_b, b->u.ptr_diff.ptr_b) &&
               tac_compare_val(a->u.ptr_diff.dst, b->u.ptr_diff.dst);
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET:
        if ((a->u.copy_to_offset.dst == NULL) != (b->u.copy_to_offset.dst == NULL))
            return false;
        if (a->u.copy_to_offset.dst &&
            strcmp(a->u.copy_to_offset.dst, b->u.copy_to_offset.dst) != 0)
            return false;
        return tac_compare_val(a->u.copy_to_offset.src, b->u.copy_to_offset.src) &&
               a->u.copy_to_offset.offset == b->u.copy_to_offset.offset;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
        if ((a->u.copy_from_offset.src == NULL) != (b->u.copy_from_offset.src == NULL))
            return false;
        if (a->u.copy_from_offset.src &&
            strcmp(a->u.copy_from_offset.src, b->u.copy_from_offset.src) != 0)
            return false;
        return a->u.copy_from_offset.offset == b->u.copy_from_offset.offset &&
               tac_compare_val(a->u.copy_from_offset.dst, b->u.copy_from_offset.dst);
    case TAC_INSTRUCTION_JUMP:
        if ((a->u.jump.target == NULL) != (b->u.jump.target == NULL))
            return false;
        if (a->u.jump.target && strcmp(a->u.jump.target, b->u.jump.target) != 0)
            return false;
        return true;
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
        if ((a->u.jump_if_zero.target == NULL) != (b->u.jump_if_zero.target == NULL))
            return false;
        if (a->u.jump_if_zero.target &&
            strcmp(a->u.jump_if_zero.target, b->u.jump_if_zero.target) != 0)
            return false;
        return tac_compare_val(a->u.jump_if_zero.condition, b->u.jump_if_zero.condition);
    case TAC_INSTRUCTION_JUMP_TABLE:
        if (a->u.jump_table.count != b->u.jump_table.count ||
            strcmp(a->u.jump_table.default_target, b->u.jump_table.default_target) != 0)
            return false;
        for (int i = 0; i < a->u.jump_table.count; i++)
            if (strcmp(a->u.jump_table.targets[i], b->u.jump_table.targets[i]) != 0)
                return false;
        return tac_compare_val(a->u.jump_table.index, b->u.jump_table.index);
    case TAC_INSTRUCTION_LABEL:
        if ((a->u.label.name == NULL) != (b->u.label.name == NULL))
            return false;
        if (a->u.label.name && strcmp(a->u.label.name, b->u.label.name) != 0)
            return false;
        return true;
    case TAC_INSTRUCTION_FUN_CALL:
    case TAC_INSTRUCTION_FUN_CALL_NORETURN:
        if ((a->u.fun_call.fun_name == NULL) != (b->u.fun_call.fun_name == NULL))
            return false;
        if (a->u.fun_call.fun_name && strcmp(a->u.fun_call.fun_name, b->u.fun_call.fun_name) != 0)
            return false;
        if (a->u.fun_call.indirect != b->u.fun_call.indirect)
            return false;
        return tac_compare_val(a->u.fun_call.args, b->u.fun_call.args) &&
               tac_compare_val(a->u.fun_call.dst, b->u.fun_call.dst) &&
               tac_compare_type(a->u.fun_call.fun_type, b->u.fun_call.fun_type);
    case TAC_INSTRUCTION_ALLOCATE_LOCAL:
        if ((a->u.allocate_local.name == NULL) != (b->u.allocate_local.name == NULL))
            return false;
        if (a->u.allocate_local.name &&
            strcmp(a->u.allocate_local.name, b->u.allocate_local.name) != 0)
            return false;
        if (a->u.allocate_local.size != b->u.allocate_local.size)
            return false;
        if (a->u.allocate_local.alignment != b->u.allocate_local.alignment)
            return false;
        break;
    }
    return tac_compare_instruction(a->next, b->next);
}

// Compare two Tac_TopLevel structures recursively
// Compare one toplevel, not the ones after it.
static bool tac_compare_one_toplevel(const Tac_TopLevel *a, const Tac_TopLevel *b)
{
    if (a->kind != b->kind)
        return false;
    switch (a->kind) {
    case TAC_TOPLEVEL_FUNCTION:
        if ((a->u.function.name == NULL) != (b->u.function.name == NULL))
            return false;
        if (a->u.function.name && strcmp(a->u.function.name, b->u.function.name) != 0)
            return false;
        if (a->u.function.global != b->u.function.global)
            return false;
        if (a->u.function.variadic != b->u.function.variadic)
            return false;
        if (a->u.function.noret != b->u.function.noret)
            return false;
        return tac_compare_type(a->u.function.type, b->u.function.type) &&
               tac_compare_param(a->u.function.params, b->u.function.params) &&
               tac_compare_param(a->u.function.locals, b->u.function.locals) &&
               tac_compare_static_local(a->u.function.static_locals, b->u.function.static_locals) &&
               tac_compare_instruction(a->u.function.body, b->u.function.body);
    case TAC_TOPLEVEL_STATIC_VARIABLE:
        if ((a->u.static_variable.name == NULL) != (b->u.static_variable.name == NULL))
            return false;
        if (a->u.static_variable.name &&
            strcmp(a->u.static_variable.name, b->u.static_variable.name) != 0)
            return false;
        if (a->u.static_variable.global != b->u.static_variable.global ||
            a->u.static_variable.alignment != b->u.static_variable.alignment)
            return false;
        return tac_compare_type(a->u.static_variable.type, b->u.static_variable.type) &&
               tac_compare_static_init(a->u.static_variable.init_list,
                                       b->u.static_variable.init_list);
    case TAC_TOPLEVEL_STATIC_CONSTANT:
        if ((a->u.static_constant.name == NULL) != (b->u.static_constant.name == NULL))
            return false;
        if (a->u.static_constant.name &&
            strcmp(a->u.static_constant.name, b->u.static_constant.name) != 0)
            return false;
        return tac_compare_type(a->u.static_constant.type, b->u.static_constant.type) &&
               tac_compare_static_init(a->u.static_constant.init, b->u.static_constant.init);
    case TAC_TOPLEVEL_EXTERN:
        if ((a->u.extern_.name == NULL) != (b->u.extern_.name == NULL))
            return false;
        if (a->u.extern_.name && strcmp(a->u.extern_.name, b->u.extern_.name) != 0)
            return false;
        return tac_compare_type(a->u.extern_.type, b->u.extern_.type);
    }
    return false;
}

// Compare two TopLevel lists.
bool tac_compare_toplevel(const Tac_TopLevel *a, const Tac_TopLevel *b)
{
    for (; a && b; a = a->next, b = b->next)
        if (a != b && !tac_compare_one_toplevel(a, b))
            return false;
    return a == b;
}

bool tac_compare_program(const Tac_Program *a, const Tac_Program *b)
{
    if (!a && !b)
        return true;
    if (!a || !b)
        return false;
    return tac_compare_toplevel(a->decls, b->decls);
}
