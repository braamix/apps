#include <inttypes.h>
#include <stdio.h>

#include "tac.h"
#include "xalloc.h"

static int node_id = 0;

static int gen_node_id()
{
    return node_id++;
}

static void emit_string(FILE *fd, const char *str)
{
    if (!str)
        return;
    while (*str) {
        if (*str == '"' || *str == '\\')
            fputc('\\', fd);
        fputc(*str, fd);
        str++;
    }
}

static const char *unary_op_name(Tac_UnaryOperator op)
{
    switch (op) {
    case TAC_UNARY_COMPLEMENT:
        return "complement";
    case TAC_UNARY_COMPLEMENT_UNSIGNED:
        return "complement_unsigned";
    case TAC_UNARY_NEGATE:
        return "negate";
    case TAC_UNARY_NOT:
        return "not";
    case TAC_UNARY_NEGATE_UNSIGNED:
        return "negate_unsigned";
    case TAC_UNARY_NEGATE_DOUBLE:
        return "negate_double";
    case TAC_UNARY_SQRT_DOUBLE:
        return "sqrt_double";
    }
    return "?";
}

static const char *binary_op_name(Tac_BinaryOperator op)
{
    switch (op) {
    case TAC_BINARY_ADD:
        return "add";
    case TAC_BINARY_SUBTRACT:
        return "subtract";
    case TAC_BINARY_MULTIPLY:
        return "multiply";
    case TAC_BINARY_DIVIDE:
        return "divide";
    case TAC_BINARY_REMAINDER:
        return "remainder";
    case TAC_BINARY_EQUAL:
        return "equal";
    case TAC_BINARY_NOT_EQUAL:
        return "not_equal";
    case TAC_BINARY_LESS_THAN:
        return "less_than";
    case TAC_BINARY_LESS_OR_EQUAL:
        return "less_or_equal";
    case TAC_BINARY_GREATER_THAN:
        return "greater_than";
    case TAC_BINARY_GREATER_OR_EQUAL:
        return "greater_or_equal";
    case TAC_BINARY_BITWISE_AND:
        return "bitwise_and";
    case TAC_BINARY_BITWISE_OR:
        return "bitwise_or";
    case TAC_BINARY_BITWISE_XOR:
        return "bitwise_xor";
    case TAC_BINARY_LEFT_SHIFT:
        return "left_shift";
    case TAC_BINARY_RIGHT_SHIFT:
        return "right_shift";
    case TAC_BINARY_DIVIDE_UNSIGNED:
        return "divide_unsigned";
    case TAC_BINARY_REMAINDER_UNSIGNED:
        return "remainder_unsigned";
    case TAC_BINARY_LESS_THAN_UNSIGNED:
        return "less_than_unsigned";
    case TAC_BINARY_LESS_OR_EQUAL_UNSIGNED:
        return "less_or_equal_unsigned";
    case TAC_BINARY_GREATER_THAN_UNSIGNED:
        return "greater_than_unsigned";
    case TAC_BINARY_GREATER_OR_EQUAL_UNSIGNED:
        return "greater_or_equal_unsigned";
    case TAC_BINARY_RIGHT_SHIFT_LOGICAL:
        return "right_shift_logical";
    case TAC_BINARY_ADD_UNSIGNED:
        return "add_unsigned";
    case TAC_BINARY_SUBTRACT_UNSIGNED:
        return "subtract_unsigned";
    case TAC_BINARY_MULTIPLY_UNSIGNED:
        return "multiply_unsigned";
    case TAC_BINARY_ADD_DOUBLE:
        return "add_double";
    case TAC_BINARY_SUBTRACT_DOUBLE:
        return "subtract_double";
    case TAC_BINARY_MULTIPLY_DOUBLE:
        return "multiply_double";
    case TAC_BINARY_DIVIDE_DOUBLE:
        return "divide_double";
    case TAC_BINARY_LESS_THAN_DOUBLE:
        return "less_than_double";
    case TAC_BINARY_LESS_OR_EQUAL_DOUBLE:
        return "less_or_equal_double";
    case TAC_BINARY_GREATER_THAN_DOUBLE:
        return "greater_than_double";
    case TAC_BINARY_GREATER_OR_EQUAL_DOUBLE:
        return "greater_or_equal_double";
    }
    return "?";
}

