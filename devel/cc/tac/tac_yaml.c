#include <inttypes.h>
#include <stdio.h>

#include "tac.h"
#include "xalloc.h"

#define INDENT_STEP 2

static coro(braam_call *) void print_indent(FILE *fd, int level)
{
    for (int i = 0; i < level * INDENT_STEP; i++) {
        await fputc(' ', fd);
    }
}

static coro(braam_call *) void export_yaml_const(FILE *fd, const Tac_Const *c, int level)
{
    await print_indent(fd, level);
    await fprintf(fd, "kind: ");
    switch (c->kind) {
    case TAC_CONST_INT:
        await fprintf(fd, "int\n");
        await print_indent(fd, level);
        await fprintf(fd, "value: %" PRId64 "\n", c->u.int_val);
        break;
    case TAC_CONST_LONG:
        await fprintf(fd, "long\n");
        await print_indent(fd, level);
        await fprintf(fd, "value: %ld\n", c->u.long_val);
        break;
    case TAC_CONST_LONG_LONG:
        await fprintf(fd, "long_long\n");
        await print_indent(fd, level);
        await fprintf(fd, "value: %lld\n", c->u.long_long_val);
        break;
    case TAC_CONST_UINT:
        await fprintf(fd, "uint\n");
        await print_indent(fd, level);
        await fprintf(fd, "value: %" PRIu64 "\n", c->u.uint_val);
        break;
    case TAC_CONST_ULONG:
        await fprintf(fd, "ulong\n");
        await print_indent(fd, level);
        await fprintf(fd, "value: %lu\n", c->u.ulong_val);
        break;
    case TAC_CONST_ULONG_LONG:
        await fprintf(fd, "ulong_long\n");
        await print_indent(fd, level);
        await fprintf(fd, "value: %llu\n", c->u.ulong_long_val);
        break;
    case TAC_CONST_FLOAT: {
        await fprintf(fd, "float\n");
        await print_indent(fd, level);
        char hex[64];
        tac_format_hex_double(hex, sizeof hex, (double)c->u.float_val);
        await fprintf(fd, "value: %s\n", hex);
        break;
    }
    case TAC_CONST_DOUBLE: {
        await fprintf(fd, "double\n");
        await print_indent(fd, level);
        char hex[64];
        tac_format_hex_double(hex, sizeof hex, c->u.double_val);
        await fprintf(fd, "value: %s\n", hex);
        break;
    }
    case TAC_CONST_LONG_DOUBLE:
        await fprintf(fd, "long_double\n");
        await print_indent(fd, level);
        {
            char buf[F128_BUFSIZE];
            await fprintf(fd, "value: %s\n", f128_format(c->u.long_double_val, buf));
            break;
        }
    case TAC_CONST_SCHAR:
        await fprintf(fd, "char\n");
        await print_indent(fd, level);
        await fprintf(fd, "value: %d\n", c->u.char_val);
        break;
    case TAC_CONST_UCHAR:
        await fprintf(fd, "uchar\n");
        await print_indent(fd, level);
        await fprintf(fd, "value: %u\n", (unsigned)c->u.uchar_val);
        break;
    }
}

static coro(braam_call *) void export_yaml_val(FILE *fd, const Tac_Val *val, int level)
{
    if (val->kind == TAC_VAL_CONSTANT) {
        await print_indent(fd, level);
        await fprintf(fd, "kind: constant\n");
        await print_indent(fd, level);
        await fprintf(fd, "const:\n");
        await export_yaml_const(fd, val->u.constant, level + 1);
    } else {
        await print_indent(fd, level);
        await fprintf(fd, "kind: var\n");
        await print_indent(fd, level);
        await fprintf(fd, "name: %s\n", val->u.var_name ? val->u.var_name : "");
    }
}

static coro(braam_call *) void export_yaml_val_list(FILE *fd, const Tac_Val *val, int level)
{
    while (val) {
        await print_indent(fd, level);
        await fprintf(fd, "- val:\n");
        await export_yaml_val(fd, val, level + 1);
        val = val->next;
    }
}

static coro(braam_call *) void export_yaml_type(FILE *fd, const Tac_Type *type, int level);

static coro(braam_call *) void export_yaml_type_list(FILE *fd, const Tac_Type *type, int level)
{
    while (type) {
        await print_indent(fd, level);
        await fprintf(fd, "- type:\n");
        await export_yaml_type(fd, type, level + 1);
        type = type->next;
    }
}

