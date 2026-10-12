#include <inttypes.h>
#include <stdio.h>

#include "tac.h"
#include "xalloc.h"

#define INDENT_STEP 2

static void print_indent(FILE *fd, int level)
{
    for (int i = 0; i < level * INDENT_STEP; i++) {
        fputc(' ', fd);
    }
}

static void export_yaml_const(FILE *fd, const Tac_Const *c, int level)
{
    print_indent(fd, level);
    fprintf(fd, "kind: ");
    switch (c->kind) {
    case TAC_CONST_INT:
        fprintf(fd, "int\n");
        print_indent(fd, level);
        fprintf(fd, "value: %" PRId64 "\n", c->u.int_val);
        break;
    case TAC_CONST_LONG:
        fprintf(fd, "long\n");
        print_indent(fd, level);
        fprintf(fd, "value: %ld\n", c->u.long_val);
        break;
    case TAC_CONST_LONG_LONG:
        fprintf(fd, "long_long\n");
        print_indent(fd, level);
        fprintf(fd, "value: %lld\n", c->u.long_long_val);
        break;
    case TAC_CONST_UINT:
        fprintf(fd, "uint\n");
        print_indent(fd, level);
        fprintf(fd, "value: %" PRIu64 "\n", c->u.uint_val);
        break;
    case TAC_CONST_ULONG:
        fprintf(fd, "ulong\n");
        print_indent(fd, level);
        fprintf(fd, "value: %lu\n", c->u.ulong_val);
        break;
    case TAC_CONST_ULONG_LONG:
        fprintf(fd, "ulong_long\n");
        print_indent(fd, level);
        fprintf(fd, "value: %llu\n", c->u.ulong_long_val);
        break;
    case TAC_CONST_FLOAT: {
        fprintf(fd, "float\n");
        print_indent(fd, level);
        char hex[64];
        tac_format_hex_double(hex, sizeof hex, (double)c->u.float_val);
        fprintf(fd, "value: %s\n", hex);
        break;
    }
    case TAC_CONST_DOUBLE: {
        fprintf(fd, "double\n");
        print_indent(fd, level);
        char hex[64];
        tac_format_hex_double(hex, sizeof hex, c->u.double_val);
        fprintf(fd, "value: %s\n", hex);
        break;
    }
    case TAC_CONST_LONG_DOUBLE:
        fprintf(fd, "long_double\n");
        print_indent(fd, level);
        {
            char buf[F128_BUFSIZE];
            fprintf(fd, "value: %s\n", f128_format(c->u.long_double_val, buf));
            break;
        }
    case TAC_CONST_SCHAR:
        fprintf(fd, "char\n");
        print_indent(fd, level);
        fprintf(fd, "value: %d\n", c->u.char_val);
        break;
    case TAC_CONST_UCHAR:
        fprintf(fd, "uchar\n");
        print_indent(fd, level);
        fprintf(fd, "value: %u\n", (unsigned)c->u.uchar_val);
        break;
    }
}

static void export_yaml_val(FILE *fd, const Tac_Val *val, int level)
{
    if (val->kind == TAC_VAL_CONSTANT) {
        print_indent(fd, level);
        fprintf(fd, "kind: constant\n");
        print_indent(fd, level);
        fprintf(fd, "const:\n");
        export_yaml_const(fd, val->u.constant, level + 1);
    } else {
        print_indent(fd, level);
        fprintf(fd, "kind: var\n");
        print_indent(fd, level);
        fprintf(fd, "name: %s\n", val->u.var_name ? val->u.var_name : "");
    }
}

static void export_yaml_val_list(FILE *fd, const Tac_Val *val, int level)
{
    while (val) {
        print_indent(fd, level);
        fprintf(fd, "- val:\n");
        export_yaml_val(fd, val, level + 1);
        val = val->next;
    }
}

static void export_yaml_type(FILE *fd, const Tac_Type *type, int level);

static void export_yaml_type_list(FILE *fd, const Tac_Type *type, int level)
{
    while (type) {
        print_indent(fd, level);
        fprintf(fd, "- type:\n");
        export_yaml_type(fd, type, level + 1);
        type = type->next;
    }
}