static void emit_const(FILE *fd, const Tac_Const *c, int parent_id)
{
    int id = gen_node_id();
    fprintf(fd, "  n%d [label=\"Const: ", id);
    switch (c->kind) {
    case TAC_CONST_INT:
        fprintf(fd, "int %" PRId64, c->u.int_val);
        break;
    case TAC_CONST_LONG:
        fprintf(fd, "long %ld", c->u.long_val);
        break;
    case TAC_CONST_LONG_LONG:
        fprintf(fd, "long_long %lld", c->u.long_long_val);
        break;
    case TAC_CONST_UINT:
        fprintf(fd, "uint %" PRIu64, c->u.uint_val);
        break;
    case TAC_CONST_ULONG:
        fprintf(fd, "ulong %lu", c->u.ulong_val);
        break;
    case TAC_CONST_ULONG_LONG:
        fprintf(fd, "ulong_long %llu", c->u.ulong_long_val);
        break;
    case TAC_CONST_FLOAT: {
        char hex[64];
        tac_format_hex_double(hex, sizeof hex, (double)c->u.float_val);
        fprintf(fd, "float %s", hex);
        break;
    }
    case TAC_CONST_DOUBLE: {
        char hex[64];
        tac_format_hex_double(hex, sizeof hex, c->u.double_val);
        fprintf(fd, "double %s", hex);
        break;
    }
    case TAC_CONST_LONG_DOUBLE: {
        char buf[F128_BUFSIZE];
        fprintf(fd, "long_double %s", f128_format(c->u.long_double_val, buf));
        break;
    }
    case TAC_CONST_SCHAR:
        fprintf(fd, "char %d", c->u.char_val);
        break;
    case TAC_CONST_UCHAR:
        fprintf(fd, "uchar %u", (unsigned)c->u.uchar_val);
        break;
    }
    fprintf(fd, "\", shape=oval];\n");
    fprintf(fd, "  n%d -> n%d [label=\"const\"];\n", parent_id, id);
}

static void emit_val(FILE *fd, const Tac_Val *val, int parent_id, const char *edge_label)
{
    if (!val)
        return;
    int id = gen_node_id();
    if (val->kind == TAC_VAL_CONSTANT) {
        fprintf(fd, "  n%d [label=\"Val: constant\", shape=oval];\n", id);
        fprintf(fd, "  n%d -> n%d [label=\"%s\"];\n", parent_id, id, edge_label);
        emit_const(fd, val->u.constant, id);
    } else {
        fprintf(fd, "  n%d [label=\"Val: var ", id);
        emit_string(fd, val->u.var_name);
        fprintf(fd, "\", shape=oval];\n");
        fprintf(fd, "  n%d -> n%d [label=\"%s\"];\n", parent_id, id, edge_label);
    }
}

static void emit_type(FILE *fd, const Tac_Type *type, int parent_id, const char *edge_label)
{
    if (!type)
        return;
    int id = gen_node_id();
    fprintf(fd, "  n%d [label=\"Type: ", id);
    switch (type->kind) {
    case TAC_TYPE_SCHAR:
        fprintf(fd, "schar");
        break;
    case TAC_TYPE_UCHAR:
        fprintf(fd, "uchar");
        break;
    case TAC_TYPE_SHORT:
        fprintf(fd, "short");
        break;
    case TAC_TYPE_INT:
        fprintf(fd, "int");
        break;
    case TAC_TYPE_LONG:
        fprintf(fd, "long");
        break;
    case TAC_TYPE_LONG_LONG:
        fprintf(fd, "long_long");
        break;
    case TAC_TYPE_USHORT:
        fprintf(fd, "ushort");
        break;
    case TAC_TYPE_UINT:
        fprintf(fd, "uint");
        break;
    case TAC_TYPE_ULONG:
        fprintf(fd, "ulong");
        break;
    case TAC_TYPE_ULONG_LONG:
        fprintf(fd, "ulong_long");
        break;
    case TAC_TYPE_FLOAT:
        fprintf(fd, "float");
        break;
    case TAC_TYPE_DOUBLE:
        fprintf(fd, "double");
        break;
    case TAC_TYPE_LONG_DOUBLE:
        fprintf(fd, "long_double");
        break;
    case TAC_TYPE_VOID:
        fprintf(fd, "void");
        break;
    case TAC_TYPE_FUN_TYPE:
        fprintf(fd, "fun_type");
        break;
    case TAC_TYPE_POINTER:
        fprintf(fd, "pointer");
        break;
    case TAC_TYPE_ARRAY:
        fprintf(fd, "array[%d]", type->u.array.size);
        break;
    case TAC_TYPE_STRUCTURE:
        fprintf(fd, "struct ");
        emit_string(fd, type->u.structure.tag);
        break;
    }
    fprintf(fd, "\", shape=oval];\n");
    fprintf(fd, "  n%d -> n%d [label=\"%s\"];\n", parent_id, id, edge_label);
    switch (type->kind) {
    case TAC_TYPE_FUN_TYPE:
        emit_type(fd, type->u.fun_type.ret_type, id, "ret_type");
        for (const Tac_Type *pt = type->u.fun_type.param_types; pt; pt = pt->next)
            emit_type(fd, pt, id, "param_type");
        break;
    case TAC_TYPE_POINTER:
        emit_type(fd, type->u.pointer.target_type, id, "target");
        break;
    case TAC_TYPE_ARRAY:
        emit_type(fd, type->u.array.elem_type, id, "elem_type");
        break;
    default:
        break;
    }
}