static coro(braam_call *) void export_yaml_type(FILE *fd, const Tac_Type *type, int level)
{
    await print_indent(fd, level);
    await fprintf(fd, "kind: ");
    switch (type->kind) {
    case TAC_TYPE_SCHAR:
        await fprintf(fd, "schar\n");
        break;
    case TAC_TYPE_UCHAR:
        await fprintf(fd, "uchar\n");
        break;
    case TAC_TYPE_SHORT:
        await fprintf(fd, "short\n");
        break;
    case TAC_TYPE_INT:
        await fprintf(fd, "int\n");
        break;
    case TAC_TYPE_LONG:
        await fprintf(fd, "long\n");
        break;
    case TAC_TYPE_LONG_LONG:
        await fprintf(fd, "long_long\n");
        break;
    case TAC_TYPE_USHORT:
        await fprintf(fd, "ushort\n");
        break;
    case TAC_TYPE_UINT:
        await fprintf(fd, "uint\n");
        break;
    case TAC_TYPE_ULONG:
        await fprintf(fd, "ulong\n");
        break;
    case TAC_TYPE_ULONG_LONG:
        await fprintf(fd, "ulong_long\n");
        break;
    case TAC_TYPE_FLOAT:
        await fprintf(fd, "float\n");
        break;
    case TAC_TYPE_DOUBLE:
        await fprintf(fd, "double\n");
        break;
    case TAC_TYPE_LONG_DOUBLE:
        await fprintf(fd, "long_double\n");
        break;
    case TAC_TYPE_VOID:
        await fprintf(fd, "void\n");
        break;
    case TAC_TYPE_FUN_TYPE:
        await fprintf(fd, "fun_type\n");
        if (type->u.fun_type.param_types) {
            await print_indent(fd, level);
            await fprintf(fd, "param_types:\n");
            await export_yaml_type_list(fd, type->u.fun_type.param_types, level + 1);
        }
        await print_indent(fd, level);
        await fprintf(fd, "ret_type:\n");
        await export_yaml_type(fd, type->u.fun_type.ret_type, level + 1);
        if (type->u.fun_type.variadic) {
            await print_indent(fd, level);
            await fprintf(fd, "variadic: true\n");
        }
        if (type->u.fun_type.va_sized) {
            await print_indent(fd, level);
            await fprintf(fd, "va_sized: true\n");
        }
        break;
    case TAC_TYPE_POINTER:
        await fprintf(fd, "pointer\n");
        await print_indent(fd, level);
        await fprintf(fd, "target:\n");
        await export_yaml_type(fd, type->u.pointer.target_type, level + 1);
        break;
    case TAC_TYPE_ARRAY:
        await fprintf(fd, "array\n");
        await print_indent(fd, level);
        await fprintf(fd, "elem_type:\n");
        await export_yaml_type(fd, type->u.array.elem_type, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "size: %d\n", type->u.array.size);
        break;
    case TAC_TYPE_STRUCTURE:
        await fprintf(fd, "structure\n");
        await print_indent(fd, level);
        await fprintf(fd, "tag: %s\n", type->u.structure.tag ? type->u.structure.tag : "");
        await print_indent(fd, level);
        await fprintf(fd, "size: %d\n", type->u.structure.size);
        if (!tac_yaml_types)
            break;
        await print_indent(fd, level);
        await fprintf(fd, "alignment: %d\n", type->u.structure.alignment);
        if (type->u.structure.is_union) {
            await print_indent(fd, level);
            await fprintf(fd, "union: true\n");
        }
        if (type->u.structure.members) {
            await print_indent(fd, level);
            await fprintf(fd, "members:\n");
            for (const Tac_Member *m = type->u.structure.members; m; m = m->next) {
                await print_indent(fd, level + 1);
                await fprintf(fd, "- member: %s\n", m->name ? m->name : "");
                await print_indent(fd, level + 2);
                await fprintf(fd, "offset: %d\n", m->offset);
                await print_indent(fd, level + 2);
                await fprintf(fd, "type:\n");
                await export_yaml_type(fd, m->type, level + 3);
            }
        }
        break;
    }
}

bool tac_yaml_types = true;

// "<key>: <type>" on one line, in the compact tac_type_str spelling.
static coro(braam_call *) void export_yaml_type_str(FILE *fd, const char *key, const Tac_Type *type,
                                                    int level)
{
    char *ts = tac_type_str(type);
    await print_indent(fd, level);
    await fprintf(fd, "%s: %s\n", key, ts);
    xfree(ts);
}

static coro(braam_call *) void export_yaml_param_list(FILE *fd, const char *key,
                                                      const Tac_Param *param, int level)
{
    while (param) {
        await print_indent(fd, level);
        await fprintf(fd, "- %s: %s\n", key, param->name ? param->name : "");
        if (tac_yaml_types && param->type)
            await export_yaml_type_str(fd, "type", param->type, level + 1);
        param = param->next;
    }
}

