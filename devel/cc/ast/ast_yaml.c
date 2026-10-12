#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"

#define INDENT_STEP 2

static coro(braam_call *) void print_indent(FILE *fd, int level)
{
    for (int i = 0; i < level * INDENT_STEP; i++) {
        await fputc(' ', fd);
    }
}

static coro(braam_call *) void export_type(FILE *fd, Type *type, int level);
static coro(braam_call *) void export_expr(FILE *fd, Expr *expr, int level);
static coro(braam_call *) void export_stmt(FILE *fd, Stmt *stmt, int level);
static coro(braam_call *) void export_decl(FILE *fd, Declaration *decl, int level);
static coro(braam_call *) void export_decl_spec(FILE *fd, DeclSpec *ds, int level);

static coro(braam_call *) void export_ident(FILE *fd, Ident ident, int level)
{
    if (ident) {
        await print_indent(fd, level);
        await fprintf(fd, "name: %s\n", ident);
    }
}

static coro(braam_call *) void export_type_qualifier(FILE *fd, const TypeQualifier *qual, int level)
{
    while (qual) {
        await print_indent(fd, level);
        await fprintf(fd, "- kind: ");
        switch (qual->kind) {
        case TYPE_QUALIFIER_CONST:
            await fprintf(fd, "const\n");
            break;
        case TYPE_QUALIFIER_RESTRICT:
            await fprintf(fd, "restrict\n");
            break;
        case TYPE_QUALIFIER_VOLATILE:
            await fprintf(fd, "volatile\n");
            break;
        case TYPE_QUALIFIER_ATOMIC:
            await fprintf(fd, "atomic\n");
            break;
        }
        qual = qual->next;
    }
}

static coro(braam_call *) void export_alignment_spec(FILE *fd, AlignmentSpec *as, int level);

static coro(braam_call *) void export_field(FILE *fd, Field *field, int level)
{
    while (field) {
        await print_indent(fd, level);
        switch (field->kind) {
        case FIELD_MEMBER:
            await fprintf(fd, "- field:\n");
            await print_indent(fd, level + 1);
            await fprintf(fd, "type:\n");
            await export_type(fd, field->u.member.type, level + 2);
            await export_ident(fd, field->u.member.name, level + 1);
            if (field->u.member.bitfield) {
                await print_indent(fd, level + 1);
                await fprintf(fd, "bitfield:\n");
                await export_expr(fd, field->u.member.bitfield, level + 2);
            }
            await export_alignment_spec(fd, field->u.member.align_spec, level + 1);
            break;
        case FIELD_STATIC_ASSERT:
            await fprintf(fd, "- static_assert:\n");
            await print_indent(fd, level + 1);
            await fprintf(fd, "condition:\n");
            await export_expr(fd, field->u.static_assrt.condition, level + 2);
            if (field->u.static_assrt.message) {
                await print_indent(fd, level + 1);
                await fprintf(fd, "message: %s\n", field->u.static_assrt.message);
            }
            break;
        }
        field = field->next;
    }
}

static coro(braam_call *) void export_enumerator(FILE *fd, Enumerator *enumr, int level)
{
    while (enumr) {
        await print_indent(fd, level);
        await fprintf(fd, "- enumerator:\n");
        await export_ident(fd, enumr->name, level + 1);
        if (enumr->value) {
            await print_indent(fd, level + 1);
            await fprintf(fd, "value:\n");
            await export_expr(fd, enumr->value, level + 2);
        }
        enumr = enumr->next;
    }
}

static coro(braam_call *) void export_param(FILE *fd, Param *param, int level)
{
    while (param) {
        await print_indent(fd, level);
        await fprintf(fd, "- param:\n");
        await export_ident(fd, param->name, level + 1);
        await print_indent(fd, level + 1);
        await fprintf(fd, "type:\n");
        await export_type(fd, param->type, level + 2);
        if (param->specifiers) {
            await print_indent(fd, level + 1);
            await fprintf(fd, "specifiers:\n");
            await export_decl_spec(fd, param->specifiers, level + 2);
        }
        param = param->next;
    }
}

