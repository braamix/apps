#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "tac.h"
#include "xalloc.h"

#define INDENT_STEP 2

// Platform-independent "%a"-style hex float (see tac.h).
void tac_format_hex_double(char *out, size_t outsz, double v)
{
    if (v == 0.0 || isinf(v) || isnan(v)) { // zero/inf/nan: agree on both libcs
        snprintf(out, outsz, "%a", v);
        return;
    }
    int e;
    double m   = frexp(v, &e); // v = m * 2^e, 0.5 <= |m| < 1
    double sig = m * 2.0;      // 1 <= |sig| < 2  (a normal double)
    char buf[64];
    snprintf(buf, sizeof buf, "%a", sig); // "0x1.<frac>p+0" / "-0x1.<frac>p+0"
    char *p = strrchr(buf, 'p');
    *p      = '\0';
    snprintf(out, outsz, "%sp%+d", buf, e - 1);
}

// Render len bytes of a decoded string literal as printable text for the TAC dumps
// (see tac.h).  A decoded literal may hold embedded NULs and other non-printables, so
// the byte count is passed explicitly and every byte that cannot be shown as itself
// becomes a C escape — a named one where C has it, otherwise three octal digits, which
// unlike "\0" or "\x41" cannot run into a following digit.
char *tac_escape_string_bytes(const char *s, size_t len)
{
    if (!s)
        len = 0;
    char *out = xalloc(len * 4 + 1, __func__, __FILE__, __LINE__); // worst case "\ooo"
    char *p   = out;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
        case '\\':
        case '"':
            *p++ = '\\';
            *p++ = (char)c;
            break;
        case '\n':
            *p++ = '\\';
            *p++ = 'n';
            break;
        case '\r':
            *p++ = '\\';
            *p++ = 'r';
            break;
        case '\t':
            *p++ = '\\';
            *p++ = 't';
            break;
        default:
            if (c >= 0x20 && c < 0x7f) {
                *p++ = (char)c;
            } else {
                p += sprintf(p, "\\%03o", c);
            }
            break;
        }
    }
    *p = '\0';
    return out;
}

// Helper function to print indentation to a file
static void print_indent(FILE *fd, int depth)
{
    for (int i = 0; i < depth * INDENT_STEP; i++) {
        fputc(' ', fd);
    }
}

// Print a Tac_Const to a file
void tac_print_const(FILE *fd, const Tac_Const *constant, int depth)
{
    if (!constant) {
        print_indent(fd, depth);
        fprintf(fd, "Const: NULL\n");
        return;
    }
    print_indent(fd, depth);
    fprintf(fd, "Const: ");
    switch (constant->kind) {
    case TAC_CONST_INT:
        fprintf(fd, "int %" PRId64 "\n", constant->u.int_val);
        break;
    case TAC_CONST_LONG:
        fprintf(fd, "long %ld\n", constant->u.long_val);
        break;
    case TAC_CONST_LONG_LONG:
        fprintf(fd, "long long %lld\n", constant->u.long_long_val);
        break;
    case TAC_CONST_UINT:
        fprintf(fd, "uint %" PRIu64 "\n", constant->u.uint_val);
        break;
    case TAC_CONST_ULONG:
        fprintf(fd, "ulong %lu\n", constant->u.ulong_val);
        break;
    case TAC_CONST_ULONG_LONG:
        fprintf(fd, "ulong long %llu\n", constant->u.ulong_long_val);
        break;
    case TAC_CONST_FLOAT:
        fprintf(fd, "float %f\n", (double)constant->u.float_val);
        break;
    case TAC_CONST_DOUBLE: {
        char hex[64];
        tac_format_hex_double(hex, sizeof hex, constant->u.double_val);
        fprintf(fd, "double %s\n", hex);
        break;
    }
    case TAC_CONST_LONG_DOUBLE: {
        char buf[F128_BUFSIZE];
        fprintf(fd, "long_double %s\n", f128_format(constant->u.long_double_val, buf));
        break;
    }
    case TAC_CONST_SCHAR:
        fprintf(fd, "char %d\n", constant->u.char_val);
        break;
    case TAC_CONST_UCHAR:
        fprintf(fd, "uchar %u\n", constant->u.uchar_val);
        break;
    }
}