static coro(braam_call *) void export_yaml_static_init(FILE *fd, const Tac_StaticInit *init,
                                                       int level)
{
    await print_indent(fd, level);
    await fprintf(fd, "kind: ");
    switch (init->kind) {
    case TAC_STATIC_INIT_I8:
        await fprintf(fd, "i8\n");
        await print_indent(fd, level);
        await fprintf(fd, "value: %d\n", (int)init->u.char_val);
        break;
    case TAC_STATIC_INIT_I16:
        await fprintf(fd, "i16\n");
        await print_indent(fd, level);
        await fprintf(fd, "value: %" PRId16 "\n", init->u.short_val);
        break;
    case TAC_STATIC_INIT_I32:
        await fprintf(fd, "i32\n");
        await print_indent(fd, level);
        await fprintf(fd, "value: %" PRId32 "\n", init->u.int_val);
        break;
    case TAC_STATIC_INIT_I64:
        await fprintf(fd, "i64\n");
        await print_indent(fd, level);
        await fprintf(fd, "value: %" PRId64 "\n", init->u.long_val);
        break;
    case TAC_STATIC_INIT_U8:
        await fprintf(fd, "u8\n");
        await print_indent(fd, level);
        await fprintf(fd, "value: %u\n", (unsigned)init->u.uchar_val);
        break;
    case TAC_STATIC_INIT_U16:
        await fprintf(fd, "u16\n");
        await print_indent(fd, level);
        await fprintf(fd, "value: %" PRIu16 "\n", init->u.ushort_val);
        break;
    case TAC_STATIC_INIT_U32:
        await fprintf(fd, "u32\n");
        await print_indent(fd, level);
        await fprintf(fd, "value: %" PRIu32 "\n", init->u.uint_val);
        break;
    case TAC_STATIC_INIT_U64:
        await fprintf(fd, "u64\n");
        await print_indent(fd, level);
        await fprintf(fd, "value: %" PRIu64 "\n", init->u.ulong_val);
        break;
    case TAC_STATIC_INIT_FLOAT: {
        await fprintf(fd, "float\n");
        await print_indent(fd, level);
        char hex[64];
        tac_format_hex_double(hex, sizeof hex, (double)init->u.float_val);
        await fprintf(fd, "value: %s\n", hex);
        break;
    }
    case TAC_STATIC_INIT_DOUBLE: {
        await fprintf(fd, "double\n");
        await print_indent(fd, level);
        char hex[64];
        tac_format_hex_double(hex, sizeof hex, init->u.double_val);
        await fprintf(fd, "value: %s\n", hex);
        break;
    }
    case TAC_STATIC_INIT_LONG_DOUBLE:
        await fprintf(fd, "long_double\n");
        await print_indent(fd, level);
        {
            char buf[F128_BUFSIZE];
            await fprintf(fd, "value: %s\n", f128_format(init->u.long_double_val, buf));
            break;
        }
    case TAC_STATIC_INIT_ZERO:
        await fprintf(fd, "zero\n");
        await print_indent(fd, level);
        await fprintf(fd, "bytes: %d\n", init->u.zero_bytes);
        break;
    case TAC_STATIC_INIT_STRING: {
        await fprintf(fd, "string\n");
        await print_indent(fd, level);
        char *text = tac_escape_string_bytes(init->u.string.val, init->u.string.len);
        await fprintf(fd, "value: %s\n", text);
        xfree(text);
        await print_indent(fd, level);
        await fprintf(fd, "null_terminated: %s\n",
                      init->u.string.null_terminated ? "true" : "false");
        break;
    }
    case TAC_STATIC_INIT_POINTER:
        await fprintf(fd, "pointer\n");
        await print_indent(fd, level);
        await fprintf(fd, "name: %s\n", init->u.pointer.name ? init->u.pointer.name : "");
        if (init->u.pointer.byte_offset) {
            await print_indent(fd, level);
            await fprintf(fd, "byte_offset: %d\n", init->u.pointer.byte_offset);
        }
        break;
    case TAC_STATIC_INIT_FAT_POINTER:
        await fprintf(fd, "fat_pointer\n");
        await print_indent(fd, level);
        await fprintf(fd, "name: %s\n", init->u.pointer.name ? init->u.pointer.name : "");
        await print_indent(fd, level);
        await fprintf(fd, "byte_offset: %d\n", init->u.pointer.byte_offset);
        break;
    }
}

static coro(braam_call *) void export_yaml_static_init_list(FILE *fd, const Tac_StaticInit *init,
                                                            int level)
{
    while (init) {
        await print_indent(fd, level);
        await fprintf(fd, "- init:\n");
        await export_yaml_static_init(fd, init, level + 1);
        init = init->next;
    }
}