static coro(braam_call *) void export_type(FILE *fd, Type *type, int level)
{
    if (!type)
        return;
    await print_indent(fd, level);
    await fprintf(fd, "kind: ");
    switch (type->kind) {
    case TYPE_VOID:
        await fprintf(fd, "void\n");
        break;
    case TYPE_BOOL:
        await fprintf(fd, "bool\n");
        break;
    case TYPE_CHAR:
        await fprintf(fd, "char\n");
        break;
    case TYPE_SCHAR:
        await fprintf(fd, "signed_char\n");
        break;
    case TYPE_UCHAR:
        await fprintf(fd, "unsigned_char\n");
        break;
    case TYPE_SHORT:
        await fprintf(fd, "short\n");
        break;
    case TYPE_USHORT:
        await fprintf(fd, "unsigned_short\n");
        break;
    case TYPE_INT:
        await fprintf(fd, "int\n");
        break;
    case TYPE_UINT:
        await fprintf(fd, "unsigned_int\n");
        break;
    case TYPE_LONG:
        await fprintf(fd, "long\n");
        break;
    case TYPE_ULONG:
        await fprintf(fd, "unsigned_long\n");
        break;
    case TYPE_LONG_LONG:
        await fprintf(fd, "long_long\n");
        break;
    case TYPE_ULONG_LONG:
        await fprintf(fd, "unsigned_long_long\n");
        break;
    case TYPE_SIGNED:
        await fprintf(fd, "signed\n");
        break;
    case TYPE_UNSIGNED:
        await fprintf(fd, "unsigned\n");
        break;
    case TYPE_FLOAT:
        await fprintf(fd, "float\n");
        break;
    case TYPE_DOUBLE:
        await fprintf(fd, "double\n");
        break;
    case TYPE_LONG_DOUBLE:
        await fprintf(fd, "long_double\n");
        break;
    case TYPE_COMPLEX:
        await fprintf(fd, "complex\n");
        break;
    case TYPE_IMAGINARY:
        await fprintf(fd, "imaginary\n");
        break;
    case TYPE_POINTER:
        await fprintf(fd, "pointer\n");
        break;
    case TYPE_ARRAY:
        await fprintf(fd, "array\n");
        break;
    case TYPE_FUNCTION:
        await fprintf(fd, "function\n");
        break;
    case TYPE_STRUCT:
        await fprintf(fd, "struct\n");
        break;
    case TYPE_UNION:
        await fprintf(fd, "union\n");
        break;
    case TYPE_ENUM:
        await fprintf(fd, "enum\n");
        break;
    case TYPE_TYPEDEF_NAME:
        await fprintf(fd, "typedef_name\n");
        break;
    case TYPE_ATOMIC:
        await fprintf(fd, "atomic\n");
        break;
    }
    if (type->qualifiers) {
        await print_indent(fd, level);
        await fprintf(fd, "qualifiers:\n");
        await export_type_qualifier(fd, type->qualifiers, level + 1);
    }
    switch (type->kind) {
    case TYPE_COMPLEX:
    case TYPE_IMAGINARY:
        await print_indent(fd, level);
        await fprintf(fd, "base:\n");
        await export_type(fd, type->u.complex.base, level + 1);
        break;
    case TYPE_POINTER:
        await print_indent(fd, level);
        await fprintf(fd, "target:\n");
        await export_type(fd, type->u.pointer.target, level + 1);
        if (type->u.pointer.qualifiers) {
            await print_indent(fd, level);
            await fprintf(fd, "pointer_qualifiers:\n");
            await export_type_qualifier(fd, type->u.pointer.qualifiers, level + 1);
        }
        break;
    case TYPE_ARRAY:
        await print_indent(fd, level);
        await fprintf(fd, "element:\n");
        await export_type(fd, type->u.array.element, level + 1);
        if (type->u.array.size) {
            await print_indent(fd, level);
            await fprintf(fd, "size:\n");
            await export_expr(fd, type->u.array.size, level + 1);
        }
        if (type->u.array.qualifiers) {
            await print_indent(fd, level);
            await fprintf(fd, "array_qualifiers:\n");
            await export_type_qualifier(fd, type->u.array.qualifiers, level + 1);
        }
        await print_indent(fd, level);
        await fprintf(fd, "is_static: %s\n", type->u.array.is_static ? "true" : "false");
        break;
    case TYPE_FUNCTION:
        await print_indent(fd, level);
        await fprintf(fd, "return_type:\n");
        await export_type(fd, type->u.function.return_type, level + 1);
        if (type->u.function.params) {
            await print_indent(fd, level);
            await fprintf(fd, "params:\n");
            await export_param(fd, type->u.function.params, level + 1);
        }
        await print_indent(fd, level);
        await fprintf(fd, "variadic: %s\n", type->u.function.variadic ? "true" : "false");
        break;
    case TYPE_STRUCT:
    case TYPE_UNION:
        await export_ident(fd, type->u.struct_t.name, level);
        if (type->u.struct_t.fields) {
            await print_indent(fd, level);
            await fprintf(fd, "fields:\n");
            await export_field(fd, type->u.struct_t.fields, level + 1);
        }
        if (type->u.struct_t.frame_yield) {
            await print_indent(fd, level);
            await fprintf(fd, "frame_yield:\n");
            await export_type(fd, type->u.struct_t.frame_yield, level + 1);
            await print_indent(fd, level);
            await fprintf(fd, "frame_result:\n");
            await export_type(fd, type->u.struct_t.frame_result, level + 1);
        }
        break;
    case TYPE_ENUM:
        await export_ident(fd, type->u.enum_t.name, level);
        if (type->u.enum_t.enumerators) {
            await print_indent(fd, level);
            await fprintf(fd, "enumerators:\n");
            await export_enumerator(fd, type->u.enum_t.enumerators, level + 1);
        }
        break;
    case TYPE_TYPEDEF_NAME:
        await export_ident(fd, type->u.typedef_name.name, level);
        break;
    case TYPE_ATOMIC:
        await print_indent(fd, level);
        await fprintf(fd, "base:\n");
        await export_type(fd, type->u.atomic.base, level + 1);
        break;
    default:
        break;
    }
}