static void export_yaml_type(FILE *fd, const Tac_Type *type, int level)
{
    print_indent(fd, level);
    fprintf(fd, "kind: ");
    switch (type->kind) {
    case TAC_TYPE_SCHAR:
        fprintf(fd, "schar\n");
        break;
    case TAC_TYPE_UCHAR:
        fprintf(fd, "uchar\n");
        break;
    case TAC_TYPE_SHORT:
        fprintf(fd, "short\n");
        break;
    case TAC_TYPE_INT:
        fprintf(fd, "int\n");
        break;
    case TAC_TYPE_LONG:
        fprintf(fd, "long\n");
        break;
    case TAC_TYPE_LONG_LONG:
        fprintf(fd, "long_long\n");
        break;
    case TAC_TYPE_USHORT:
        fprintf(fd, "ushort\n");
        break;
    case TAC_TYPE_UINT:
        fprintf(fd, "uint\n");
        break;
    case TAC_TYPE_ULONG:
        fprintf(fd, "ulong\n");
        break;
    case TAC_TYPE_ULONG_LONG:
        fprintf(fd, "ulong_long\n");
        break;
    case TAC_TYPE_FLOAT:
        fprintf(fd, "float\n");
        break;
    case TAC_TYPE_DOUBLE:
        fprintf(fd, "double\n");
        break;
    case TAC_TYPE_LONG_DOUBLE:
        fprintf(fd, "long_double\n");
        break;
    case TAC_TYPE_VOID:
        fprintf(fd, "void\n");
        break;
    case TAC_TYPE_FUN_TYPE:
        fprintf(fd, "fun_type\n");
        if (type->u.fun_type.param_types) {
            print_indent(fd, level);
            fprintf(fd, "param_types:\n");
            export_yaml_type_list(fd, type->u.fun_type.param_types, level + 1);
        }
        print_indent(fd, level);
        fprintf(fd, "ret_type:\n");
        export_yaml_type(fd, type->u.fun_type.ret_type, level + 1);
        if (type->u.fun_type.variadic) {
            print_indent(fd, level);
            fprintf(fd, "variadic: true\n");
        }
        break;
    case TAC_TYPE_POINTER:
        fprintf(fd, "pointer\n");
        print_indent(fd, level);
        fprintf(fd, "target:\n");
        export_yaml_type(fd, type->u.pointer.target_type, level + 1);
        break;
    case TAC_TYPE_ARRAY:
        fprintf(fd, "array\n");
        print_indent(fd, level);
        fprintf(fd, "elem_type:\n");
        export_yaml_type(fd, type->u.array.elem_type, level + 1);
        print_indent(fd, level);
        fprintf(fd, "size: %d\n", type->u.array.size);
        break;
    case TAC_TYPE_STRUCTURE:
        fprintf(fd, "structure\n");
        print_indent(fd, level);
        fprintf(fd, "tag: %s\n", type->u.structure.tag ? type->u.structure.tag : "");
        print_indent(fd, level);
        fprintf(fd, "size: %d\n", type->u.structure.size);
        if (!tac_yaml_types)
            break;
        print_indent(fd, level);
        fprintf(fd, "alignment: %d\n", type->u.structure.alignment);
        if (type->u.structure.is_union) {
            print_indent(fd, level);
            fprintf(fd, "union: true\n");
        }
        if (type->u.structure.members) {
            print_indent(fd, level);
            fprintf(fd, "members:\n");
            for (const Tac_Member *m = type->u.structure.members; m; m = m->next) {
                print_indent(fd, level + 1);
                fprintf(fd, "- member: %s\n", m->name ? m->name : "");
                print_indent(fd, level + 2);
                fprintf(fd, "offset: %d\n", m->offset);
                print_indent(fd, level + 2);
                fprintf(fd, "type:\n");
                export_yaml_type(fd, m->type, level + 3);
            }
        }
        break;
    }
}

bool tac_yaml_types = true;

// "<key>: <type>" on one line, in the compact tac_type_str spelling.
static void export_yaml_type_str(FILE *fd, const char *key, const Tac_Type *type, int level)
{
    char *ts = tac_type_str(type);
    print_indent(fd, level);
    fprintf(fd, "%s: %s\n", key, ts);
    xfree(ts);
}

static void export_yaml_param_list(FILE *fd, const char *key, const Tac_Param *param, int level)
{
    while (param) {
        print_indent(fd, level);
        fprintf(fd, "- %s: %s\n", key, param->name ? param->name : "");
        if (tac_yaml_types && param->type)
            export_yaml_type_str(fd, "type", param->type, level + 1);
        param = param->next;
    }
}