static coro(braam_call *) void export_yaml_instruction(FILE *fd, const Tac_Instruction *instr,
                                                       int level)
{
    await print_indent(fd, level);
    await fprintf(fd, "kind: ");
    switch (instr->kind) {
    case TAC_INSTRUCTION_RETURN:
        await fprintf(fd, "return\n");
        if (instr->u.return_.src) {
            await print_indent(fd, level);
            await fprintf(fd, "src:\n");
            await export_yaml_val(fd, instr->u.return_.src, level + 1);
        }
        break;
    case TAC_INSTRUCTION_SIGN_EXTEND:
        await fprintf(fd, "sign_extend\n");
        await print_indent(fd, level);
        await fprintf(fd, "src:\n");
        await export_yaml_val(fd, instr->u.sign_extend.src, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.sign_extend.dst, level + 1);
        break;
    case TAC_INSTRUCTION_TRUNCATE:
        await fprintf(fd, "truncate\n");
        await print_indent(fd, level);
        await fprintf(fd, "src:\n");
        await export_yaml_val(fd, instr->u.truncate.src, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.truncate.dst, level + 1);
        break;
    case TAC_INSTRUCTION_ZERO_EXTEND:
        await fprintf(fd, "zero_extend\n");
        await print_indent(fd, level);
        await fprintf(fd, "src:\n");
        await export_yaml_val(fd, instr->u.zero_extend.src, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.zero_extend.dst, level + 1);
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_INT:
        await fprintf(fd, "double_to_int\n");
        await print_indent(fd, level);
        await fprintf(fd, "src:\n");
        await export_yaml_val(fd, instr->u.double_to_int.src, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.double_to_int.dst, level + 1);
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_UINT:
        await fprintf(fd, "double_to_uint\n");
        await print_indent(fd, level);
        await fprintf(fd, "src:\n");
        await export_yaml_val(fd, instr->u.double_to_uint.src, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.double_to_uint.dst, level + 1);
        break;
    case TAC_INSTRUCTION_INT_TO_DOUBLE:
        await fprintf(fd, "int_to_double\n");
        await print_indent(fd, level);
        await fprintf(fd, "src:\n");
        await export_yaml_val(fd, instr->u.int_to_double.src, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.int_to_double.dst, level + 1);
        break;
    case TAC_INSTRUCTION_UINT_TO_DOUBLE:
        await fprintf(fd, "uint_to_double\n");
        await print_indent(fd, level);
        await fprintf(fd, "src:\n");
        await export_yaml_val(fd, instr->u.uint_to_double.src, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.uint_to_double.dst, level + 1);
        break;
    case TAC_INSTRUCTION_FLOAT_TO_DOUBLE:
        await fprintf(fd, "float_to_double\n");
        await print_indent(fd, level);
        await fprintf(fd, "src:\n");
        await export_yaml_val(fd, instr->u.float_to_double.src, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.float_to_double.dst, level + 1);
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_FLOAT:
        await fprintf(fd, "double_to_float\n");
        await print_indent(fd, level);
        await fprintf(fd, "src:\n");
        await export_yaml_val(fd, instr->u.double_to_float.src, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.double_to_float.dst, level + 1);
        break;
    case TAC_INSTRUCTION_INT_TO_FLOAT:
        await fprintf(fd, "int_to_float\n");
        await print_indent(fd, level);
        await fprintf(fd, "src:\n");
        await export_yaml_val(fd, instr->u.int_to_float.src, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.int_to_float.dst, level + 1);
        break;
    case TAC_INSTRUCTION_UINT_TO_FLOAT:
        await fprintf(fd, "uint_to_float\n");
        await print_indent(fd, level);
        await fprintf(fd, "src:\n");
        await export_yaml_val(fd, instr->u.uint_to_float.src, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.uint_to_float.dst, level + 1);
        break;
    case TAC_INSTRUCTION_FLOAT_TO_INT:
        await fprintf(fd, "float_to_int\n");
        await print_indent(fd, level);
        await fprintf(fd, "src:\n");
        await export_yaml_val(fd, instr->u.float_to_int.src, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.float_to_int.dst, level + 1);
        break;
    case TAC_INSTRUCTION_FLOAT_TO_UINT:
        await fprintf(fd, "float_to_uint\n");
        await print_indent(fd, level);
        await fprintf(fd, "src:\n");
        await export_yaml_val(fd, instr->u.float_to_uint.src, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.float_to_uint.dst, level + 1);
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_INT:
        await fprintf(fd, "long_double_to_int\n");
        await print_indent(fd, level);
        await fprintf(fd, "src:\n");
        await export_yaml_val(fd, instr->u.long_double_to_int.src, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.long_double_to_int.dst, level + 1);
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_UINT:
        await fprintf(fd, "long_double_to_uint\n");
        await print_indent(fd, level);
        await fprintf(fd, "src:\n");
        await export_yaml_val(fd, instr->u.long_double_to_uint.src, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.long_double_to_uint.dst, level + 1);
        break;
    case TAC_INSTRUCTION_INT_TO_LONG_DOUBLE:
        await fprintf(fd, "int_to_long_double\n");
        await print_indent(fd, level);
        await fprintf(fd, "src:\n");
        await export_yaml_val(fd, instr->u.int_to_long_double.src, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.int_to_long_double.dst, level + 1);
        break;
    case TAC_INSTRUCTION_UINT_TO_LONG_DOUBLE:
        await fprintf(fd, "uint_to_long_double\n");
        await print_indent(fd, level);
        await fprintf(fd, "src:\n");
        await export_yaml_val(fd, instr->u.uint_to_long_double.src, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.uint_to_long_double.dst, level + 1);
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_DOUBLE:
        await fprintf(fd, "long_double_to_double\n");
        await print_indent(fd, level);
        await fprintf(fd, "src:\n");
        await export_yaml_val(fd, instr->u.long_double_to_double.src, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.long_double_to_double.dst, level + 1);
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_LONG_DOUBLE:
        await fprintf(fd, "double_to_long_double\n");
        await print_indent(fd, level);
        await fprintf(fd, "src:\n");
        await export_yaml_val(fd, instr->u.double_to_long_double.src, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.double_to_long_double.dst, level + 1);
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_FLOAT:
        await fprintf(fd, "long_double_to_float\n");
        await print_indent(fd, level);
        await fprintf(fd, "src:\n");
        await export_yaml_val(fd, instr->u.long_double_to_float.src, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.long_double_to_float.dst, level + 1);
        break;
    case TAC_INSTRUCTION_FLOAT_TO_LONG_DOUBLE:
        await fprintf(fd, "float_to_long_double\n");
        await print_indent(fd, level);
        await fprintf(fd, "src:\n");
        await export_yaml_val(fd, instr->u.float_to_long_double.src, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.float_to_long_double.dst, level + 1);
        break;
    case TAC_INSTRUCTION_PTR_TO_CHAR_PTR:
        await fprintf(fd, "ptr_to_char_ptr\n");
        await print_indent(fd, level);
        await fprintf(fd, "src:\n");
        await export_yaml_val(fd, instr->u.ptr_to_char_ptr.src, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.ptr_to_char_ptr.dst, level + 1);
        break;
    case TAC_INSTRUCTION_CHAR_PTR_TO_PTR:
        await fprintf(fd, "char_ptr_to_ptr\n");
        await print_indent(fd, level);
        await fprintf(fd, "src:\n");
        await export_yaml_val(fd, instr->u.char_ptr_to_ptr.src, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.char_ptr_to_ptr.dst, level + 1);
        break;
    case TAC_INSTRUCTION_UNARY: {
        await fprintf(fd, "unary\n");
        await print_indent(fd, level);
        await fprintf(fd, "op: ");
        switch (instr->u.unary.op) {
        case TAC_UNARY_COMPLEMENT:
            await fprintf(fd, "complement\n");
            break;
        case TAC_UNARY_COMPLEMENT_UNSIGNED:
            await fprintf(fd, "complement_unsigned\n");
            break;
        case TAC_UNARY_NEGATE:
            await fprintf(fd, "negate\n");
            break;
        case TAC_UNARY_NOT:
            await fprintf(fd, "not\n");
            break;
        case TAC_UNARY_NEGATE_UNSIGNED:
            await fprintf(fd, "negate_unsigned\n");
            break;
        case TAC_UNARY_NEGATE_DOUBLE:
            await fprintf(fd, "negate_double\n");
            break;
        case TAC_UNARY_SQRT_DOUBLE:
            await fprintf(fd, "sqrt_double\n");
            break;
        }
        await print_indent(fd, level);
        await fprintf(fd, "src:\n");
        await export_yaml_val(fd, instr->u.unary.src, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.unary.dst, level + 1);
        break;
    }
    case TAC_INSTRUCTION_BINARY: {
        await fprintf(fd, "binary\n");
        await print_indent(fd, level);
        await fprintf(fd, "op: ");
        switch (instr->u.binary.op) {
        case TAC_BINARY_ADD:
            await fprintf(fd, "add\n");
            break;
        case TAC_BINARY_SUBTRACT:
            await fprintf(fd, "subtract\n");
            break;
        case TAC_BINARY_MULTIPLY:
            await fprintf(fd, "multiply\n");
            break;
        case TAC_BINARY_DIVIDE:
            await fprintf(fd, "divide\n");
            break;
        case TAC_BINARY_REMAINDER:
            await fprintf(fd, "remainder\n");
            break;
        case TAC_BINARY_EQUAL:
            await fprintf(fd, "equal\n");
            break;
        case TAC_BINARY_NOT_EQUAL:
            await fprintf(fd, "not_equal\n");
            break;
        case TAC_BINARY_LESS_THAN:
            await fprintf(fd, "less_than\n");
            break;
        case TAC_BINARY_LESS_OR_EQUAL:
            await fprintf(fd, "less_or_equal\n");
            break;
        case TAC_BINARY_GREATER_THAN:
            await fprintf(fd, "greater_than\n");
            break;
        case TAC_BINARY_GREATER_OR_EQUAL:
            await fprintf(fd, "greater_or_equal\n");
            break;
        case TAC_BINARY_BITWISE_AND:
            await fprintf(fd, "bitwise_and\n");
            break;
        case TAC_BINARY_BITWISE_OR:
            await fprintf(fd, "bitwise_or\n");
            break;
        case TAC_BINARY_BITWISE_XOR:
            await fprintf(fd, "bitwise_xor\n");
            break;
        case TAC_BINARY_LEFT_SHIFT:
            await fprintf(fd, "left_shift\n");
            break;
        case TAC_BINARY_RIGHT_SHIFT:
            await fprintf(fd, "right_shift\n");
            break;
        case TAC_BINARY_DIVIDE_UNSIGNED:
            await fprintf(fd, "divide_unsigned\n");
            break;
        case TAC_BINARY_REMAINDER_UNSIGNED:
            await fprintf(fd, "remainder_unsigned\n");
            break;
        case TAC_BINARY_LESS_THAN_UNSIGNED:
            await fprintf(fd, "less_than_unsigned\n");
            break;
        case TAC_BINARY_LESS_OR_EQUAL_UNSIGNED:
            await fprintf(fd, "less_or_equal_unsigned\n");
            break;
        case TAC_BINARY_GREATER_THAN_UNSIGNED:
            await fprintf(fd, "greater_than_unsigned\n");
            break;
        case TAC_BINARY_GREATER_OR_EQUAL_UNSIGNED:
            await fprintf(fd, "greater_or_equal_unsigned\n");
            break;
        case TAC_BINARY_RIGHT_SHIFT_LOGICAL:
            await fprintf(fd, "right_shift_logical\n");
            break;
        case TAC_BINARY_ADD_UNSIGNED:
            await fprintf(fd, "add_unsigned\n");
            break;
        case TAC_BINARY_SUBTRACT_UNSIGNED:
            await fprintf(fd, "subtract_unsigned\n");
            break;
        case TAC_BINARY_MULTIPLY_UNSIGNED:
            await fprintf(fd, "multiply_unsigned\n");
            break;
        case TAC_BINARY_ADD_DOUBLE:
            await fprintf(fd, "add_double\n");
            break;
        case TAC_BINARY_SUBTRACT_DOUBLE:
            await fprintf(fd, "subtract_double\n");
            break;
        case TAC_BINARY_MULTIPLY_DOUBLE:
            await fprintf(fd, "multiply_double\n");
            break;
        case TAC_BINARY_DIVIDE_DOUBLE:
            await fprintf(fd, "divide_double\n");
            break;
        case TAC_BINARY_LESS_THAN_DOUBLE:
            await fprintf(fd, "less_than_double\n");
            break;
        case TAC_BINARY_LESS_OR_EQUAL_DOUBLE:
            await fprintf(fd, "less_or_equal_double\n");
            break;
        case TAC_BINARY_GREATER_THAN_DOUBLE:
            await fprintf(fd, "greater_than_double\n");
            break;
        case TAC_BINARY_GREATER_OR_EQUAL_DOUBLE:
            await fprintf(fd, "greater_or_equal_double\n");
            break;
        }
        await print_indent(fd, level);
        await fprintf(fd, "src1:\n");
        await export_yaml_val(fd, instr->u.binary.src1, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "src2:\n");
        await export_yaml_val(fd, instr->u.binary.src2, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.binary.dst, level + 1);
        break;
    }
    case TAC_INSTRUCTION_COPY:
        await fprintf(fd, "copy\n");
        if (instr->is_volatile) {
            await print_indent(fd, level);
            await fprintf(fd, "volatile: true\n");
        }
        await print_indent(fd, level);
        await fprintf(fd, "src:\n");
        await export_yaml_val(fd, instr->u.copy.src, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.copy.dst, level + 1);
        break;
    case TAC_INSTRUCTION_GET_ADDRESS:
    case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
    case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
        await fprintf(fd, "%s\n",
                      instr->kind == TAC_INSTRUCTION_GET_ADDRESS_BYTE    ? "get_address_byte"
                      : instr->kind == TAC_INSTRUCTION_GET_ADDRESS_DECAY ? "get_address_decay"
                                                                         : "get_address");
        await print_indent(fd, level);
        await fprintf(fd, "src:\n");
        await export_yaml_val(fd, instr->u.get_address.src, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.get_address.dst, level + 1);
        break;
    case TAC_INSTRUCTION_LOAD:
    case TAC_INSTRUCTION_LOAD_BYTE:
        await fprintf(fd, "%s\n", instr->kind == TAC_INSTRUCTION_LOAD_BYTE ? "load_byte" : "load");
        if (instr->is_volatile) {
            await print_indent(fd, level);
            await fprintf(fd, "volatile: true\n");
        }
        await print_indent(fd, level);
        await fprintf(fd, "src_ptr:\n");
        await export_yaml_val(fd, instr->u.load.src_ptr, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.load.dst, level + 1);
        break;
    case TAC_INSTRUCTION_STORE:
    case TAC_INSTRUCTION_STORE_BYTE:
        await fprintf(fd, "%s\n",
                      instr->kind == TAC_INSTRUCTION_STORE_BYTE ? "store_byte" : "store");
        if (instr->is_volatile) {
            await print_indent(fd, level);
            await fprintf(fd, "volatile: true\n");
        }
        await print_indent(fd, level);
        await fprintf(fd, "src:\n");
        await export_yaml_val(fd, instr->u.store.src, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst_ptr:\n");
        await export_yaml_val(fd, instr->u.store.dst_ptr, level + 1);
        break;
    case TAC_INSTRUCTION_ADD_PTR:
        await fprintf(fd, "add_ptr\n");
        await print_indent(fd, level);
        await fprintf(fd, "ptr:\n");
        await export_yaml_val(fd, instr->u.add_ptr.ptr, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "index:\n");
        await export_yaml_val(fd, instr->u.add_ptr.index, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "scale: %d\n", instr->u.add_ptr.scale);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.add_ptr.dst, level + 1);
        break;
    case TAC_INSTRUCTION_PTR_DIFF:
        await fprintf(fd, "ptr_diff\n");
        await print_indent(fd, level);
        await fprintf(fd, "ptr_a:\n");
        await export_yaml_val(fd, instr->u.ptr_diff.ptr_a, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "ptr_b:\n");
        await export_yaml_val(fd, instr->u.ptr_diff.ptr_b, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.ptr_diff.dst, level + 1);
        break;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET:
        await fprintf(fd, "%s\n",
                      instr->kind == TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET ? "copy_byte_to_offset"
                                                                         : "copy_to_offset");
        if (instr->is_volatile) {
            await print_indent(fd, level);
            await fprintf(fd, "volatile: true\n");
        }
        await print_indent(fd, level);
        await fprintf(fd, "src:\n");
        await export_yaml_val(fd, instr->u.copy_to_offset.src, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "dst: %s\n",
                      instr->u.copy_to_offset.dst ? instr->u.copy_to_offset.dst : "");
        await print_indent(fd, level);
        await fprintf(fd, "offset: %d\n", instr->u.copy_to_offset.offset);
        break;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
        await fprintf(fd, "%s\n",
                      instr->kind == TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET ? "copy_byte_from_offset"
                                                                           : "copy_from_offset");
        if (instr->is_volatile) {
            await print_indent(fd, level);
            await fprintf(fd, "volatile: true\n");
        }
        await print_indent(fd, level);
        await fprintf(fd, "src: %s\n",
                      instr->u.copy_from_offset.src ? instr->u.copy_from_offset.src : "");
        await print_indent(fd, level);
        await fprintf(fd, "offset: %d\n", instr->u.copy_from_offset.offset);
        await print_indent(fd, level);
        await fprintf(fd, "dst:\n");
        await export_yaml_val(fd, instr->u.copy_from_offset.dst, level + 1);
        break;
    case TAC_INSTRUCTION_JUMP:
        await fprintf(fd, "jump\n");
        await print_indent(fd, level);
        await fprintf(fd, "target: %s\n", instr->u.jump.target ? instr->u.jump.target : "");
        break;
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
        await fprintf(fd, "jump_if_zero\n");
        await print_indent(fd, level);
        await fprintf(fd, "condition:\n");
        await export_yaml_val(fd, instr->u.jump_if_zero.condition, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "target: %s\n",
                      instr->u.jump_if_zero.target ? instr->u.jump_if_zero.target : "");
        break;
    case TAC_INSTRUCTION_JUMP_TABLE:
        await fprintf(fd, "jump_table\n");
        await print_indent(fd, level);
        await fprintf(fd, "index:\n");
        await export_yaml_val(fd, instr->u.jump_table.index, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "targets:\n");
        for (int i = 0; i < instr->u.jump_table.count; i++) {
            await print_indent(fd, level + 1);
            await fprintf(fd, "- %s\n", instr->u.jump_table.targets[i]);
        }
        await print_indent(fd, level);
        await fprintf(fd, "default: %s\n", instr->u.jump_table.default_target);
        break;
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
        await fprintf(fd, "jump_if_not_zero\n");
        await print_indent(fd, level);
        await fprintf(fd, "condition:\n");
        await export_yaml_val(fd, instr->u.jump_if_not_zero.condition, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "target: %s\n",
                      instr->u.jump_if_not_zero.target ? instr->u.jump_if_not_zero.target : "");
        break;
    case TAC_INSTRUCTION_LABEL:
        await fprintf(fd, "label\n");
        await print_indent(fd, level);
        await fprintf(fd, "name: %s\n", instr->u.label.name ? instr->u.label.name : "");
        break;
    case TAC_INSTRUCTION_FUN_CALL:
    case TAC_INSTRUCTION_FUN_CALL_NORETURN:
        await fprintf(
            fd, "%s\n",
            instr->kind == TAC_INSTRUCTION_FUN_CALL_NORETURN ? "fun_call_noreturn" : "fun_call");
        await print_indent(fd, level);
        await fprintf(fd, "fun_name: %s\n",
                      instr->u.fun_call.fun_name ? instr->u.fun_call.fun_name : "");
        if (instr->u.fun_call.indirect) {
            await print_indent(fd, level);
            await fprintf(fd, "indirect: true\n");
        }
        if (instr->u.fun_call.args) {
            await print_indent(fd, level);
            await fprintf(fd, "args:\n");
            await export_yaml_val_list(fd, instr->u.fun_call.args, level + 1);
        }
        if (instr->u.fun_call.dst) {
            await print_indent(fd, level);
            await fprintf(fd, "dst:\n");
            await export_yaml_val(fd, instr->u.fun_call.dst, level + 1);
        }
        if (tac_yaml_types && instr->u.fun_call.fun_type)
            await export_yaml_type_str(fd, "fun_type", instr->u.fun_call.fun_type, level);
        break;
    case TAC_INSTRUCTION_ALLOCATE_LOCAL:
        await fprintf(fd, "allocate_local\n");
        await print_indent(fd, level);
        await fprintf(fd, "name: %s\n",
                      instr->u.allocate_local.name ? instr->u.allocate_local.name : "");
        await print_indent(fd, level);
        await fprintf(fd, "size: %d\n", instr->u.allocate_local.size);
        await print_indent(fd, level);
        await fprintf(fd, "alignment: %d\n", instr->u.allocate_local.alignment);
        break;
    }
}