static void emit_static_init(FILE *fd, const Tac_StaticInit *init, int parent_id)
{
    while (init) {
        int id = gen_node_id();
        fprintf(fd, "  n%d [label=\"StaticInit: ", id);
        switch (init->kind) {
        case TAC_STATIC_INIT_I8:
            fprintf(fd, "i8 %d", (int)init->u.char_val);
            break;
        case TAC_STATIC_INIT_I16:
            fprintf(fd, "i16 %" PRId16, init->u.short_val);
            break;
        case TAC_STATIC_INIT_I32:
            fprintf(fd, "i32 %" PRId32, init->u.int_val);
            break;
        case TAC_STATIC_INIT_I64:
            fprintf(fd, "i64 %" PRId64, init->u.long_val);
            break;
        case TAC_STATIC_INIT_U8:
            fprintf(fd, "u8 %u", (unsigned)init->u.uchar_val);
            break;
        case TAC_STATIC_INIT_U16:
            fprintf(fd, "u16 %" PRIu16, init->u.ushort_val);
            break;
        case TAC_STATIC_INIT_U32:
            fprintf(fd, "u32 %" PRIu32, init->u.uint_val);
            break;
        case TAC_STATIC_INIT_U64:
            fprintf(fd, "u64 %" PRIu64, init->u.ulong_val);
            break;
        case TAC_STATIC_INIT_FLOAT: {
            char hex[64];
            tac_format_hex_double(hex, sizeof hex, (double)init->u.float_val);
            fprintf(fd, "float %s", hex);
            break;
        }
        case TAC_STATIC_INIT_DOUBLE: {
            char hex[64];
            tac_format_hex_double(hex, sizeof hex, init->u.double_val);
            fprintf(fd, "double %s", hex);
            break;
        }
        case TAC_STATIC_INIT_LONG_DOUBLE: {
            char buf[F128_BUFSIZE];
            fprintf(fd, "long_double %s", f128_format(init->u.long_double_val, buf));
            break;
        }
        case TAC_STATIC_INIT_ZERO:
            fprintf(fd, "zero %d", init->u.zero_bytes);
            break;
        case TAC_STATIC_INIT_STRING: {
            fprintf(fd, "string ");
            // C-escape the bytes first (they may include NULs), then let emit_string
            // escape that text for the DOT label.
            char *text = tac_escape_string_bytes(init->u.string.val, init->u.string.len);
            emit_string(fd, text);
            xfree(text);
            break;
        }
        case TAC_STATIC_INIT_POINTER:
            if (init->u.pointer.byte_offset)
                fprintf(fd, "pointer offset=%d ", init->u.pointer.byte_offset);
            else
                fprintf(fd, "pointer ");
            emit_string(fd, init->u.pointer.name);
            break;
        case TAC_STATIC_INIT_FAT_POINTER:
            fprintf(fd, "fat_pointer offset=%d ", init->u.pointer.byte_offset);
            emit_string(fd, init->u.pointer.name);
            break;
        }
        fprintf(fd, "\", shape=box];\n");
        fprintf(fd, "  n%d -> n%d [label=\"init\"];\n", parent_id, id);
        init = init->next;
    }
}