static void export_yaml_static_init(FILE *fd, const Tac_StaticInit *init, int level)
{
    print_indent(fd, level);
    fprintf(fd, "kind: ");
    switch (init->kind) {
    case TAC_STATIC_INIT_I8:
        fprintf(fd, "i8\n");
        print_indent(fd, level);
        fprintf(fd, "value: %d\n", (int)init->u.char_val);
        break;
    case TAC_STATIC_INIT_I16:
        fprintf(fd, "i16\n");
        print_indent(fd, level);
        fprintf(fd, "value: %" PRId16 "\n", init->u.short_val);
        break;
    case TAC_STATIC_INIT_I32:
        fprintf(fd, "i32\n");
        print_indent(fd, level);
        fprintf(fd, "value: %" PRId32 "\n", init->u.int_val);
        break;
    case TAC_STATIC_INIT_I64:
        fprintf(fd, "i64\n");
        print_indent(fd, level);
        fprintf(fd, "value: %" PRId64 "\n", init->u.long_val);
        break;
    case TAC_STATIC_INIT_U8:
        fprintf(fd, "u8\n");
        print_indent(fd, level);
        fprintf(fd, "value: %u\n", (unsigned)init->u.uchar_val);
        break;
    case TAC_STATIC_INIT_U16:
        fprintf(fd, "u16\n");
        print_indent(fd, level);
        fprintf(fd, "value: %" PRIu16 "\n", init->u.ushort_val);
        break;
    case TAC_STATIC_INIT_U32:
        fprintf(fd, "u32\n");
        print_indent(fd, level);
        fprintf(fd, "value: %" PRIu32 "\n", init->u.uint_val);
        break;
    case TAC_STATIC_INIT_U64:
        fprintf(fd, "u64\n");
        print_indent(fd, level);
        fprintf(fd, "value: %" PRIu64 "\n", init->u.ulong_val);
        break;
    case TAC_STATIC_INIT_FLOAT: {
        fprintf(fd, "float\n");
        print_indent(fd, level);
        char hex[64];
        tac_format_hex_double(hex, sizeof hex, (double)init->u.float_val);
        fprintf(fd, "value: %s\n", hex);
        break;
    }
    case TAC_STATIC_INIT_DOUBLE: {
        fprintf(fd, "double\n");
        print_indent(fd, level);
        char hex[64];
        tac_format_hex_double(hex, sizeof hex, init->u.double_val);
        fprintf(fd, "value: %s\n", hex);
        break;
    }
    case TAC_STATIC_INIT_LONG_DOUBLE:
        fprintf(fd, "long_double\n");
        print_indent(fd, level);
        {
            char buf[F128_BUFSIZE];
            fprintf(fd, "value: %s\n", f128_format(init->u.long_double_val, buf));
            break;
        }
    case TAC_STATIC_INIT_ZERO:
        fprintf(fd, "zero\n");
        print_indent(fd, level);
        fprintf(fd, "bytes: %d\n", init->u.zero_bytes);
        break;
    case TAC_STATIC_INIT_STRING: {
        fprintf(fd, "string\n");
        print_indent(fd, level);
        char *text = tac_escape_string_bytes(init->u.string.val, init->u.string.len);
        fprintf(fd, "value: %s\n", text);
        xfree(text);
        print_indent(fd, level);
        fprintf(fd, "null_terminated: %s\n", init->u.string.null_terminated ? "true" : "false");
        break;
    }
    case TAC_STATIC_INIT_POINTER:
        fprintf(fd, "pointer\n");
        print_indent(fd, level);
        fprintf(fd, "name: %s\n", init->u.pointer.name ? init->u.pointer.name : "");
        if (init->u.pointer.byte_offset) {
            print_indent(fd, level);
            fprintf(fd, "byte_offset: %d\n", init->u.pointer.byte_offset);
        }
        break;
    case TAC_STATIC_INIT_FAT_POINTER:
        fprintf(fd, "fat_pointer\n");
        print_indent(fd, level);
        fprintf(fd, "name: %s\n", init->u.pointer.name ? init->u.pointer.name : "");
        print_indent(fd, level);
        fprintf(fd, "byte_offset: %d\n", init->u.pointer.byte_offset);
        break;
    }
}

static void export_yaml_static_init_list(FILE *fd, const Tac_StaticInit *init, int level)
{
    while (init) {
        print_indent(fd, level);
        fprintf(fd, "- init:\n");
        export_yaml_static_init(fd, init, level + 1);
        init = init->next;
    }
}