static coro(braam_call *) void export_storage_class(FILE *fd, StorageClass kind, int level)
{
    if (!kind)
        return;
    await print_indent(fd, level);
    await fprintf(fd, "storage_class: ");
    switch (kind) {
    case STORAGE_CLASS_TYPEDEF:
        await fprintf(fd, "typedef\n");
        break;
    case STORAGE_CLASS_EXTERN:
        await fprintf(fd, "extern\n");
        break;
    case STORAGE_CLASS_STATIC:
        await fprintf(fd, "static\n");
        break;
    case STORAGE_CLASS_THREAD_LOCAL:
        await fprintf(fd, "thread_local\n");
        break;
    case STORAGE_CLASS_AUTO:
        await fprintf(fd, "auto\n");
        break;
    case STORAGE_CLASS_REGISTER:
        await fprintf(fd, "register\n");
        break;
    case STORAGE_CLASS_NONE:
        await fprintf(fd, "none\n");
        break;
    }
}

static coro(braam_call *) void export_function_spec(FILE *fd, const FunctionSpec *fs, int level)
{
    while (fs) {
        await print_indent(fd, level);
        await fprintf(fd, "- kind: ");
        switch (fs->kind) {
        case FUNC_SPEC_INLINE:
            await fprintf(fd, "inline\n");
            break;
        case FUNC_SPEC_NORETURN:
            await fprintf(fd, "noreturn\n");
            break;
        case FUNC_SPEC_CORO:
            await fprintf(fd, "coro\n");
            await print_indent(fd, level + 1);
            await fprintf(fd, "yield_type:\n");
            await export_type(fd, fs->yield_type, level + 2);
            break;
        }
        fs = fs->next;
    }
}

static coro(braam_call *) void export_alignment_spec(FILE *fd, AlignmentSpec *as, int level)
{
    if (!as)
        return;
    await print_indent(fd, level);
    await fprintf(fd, "alignment:\n");
    await print_indent(fd, level + 1);
    await fprintf(fd, "kind: %s\n", as->kind == ALIGN_SPEC_TYPE ? "type" : "expr");
    if (as->kind == ALIGN_SPEC_TYPE) {
        await print_indent(fd, level + 1);
        await fprintf(fd, "type:\n");
        await export_type(fd, as->u.type, level + 2);
    } else {
        await print_indent(fd, level + 1);
        await fprintf(fd, "expr:\n");
        await export_expr(fd, as->u.expr, level + 2);
    }
}

static coro(braam_call *) void export_init_item(FILE *fd, InitItem *item, int level)
{
    if (!item)
        return;
    await print_indent(fd, level);
    await fprintf(fd, "items:\n");
    while (item) {
        await print_indent(fd, level + 1);
        await fprintf(fd, "- item:\n");
        Designator *des = item->designators;
        if (des) {
            await print_indent(fd, level + 2);
            await fprintf(fd, "designators:\n");
            while (des) {
                await print_indent(fd, level + 3);
                await fprintf(fd, "- kind: %s\n",
                              des->kind == DESIGNATOR_ARRAY ? "array" : "field");
                if (des->kind == DESIGNATOR_ARRAY) {
                    await print_indent(fd, level + 4);
                    await fprintf(fd, "expr:\n");
                    await export_expr(fd, des->u.expr, level + 5);
                } else {
                    await export_ident(fd, des->u.name, level + 4);
                }
                des = des->next;
            }
        }
        await print_indent(fd, level + 2);
        await fprintf(fd, "init:\n");
        await print_indent(fd, level + 3);
        await fprintf(fd, "kind: %s\n",
                      item->init->kind == INITIALIZER_SINGLE ? "single" : "compound");
        if (item->init->kind == INITIALIZER_SINGLE) {
            await print_indent(fd, level + 3);
            await fprintf(fd, "expr:\n");
            await export_expr(fd, item->init->u.expr, level + 4);
        } else {
            await export_init_item(fd, item->init->u.items,
                                   level + 4); // Recursive for nested compounds
        }
        item = item->next;
    }
}

static coro(braam_call *) void export_init_declarator(FILE *fd, InitDeclarator *id, int level)
{
    while (id) {
        await print_indent(fd, level);
        await fprintf(fd, "- declarator:\n");
        await print_indent(fd, level + 1);
        await fprintf(fd, "type:\n");
        await export_type(fd, id->type, level + 2);
        await export_ident(fd, id->name, level + 1);
        if (id->init) {
            await print_indent(fd, level + 1);
            await fprintf(fd, "init:\n");
            await print_indent(fd, level + 2);
            await fprintf(fd, "kind: %s\n",
                          id->init->kind == INITIALIZER_SINGLE ? "single" : "compound");
            if (id->init->kind == INITIALIZER_SINGLE) {
                await print_indent(fd, level + 2);
                await fprintf(fd, "expr:\n");
                await export_expr(fd, id->init->u.expr, level + 3);
            } else {
                await export_init_item(fd, id->init->u.items, level + 2);
            }
        }
        id = id->next;
    }
}

static coro(braam_call *) void export_decl_spec(FILE *fd, DeclSpec *ds, int level)
{
    if (ds->qualifiers) {
        await print_indent(fd, level);
        await fprintf(fd, "qualifiers:\n");
        await export_type_qualifier(fd, ds->qualifiers, level + 1);
    }
    await export_storage_class(fd, ds->storage, level);
    if (ds->func_specs) {
        await print_indent(fd, level);
        await fprintf(fd, "function_specs:\n");
        await export_function_spec(fd, ds->func_specs, level + 1);
    }
    await export_alignment_spec(fd, ds->align_spec, level);
}