static void emit_instruction(FILE *fd, const Tac_Instruction *instr, int parent_id)
{
    int id = gen_node_id();
    fprintf(fd, "  n%d [label=\"", id);
    switch (instr->kind) {
    case TAC_INSTRUCTION_RETURN:
        fprintf(fd, "Return");
        break;
    case TAC_INSTRUCTION_SIGN_EXTEND:
        fprintf(fd, "SignExtend");
        break;
    case TAC_INSTRUCTION_TRUNCATE:
        fprintf(fd, "Truncate");
        break;
    case TAC_INSTRUCTION_ZERO_EXTEND:
        fprintf(fd, "ZeroExtend");
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_INT:
        fprintf(fd, "DoubleToInt");
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_UINT:
        fprintf(fd, "DoubleToUInt");
        break;
    case TAC_INSTRUCTION_INT_TO_DOUBLE:
        fprintf(fd, "IntToDouble");
        break;
    case TAC_INSTRUCTION_UINT_TO_DOUBLE:
        fprintf(fd, "UIntToDouble");
        break;
    case TAC_INSTRUCTION_FLOAT_TO_DOUBLE:
        fprintf(fd, "FloatToDouble");
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_FLOAT:
        fprintf(fd, "DoubleToFloat");
        break;
    case TAC_INSTRUCTION_INT_TO_FLOAT:
        fprintf(fd, "IntToFloat");
        break;
    case TAC_INSTRUCTION_UINT_TO_FLOAT:
        fprintf(fd, "UIntToFloat");
        break;
    case TAC_INSTRUCTION_FLOAT_TO_INT:
        fprintf(fd, "FloatToInt");
        break;
    case TAC_INSTRUCTION_FLOAT_TO_UINT:
        fprintf(fd, "FloatToUInt");
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_INT:
        fprintf(fd, "LongDoubleToInt");
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_UINT:
        fprintf(fd, "LongDoubleToUInt");
        break;
    case TAC_INSTRUCTION_INT_TO_LONG_DOUBLE:
        fprintf(fd, "IntToLongDouble");
        break;
    case TAC_INSTRUCTION_UINT_TO_LONG_DOUBLE:
        fprintf(fd, "UIntToLongDouble");
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_DOUBLE:
        fprintf(fd, "LongDoubleToDouble");
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_LONG_DOUBLE:
        fprintf(fd, "DoubleToLongDouble");
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_FLOAT:
        fprintf(fd, "LongDoubleToFloat");
        break;
    case TAC_INSTRUCTION_FLOAT_TO_LONG_DOUBLE:
        fprintf(fd, "FloatToLongDouble");
        break;
    case TAC_INSTRUCTION_PTR_TO_CHAR_PTR:
        fprintf(fd, "PtrToCharPtr");
        break;
    case TAC_INSTRUCTION_CHAR_PTR_TO_PTR:
        fprintf(fd, "CharPtrToPtr");
        break;
    case TAC_INSTRUCTION_UNARY:
        fprintf(fd, "Unary: %s", unary_op_name(instr->u.unary.op));
        break;
    case TAC_INSTRUCTION_BINARY:
        fprintf(fd, "Binary: %s", binary_op_name(instr->u.binary.op));
        break;
    case TAC_INSTRUCTION_COPY:
        fprintf(fd, "Copy");
        break;
    case TAC_INSTRUCTION_GET_ADDRESS:
        fprintf(fd, "GetAddress");
        break;
    case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
        fprintf(fd, "GetAddressByte");
        break;
    case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
        fprintf(fd, "GetAddressDecay");
        break;
    case TAC_INSTRUCTION_LOAD:
        fprintf(fd, "Load");
        break;
    case TAC_INSTRUCTION_LOAD_BYTE:
        fprintf(fd, "LoadByte");
        break;
    case TAC_INSTRUCTION_STORE:
        fprintf(fd, "Store");
        break;
    case TAC_INSTRUCTION_STORE_BYTE:
        fprintf(fd, "StoreByte");
        break;
    case TAC_INSTRUCTION_ADD_PTR:
        fprintf(fd, "AddPtr scale=%d", instr->u.add_ptr.scale);
        break;
    case TAC_INSTRUCTION_PTR_DIFF:
        fprintf(fd, "PtrDiff");
        break;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
        fprintf(fd, "CopyToOffset offset=%d dst=", instr->u.copy_to_offset.offset);
        emit_string(fd, instr->u.copy_to_offset.dst);
        break;
    case TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET:
        fprintf(fd, "CopyByteToOffset offset=%d dst=", instr->u.copy_to_offset.offset);
        emit_string(fd, instr->u.copy_to_offset.dst);
        break;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
        fprintf(fd, "CopyFromOffset src=");
        emit_string(fd, instr->u.copy_from_offset.src);
        fprintf(fd, " offset=%d", instr->u.copy_from_offset.offset);
        break;
    case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
        fprintf(fd, "CopyByteFromOffset src=");
        emit_string(fd, instr->u.copy_from_offset.src);
        fprintf(fd, " offset=%d", instr->u.copy_from_offset.offset);
        break;
    case TAC_INSTRUCTION_JUMP:
        fprintf(fd, "Jump: ");
        emit_string(fd, instr->u.jump.target);
        break;
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
        fprintf(fd, "JumpIfZero: ");
        emit_string(fd, instr->u.jump_if_zero.target);
        break;
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
        fprintf(fd, "JumpIfNotZero: ");
        emit_string(fd, instr->u.jump_if_not_zero.target);
        break;
    case TAC_INSTRUCTION_JUMP_TABLE:
        fprintf(fd, "JumpTable: %d targets, default ", instr->u.jump_table.count);
        emit_string(fd, instr->u.jump_table.default_target);
        break;
    case TAC_INSTRUCTION_LABEL:
        fprintf(fd, "Label: ");
        emit_string(fd, instr->u.label.name);
        break;
    case TAC_INSTRUCTION_FUN_CALL:
    case TAC_INSTRUCTION_FUN_CALL_NORETURN:
        fprintf(fd, "%s: ",
                instr->kind == TAC_INSTRUCTION_FUN_CALL_NORETURN ? "FunCallNoreturn" : "FunCall");
        emit_string(fd, instr->u.fun_call.fun_name);
        if (instr->u.fun_call.indirect)
            fprintf(fd, " (indirect)");
        break;
    case TAC_INSTRUCTION_ALLOCATE_LOCAL:
        fprintf(fd, "AllocateLocal: ");
        emit_string(fd, instr->u.allocate_local.name);
        fprintf(fd, " size=%d align=%d", instr->u.allocate_local.size,
                instr->u.allocate_local.alignment);
        break;
    }
    fprintf(fd, "\", shape=box];\n");
    fprintf(fd, "  n%d -> n%d [label=\"instr\"];\n", parent_id, id);
    switch (instr->kind) {
    case TAC_INSTRUCTION_RETURN:
        if (instr->u.return_.src)
            emit_val(fd, instr->u.return_.src, id, "src");
        break;
    case TAC_INSTRUCTION_SIGN_EXTEND:
        emit_val(fd, instr->u.sign_extend.src, id, "src");
        emit_val(fd, instr->u.sign_extend.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_TRUNCATE:
        emit_val(fd, instr->u.truncate.src, id, "src");
        emit_val(fd, instr->u.truncate.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_ZERO_EXTEND:
        emit_val(fd, instr->u.zero_extend.src, id, "src");
        emit_val(fd, instr->u.zero_extend.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_INT:
        emit_val(fd, instr->u.double_to_int.src, id, "src");
        emit_val(fd, instr->u.double_to_int.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_UINT:
        emit_val(fd, instr->u.double_to_uint.src, id, "src");
        emit_val(fd, instr->u.double_to_uint.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_INT_TO_DOUBLE:
        emit_val(fd, instr->u.int_to_double.src, id, "src");
        emit_val(fd, instr->u.int_to_double.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_UINT_TO_DOUBLE:
        emit_val(fd, instr->u.uint_to_double.src, id, "src");
        emit_val(fd, instr->u.uint_to_double.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_FLOAT_TO_DOUBLE:
        emit_val(fd, instr->u.float_to_double.src, id, "src");
        emit_val(fd, instr->u.float_to_double.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_FLOAT:
        emit_val(fd, instr->u.double_to_float.src, id, "src");
        emit_val(fd, instr->u.double_to_float.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_INT_TO_FLOAT:
        emit_val(fd, instr->u.int_to_float.src, id, "src");
        emit_val(fd, instr->u.int_to_float.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_UINT_TO_FLOAT:
        emit_val(fd, instr->u.uint_to_float.src, id, "src");
        emit_val(fd, instr->u.uint_to_float.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_FLOAT_TO_INT:
        emit_val(fd, instr->u.float_to_int.src, id, "src");
        emit_val(fd, instr->u.float_to_int.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_FLOAT_TO_UINT:
        emit_val(fd, instr->u.float_to_uint.src, id, "src");
        emit_val(fd, instr->u.float_to_uint.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_INT:
        emit_val(fd, instr->u.long_double_to_int.src, id, "src");
        emit_val(fd, instr->u.long_double_to_int.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_UINT:
        emit_val(fd, instr->u.long_double_to_uint.src, id, "src");
        emit_val(fd, instr->u.long_double_to_uint.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_INT_TO_LONG_DOUBLE:
        emit_val(fd, instr->u.int_to_long_double.src, id, "src");
        emit_val(fd, instr->u.int_to_long_double.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_UINT_TO_LONG_DOUBLE:
        emit_val(fd, instr->u.uint_to_long_double.src, id, "src");
        emit_val(fd, instr->u.uint_to_long_double.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_DOUBLE:
        emit_val(fd, instr->u.long_double_to_double.src, id, "src");
        emit_val(fd, instr->u.long_double_to_double.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_LONG_DOUBLE:
        emit_val(fd, instr->u.double_to_long_double.src, id, "src");
        emit_val(fd, instr->u.double_to_long_double.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_FLOAT:
        emit_val(fd, instr->u.long_double_to_float.src, id, "src");
        emit_val(fd, instr->u.long_double_to_float.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_FLOAT_TO_LONG_DOUBLE:
        emit_val(fd, instr->u.float_to_long_double.src, id, "src");
        emit_val(fd, instr->u.float_to_long_double.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_PTR_TO_CHAR_PTR:
        emit_val(fd, instr->u.ptr_to_char_ptr.src, id, "src");
        emit_val(fd, instr->u.ptr_to_char_ptr.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_CHAR_PTR_TO_PTR:
        emit_val(fd, instr->u.char_ptr_to_ptr.src, id, "src");
        emit_val(fd, instr->u.char_ptr_to_ptr.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_UNARY:
        emit_val(fd, instr->u.unary.src, id, "src");
        emit_val(fd, instr->u.unary.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_BINARY:
        emit_val(fd, instr->u.binary.src1, id, "src1");
        emit_val(fd, instr->u.binary.src2, id, "src2");
        emit_val(fd, instr->u.binary.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_COPY:
        emit_val(fd, instr->u.copy.src, id, "src");
        emit_val(fd, instr->u.copy.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_GET_ADDRESS:
    case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
    case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
        emit_val(fd, instr->u.get_address.src, id, "src");
        emit_val(fd, instr->u.get_address.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_LOAD:
    case TAC_INSTRUCTION_LOAD_BYTE:
        emit_val(fd, instr->u.load.src_ptr, id, "src_ptr");
        emit_val(fd, instr->u.load.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_STORE:
    case TAC_INSTRUCTION_STORE_BYTE:
        emit_val(fd, instr->u.store.src, id, "src");
        emit_val(fd, instr->u.store.dst_ptr, id, "dst_ptr");
        break;
    case TAC_INSTRUCTION_ADD_PTR:
        emit_val(fd, instr->u.add_ptr.ptr, id, "ptr");
        emit_val(fd, instr->u.add_ptr.index, id, "index");
        emit_val(fd, instr->u.add_ptr.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_PTR_DIFF:
        emit_val(fd, instr->u.ptr_diff.ptr_a, id, "ptr_a");
        emit_val(fd, instr->u.ptr_diff.ptr_b, id, "ptr_b");
        emit_val(fd, instr->u.ptr_diff.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET:
        emit_val(fd, instr->u.copy_to_offset.src, id, "src");
        break;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
        emit_val(fd, instr->u.copy_from_offset.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_JUMP:
    case TAC_INSTRUCTION_LABEL:
        break;
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
        emit_val(fd, instr->u.jump_if_zero.condition, id, "cond");
        break;
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
        emit_val(fd, instr->u.jump_if_not_zero.condition, id, "cond");
        break;
    case TAC_INSTRUCTION_JUMP_TABLE:
        emit_val(fd, instr->u.jump_table.index, id, "index");
        break;
    case TAC_INSTRUCTION_FUN_CALL:
    case TAC_INSTRUCTION_FUN_CALL_NORETURN:
        for (const Tac_Val *arg = instr->u.fun_call.args; arg; arg = arg->next)
            emit_val(fd, arg, id, "arg");
        if (instr->u.fun_call.dst)
            emit_val(fd, instr->u.fun_call.dst, id, "dst");
        break;
    case TAC_INSTRUCTION_ALLOCATE_LOCAL:
        break; // no Tac_Val operands
    }
}

void tac_export_dot(FILE *fd, const Tac_TopLevel *tl)
{
    if (!tl) {
        fprintf(fd, "digraph TAC { empty [label=\"null\"]; }\n");
        return;
    }
    node_id = 0;
    fprintf(fd, "digraph TAC {\n");
    fprintf(fd, "  graph [margin=\"0,0\", pad=\"0.1\", ranksep=0.3, nodesep=0.2];\n");
    fprintf(fd, "  node [width=0.3, height=0.3, margin=\"0.02,0.01\"];\n");
    int root = gen_node_id();
    switch (tl->kind) {
    case TAC_TOPLEVEL_FUNCTION:
        fprintf(fd, "  n%d [label=\"Function: ", root);
        emit_string(fd, tl->u.function.name);
        fprintf(fd, "%s\", shape=box];\n", tl->u.function.global ? " (global)" : "");
        for (const Tac_Param *p = tl->u.function.params; p; p = p->next) {
            int pid = gen_node_id();
            fprintf(fd, "  n%d [label=\"Param: ", pid);
            emit_string(fd, p->name);
            if (p->type) {
                char *ts = tac_type_str(p->type);
                fprintf(fd, " : ");
                emit_string(fd, ts);
                xfree(ts);
            }
            fprintf(fd, "\", shape=box];\n");
            fprintf(fd, "  n%d -> n%d [label=\"param\"];\n", root, pid);
        }
        for (const Tac_StaticLocal *sl = tl->u.function.static_locals; sl; sl = sl->next) {
            int sid = gen_node_id();
            fprintf(fd, "  n%d [label=\"StaticLocal: ", sid);
            emit_string(fd, sl->name);
            fprintf(fd, "\", shape=box];\n");
            fprintf(fd, "  n%d -> n%d [label=\"static_local\"];\n", root, sid);
            emit_type(fd, sl->type, sid, "type");
            emit_static_init(fd, sl->init_list, sid);
        }
        for (const Tac_Instruction *in = tl->u.function.body; in; in = in->next)
            emit_instruction(fd, in, root);
        break;
    case TAC_TOPLEVEL_STATIC_VARIABLE:
        fprintf(fd, "  n%d [label=\"StaticVariable: ", root);
        emit_string(fd, tl->u.static_variable.name);
        fprintf(fd, "%s\", shape=box];\n", tl->u.static_variable.global ? " (global)" : "");
        emit_type(fd, tl->u.static_variable.type, root, "type");
        emit_static_init(fd, tl->u.static_variable.init_list, root);
        break;
    case TAC_TOPLEVEL_STATIC_CONSTANT:
        fprintf(fd, "  n%d [label=\"StaticConstant: ", root);
        emit_string(fd, tl->u.static_constant.name);
        fprintf(fd, "\", shape=box];\n");
        emit_type(fd, tl->u.static_constant.type, root, "type");
        if (tl->u.static_constant.init)
            emit_static_init(fd, tl->u.static_constant.init, root);
        break;
    case TAC_TOPLEVEL_EXTERN:
        fprintf(fd, "  n%d [label=\"Extern: ", root);
        emit_string(fd, tl->u.extern_.name);
        fprintf(fd, "\", shape=box];\n");
        emit_type(fd, tl->u.extern_.type, root, "type");
        break;
    }
    fprintf(fd, "}\n");
}