static void export_yaml_instruction(FILE *fd, const Tac_Instruction *instr, int level)
{
    print_indent(fd, level);
    fprintf(fd, "kind: ");
    switch (instr->kind) {
    case TAC_INSTRUCTION_RETURN:
        fprintf(fd, "return\n");
        if (instr->u.return_.src) {
            print_indent(fd, level);
            fprintf(fd, "src:\n");
            export_yaml_val(fd, instr->u.return_.src, level + 1);
        }
        break;
    case TAC_INSTRUCTION_SIGN_EXTEND:
        fprintf(fd, "sign_extend\n");
        print_indent(fd, level);
        fprintf(fd, "src:\n");
        export_yaml_val(fd, instr->u.sign_extend.src, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.sign_extend.dst, level + 1);
        break;
    case TAC_INSTRUCTION_TRUNCATE:
        fprintf(fd, "truncate\n");
        print_indent(fd, level);
        fprintf(fd, "src:\n");
        export_yaml_val(fd, instr->u.truncate.src, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.truncate.dst, level + 1);
        break;
    case TAC_INSTRUCTION_ZERO_EXTEND:
        fprintf(fd, "zero_extend\n");
        print_indent(fd, level);
        fprintf(fd, "src:\n");
        export_yaml_val(fd, instr->u.zero_extend.src, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.zero_extend.dst, level + 1);
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_INT:
        fprintf(fd, "double_to_int\n");
        print_indent(fd, level);
        fprintf(fd, "src:\n");
        export_yaml_val(fd, instr->u.double_to_int.src, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.double_to_int.dst, level + 1);
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_UINT:
        fprintf(fd, "double_to_uint\n");
        print_indent(fd, level);
        fprintf(fd, "src:\n");
        export_yaml_val(fd, instr->u.double_to_uint.src, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.double_to_uint.dst, level + 1);
        break;
    case TAC_INSTRUCTION_INT_TO_DOUBLE:
        fprintf(fd, "int_to_double\n");
        print_indent(fd, level);
        fprintf(fd, "src:\n");
        export_yaml_val(fd, instr->u.int_to_double.src, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.int_to_double.dst, level + 1);
        break;
    case TAC_INSTRUCTION_UINT_TO_DOUBLE:
        fprintf(fd, "uint_to_double\n");
        print_indent(fd, level);
        fprintf(fd, "src:\n");
        export_yaml_val(fd, instr->u.uint_to_double.src, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.uint_to_double.dst, level + 1);
        break;
    case TAC_INSTRUCTION_FLOAT_TO_DOUBLE:
        fprintf(fd, "float_to_double\n");
        print_indent(fd, level);
        fprintf(fd, "src:\n");
        export_yaml_val(fd, instr->u.float_to_double.src, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.float_to_double.dst, level + 1);
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_FLOAT:
        fprintf(fd, "double_to_float\n");
        print_indent(fd, level);
        fprintf(fd, "src:\n");
        export_yaml_val(fd, instr->u.double_to_float.src, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.double_to_float.dst, level + 1);
        break;
    case TAC_INSTRUCTION_INT_TO_FLOAT:
        fprintf(fd, "int_to_float\n");
        print_indent(fd, level);
        fprintf(fd, "src:\n");
        export_yaml_val(fd, instr->u.int_to_float.src, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.int_to_float.dst, level + 1);
        break;
    case TAC_INSTRUCTION_UINT_TO_FLOAT:
        fprintf(fd, "uint_to_float\n");
        print_indent(fd, level);
        fprintf(fd, "src:\n");
        export_yaml_val(fd, instr->u.uint_to_float.src, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.uint_to_float.dst, level + 1);
        break;
    case TAC_INSTRUCTION_FLOAT_TO_INT:
        fprintf(fd, "float_to_int\n");
        print_indent(fd, level);
        fprintf(fd, "src:\n");
        export_yaml_val(fd, instr->u.float_to_int.src, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.float_to_int.dst, level + 1);
        break;
    case TAC_INSTRUCTION_FLOAT_TO_UINT:
        fprintf(fd, "float_to_uint\n");
        print_indent(fd, level);
        fprintf(fd, "src:\n");
        export_yaml_val(fd, instr->u.float_to_uint.src, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.float_to_uint.dst, level + 1);
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_INT:
        fprintf(fd, "long_double_to_int\n");
        print_indent(fd, level);
        fprintf(fd, "src:\n");
        export_yaml_val(fd, instr->u.long_double_to_int.src, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.long_double_to_int.dst, level + 1);
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_UINT:
        fprintf(fd, "long_double_to_uint\n");
        print_indent(fd, level);
        fprintf(fd, "src:\n");
        export_yaml_val(fd, instr->u.long_double_to_uint.src, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.long_double_to_uint.dst, level + 1);
        break;
    case TAC_INSTRUCTION_INT_TO_LONG_DOUBLE:
        fprintf(fd, "int_to_long_double\n");
        print_indent(fd, level);
        fprintf(fd, "src:\n");
        export_yaml_val(fd, instr->u.int_to_long_double.src, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.int_to_long_double.dst, level + 1);
        break;
    case TAC_INSTRUCTION_UINT_TO_LONG_DOUBLE:
        fprintf(fd, "uint_to_long_double\n");
        print_indent(fd, level);
        fprintf(fd, "src:\n");
        export_yaml_val(fd, instr->u.uint_to_long_double.src, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.uint_to_long_double.dst, level + 1);
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_DOUBLE:
        fprintf(fd, "long_double_to_double\n");
        print_indent(fd, level);
        fprintf(fd, "src:\n");
        export_yaml_val(fd, instr->u.long_double_to_double.src, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.long_double_to_double.dst, level + 1);
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_LONG_DOUBLE:
        fprintf(fd, "double_to_long_double\n");
        print_indent(fd, level);
        fprintf(fd, "src:\n");
        export_yaml_val(fd, instr->u.double_to_long_double.src, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.double_to_long_double.dst, level + 1);
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_FLOAT:
        fprintf(fd, "long_double_to_float\n");
        print_indent(fd, level);
        fprintf(fd, "src:\n");
        export_yaml_val(fd, instr->u.long_double_to_float.src, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.long_double_to_float.dst, level + 1);
        break;
    case TAC_INSTRUCTION_FLOAT_TO_LONG_DOUBLE:
        fprintf(fd, "float_to_long_double\n");
        print_indent(fd, level);
        fprintf(fd, "src:\n");
        export_yaml_val(fd, instr->u.float_to_long_double.src, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.float_to_long_double.dst, level + 1);
        break;
    case TAC_INSTRUCTION_PTR_TO_CHAR_PTR:
        fprintf(fd, "ptr_to_char_ptr\n");
        print_indent(fd, level);
        fprintf(fd, "src:\n");
        export_yaml_val(fd, instr->u.ptr_to_char_ptr.src, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.ptr_to_char_ptr.dst, level + 1);
        break;
    case TAC_INSTRUCTION_CHAR_PTR_TO_PTR:
        fprintf(fd, "char_ptr_to_ptr\n");
        print_indent(fd, level);
        fprintf(fd, "src:\n");
        export_yaml_val(fd, instr->u.char_ptr_to_ptr.src, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.char_ptr_to_ptr.dst, level + 1);
        break;
    case TAC_INSTRUCTION_UNARY: {
        fprintf(fd, "unary\n");
        print_indent(fd, level);
        fprintf(fd, "op: ");
        switch (instr->u.unary.op) {
        case TAC_UNARY_COMPLEMENT:
            fprintf(fd, "complement\n");
            break;
        case TAC_UNARY_COMPLEMENT_UNSIGNED:
            fprintf(fd, "complement_unsigned\n");
            break;
        case TAC_UNARY_NEGATE:
            fprintf(fd, "negate\n");
            break;
        case TAC_UNARY_NOT:
            fprintf(fd, "not\n");
            break;
        case TAC_UNARY_NEGATE_UNSIGNED:
            fprintf(fd, "negate_unsigned\n");
            break;
        case TAC_UNARY_NEGATE_DOUBLE:
            fprintf(fd, "negate_double\n");
            break;
        case TAC_UNARY_SQRT_DOUBLE:
            fprintf(fd, "sqrt_double\n");
            break;
        }
        print_indent(fd, level);
        fprintf(fd, "src:\n");
        export_yaml_val(fd, instr->u.unary.src, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.unary.dst, level + 1);
        break;
    }
    case TAC_INSTRUCTION_BINARY: {
        fprintf(fd, "binary\n");
        print_indent(fd, level);
        fprintf(fd, "op: ");
        switch (instr->u.binary.op) {
        case TAC_BINARY_ADD:
            fprintf(fd, "add\n");
            break;
        case TAC_BINARY_SUBTRACT:
            fprintf(fd, "subtract\n");
            break;
        case TAC_BINARY_MULTIPLY:
            fprintf(fd, "multiply\n");
            break;
        case TAC_BINARY_DIVIDE:
            fprintf(fd, "divide\n");
            break;
        case TAC_BINARY_REMAINDER:
            fprintf(fd, "remainder\n");
            break;
        case TAC_BINARY_EQUAL:
            fprintf(fd, "equal\n");
            break;
        case TAC_BINARY_NOT_EQUAL:
            fprintf(fd, "not_equal\n");
            break;
        case TAC_BINARY_LESS_THAN:
            fprintf(fd, "less_than\n");
            break;
        case TAC_BINARY_LESS_OR_EQUAL:
            fprintf(fd, "less_or_equal\n");
            break;
        case TAC_BINARY_GREATER_THAN:
            fprintf(fd, "greater_than\n");
            break;
        case TAC_BINARY_GREATER_OR_EQUAL:
            fprintf(fd, "greater_or_equal\n");
            break;
        case TAC_BINARY_BITWISE_AND:
            fprintf(fd, "bitwise_and\n");
            break;
        case TAC_BINARY_BITWISE_OR:
            fprintf(fd, "bitwise_or\n");
            break;
        case TAC_BINARY_BITWISE_XOR:
            fprintf(fd, "bitwise_xor\n");
            break;
        case TAC_BINARY_LEFT_SHIFT:
            fprintf(fd, "left_shift\n");
            break;
        case TAC_BINARY_RIGHT_SHIFT:
            fprintf(fd, "right_shift\n");
            break;
        case TAC_BINARY_DIVIDE_UNSIGNED:
            fprintf(fd, "divide_unsigned\n");
            break;
        case TAC_BINARY_REMAINDER_UNSIGNED:
            fprintf(fd, "remainder_unsigned\n");
            break;
        case TAC_BINARY_LESS_THAN_UNSIGNED:
            fprintf(fd, "less_than_unsigned\n");
            break;
        case TAC_BINARY_LESS_OR_EQUAL_UNSIGNED:
            fprintf(fd, "less_or_equal_unsigned\n");
            break;
        case TAC_BINARY_GREATER_THAN_UNSIGNED:
            fprintf(fd, "greater_than_unsigned\n");
            break;
        case TAC_BINARY_GREATER_OR_EQUAL_UNSIGNED:
            fprintf(fd, "greater_or_equal_unsigned\n");
            break;
        case TAC_BINARY_RIGHT_SHIFT_LOGICAL:
            fprintf(fd, "right_shift_logical\n");
            break;
        case TAC_BINARY_ADD_UNSIGNED:
            fprintf(fd, "add_unsigned\n");
            break;
        case TAC_BINARY_SUBTRACT_UNSIGNED:
            fprintf(fd, "subtract_unsigned\n");
            break;
        case TAC_BINARY_MULTIPLY_UNSIGNED:
            fprintf(fd, "multiply_unsigned\n");
            break;
        case TAC_BINARY_ADD_DOUBLE:
            fprintf(fd, "add_double\n");
            break;
        case TAC_BINARY_SUBTRACT_DOUBLE:
            fprintf(fd, "subtract_double\n");
            break;
        case TAC_BINARY_MULTIPLY_DOUBLE:
            fprintf(fd, "multiply_double\n");
            break;
        case TAC_BINARY_DIVIDE_DOUBLE:
            fprintf(fd, "divide_double\n");
            break;
        case TAC_BINARY_LESS_THAN_DOUBLE:
            fprintf(fd, "less_than_double\n");
            break;
        case TAC_BINARY_LESS_OR_EQUAL_DOUBLE:
            fprintf(fd, "less_or_equal_double\n");
            break;
        case TAC_BINARY_GREATER_THAN_DOUBLE:
            fprintf(fd, "greater_than_double\n");
            break;
        case TAC_BINARY_GREATER_OR_EQUAL_DOUBLE:
            fprintf(fd, "greater_or_equal_double\n");
            break;
        }
        print_indent(fd, level);
        fprintf(fd, "src1:\n");
        export_yaml_val(fd, instr->u.binary.src1, level + 1);
        print_indent(fd, level);
        fprintf(fd, "src2:\n");
        export_yaml_val(fd, instr->u.binary.src2, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.binary.dst, level + 1);
        break;
    }
    case TAC_INSTRUCTION_COPY:
        fprintf(fd, "copy\n");
        if (instr->is_volatile) {
            print_indent(fd, level);
            fprintf(fd, "volatile: true\n");
        }
        print_indent(fd, level);
        fprintf(fd, "src:\n");
        export_yaml_val(fd, instr->u.copy.src, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.copy.dst, level + 1);
        break;
    case TAC_INSTRUCTION_GET_ADDRESS:
    case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
    case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
        fprintf(fd, "%s\n",
                instr->kind == TAC_INSTRUCTION_GET_ADDRESS_BYTE    ? "get_address_byte"
                : instr->kind == TAC_INSTRUCTION_GET_ADDRESS_DECAY ? "get_address_decay"
                                                                   : "get_address");
        print_indent(fd, level);
        fprintf(fd, "src:\n");
        export_yaml_val(fd, instr->u.get_address.src, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.get_address.dst, level + 1);
        break;
    case TAC_INSTRUCTION_LOAD:
    case TAC_INSTRUCTION_LOAD_BYTE:
        fprintf(fd, "%s\n", instr->kind == TAC_INSTRUCTION_LOAD_BYTE ? "load_byte" : "load");
        if (instr->is_volatile) {
            print_indent(fd, level);
            fprintf(fd, "volatile: true\n");
        }
        print_indent(fd, level);
        fprintf(fd, "src_ptr:\n");
        export_yaml_val(fd, instr->u.load.src_ptr, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.load.dst, level + 1);
        break;
    case TAC_INSTRUCTION_STORE:
    case TAC_INSTRUCTION_STORE_BYTE:
        fprintf(fd, "%s\n", instr->kind == TAC_INSTRUCTION_STORE_BYTE ? "store_byte" : "store");
        if (instr->is_volatile) {
            print_indent(fd, level);
            fprintf(fd, "volatile: true\n");
        }
        print_indent(fd, level);
        fprintf(fd, "src:\n");
        export_yaml_val(fd, instr->u.store.src, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst_ptr:\n");
        export_yaml_val(fd, instr->u.store.dst_ptr, level + 1);
        break;
    case TAC_INSTRUCTION_ADD_PTR:
        fprintf(fd, "add_ptr\n");
        print_indent(fd, level);
        fprintf(fd, "ptr:\n");
        export_yaml_val(fd, instr->u.add_ptr.ptr, level + 1);
        print_indent(fd, level);
        fprintf(fd, "index:\n");
        export_yaml_val(fd, instr->u.add_ptr.index, level + 1);
        print_indent(fd, level);
        fprintf(fd, "scale: %d\n", instr->u.add_ptr.scale);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.add_ptr.dst, level + 1);
        break;
    case TAC_INSTRUCTION_PTR_DIFF:
        fprintf(fd, "ptr_diff\n");
        print_indent(fd, level);
        fprintf(fd, "ptr_a:\n");
        export_yaml_val(fd, instr->u.ptr_diff.ptr_a, level + 1);
        print_indent(fd, level);
        fprintf(fd, "ptr_b:\n");
        export_yaml_val(fd, instr->u.ptr_diff.ptr_b, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.ptr_diff.dst, level + 1);
        break;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET:
        fprintf(fd, "%s\n",
                instr->kind == TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET ? "copy_byte_to_offset"
                                                                   : "copy_to_offset");
        if (instr->is_volatile) {
            print_indent(fd, level);
            fprintf(fd, "volatile: true\n");
        }
        print_indent(fd, level);
        fprintf(fd, "src:\n");
        export_yaml_val(fd, instr->u.copy_to_offset.src, level + 1);
        print_indent(fd, level);
        fprintf(fd, "dst: %s\n", instr->u.copy_to_offset.dst ? instr->u.copy_to_offset.dst : "");
        print_indent(fd, level);
        fprintf(fd, "offset: %d\n", instr->u.copy_to_offset.offset);
        break;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
        fprintf(fd, "%s\n",
                instr->kind == TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET ? "copy_byte_from_offset"
                                                                     : "copy_from_offset");
        if (instr->is_volatile) {
            print_indent(fd, level);
            fprintf(fd, "volatile: true\n");
        }
        print_indent(fd, level);
        fprintf(fd, "src: %s\n",
                instr->u.copy_from_offset.src ? instr->u.copy_from_offset.src : "");
        print_indent(fd, level);
        fprintf(fd, "offset: %d\n", instr->u.copy_from_offset.offset);
        print_indent(fd, level);
        fprintf(fd, "dst:\n");
        export_yaml_val(fd, instr->u.copy_from_offset.dst, level + 1);
        break;
    case TAC_INSTRUCTION_JUMP:
        fprintf(fd, "jump\n");
        print_indent(fd, level);
        fprintf(fd, "target: %s\n", instr->u.jump.target ? instr->u.jump.target : "");
        break;
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
        fprintf(fd, "jump_if_zero\n");
        print_indent(fd, level);
        fprintf(fd, "condition:\n");
        export_yaml_val(fd, instr->u.jump_if_zero.condition, level + 1);
        print_indent(fd, level);
        fprintf(fd, "target: %s\n",
                instr->u.jump_if_zero.target ? instr->u.jump_if_zero.target : "");
        break;
    case TAC_INSTRUCTION_JUMP_TABLE:
        fprintf(fd, "jump_table\n");
        print_indent(fd, level);
        fprintf(fd, "index:\n");
        export_yaml_val(fd, instr->u.jump_table.index, level + 1);
        print_indent(fd, level);
        fprintf(fd, "targets:\n");
        for (int i = 0; i < instr->u.jump_table.count; i++) {
            print_indent(fd, level + 1);
            fprintf(fd, "- %s\n", instr->u.jump_table.targets[i]);
        }
        print_indent(fd, level);
        fprintf(fd, "default: %s\n", instr->u.jump_table.default_target);
        break;
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
        fprintf(fd, "jump_if_not_zero\n");
        print_indent(fd, level);
        fprintf(fd, "condition:\n");
        export_yaml_val(fd, instr->u.jump_if_not_zero.condition, level + 1);
        print_indent(fd, level);
        fprintf(fd, "target: %s\n",
                instr->u.jump_if_not_zero.target ? instr->u.jump_if_not_zero.target : "");
        break;
    case TAC_INSTRUCTION_LABEL:
        fprintf(fd, "label\n");
        print_indent(fd, level);
        fprintf(fd, "name: %s\n", instr->u.label.name ? instr->u.label.name : "");
        break;
    case TAC_INSTRUCTION_FUN_CALL:
    case TAC_INSTRUCTION_FUN_CALL_NORETURN:
        fprintf(
            fd, "%s\n",
            instr->kind == TAC_INSTRUCTION_FUN_CALL_NORETURN ? "fun_call_noreturn" : "fun_call");
        print_indent(fd, level);
        fprintf(fd, "fun_name: %s\n", instr->u.fun_call.fun_name ? instr->u.fun_call.fun_name : "");
        if (instr->u.fun_call.indirect) {
            print_indent(fd, level);
            fprintf(fd, "indirect: true\n");
        }
        if (instr->u.fun_call.args) {
            print_indent(fd, level);
            fprintf(fd, "args:\n");
            export_yaml_val_list(fd, instr->u.fun_call.args, level + 1);
        }
        if (instr->u.fun_call.dst) {
            print_indent(fd, level);
            fprintf(fd, "dst:\n");
            export_yaml_val(fd, instr->u.fun_call.dst, level + 1);
        }
        if (tac_yaml_types && instr->u.fun_call.fun_type)
            export_yaml_type_str(fd, "fun_type", instr->u.fun_call.fun_type, level);
        break;
    case TAC_INSTRUCTION_ALLOCATE_LOCAL:
        fprintf(fd, "allocate_local\n");
        print_indent(fd, level);
        fprintf(fd, "name: %s\n", instr->u.allocate_local.name ? instr->u.allocate_local.name : "");
        print_indent(fd, level);
        fprintf(fd, "size: %d\n", instr->u.allocate_local.size);
        print_indent(fd, level);
        fprintf(fd, "alignment: %d\n", instr->u.allocate_local.alignment);
        break;
    }
}