static coro(braam_call *) void export_decl(FILE *fd, Declaration *decl, int level)
{
    while (decl) {
        await print_indent(fd, level);
        await fprintf(fd, "- declaration:\n");
        await print_indent(fd, level + 1);
        await fprintf(fd, "kind: ");
        switch (decl->kind) {
        case DECL_VAR:
            await fprintf(fd, "var\n");
            break;
        case DECL_STATIC_ASSERT:
            await fprintf(fd, "static_assert\n");
            break;
        case DECL_EMPTY:
            await fprintf(fd, "empty\n");
            break;
        }
        if (decl->kind == DECL_VAR) {
            if (decl->u.var.specifiers) {
                await print_indent(fd, level + 1);
                await fprintf(fd, "specifiers:\n");
                await export_decl_spec(fd, decl->u.var.specifiers, level + 2);
            }
            if (decl->u.var.declarators) {
                await print_indent(fd, level + 1);
                await fprintf(fd, "declarators:\n");
                await export_init_declarator(fd, decl->u.var.declarators, level + 2);
            }
        } else if (decl->kind == DECL_STATIC_ASSERT) {
            await print_indent(fd, level + 1);
            await fprintf(fd, "condition:\n");
            await export_expr(fd, decl->u.static_assrt.condition, level + 2);
            if (decl->u.static_assrt.message) {
                await print_indent(fd, level + 1);
                await fprintf(fd, "message: %s\n", decl->u.static_assrt.message);
            }
        } else {
            if (decl->u.empty.specifiers) {
                await print_indent(fd, level + 1);
                await fprintf(fd, "specifiers:\n");
                await export_decl_spec(fd, decl->u.empty.specifiers, level + 2);
            }
            await print_indent(fd, level + 1);
            await fprintf(fd, "type:\n");
            await export_type(fd, decl->u.empty.type, level + 2);
        }
        decl = decl->next;
    }
}

static coro(braam_call *) void export_literal(FILE *fd, const Literal *lit, int level)
{
    await print_indent(fd, level);
    await fprintf(fd, "kind: ");
    switch (lit->kind) {
    case LITERAL_INT:
        await fprintf(fd, "int\n");
        break;
    case LITERAL_LONG:
        await fprintf(fd, "long\n");
        break;
    case LITERAL_LONG_LONG:
        await fprintf(fd, "long_long\n");
        break;
    case LITERAL_UINT:
        await fprintf(fd, "uint\n");
        break;
    case LITERAL_ULONG:
        await fprintf(fd, "ulong\n");
        break;
    case LITERAL_ULONG_LONG:
        await fprintf(fd, "ulong_long\n");
        break;
    case LITERAL_FLOAT:
        await fprintf(fd, "float\n");
        break;
    case LITERAL_DOUBLE:
        await fprintf(fd, "double\n");
        break;
    case LITERAL_LONG_DOUBLE:
        await fprintf(fd, "long_double\n");
        break;
    case LITERAL_CHAR:
        await fprintf(fd, "char\n");
        break;
    case LITERAL_STRING:
        await fprintf(fd, "string\n");
        break;
    case LITERAL_ENUM:
        await fprintf(fd, "enum\n");
        break;
    }
    await print_indent(fd, level);
    await fprintf(fd, "value: ");
    switch (lit->kind) {
    case LITERAL_INT:
        await fprintf(fd, "%" PRId64 "\n", lit->u.int_val);
        break;
    case LITERAL_LONG:
        await fprintf(fd, "%" PRId64 "\n", lit->u.long_val);
        break;
    case LITERAL_LONG_LONG:
        await fprintf(fd, "%lld\n", lit->u.long_long_val);
        break;
    case LITERAL_UINT:
        await fprintf(fd, "%" PRIu64 "\n", lit->u.uint_val);
        break;
    case LITERAL_ULONG:
        await fprintf(fd, "%" PRIu64 "\n", lit->u.ulong_val);
        break;
    case LITERAL_ULONG_LONG:
        await fprintf(fd, "%llu\n", lit->u.ulong_long_val);
        break;
    case LITERAL_FLOAT:
        await fprintf(fd, "%a\n", lit->u.real_val);
        break;
    case LITERAL_DOUBLE:
        await fprintf(fd, "%a\n", lit->u.real_val);
        break;
    case LITERAL_LONG_DOUBLE: {
        char buf[F128_BUFSIZE];
        await fprintf(fd, "%s\n", f128_format(lit->u.long_double_val, buf));
        break;
    }
    case LITERAL_CHAR:
        await fprintf(fd, "'%c'\n", lit->u.char_val);
        break;
    case LITERAL_STRING:
        await fprintf(fd, "%s\n", lit->u.string_val);
        break;
    case LITERAL_ENUM:
        await fprintf(fd, "%s\n", lit->u.enum_const);
        break;
    }
}