// Print a Tac_Val recursively to a file
void tac_print_val(FILE *fd, const Tac_Val *val, int depth)
{
    if (!val) {
        print_indent(fd, depth);
        fprintf(fd, "Val: NULL\n");
        return;
    }
    print_indent(fd, depth);
    fprintf(fd, "Val: %s\n", val->kind == TAC_VAL_CONSTANT ? "CONSTANT" : "VAR");
    print_indent(fd, depth + 1);
    if (val->kind == TAC_VAL_CONSTANT) {
        fprintf(fd, "Constant:\n");
        tac_print_const(fd, val->u.constant, depth + 2);
    } else {
        fprintf(fd, "Var: %s\n", val->u.var_name ? val->u.var_name : "(null)");
    }
    if (val->next) {
        print_indent(fd, depth + 1);
        fprintf(fd, "Next:\n");
        tac_print_val(fd, val->next, depth + 2);
    }
}

// Print a Tac_Type recursively to a file
void tac_print_type(FILE *fd, const Tac_Type *type, int depth)
{
    if (!type) {
        print_indent(fd, depth);
        fprintf(fd, "Type: NULL\n");
        return;
    }
    print_indent(fd, depth);
    fprintf(fd, "Type: ");
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
        fprintf(fd, "long long\n");
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
        fprintf(fd, "ulong long\n");
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
        break;
    case TAC_TYPE_POINTER:
        fprintf(fd, "pointer\n");
        break;
    case TAC_TYPE_ARRAY:
        fprintf(fd, "array\n");
        break;
    case TAC_TYPE_STRUCTURE:
        fprintf(fd, "structure\n");
        break;
    }
    if (type->kind == TAC_TYPE_FUN_TYPE) {
        print_indent(fd, depth + 1);
        fprintf(fd, "Params:%s\n", type->u.fun_type.variadic ? " (variadic)" : "");
        tac_print_type(fd, type->u.fun_type.param_types, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Return:\n");
        tac_print_type(fd, type->u.fun_type.ret_type, depth + 2);
    } else if (type->kind == TAC_TYPE_POINTER) {
        print_indent(fd, depth + 1);
        fprintf(fd, "Referenced:\n");
        tac_print_type(fd, type->u.pointer.target_type, depth + 2);
    } else if (type->kind == TAC_TYPE_ARRAY) {
        print_indent(fd, depth + 1);
        fprintf(fd, "Element:\n");
        tac_print_type(fd, type->u.array.elem_type, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Size: %d\n", type->u.array.size);
    } else if (type->kind == TAC_TYPE_STRUCTURE) {
        print_indent(fd, depth + 1);
        fprintf(fd, "Tag: %s%s\n", type->u.structure.tag ? type->u.structure.tag : "(null)",
                type->u.structure.is_union ? " (union)" : "");
        print_indent(fd, depth + 1);
        fprintf(fd, "Size: %d Align: %d\n", type->u.structure.size, type->u.structure.alignment);
        for (const Tac_Member *m = type->u.structure.members; m; m = m->next) {
            print_indent(fd, depth + 1);
            fprintf(fd, "Member: %s offset %d\n", m->name ? m->name : "(null)", m->offset);
            tac_print_type(fd, m->type, depth + 2);
        }
    }
    if (type->next) {
        print_indent(fd, depth + 1);
        fprintf(fd, "Next:\n");
        tac_print_type(fd, type->next, depth + 2);
    }
}

// Growable string for tac_type_str.
typedef struct {
    char *buf;
    size_t len, cap;
} TypeStr;

static void ts_puts(TypeStr *ts, const char *s)
{
    size_t n = strlen(s);
    if (ts->len + n + 1 > ts->cap) {
        size_t cap = (ts->len + n + 1) * 2;
        char *nb   = xalloc(cap, __func__, __FILE__, __LINE__);
        if (ts->buf) {
            memcpy(nb, ts->buf, ts->len);
            xfree(ts->buf);
        }
        ts->buf = nb;
        ts->cap = cap;
    }
    memcpy(ts->buf + ts->len, s, n + 1);
    ts->len += n;
}

static void ts_type(TypeStr *ts, const Tac_Type *type)
{
    static const char *const scalar[] = {
        [TAC_TYPE_SCHAR]       = "schar",
        [TAC_TYPE_UCHAR]       = "uchar",
        [TAC_TYPE_SHORT]       = "short",
        [TAC_TYPE_INT]         = "int",
        [TAC_TYPE_LONG]        = "long",
        [TAC_TYPE_LONG_LONG]   = "long_long",
        [TAC_TYPE_USHORT]      = "ushort",
        [TAC_TYPE_UINT]        = "uint",
        [TAC_TYPE_ULONG]       = "ulong",
        [TAC_TYPE_ULONG_LONG]  = "ulong_long",
        [TAC_TYPE_FLOAT]       = "float",
        [TAC_TYPE_DOUBLE]      = "double",
        [TAC_TYPE_LONG_DOUBLE] = "long_double",
        [TAC_TYPE_VOID]        = "void",
    };
    char num[64];

    if (!type) {
        ts_puts(ts, "?");
        return;
    }
    switch (type->kind) {
    case TAC_TYPE_POINTER:
        ts_puts(ts, "*");
        ts_type(ts, type->u.pointer.target_type);
        break;
    case TAC_TYPE_ARRAY:
        snprintf(num, sizeof num, "[%d]", type->u.array.size);
        ts_puts(ts, num);
        ts_type(ts, type->u.array.elem_type);
        break;
    case TAC_TYPE_STRUCTURE:
        ts_puts(ts, type->u.structure.is_union ? "union " : "struct ");
        ts_puts(ts, type->u.structure.tag ? type->u.structure.tag : "?");
        snprintf(num, sizeof num, "(%d,%d)", type->u.structure.size, type->u.structure.alignment);
        ts_puts(ts, num);
        break;
    case TAC_TYPE_FUN_TYPE:
        ts_puts(ts, "fn(");
        for (const Tac_Type *p = type->u.fun_type.param_types; p; p = p->next) {
            ts_type(ts, p);
            if (p->next || type->u.fun_type.variadic)
                ts_puts(ts, ", ");
        }
        if (type->u.fun_type.variadic)
            ts_puts(ts, "...");
        ts_puts(ts, ") -> ");
        ts_type(ts, type->u.fun_type.ret_type);
        break;
    default:
        ts_puts(ts, scalar[type->kind]);
        break;
    }
}

char *tac_type_str(const Tac_Type *type)
{
    TypeStr ts = { 0 };
    ts_type(&ts, type);
    return ts.buf;
}

// Print a Tac_Param recursively to a file
void tac_print_param(FILE *fd, const Tac_Param *param, int depth)
{
    if (!param) {
        print_indent(fd, depth);
        fprintf(fd, "Param: NULL\n");
        return;
    }
    print_indent(fd, depth);
    if (param->type) {
        char *ts = tac_type_str(param->type);
        fprintf(fd, "Param: %s : %s\n", param->name ? param->name : "(null)", ts);
        xfree(ts);
    } else {
        fprintf(fd, "Param: %s\n", param->name ? param->name : "(null)");
    }
    if (param->next) {
        print_indent(fd, depth + 1);
        fprintf(fd, "Next:\n");
        tac_print_param(fd, param->next, depth + 2);
    }
}

// Print a Tac_StaticInit recursively to a file
void tac_print_static_init(FILE *fd, const Tac_StaticInit *init, int depth)
{
    if (!init) {
        print_indent(fd, depth);
        fprintf(fd, "StaticInit: NULL\n");
        return;
    }
    print_indent(fd, depth);
    fprintf(fd, "StaticInit: ");
    switch (init->kind) {
    case TAC_STATIC_INIT_I8:
        fprintf(fd, "i8 %d\n", init->u.char_val);
        break;
    case TAC_STATIC_INIT_I16:
        fprintf(fd, "i16 %" PRId16 "\n", init->u.short_val);
        break;
    case TAC_STATIC_INIT_I32:
        fprintf(fd, "i32 %d\n", init->u.int_val);
        break;
    case TAC_STATIC_INIT_I64:
        fprintf(fd, "i64 %" PRId64 "\n", init->u.long_val);
        break;
    case TAC_STATIC_INIT_U8:
        fprintf(fd, "u8 %u\n", init->u.uchar_val);
        break;
    case TAC_STATIC_INIT_U16:
        fprintf(fd, "u16 %" PRIu16 "\n", init->u.ushort_val);
        break;
    case TAC_STATIC_INIT_U32:
        fprintf(fd, "u32 %u\n", init->u.uint_val);
        break;
    case TAC_STATIC_INIT_U64:
        fprintf(fd, "u64 %" PRIu64 "\n", init->u.ulong_val);
        break;
    case TAC_STATIC_INIT_FLOAT: {
        char hex[64];
        tac_format_hex_double(hex, sizeof hex, (double)init->u.float_val);
        fprintf(fd, "f32 %s\n", hex);
        break;
    }
    case TAC_STATIC_INIT_DOUBLE: {
        char hex[64];
        tac_format_hex_double(hex, sizeof hex, init->u.double_val);
        fprintf(fd, "f64 %s\n", hex);
        break;
    }
    case TAC_STATIC_INIT_LONG_DOUBLE: {
        char buf[F128_BUFSIZE];
        fprintf(fd, "ld128 %s\n", f128_format(init->u.long_double_val, buf));
        break;
    }
    case TAC_STATIC_INIT_ZERO:
        fprintf(fd, "zero %d bytes\n", init->u.zero_bytes);
        break;
    case TAC_STATIC_INIT_STRING: {
        char *text = tac_escape_string_bytes(init->u.string.val, init->u.string.len);
        fprintf(fd, "string \"%s", text);
        xfree(text);
        if (init->u.string.null_terminated)
            fprintf(fd, "\\0");
        fprintf(fd, "\"\n");
        break;
    }
    case TAC_STATIC_INIT_POINTER:
        fprintf(fd, "pointer %s offset=%d\n",
                init->u.pointer.name ? init->u.pointer.name : "(null)",
                init->u.pointer.byte_offset);
        break;
    case TAC_STATIC_INIT_FAT_POINTER:
        fprintf(fd, "fat_pointer %s offset=%d\n",
                init->u.pointer.name ? init->u.pointer.name : "(null)",
                init->u.pointer.byte_offset);
        break;
    }
    if (init->next) {
        print_indent(fd, depth + 1);
        fprintf(fd, "Next:\n");
        tac_print_static_init(fd, init->next, depth + 2);
    }
}

// Print a Tac_Instruction recursively to a file
void tac_print_instruction(FILE *fd, const Tac_Instruction *instr, int depth)
{
    if (!instr) {
        print_indent(fd, depth);
        fprintf(fd, "Instruction: NULL\n");
        return;
    }
    print_indent(fd, depth);
    fprintf(fd, "Instruction: ");
    switch (instr->kind) {
    case TAC_INSTRUCTION_RETURN:
        fprintf(fd, "return\n");
        break;
    case TAC_INSTRUCTION_SIGN_EXTEND:
        fprintf(fd, "sign_extend\n");
        break;
    case TAC_INSTRUCTION_TRUNCATE:
        fprintf(fd, "truncate\n");
        break;
    case TAC_INSTRUCTION_ZERO_EXTEND:
        fprintf(fd, "zero_extend\n");
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_INT:
        fprintf(fd, "double_to_int\n");
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_UINT:
        fprintf(fd, "double_to_uint\n");
        break;
    case TAC_INSTRUCTION_INT_TO_DOUBLE:
        fprintf(fd, "int_to_double\n");
        break;
    case TAC_INSTRUCTION_UINT_TO_DOUBLE:
        fprintf(fd, "uint_to_double\n");
        break;
    case TAC_INSTRUCTION_FLOAT_TO_DOUBLE:
        fprintf(fd, "float_to_double\n");
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_FLOAT:
        fprintf(fd, "double_to_float\n");
        break;
    case TAC_INSTRUCTION_INT_TO_FLOAT:
        fprintf(fd, "int_to_float\n");
        break;
    case TAC_INSTRUCTION_UINT_TO_FLOAT:
        fprintf(fd, "uint_to_float\n");
        break;
    case TAC_INSTRUCTION_FLOAT_TO_INT:
        fprintf(fd, "float_to_int\n");
        break;
    case TAC_INSTRUCTION_FLOAT_TO_UINT:
        fprintf(fd, "float_to_uint\n");
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_INT:
        fprintf(fd, "long_double_to_int\n");
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_UINT:
        fprintf(fd, "long_double_to_uint\n");
        break;
    case TAC_INSTRUCTION_INT_TO_LONG_DOUBLE:
        fprintf(fd, "int_to_long_double\n");
        break;
    case TAC_INSTRUCTION_UINT_TO_LONG_DOUBLE:
        fprintf(fd, "uint_to_long_double\n");
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_DOUBLE:
        fprintf(fd, "long_double_to_double\n");
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_LONG_DOUBLE:
        fprintf(fd, "double_to_long_double\n");
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_FLOAT:
        fprintf(fd, "long_double_to_float\n");
        break;
    case TAC_INSTRUCTION_FLOAT_TO_LONG_DOUBLE:
        fprintf(fd, "float_to_long_double\n");
        break;
    case TAC_INSTRUCTION_PTR_TO_CHAR_PTR:
        fprintf(fd, "ptr_to_char_ptr\n");
        break;
    case TAC_INSTRUCTION_CHAR_PTR_TO_PTR:
        fprintf(fd, "char_ptr_to_ptr\n");
        break;
    case TAC_INSTRUCTION_UNARY:
        fprintf(fd, "unary %s\n",
                instr->u.unary.op == TAC_UNARY_COMPLEMENT            ? "complement"
                : instr->u.unary.op == TAC_UNARY_COMPLEMENT_UNSIGNED ? "complement_unsigned"
                : instr->u.unary.op == TAC_UNARY_NEGATE              ? "negate"
                : instr->u.unary.op == TAC_UNARY_NEGATE_UNSIGNED     ? "negate_unsigned"
                : instr->u.unary.op == TAC_UNARY_NEGATE_DOUBLE       ? "negate_double"
                : instr->u.unary.op == TAC_UNARY_SQRT_DOUBLE         ? "sqrt_double"
                                                                     : "not");
        break;
    case TAC_INSTRUCTION_BINARY:
        fprintf(fd, "binary %s\n",
                instr->u.binary.op == TAC_BINARY_ADD                    ? "add"
                : instr->u.binary.op == TAC_BINARY_SUBTRACT             ? "subtract"
                : instr->u.binary.op == TAC_BINARY_MULTIPLY             ? "multiply"
                : instr->u.binary.op == TAC_BINARY_DIVIDE               ? "divide"
                : instr->u.binary.op == TAC_BINARY_REMAINDER            ? "remainder"
                : instr->u.binary.op == TAC_BINARY_EQUAL                ? "equal"
                : instr->u.binary.op == TAC_BINARY_NOT_EQUAL            ? "not_equal"
                : instr->u.binary.op == TAC_BINARY_LESS_THAN            ? "less_than"
                : instr->u.binary.op == TAC_BINARY_LESS_OR_EQUAL        ? "less_or_equal"
                : instr->u.binary.op == TAC_BINARY_GREATER_THAN         ? "greater_than"
                : instr->u.binary.op == TAC_BINARY_GREATER_OR_EQUAL     ? "greater_or_equal"
                : instr->u.binary.op == TAC_BINARY_BITWISE_AND          ? "bitwise_and"
                : instr->u.binary.op == TAC_BINARY_BITWISE_OR           ? "bitwise_or"
                : instr->u.binary.op == TAC_BINARY_BITWISE_XOR          ? "bitwise_xor"
                : instr->u.binary.op == TAC_BINARY_LEFT_SHIFT           ? "left_shift"
                : instr->u.binary.op == TAC_BINARY_ADD_DOUBLE           ? "add_double"
                : instr->u.binary.op == TAC_BINARY_SUBTRACT_DOUBLE      ? "subtract_double"
                : instr->u.binary.op == TAC_BINARY_MULTIPLY_DOUBLE      ? "multiply_double"
                : instr->u.binary.op == TAC_BINARY_DIVIDE_DOUBLE        ? "divide_double"
                : instr->u.binary.op == TAC_BINARY_LESS_THAN_DOUBLE     ? "less_than_double"
                : instr->u.binary.op == TAC_BINARY_LESS_OR_EQUAL_DOUBLE ? "less_or_equal_double"
                : instr->u.binary.op == TAC_BINARY_GREATER_THAN_DOUBLE  ? "greater_than_double"
                : instr->u.binary.op == TAC_BINARY_GREATER_OR_EQUAL_DOUBLE
                    ? "greater_or_equal_double"
                    : "right_shift");
        break;
    case TAC_INSTRUCTION_COPY:
        fprintf(fd, "copy\n");
        break;
    case TAC_INSTRUCTION_GET_ADDRESS:
        fprintf(fd, "get_address\n");
        break;
    case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
        fprintf(fd, "get_address_byte\n");
        break;
    case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
        fprintf(fd, "get_address_decay\n");
        break;
    case TAC_INSTRUCTION_LOAD:
        fprintf(fd, "load\n");
        break;
    case TAC_INSTRUCTION_LOAD_BYTE:
        fprintf(fd, "load_byte\n");
        break;
    case TAC_INSTRUCTION_STORE:
        fprintf(fd, "store\n");
        break;
    case TAC_INSTRUCTION_STORE_BYTE:
        fprintf(fd, "store_byte\n");
        break;
    case TAC_INSTRUCTION_ADD_PTR:
        fprintf(fd, "add_ptr\n");
        break;
    case TAC_INSTRUCTION_PTR_DIFF:
        fprintf(fd, "ptr_diff\n");
        break;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
        fprintf(fd, "copy_to_offset\n");
        break;
    case TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET:
        fprintf(fd, "copy_byte_to_offset\n");
        break;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
        fprintf(fd, "copy_from_offset\n");
        break;
    case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
        fprintf(fd, "copy_byte_from_offset\n");
        break;
    case TAC_INSTRUCTION_JUMP:
        fprintf(fd, "jump\n");
        break;
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
        fprintf(fd, "jump_if_zero\n");
        break;
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
        fprintf(fd, "jump_if_not_zero\n");
        break;
    case TAC_INSTRUCTION_LABEL:
        fprintf(fd, "label\n");
        break;
    case TAC_INSTRUCTION_JUMP_TABLE:
        fprintf(fd, "jump_table\n");
        break;
    case TAC_INSTRUCTION_FUN_CALL:
        fprintf(fd, "fun_call\n");
        break;
    case TAC_INSTRUCTION_FUN_CALL_NORETURN:
        fprintf(fd, "fun_call_noreturn\n");
        break;
    case TAC_INSTRUCTION_ALLOCATE_LOCAL:
        fprintf(fd, "allocate_local\n");
        break;
    }
    switch (instr->kind) {
    case TAC_INSTRUCTION_RETURN:
        print_indent(fd, depth + 1);
        fprintf(fd, "Src:\n");
        tac_print_val(fd, instr->u.return_.src, depth + 2);
        break;
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
        print_indent(fd, depth + 1);
        fprintf(fd, "Src:\n");
        tac_print_val(fd, instr->u.sign_extend.src, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Dst:\n");
        tac_print_val(fd, instr->u.sign_extend.dst, depth + 2);
        break;
    case TAC_INSTRUCTION_UNARY:
        print_indent(fd, depth + 1);
        fprintf(fd, "Src:\n");
        tac_print_val(fd, instr->u.unary.src, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Dst:\n");
        tac_print_val(fd, instr->u.unary.dst, depth + 2);
        break;
    case TAC_INSTRUCTION_BINARY:
        print_indent(fd, depth + 1);
        fprintf(fd, "Src1:\n");
        tac_print_val(fd, instr->u.binary.src1, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Src2:\n");
        tac_print_val(fd, instr->u.binary.src2, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Dst:\n");
        tac_print_val(fd, instr->u.binary.dst, depth + 2);
        break;
    case TAC_INSTRUCTION_COPY:
        print_indent(fd, depth + 1);
        fprintf(fd, "Src:\n");
        tac_print_val(fd, instr->u.copy.src, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Dst:\n");
        tac_print_val(fd, instr->u.copy.dst, depth + 2);
        break;
    case TAC_INSTRUCTION_GET_ADDRESS:
    case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
    case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
        print_indent(fd, depth + 1);
        fprintf(fd, "Src:\n");
        tac_print_val(fd, instr->u.get_address.src, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Dst:\n");
        tac_print_val(fd, instr->u.get_address.dst, depth + 2);
        break;
    case TAC_INSTRUCTION_LOAD:
    case TAC_INSTRUCTION_LOAD_BYTE:
        print_indent(fd, depth + 1);
        fprintf(fd, "Src_ptr:\n");
        tac_print_val(fd, instr->u.load.src_ptr, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Dst:\n");
        tac_print_val(fd, instr->u.load.dst, depth + 2);
        break;
    case TAC_INSTRUCTION_STORE:
    case TAC_INSTRUCTION_STORE_BYTE:
        print_indent(fd, depth + 1);
        fprintf(fd, "Src:\n");
        tac_print_val(fd, instr->u.store.src, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Dst_ptr:\n");
        tac_print_val(fd, instr->u.store.dst_ptr, depth + 2);
        break;
    case TAC_INSTRUCTION_ADD_PTR:
        print_indent(fd, depth + 1);
        fprintf(fd, "Ptr:\n");
        tac_print_val(fd, instr->u.add_ptr.ptr, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Index:\n");
        tac_print_val(fd, instr->u.add_ptr.index, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Scale: %d\n", instr->u.add_ptr.scale);
        print_indent(fd, depth + 1);
        fprintf(fd, "Dst:\n");
        tac_print_val(fd, instr->u.add_ptr.dst, depth + 2);
        break;
    case TAC_INSTRUCTION_PTR_DIFF:
        print_indent(fd, depth + 1);
        fprintf(fd, "PtrA:\n");
        tac_print_val(fd, instr->u.ptr_diff.ptr_a, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "PtrB:\n");
        tac_print_val(fd, instr->u.ptr_diff.ptr_b, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Dst:\n");
        tac_print_val(fd, instr->u.ptr_diff.dst, depth + 2);
        break;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET:
        print_indent(fd, depth + 1);
        fprintf(fd, "Src:\n");
        tac_print_val(fd, instr->u.copy_to_offset.src, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Dst: %s\n",
                instr->u.copy_to_offset.dst ? instr->u.copy_to_offset.dst : "(null)");
        print_indent(fd, depth + 1);
        fprintf(fd, "Offset: %d\n", instr->u.copy_to_offset.offset);
        break;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
        print_indent(fd, depth + 1);
        fprintf(fd, "Src: %s\n",
                instr->u.copy_from_offset.src ? instr->u.copy_from_offset.src : "(null)");
        print_indent(fd, depth + 1);
        fprintf(fd, "Offset: %d\n", instr->u.copy_from_offset.offset);
        print_indent(fd, depth + 1);
        fprintf(fd, "Dst:\n");
        tac_print_val(fd, instr->u.copy_from_offset.dst, depth + 2);
        break;
    case TAC_INSTRUCTION_JUMP:
        print_indent(fd, depth + 1);
        fprintf(fd, "Target: %s\n", instr->u.jump.target ? instr->u.jump.target : "(null)");
        break;
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
        print_indent(fd, depth + 1);
        fprintf(fd, "Condition:\n");
        tac_print_val(fd, instr->u.jump_if_zero.condition, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Target: %s\n",
                instr->u.jump_if_zero.target ? instr->u.jump_if_zero.target : "(null)");
        break;
    case TAC_INSTRUCTION_JUMP_TABLE:
        print_indent(fd, depth + 1);
        fprintf(fd, "Index:\n");
        tac_print_val(fd, instr->u.jump_table.index, depth + 2);
        for (int i = 0; i < instr->u.jump_table.count; i++) {
            print_indent(fd, depth + 1);
            fprintf(fd, "Target %d: %s\n", i, instr->u.jump_table.targets[i]);
        }
        print_indent(fd, depth + 1);
        fprintf(fd, "Default: %s\n", instr->u.jump_table.default_target);
        break;
    case TAC_INSTRUCTION_LABEL:
        print_indent(fd, depth + 1);
        fprintf(fd, "Name: %s\n", instr->u.label.name ? instr->u.label.name : "(null)");
        break;
    case TAC_INSTRUCTION_FUN_CALL:
    case TAC_INSTRUCTION_FUN_CALL_NORETURN:
        print_indent(fd, depth + 1);
        fprintf(fd, "Fun_name: %s%s\n",
                instr->u.fun_call.fun_name ? instr->u.fun_call.fun_name : "(null)",
                instr->u.fun_call.indirect ? " (indirect)" : "");
        print_indent(fd, depth + 1);
        fprintf(fd, "Args:\n");
        tac_print_val(fd, instr->u.fun_call.args, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Dst:\n");
        tac_print_val(fd, instr->u.fun_call.dst, depth + 2);
        if (instr->u.fun_call.fun_type) {
            char *ts = tac_type_str(instr->u.fun_call.fun_type);
            print_indent(fd, depth + 1);
            fprintf(fd, "Fun_type: %s\n", ts);
            xfree(ts);
        }
        break;
    case TAC_INSTRUCTION_ALLOCATE_LOCAL:
        print_indent(fd, depth + 1);
        fprintf(fd, "Name: %s size=%d align=%d\n",
                instr->u.allocate_local.name ? instr->u.allocate_local.name : "(null)",
                instr->u.allocate_local.size, instr->u.allocate_local.alignment);
        break;
    }
    if (instr->next) {
        print_indent(fd, depth + 1);
        fprintf(fd, "Next:\n");
        tac_print_instruction(fd, instr->next, depth + 2);
    }
}

// Print a Tac_TopLevel recursively to a file
void tac_print_toplevel(FILE *fd, const Tac_TopLevel *toplevel, int depth)
{
    if (!toplevel) {
        print_indent(fd, depth);
        fprintf(fd, "TopLevel: NULL\n");
        return;
    }
    print_indent(fd, depth);
    fprintf(fd, "TopLevel: %s\n",
            toplevel->kind == TAC_TOPLEVEL_FUNCTION          ? "FUNCTION"
            : toplevel->kind == TAC_TOPLEVEL_STATIC_VARIABLE ? "STATIC_VARIABLE"
            : toplevel->kind == TAC_TOPLEVEL_EXTERN          ? "EXTERN"
                                                             : "STATIC_CONSTANT");
    switch (toplevel->kind) {
    case TAC_TOPLEVEL_FUNCTION:
        print_indent(fd, depth + 1);
        fprintf(fd, "Name: %s\n", toplevel->u.function.name ? toplevel->u.function.name : "(null)");
        print_indent(fd, depth + 1);
        fprintf(fd, "Global: %d\n", toplevel->u.function.global);
        if (toplevel->u.function.type) {
            print_indent(fd, depth + 1);
            fprintf(fd, "Type:\n");
            tac_print_type(fd, toplevel->u.function.type, depth + 2);
        }
        print_indent(fd, depth + 1);
        fprintf(fd, "Params:\n");
        tac_print_param(fd, toplevel->u.function.params, depth + 2);
        if (toplevel->u.function.locals) {
            print_indent(fd, depth + 1);
            fprintf(fd, "Locals:\n");
            tac_print_param(fd, toplevel->u.function.locals, depth + 2);
        }
        for (const Tac_StaticLocal *sl = toplevel->u.function.static_locals; sl; sl = sl->next) {
            print_indent(fd, depth + 1);
            fprintf(fd, "StaticLocal: %s\n", sl->name ? sl->name : "(null)");
            tac_print_type(fd, sl->type, depth + 2);
            if (sl->alignment) {
                print_indent(fd, depth + 2);
                fprintf(fd, "Alignment: %d\n", sl->alignment);
            }
            tac_print_static_init(fd, sl->init_list, depth + 2);
        }
        print_indent(fd, depth + 1);
        fprintf(fd, "Body:\n");
        tac_print_instruction(fd, toplevel->u.function.body, depth + 2);
        break;
    case TAC_TOPLEVEL_STATIC_VARIABLE:
        print_indent(fd, depth + 1);
        fprintf(fd, "Name: %s\n",
                toplevel->u.static_variable.name ? toplevel->u.static_variable.name : "(null)");
        print_indent(fd, depth + 1);
        fprintf(fd, "Global: %d\n", toplevel->u.static_variable.global);
        print_indent(fd, depth + 1);
        fprintf(fd, "Type:\n");
        tac_print_type(fd, toplevel->u.static_variable.type, depth + 2);
        if (toplevel->u.static_variable.alignment) {
            print_indent(fd, depth + 1);
            fprintf(fd, "Alignment: %d\n", toplevel->u.static_variable.alignment);
        }
        print_indent(fd, depth + 1);
        fprintf(fd, "Init_list:\n");
        tac_print_static_init(fd, toplevel->u.static_variable.init_list, depth + 2);
        break;
    case TAC_TOPLEVEL_STATIC_CONSTANT:
        print_indent(fd, depth + 1);
        fprintf(fd, "Name: %s\n",
                toplevel->u.static_constant.name ? toplevel->u.static_constant.name : "(null)");
        print_indent(fd, depth + 1);
        fprintf(fd, "Type:\n");
        tac_print_type(fd, toplevel->u.static_constant.type, depth + 2);
        print_indent(fd, depth + 1);
        fprintf(fd, "Init:\n");
        tac_print_static_init(fd, toplevel->u.static_constant.init, depth + 2);
        break;
    case TAC_TOPLEVEL_EXTERN:
        print_indent(fd, depth + 1);
        fprintf(fd, "Name: %s\n", toplevel->u.extern_.name ? toplevel->u.extern_.name : "(null)");
        print_indent(fd, depth + 1);
        fprintf(fd, "Type:\n");
        tac_print_type(fd, toplevel->u.extern_.type, depth + 2);
        break;
    }
    if (toplevel->next) {
        print_indent(fd, depth + 1);
        fprintf(fd, "Next:\n");
        tac_print_toplevel(fd, toplevel->next, depth + 2);
    }
}

// Main print function
void tac_print_program(FILE *fd, const Tac_Program *program)
{
    if (!program) {
        fprintf(fd, "Program: null\n");
        return;
    }
    fprintf(fd, "Program:\n");
    tac_print_toplevel(fd, program->decls, 2);
}