void tac_export_yaml_instruction_list(FILE *fd, const Tac_Instruction *instr, int level)
{
    while (instr) {
        print_indent(fd, level);
        fprintf(fd, "- instruction:\n");
        export_yaml_instruction(fd, instr, level + 1);
        instr = instr->next;
    }
}

void tac_export_yaml(FILE *fd, const Tac_TopLevel *tl)
{
    if (!fd || !tl)
        return;
    fprintf(fd, "- toplevel:\n");
    switch (tl->kind) {
    case TAC_TOPLEVEL_FUNCTION:
        fprintf(fd, "  kind: function\n");
        fprintf(fd, "  name: %s\n", tl->u.function.name ? tl->u.function.name : "");
        fprintf(fd, "  global: %s\n", tl->u.function.global ? "true" : "false");
        if (tl->u.function.noret)
            fprintf(fd, "  noret: true\n");
        if (tac_yaml_types && tl->u.function.type)
            export_yaml_type_str(fd, "type", tl->u.function.type, 1);
        if (tl->u.function.params) {
            fprintf(fd, "  params:\n");
            export_yaml_param_list(fd, "param", tl->u.function.params, 2);
        }
        if (tac_yaml_types && tl->u.function.locals) {
            fprintf(fd, "  locals:\n");
            export_yaml_param_list(fd, "local", tl->u.function.locals, 2);
        }
        if (tl->u.function.static_locals) {
            fprintf(fd, "  static_locals:\n");
            for (const Tac_StaticLocal *sl = tl->u.function.static_locals; sl; sl = sl->next) {
                fprintf(fd, "  - name: %s\n", sl->name ? sl->name : "");
                fprintf(fd, "    type:\n");
                export_yaml_type(fd, sl->type, 3);
                if (sl->alignment)
                    fprintf(fd, "    alignment: %d\n", sl->alignment);
                if (sl->init_list) {
                    fprintf(fd, "    init_list:\n");
                    export_yaml_static_init_list(fd, sl->init_list, 3);
                }
            }
        }
        if (tl->u.function.body) {
            fprintf(fd, "  body:\n");
            tac_export_yaml_instruction_list(fd, tl->u.function.body, 2);
        }
        break;
    case TAC_TOPLEVEL_STATIC_VARIABLE:
        fprintf(fd, "  kind: static_variable\n");
        fprintf(fd, "  name: %s\n", tl->u.static_variable.name ? tl->u.static_variable.name : "");
        fprintf(fd, "  global: %s\n", tl->u.static_variable.global ? "true" : "false");
        fprintf(fd, "  type:\n");
        export_yaml_type(fd, tl->u.static_variable.type, 2);
        if (tl->u.static_variable.alignment)
            fprintf(fd, "  alignment: %d\n", tl->u.static_variable.alignment);
        if (tl->u.static_variable.init_list) {
            fprintf(fd, "  init_list:\n");
            export_yaml_static_init_list(fd, tl->u.static_variable.init_list, 2);
        }
        break;
    case TAC_TOPLEVEL_STATIC_CONSTANT:
        fprintf(fd, "  kind: static_constant\n");
        fprintf(fd, "  name: %s\n", tl->u.static_constant.name ? tl->u.static_constant.name : "");
        fprintf(fd, "  type:\n");
        export_yaml_type(fd, tl->u.static_constant.type, 2);
        if (tl->u.static_constant.init) {
            fprintf(fd, "  init:\n");
            export_yaml_static_init(fd, tl->u.static_constant.init, 2);
        }
        break;
    case TAC_TOPLEVEL_EXTERN:
        fprintf(fd, "  kind: extern\n");
        fprintf(fd, "  name: %s\n", tl->u.extern_.name ? tl->u.extern_.name : "");
        fprintf(fd, "  type:\n");
        export_yaml_type(fd, tl->u.extern_.type, 2);
        break;
    }
}