static coro(braam_call *) void export_unary_op(FILE *fd, UnaryOp kind, int level)
{
    await print_indent(fd, level);
    await fprintf(fd, "op: ");
    switch (kind) {
    case UNARY_ADDRESS:
        await fprintf(fd, "address\n");
        break;
    case UNARY_DEREF:
        await fprintf(fd, "deref\n");
        break;
    case UNARY_PLUS:
        await fprintf(fd, "plus\n");
        break;
    case UNARY_NEG:
        await fprintf(fd, "neg\n");
        break;
    case UNARY_BIT_NOT:
        await fprintf(fd, "bit_not\n");
        break;
    case UNARY_LOG_NOT:
        await fprintf(fd, "log_not\n");
        break;
    case UNARY_PRE_INC:
        await fprintf(fd, "pre_inc\n");
        break;
    case UNARY_PRE_DEC:
        await fprintf(fd, "pre_dec\n");
        break;
    }
}

static coro(braam_call *) void export_binary_op(FILE *fd, BinaryOp kind, int level)
{
    await print_indent(fd, level);
    await fprintf(fd, "op: ");
    switch (kind) {
    case BINARY_MUL:
        await fprintf(fd, "mul\n");
        break;
    case BINARY_DIV:
        await fprintf(fd, "div\n");
        break;
    case BINARY_MOD:
        await fprintf(fd, "mod\n");
        break;
    case BINARY_ADD:
        await fprintf(fd, "add\n");
        break;
    case BINARY_SUB:
        await fprintf(fd, "sub\n");
        break;
    case BINARY_LEFT_SHIFT:
        await fprintf(fd, "left_shift\n");
        break;
    case BINARY_RIGHT_SHIFT:
        await fprintf(fd, "right_shift\n");
        break;
    case BINARY_LT:
        await fprintf(fd, "lt\n");
        break;
    case BINARY_GT:
        await fprintf(fd, "gt\n");
        break;
    case BINARY_LE:
        await fprintf(fd, "le\n");
        break;
    case BINARY_GE:
        await fprintf(fd, "ge\n");
        break;
    case BINARY_EQ:
        await fprintf(fd, "eq\n");
        break;
    case BINARY_NE:
        await fprintf(fd, "ne\n");
        break;
    case BINARY_BIT_AND:
        await fprintf(fd, "bit_and\n");
        break;
    case BINARY_BIT_XOR:
        await fprintf(fd, "bit_xor\n");
        break;
    case BINARY_BIT_OR:
        await fprintf(fd, "bit_or\n");
        break;
    case BINARY_LOG_AND:
        await fprintf(fd, "log_and\n");
        break;
    case BINARY_LOG_OR:
        await fprintf(fd, "log_or\n");
        break;
    case BINARY_COMMA:
        await fprintf(fd, "comma\n");
        break;
    }
}

static coro(braam_call *) void export_assign_op(FILE *fd, AssignOp kind, int level)
{
    await print_indent(fd, level);
    await fprintf(fd, "op: ");
    switch (kind) {
    case ASSIGN_SIMPLE:
        await fprintf(fd, "simple\n");
        break;
    case ASSIGN_MUL:
        await fprintf(fd, "mul\n");
        break;
    case ASSIGN_DIV:
        await fprintf(fd, "div\n");
        break;
    case ASSIGN_MOD:
        await fprintf(fd, "mod\n");
        break;
    case ASSIGN_ADD:
        await fprintf(fd, "add\n");
        break;
    case ASSIGN_SUB:
        await fprintf(fd, "sub\n");
        break;
    case ASSIGN_LEFT:
        await fprintf(fd, "left\n");
        break;
    case ASSIGN_RIGHT:
        await fprintf(fd, "right\n");
        break;
    case ASSIGN_AND:
        await fprintf(fd, "and\n");
        break;
    case ASSIGN_XOR:
        await fprintf(fd, "xor\n");
        break;
    case ASSIGN_OR:
        await fprintf(fd, "or\n");
        break;
    }
}

static coro(braam_call *) void export_generic_assoc(FILE *fd, GenericAssoc *ga, int level)
{
    while (ga) {
        await print_indent(fd, level);
        await fprintf(fd, "- assoc:\n");
        await print_indent(fd, level + 1);
        await fprintf(fd, "kind: %s\n", ga->kind == GENERIC_ASSOC_TYPE ? "type" : "default");
        if (ga->kind == GENERIC_ASSOC_TYPE) {
            await print_indent(fd, level + 1);
            await fprintf(fd, "type:\n");
            await export_type(fd, ga->u.type_assoc.type, level + 2);
            await print_indent(fd, level + 1);
            await fprintf(fd, "expr:\n");
            await export_expr(fd, ga->u.type_assoc.expr, level + 2);
        } else {
            await print_indent(fd, level + 1);
            await fprintf(fd, "expr:\n");
            await export_expr(fd, ga->u.default_assoc, level + 2);
        }
        ga = ga->next;
    }
}