coro(braam_call *) void tac_export_yaml_instruction_list(FILE *fd, const Tac_Instruction *instr,
                                                         int level)
{
    while (instr) {
        await print_indent(fd, level);
        await fprintf(fd, "- instruction:\n");
        await export_yaml_instruction(fd, instr, level + 1);
        instr = instr->next;
    }
}

coro(braam_call *) void tac_export_yaml(FILE *fd, const Tac_TopLevel *tl)
{
    if (!fd || !tl)
        return;
    await fprintf(fd, "- toplevel:\n");
    switch (tl->kind) {
    case TAC_TOPLEVEL_FUNCTION:
        await fprintf(fd, "  kind: function\n");
        await fprintf(fd, "  name: %s\n", tl->u.function.name ? tl->u.function.name : "");
        await fprintf(fd, "  global: %s\n", tl->u.function.global ? "true" : "false");
        if (tl->u.function.noret)
            await fprintf(fd, "  noret: true\n");
        if (tac_yaml_types && tl->u.function.type)
            await export_yaml_type_str(fd, "type", tl->u.function.type, 1);
        if (tl->u.function.params) {
            await fprintf(fd, "  params:\n");
            await export_yaml_param_list(fd, "param", tl->u.function.params, 2);
        }
        if (tac_yaml_types && tl->u.function.locals) {
            await fprintf(fd, "  locals:\n");
            await export_yaml_param_list(fd, "local", tl->u.function.locals, 2);
        }
        if (tl->u.function.static_locals) {
            await fprintf(fd, "  static_locals:\n");
            for (const Tac_StaticLocal *sl = tl->u.function.static_locals; sl; sl = sl->next) {
                await fprintf(fd, "  - name: %s\n", sl->name ? sl->name : "");
                await fprintf(fd, "    type:\n");
                await export_yaml_type(fd, sl->type, 3);
                if (sl->alignment)
                    await fprintf(fd, "    alignment: %d\n", sl->alignment);
                if (sl->init_list) {
                    await fprintf(fd, "    init_list:\n");
                    await export_yaml_static_init_list(fd, sl->init_list, 3);
                }
            }
        }
        if (tl->u.function.body) {
            await fprintf(fd, "  body:\n");
            await tac_export_yaml_instruction_list(fd, tl->u.function.body, 2);
        }
        break;
    case TAC_TOPLEVEL_STATIC_VARIABLE:
        await fprintf(fd, "  kind: static_variable\n");
        await fprintf(fd, "  name: %s\n",
                      tl->u.static_variable.name ? tl->u.static_variable.name : "");
        await fprintf(fd, "  global: %s\n", tl->u.static_variable.global ? "true" : "false");
        await fprintf(fd, "  type:\n");
        await export_yaml_type(fd, tl->u.static_variable.type, 2);
        if (tl->u.static_variable.alignment)
            await fprintf(fd, "  alignment: %d\n", tl->u.static_variable.alignment);
        if (tl->u.static_variable.init_list) {
            await fprintf(fd, "  init_list:\n");
            await export_yaml_static_init_list(fd, tl->u.static_variable.init_list, 2);
        }
        break;
    case TAC_TOPLEVEL_STATIC_CONSTANT:
        await fprintf(fd, "  kind: static_constant\n");
        await fprintf(fd, "  name: %s\n",
                      tl->u.static_constant.name ? tl->u.static_constant.name : "");
        await fprintf(fd, "  type:\n");
        await export_yaml_type(fd, tl->u.static_constant.type, 2);
        if (tl->u.static_constant.init) {
            await fprintf(fd, "  init:\n");
            await export_yaml_static_init(fd, tl->u.static_constant.init, 2);
        }
        break;
    case TAC_TOPLEVEL_EXTERN:
        await fprintf(fd, "  kind: extern\n");
        await fprintf(fd, "  name: %s\n", tl->u.extern_.name ? tl->u.extern_.name : "");
        await fprintf(fd, "  type:\n");
        await export_yaml_type(fd, tl->u.extern_.type, 2);
        break;
    }
}