static coro(braam_call *) void export_expr(FILE *fd, Expr *expr, int level)
{
    if (!expr)
        return;
    await print_indent(fd, level);
    await fprintf(fd, "- expr:\n");
    await print_indent(fd, level + 1);
    await fprintf(fd, "kind: ");
    switch (expr->kind) {
    case EXPR_LITERAL:
        await fprintf(fd, "literal\n");
        await export_literal(fd, expr->u.literal, level + 1);
        break;
    case EXPR_VAR:
        await fprintf(fd, "var\n");
        await export_ident(fd, expr->u.var, level + 1);
        break;
    case EXPR_UNARY_OP:
        await fprintf(fd, "unary_op\n");
        await export_unary_op(fd, expr->u.unary_op.op, level + 1);
        await print_indent(fd, level + 1);
        await fprintf(fd, "expr:\n");
        await export_expr(fd, expr->u.unary_op.expr, level + 2);
        break;
    case EXPR_BINARY_OP:
        await fprintf(fd, "binary_op\n");
        await export_binary_op(fd, expr->u.binary_op.op, level + 1);
        await print_indent(fd, level + 1);
        await fprintf(fd, "left:\n");
        await export_expr(fd, expr->u.binary_op.left, level + 2);
        await print_indent(fd, level + 1);
        await fprintf(fd, "right:\n");
        await export_expr(fd, expr->u.binary_op.right, level + 2);
        break;
    case EXPR_ASSIGN:
        await fprintf(fd, "assign\n");
        await export_assign_op(fd, expr->u.assign.op, level + 1);
        await print_indent(fd, level + 1);
        await fprintf(fd, "target:\n");
        await export_expr(fd, expr->u.assign.target, level + 2);
        await print_indent(fd, level + 1);
        await fprintf(fd, "value:\n");
        await export_expr(fd, expr->u.assign.value, level + 2);
        break;
    case EXPR_COND:
        await fprintf(fd, "cond\n");
        await print_indent(fd, level + 1);
        await fprintf(fd, "condition:\n");
        await export_expr(fd, expr->u.cond.condition, level + 2);
        await print_indent(fd, level + 1);
        await fprintf(fd, "then:\n");
        await export_expr(fd, expr->u.cond.then_expr, level + 2);
        await print_indent(fd, level + 1);
        await fprintf(fd, "else:\n");
        await export_expr(fd, expr->u.cond.else_expr, level + 2);
        break;
    case EXPR_CAST:
        await fprintf(fd, "cast\n");
        await print_indent(fd, level + 1);
        await fprintf(fd, "type:\n");
        await export_type(fd, expr->u.cast.type, level + 2);
        await print_indent(fd, level + 1);
        await fprintf(fd, "expr:\n");
        await export_expr(fd, expr->u.cast.expr, level + 2);
        break;
    case EXPR_CALL:
        await fprintf(fd, "call\n");
        await print_indent(fd, level + 1);
        await fprintf(fd, "func:\n");
        await export_expr(fd, expr->u.call.func, level + 2);
        if (expr->u.call.args) {
            await print_indent(fd, level + 1);
            await fprintf(fd, "args:\n");
            Expr *arg = expr->u.call.args;
            while (arg) {
                await export_expr(fd, arg, level + 2);
                arg = arg->next;
            }
        }
        break;
    case EXPR_COMPOUND:
        await fprintf(fd, "compound\n");
        await print_indent(fd, level + 1);
        await fprintf(fd, "type:\n");
        await export_type(fd, expr->u.compound_literal.type, level + 2);
        if (expr->u.compound_literal.init) {
            await print_indent(fd, level + 1);
            await fprintf(fd, "init:\n");
            await export_init_declarator(fd, (InitDeclarator *)expr->u.compound_literal.init,
                                         level + 2);
        }
        break;
    case EXPR_SUBSCRIPT:
        await fprintf(fd, "subscript\n");
        await print_indent(fd, level + 1);
        await fprintf(fd, "left:\n");
        await export_expr(fd, expr->u.subscript.left, level + 2);
        await print_indent(fd, level + 1);
        await fprintf(fd, "right:\n");
        await export_expr(fd, expr->u.subscript.right, level + 2);
        break;
    case EXPR_FIELD_ACCESS:
        await fprintf(fd, "field_access\n");
        await print_indent(fd, level + 1);
        await fprintf(fd, "expr:\n");
        await export_expr(fd, expr->u.field_access.expr, level + 2);
        await export_ident(fd, expr->u.field_access.field, level + 1);
        break;
    case EXPR_PTR_ACCESS:
        await fprintf(fd, "ptr_access\n");
        await print_indent(fd, level + 1);
        await fprintf(fd, "expr:\n");
        await export_expr(fd, expr->u.ptr_access.expr, level + 2);
        await export_ident(fd, expr->u.ptr_access.field, level + 1);
        break;
    case EXPR_POST_INC:
        await fprintf(fd, "post_inc\n");
        await export_expr(fd, expr->u.post_inc, level + 1);
        break;
    case EXPR_POST_DEC:
        await fprintf(fd, "post_dec\n");
        await export_expr(fd, expr->u.post_dec, level + 1);
        break;
    case EXPR_SIZEOF_EXPR:
        await fprintf(fd, "sizeof_expr\n");
        await print_indent(fd, level + 1);
        await fprintf(fd, "expr:\n");
        await export_expr(fd, expr->u.sizeof_expr, level + 2);
        break;
    case EXPR_SIZEOF_TYPE:
        await fprintf(fd, "sizeof_type\n");
        await print_indent(fd, level + 1);
        await fprintf(fd, "type:\n");
        await export_type(fd, expr->u.sizeof_type, level + 2);
        break;
    case EXPR_ALIGNOF:
        await fprintf(fd, "alignof\n");
        await print_indent(fd, level + 1);
        await fprintf(fd, "type:\n");
        await export_type(fd, expr->u.align_of, level + 2);
        break;
    case EXPR_VA_CLASS:
        await fprintf(fd, "va_class\n");
        await print_indent(fd, level + 1);
        await fprintf(fd, "type:\n");
        await export_type(fd, expr->u.va_class, level + 2);
        break;
    case EXPR_GENERIC:
        await fprintf(fd, "generic\n");
        await print_indent(fd, level + 1);
        await fprintf(fd, "controlling_expr:\n");
        await export_expr(fd, expr->u.generic.controlling_expr, level + 2);
        if (expr->u.generic.associations) {
            await print_indent(fd, level + 1);
            await fprintf(fd, "associations:\n");
            await export_generic_assoc(fd, expr->u.generic.associations, level + 2);
        }
        break;
    case EXPR_YIELD:
        await fprintf(fd, "yield\n");
        if (expr->u.yield_expr) {
            await print_indent(fd, level + 1);
            await fprintf(fd, "expr:\n");
            await export_expr(fd, expr->u.yield_expr, level + 2);
        }
        break;
    case EXPR_AWAIT:
        await fprintf(fd, "await\n");
        await print_indent(fd, level + 1);
        await fprintf(fd, "expr:\n");
        await export_expr(fd, expr->u.await_expr, level + 2);
        break;
    case EXPR_CO_OP:
        await fprintf(fd, "co_op\n");
        await print_indent(fd, level + 1);
        await fprintf(fd, "op: %s\n", co_op_name[expr->u.co_op.op]);
        if (expr->u.co_op.args) {
            await print_indent(fd, level + 1);
            await fprintf(fd, "args:\n");
            for (Expr *arg = expr->u.co_op.args; arg; arg = arg->next)
                await export_expr(fd, arg, level + 2);
        }
        break;
    }
    if (expr->type) {
        await print_indent(fd, level + 1);
        await fprintf(fd, "type:\n");
        await export_type(fd, expr->type, level + 2);
    }
}

static coro(braam_call *) void export_decl_or_stmt(FILE *fd, DeclOrStmt *dos, int level)
{
    while (dos) {
        await print_indent(fd, level);
        await fprintf(fd, "- %s:\n", dos->kind == DECL_OR_STMT_DECL ? "decl" : "stmt");
        if (dos->kind == DECL_OR_STMT_DECL) {
            await export_decl(fd, dos->u.decl, level + 1);
        } else {
            await export_stmt(fd, dos->u.stmt, level + 1);
        }
        dos = dos->next;
    }
}

static coro(braam_call *) void export_for_init(FILE *fd, ForInit *fi, int level)
{
    await print_indent(fd, level);
    await fprintf(fd, "kind: %s\n", fi->kind == FOR_INIT_EXPR ? "expr" : "decl");
    if (fi->kind == FOR_INIT_EXPR) {
        await print_indent(fd, level);
        await fprintf(fd, "expr:\n");
        await export_expr(fd, fi->u.expr, level + 1);
    } else {
        await print_indent(fd, level);
        await fprintf(fd, "decl:\n");
        await export_decl(fd, fi->u.decl, level + 1);
    }
}

static coro(braam_call *) void export_stmt(FILE *fd, Stmt *stmt, int level)
{
    if (!stmt)
        return;
    await print_indent(fd, level);
    await fprintf(fd, "kind: ");
    switch (stmt->kind) {
    case STMT_EXPR:
        await fprintf(fd, "expr\n");
        break;
    case STMT_COMPOUND:
        await fprintf(fd, "compound\n");
        break;
    case STMT_IF:
        await fprintf(fd, "if\n");
        break;
    case STMT_SWITCH:
        await fprintf(fd, "switch\n");
        break;
    case STMT_WHILE:
        await fprintf(fd, "while\n");
        break;
    case STMT_DO_WHILE:
        await fprintf(fd, "do_while\n");
        break;
    case STMT_FOR:
        await fprintf(fd, "for\n");
        break;
    case STMT_GOTO:
        await fprintf(fd, "goto\n");
        break;
    case STMT_CONTINUE:
        await fprintf(fd, "continue\n");
        break;
    case STMT_BREAK:
        await fprintf(fd, "break\n");
        break;
    case STMT_RETURN:
        await fprintf(fd, "return\n");
        break;
    case STMT_LABELED:
        await fprintf(fd, "labeled\n");
        break;
    case STMT_CASE:
        await fprintf(fd, "case\n");
        break;
    case STMT_DEFAULT:
        await fprintf(fd, "default\n");
        break;
    case STMT_DEFER:
        await fprintf(fd, "defer\n");
        break;
    }
    switch (stmt->kind) {
    case STMT_EXPR:
        if (stmt->u.expr) {
            await print_indent(fd, level);
            await fprintf(fd, "expr:\n");
            await export_expr(fd, stmt->u.expr, level + 1);
        }
        break;
    case STMT_COMPOUND:
        if (stmt->u.compound) {
            await print_indent(fd, level);
            await fprintf(fd, "body:\n");
            await export_decl_or_stmt(fd, stmt->u.compound, level + 1);
        }
        break;
    case STMT_IF:
        await print_indent(fd, level);
        await fprintf(fd, "condition:\n");
        await export_expr(fd, stmt->u.if_stmt.condition, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "then:\n");
        await export_stmt(fd, stmt->u.if_stmt.then_stmt, level + 1);
        if (stmt->u.if_stmt.else_stmt) {
            await print_indent(fd, level);
            await fprintf(fd, "else:\n");
            await export_stmt(fd, stmt->u.if_stmt.else_stmt, level + 1);
        }
        break;
    case STMT_SWITCH:
        await print_indent(fd, level);
        await fprintf(fd, "expr:\n");
        await export_expr(fd, stmt->u.switch_stmt.expr, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "body:\n");
        await export_stmt(fd, stmt->u.switch_stmt.body, level + 1);
        break;
    case STMT_WHILE:
        await print_indent(fd, level);
        await fprintf(fd, "condition:\n");
        await export_expr(fd, stmt->u.while_stmt.condition, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "body:\n");
        await export_stmt(fd, stmt->u.while_stmt.body, level + 1);
        break;
    case STMT_DO_WHILE:
        await print_indent(fd, level);
        await fprintf(fd, "body:\n");
        await export_stmt(fd, stmt->u.do_while.body, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "condition:\n");
        await export_expr(fd, stmt->u.do_while.condition, level + 1);
        break;
    case STMT_FOR:
        await print_indent(fd, level);
        await fprintf(fd, "init:\n");
        await export_for_init(fd, stmt->u.for_stmt.init, level + 1);
        if (stmt->u.for_stmt.condition) {
            await print_indent(fd, level);
            await fprintf(fd, "condition:\n");
            await export_expr(fd, stmt->u.for_stmt.condition, level + 1);
        }
        if (stmt->u.for_stmt.update) {
            await print_indent(fd, level);
            await fprintf(fd, "update:\n");
            await export_expr(fd, stmt->u.for_stmt.update, level + 1);
        }
        await print_indent(fd, level);
        await fprintf(fd, "body:\n");
        await export_stmt(fd, stmt->u.for_stmt.body, level + 1);
        break;
    case STMT_GOTO:
        await export_ident(fd, stmt->u.goto_label, level);
        break;
    case STMT_LABELED:
        await export_ident(fd, stmt->u.labeled.label, level);
        await print_indent(fd, level);
        await fprintf(fd, "stmt:\n");
        await export_stmt(fd, stmt->u.labeled.stmt, level + 1);
        break;
    case STMT_CASE:
        await print_indent(fd, level);
        await fprintf(fd, "expr:\n");
        await export_expr(fd, stmt->u.case_stmt.expr, level + 1);
        await print_indent(fd, level);
        await fprintf(fd, "stmt:\n");
        await export_stmt(fd, stmt->u.case_stmt.stmt, level + 1);
        break;
    case STMT_DEFAULT:
        await print_indent(fd, level);
        await fprintf(fd, "stmt:\n");
        await export_stmt(fd, stmt->u.default_stmt, level + 1);
        break;
    case STMT_DEFER:
        await print_indent(fd, level);
        await fprintf(fd, "stmt:\n");
        await export_stmt(fd, stmt->u.defer_stmt, level + 1);
        break;
    case STMT_RETURN:
        if (stmt->u.expr) {
            await print_indent(fd, level);
            await fprintf(fd, "expr:\n");
            await export_expr(fd, stmt->u.expr, level + 1);
        }
        break;
    default:
        break;
    }
}

coro(braam_call *) void export_yaml(FILE *fd, Program *program)
{
    if (!program || !fd)
        return;
    await fprintf(fd, "program:\n");
    ExternalDecl *decl = program->decls;
    while (decl) {
        await print_indent(fd, 1);
        await fprintf(fd, "- external_decl:\n");
        await print_indent(fd, 2);
        await fprintf(fd, "kind: %s\n",
                      decl->kind == EXTERNAL_DECL_FUNCTION ? "function" : "declaration");
        if (decl->kind == EXTERNAL_DECL_FUNCTION) {
            await print_indent(fd, 2);
            await fprintf(fd, "type:\n");
            await export_type(fd, decl->u.function.type, 3);
            await export_ident(fd, decl->u.function.name, 2);
            if (decl->u.function.specifiers) {
                await print_indent(fd, 2);
                await fprintf(fd, "specifiers:\n");
                await export_decl_spec(fd, decl->u.function.specifiers, 3);
            }
            if (decl->u.function.param_decls) {
                await print_indent(fd, 2);
                await fprintf(fd, "param_decls:\n");
                await export_decl(fd, decl->u.function.param_decls, 3);
            }
            if (decl->u.function.body) {
                await print_indent(fd, 2);
                await fprintf(fd, "body:\n");
                await export_stmt(fd, decl->u.function.body, 3);
            }
        } else {
            await print_indent(fd, 2);
            await fprintf(fd, "declaration:\n");
            await export_decl(fd, decl->u.declaration, 3);
        }
        decl = decl->next;
    }
}
