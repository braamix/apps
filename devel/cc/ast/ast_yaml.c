#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"

#define INDENT_STEP 2

static void print_indent(FILE *fd, int level)
{
    for (int i = 0; i < level * INDENT_STEP; i++) {
        fputc(' ', fd);
    }
}

static void export_type(FILE *fd, Type *type, int level);
static void export_expr(FILE *fd, Expr *expr, int level);
static void export_stmt(FILE *fd, Stmt *stmt, int level);
static void export_decl(FILE *fd, Declaration *decl, int level);
static void export_decl_spec(FILE *fd, DeclSpec *ds, int level);

static void export_ident(FILE *fd, Ident ident, int level)
{
    if (ident) {
        print_indent(fd, level);
        fprintf(fd, "name: %s\n", ident);
    }
}

static void export_type_qualifier(FILE *fd, const TypeQualifier *qual, int level)
{
    while (qual) {
        print_indent(fd, level);
        fprintf(fd, "- kind: ");
        switch (qual->kind) {
        case TYPE_QUALIFIER_CONST:
            fprintf(fd, "const\n");
            break;
        case TYPE_QUALIFIER_RESTRICT:
            fprintf(fd, "restrict\n");
            break;
        case TYPE_QUALIFIER_VOLATILE:
            fprintf(fd, "volatile\n");
            break;
        case TYPE_QUALIFIER_ATOMIC:
            fprintf(fd, "atomic\n");
            break;
        }
        qual = qual->next;
    }
}

static void export_alignment_spec(FILE *fd, AlignmentSpec *as, int level);

static void export_field(FILE *fd, Field *field, int level)
{
    while (field) {
        print_indent(fd, level);
        switch (field->kind) {
        case FIELD_MEMBER:
            fprintf(fd, "- field:\n");
            print_indent(fd, level + 1);
            fprintf(fd, "type:\n");
            export_type(fd, field->u.member.type, level + 2);
            export_ident(fd, field->u.member.name, level + 1);
            if (field->u.member.bitfield) {
                print_indent(fd, level + 1);
                fprintf(fd, "bitfield:\n");
                export_expr(fd, field->u.member.bitfield, level + 2);
            }
            export_alignment_spec(fd, field->u.member.align_spec, level + 1);
            break;
        case FIELD_STATIC_ASSERT:
            fprintf(fd, "- static_assert:\n");
            print_indent(fd, level + 1);
            fprintf(fd, "condition:\n");
            export_expr(fd, field->u.static_assrt.condition, level + 2);
            if (field->u.static_assrt.message) {
                print_indent(fd, level + 1);
                fprintf(fd, "message: %s\n", field->u.static_assrt.message);
            }
            break;
        }
        field = field->next;
    }
}

static void export_enumerator(FILE *fd, Enumerator *enumr, int level)
{
    while (enumr) {
        print_indent(fd, level);
        fprintf(fd, "- enumerator:\n");
        export_ident(fd, enumr->name, level + 1);
        if (enumr->value) {
            print_indent(fd, level + 1);
            fprintf(fd, "value:\n");
            export_expr(fd, enumr->value, level + 2);
        }
        enumr = enumr->next;
    }
}

static void export_param(FILE *fd, Param *param, int level)
{
    while (param) {
        print_indent(fd, level);
        fprintf(fd, "- param:\n");
        export_ident(fd, param->name, level + 1);
        print_indent(fd, level + 1);
        fprintf(fd, "type:\n");
        export_type(fd, param->type, level + 2);
        if (param->specifiers) {
            print_indent(fd, level + 1);
            fprintf(fd, "specifiers:\n");
            export_decl_spec(fd, param->specifiers, level + 2);
        }
        param = param->next;
    }
}

static void export_type(FILE *fd, Type *type, int level)
{
    if (!type)
        return;
    print_indent(fd, level);
    fprintf(fd, "kind: ");
    switch (type->kind) {
    case TYPE_VOID:
        fprintf(fd, "void\n");
        break;
    case TYPE_BOOL:
        fprintf(fd, "bool\n");
        break;
    case TYPE_CHAR:
        fprintf(fd, "char\n");
        break;
    case TYPE_SCHAR:
        fprintf(fd, "signed_char\n");
        break;
    case TYPE_UCHAR:
        fprintf(fd, "unsigned_char\n");
        break;
    case TYPE_SHORT:
        fprintf(fd, "short\n");
        break;
    case TYPE_USHORT:
        fprintf(fd, "unsigned_short\n");
        break;
    case TYPE_INT:
        fprintf(fd, "int\n");
        break;
    case TYPE_UINT:
        fprintf(fd, "unsigned_int\n");
        break;
    case TYPE_LONG:
        fprintf(fd, "long\n");
        break;
    case TYPE_ULONG:
        fprintf(fd, "unsigned_long\n");
        break;
    case TYPE_LONG_LONG:
        fprintf(fd, "long_long\n");
        break;
    case TYPE_ULONG_LONG:
        fprintf(fd, "unsigned_long_long\n");
        break;
    case TYPE_SIGNED:
        fprintf(fd, "signed\n");
        break;
    case TYPE_UNSIGNED:
        fprintf(fd, "unsigned\n");
        break;
    case TYPE_FLOAT:
        fprintf(fd, "float\n");
        break;
    case TYPE_DOUBLE:
        fprintf(fd, "double\n");
        break;
    case TYPE_LONG_DOUBLE:
        fprintf(fd, "long_double\n");
        break;
    case TYPE_COMPLEX:
        fprintf(fd, "complex\n");
        break;
    case TYPE_IMAGINARY:
        fprintf(fd, "imaginary\n");
        break;
    case TYPE_POINTER:
        fprintf(fd, "pointer\n");
        break;
    case TYPE_ARRAY:
        fprintf(fd, "array\n");
        break;
    case TYPE_FUNCTION:
        fprintf(fd, "function\n");
        break;
    case TYPE_STRUCT:
        fprintf(fd, "struct\n");
        break;
    case TYPE_UNION:
        fprintf(fd, "union\n");
        break;
    case TYPE_ENUM:
        fprintf(fd, "enum\n");
        break;
    case TYPE_TYPEDEF_NAME:
        fprintf(fd, "typedef_name\n");
        break;
    case TYPE_ATOMIC:
        fprintf(fd, "atomic\n");
        break;
    }
    if (type->qualifiers) {
        print_indent(fd, level);
        fprintf(fd, "qualifiers:\n");
        export_type_qualifier(fd, type->qualifiers, level + 1);
    }
    switch (type->kind) {
    case TYPE_COMPLEX:
    case TYPE_IMAGINARY:
        print_indent(fd, level);
        fprintf(fd, "base:\n");
        export_type(fd, type->u.complex.base, level + 1);
        break;
    case TYPE_POINTER:
        print_indent(fd, level);
        fprintf(fd, "target:\n");
        export_type(fd, type->u.pointer.target, level + 1);
        if (type->u.pointer.qualifiers) {
            print_indent(fd, level);
            fprintf(fd, "pointer_qualifiers:\n");
            export_type_qualifier(fd, type->u.pointer.qualifiers, level + 1);
        }
        break;
    case TYPE_ARRAY:
        print_indent(fd, level);
        fprintf(fd, "element:\n");
        export_type(fd, type->u.array.element, level + 1);
        if (type->u.array.size) {
            print_indent(fd, level);
            fprintf(fd, "size:\n");
            export_expr(fd, type->u.array.size, level + 1);
        }
        if (type->u.array.qualifiers) {
            print_indent(fd, level);
            fprintf(fd, "array_qualifiers:\n");
            export_type_qualifier(fd, type->u.array.qualifiers, level + 1);
        }
        print_indent(fd, level);
        fprintf(fd, "is_static: %s\n", type->u.array.is_static ? "true" : "false");
        break;
    case TYPE_FUNCTION:
        print_indent(fd, level);
        fprintf(fd, "return_type:\n");
        export_type(fd, type->u.function.return_type, level + 1);
        if (type->u.function.params) {
            print_indent(fd, level);
            fprintf(fd, "params:\n");
            export_param(fd, type->u.function.params, level + 1);
        }
        print_indent(fd, level);
        fprintf(fd, "variadic: %s\n", type->u.function.variadic ? "true" : "false");
        break;
    case TYPE_STRUCT:
    case TYPE_UNION:
        export_ident(fd, type->u.struct_t.name, level);
        if (type->u.struct_t.fields) {
            print_indent(fd, level);
            fprintf(fd, "fields:\n");
            export_field(fd, type->u.struct_t.fields, level + 1);
        }
        if (type->u.struct_t.frame_yield) {
            print_indent(fd, level);
            fprintf(fd, "frame_yield:\n");
            export_type(fd, type->u.struct_t.frame_yield, level + 1);
            print_indent(fd, level);
            fprintf(fd, "frame_result:\n");
            export_type(fd, type->u.struct_t.frame_result, level + 1);
        }
        break;
    case TYPE_ENUM:
        export_ident(fd, type->u.enum_t.name, level);
        if (type->u.enum_t.enumerators) {
            print_indent(fd, level);
            fprintf(fd, "enumerators:\n");
            export_enumerator(fd, type->u.enum_t.enumerators, level + 1);
        }
        break;
    case TYPE_TYPEDEF_NAME:
        export_ident(fd, type->u.typedef_name.name, level);
        break;
    case TYPE_ATOMIC:
        print_indent(fd, level);
        fprintf(fd, "base:\n");
        export_type(fd, type->u.atomic.base, level + 1);
        break;
    default:
        break;
    }
}

static void export_storage_class(FILE *fd, StorageClass kind, int level)
{
    if (!kind)
        return;
    print_indent(fd, level);
    fprintf(fd, "storage_class: ");
    switch (kind) {
    case STORAGE_CLASS_TYPEDEF:
        fprintf(fd, "typedef\n");
        break;
    case STORAGE_CLASS_EXTERN:
        fprintf(fd, "extern\n");
        break;
    case STORAGE_CLASS_STATIC:
        fprintf(fd, "static\n");
        break;
    case STORAGE_CLASS_THREAD_LOCAL:
        fprintf(fd, "thread_local\n");
        break;
    case STORAGE_CLASS_AUTO:
        fprintf(fd, "auto\n");
        break;
    case STORAGE_CLASS_REGISTER:
        fprintf(fd, "register\n");
        break;
    case STORAGE_CLASS_NONE:
        fprintf(fd, "none\n");
        break;
    }
}

static void export_function_spec(FILE *fd, const FunctionSpec *fs, int level)
{
    while (fs) {
        print_indent(fd, level);
        fprintf(fd, "- kind: ");
        switch (fs->kind) {
        case FUNC_SPEC_INLINE:
            fprintf(fd, "inline\n");
            break;
        case FUNC_SPEC_NORETURN:
            fprintf(fd, "noreturn\n");
            break;
        case FUNC_SPEC_CORO:
            fprintf(fd, "coro\n");
            print_indent(fd, level + 1);
            fprintf(fd, "yield_type:\n");
            export_type(fd, fs->yield_type, level + 2);
            break;
        }
        fs = fs->next;
    }
}

static void export_alignment_spec(FILE *fd, AlignmentSpec *as, int level)
{
    if (!as)
        return;
    print_indent(fd, level);
    fprintf(fd, "alignment:\n");
    print_indent(fd, level + 1);
    fprintf(fd, "kind: %s\n", as->kind == ALIGN_SPEC_TYPE ? "type" : "expr");
    if (as->kind == ALIGN_SPEC_TYPE) {
        print_indent(fd, level + 1);
        fprintf(fd, "type:\n");
        export_type(fd, as->u.type, level + 2);
    } else {
        print_indent(fd, level + 1);
        fprintf(fd, "expr:\n");
        export_expr(fd, as->u.expr, level + 2);
    }
}

static void export_init_item(FILE *fd, InitItem *item, int level)
{
    if (!item)
        return;
    print_indent(fd, level);
    fprintf(fd, "items:\n");
    while (item) {
        print_indent(fd, level + 1);
        fprintf(fd, "- item:\n");
        Designator *des = item->designators;
        if (des) {
            print_indent(fd, level + 2);
            fprintf(fd, "designators:\n");
            while (des) {
                print_indent(fd, level + 3);
                fprintf(fd, "- kind: %s\n", des->kind == DESIGNATOR_ARRAY ? "array" : "field");
                if (des->kind == DESIGNATOR_ARRAY) {
                    print_indent(fd, level + 4);
                    fprintf(fd, "expr:\n");
                    export_expr(fd, des->u.expr, level + 5);
                } else {
                    export_ident(fd, des->u.name, level + 4);
                }
                des = des->next;
            }
        }
        print_indent(fd, level + 2);
        fprintf(fd, "init:\n");
        print_indent(fd, level + 3);
        fprintf(fd, "kind: %s\n", item->init->kind == INITIALIZER_SINGLE ? "single" : "compound");
        if (item->init->kind == INITIALIZER_SINGLE) {
            print_indent(fd, level + 3);
            fprintf(fd, "expr:\n");
            export_expr(fd, item->init->u.expr, level + 4);
        } else {
            export_init_item(fd, item->init->u.items, level + 4); // Recursive for nested compounds
        }
        item = item->next;
    }
}

static void export_init_declarator(FILE *fd, InitDeclarator *id, int level)
{
    while (id) {
        print_indent(fd, level);
        fprintf(fd, "- declarator:\n");
        print_indent(fd, level + 1);
        fprintf(fd, "type:\n");
        export_type(fd, id->type, level + 2);
        export_ident(fd, id->name, level + 1);
        if (id->init) {
            print_indent(fd, level + 1);
            fprintf(fd, "init:\n");
            print_indent(fd, level + 2);
            fprintf(fd, "kind: %s\n", id->init->kind == INITIALIZER_SINGLE ? "single" : "compound");
            if (id->init->kind == INITIALIZER_SINGLE) {
                print_indent(fd, level + 2);
                fprintf(fd, "expr:\n");
                export_expr(fd, id->init->u.expr, level + 3);
            } else {
                export_init_item(fd, id->init->u.items, level + 2);
            }
        }
        id = id->next;
    }
}

static void export_decl_spec(FILE *fd, DeclSpec *ds, int level)
{
    if (ds->qualifiers) {
        print_indent(fd, level);
        fprintf(fd, "qualifiers:\n");
        export_type_qualifier(fd, ds->qualifiers, level + 1);
    }
    export_storage_class(fd, ds->storage, level);
    if (ds->func_specs) {
        print_indent(fd, level);
        fprintf(fd, "function_specs:\n");
        export_function_spec(fd, ds->func_specs, level + 1);
    }
    export_alignment_spec(fd, ds->align_spec, level);
}

static void export_decl(FILE *fd, Declaration *decl, int level)
{
    while (decl) {
        print_indent(fd, level);
        fprintf(fd, "- declaration:\n");
        print_indent(fd, level + 1);
        fprintf(fd, "kind: ");
        switch (decl->kind) {
        case DECL_VAR:
            fprintf(fd, "var\n");
            break;
        case DECL_STATIC_ASSERT:
            fprintf(fd, "static_assert\n");
            break;
        case DECL_EMPTY:
            fprintf(fd, "empty\n");
            break;
        }
        if (decl->kind == DECL_VAR) {
            if (decl->u.var.specifiers) {
                print_indent(fd, level + 1);
                fprintf(fd, "specifiers:\n");
                export_decl_spec(fd, decl->u.var.specifiers, level + 2);
            }
            if (decl->u.var.declarators) {
                print_indent(fd, level + 1);
                fprintf(fd, "declarators:\n");
                export_init_declarator(fd, decl->u.var.declarators, level + 2);
            }
        } else if (decl->kind == DECL_STATIC_ASSERT) {
            print_indent(fd, level + 1);
            fprintf(fd, "condition:\n");
            export_expr(fd, decl->u.static_assrt.condition, level + 2);
            if (decl->u.static_assrt.message) {
                print_indent(fd, level + 1);
                fprintf(fd, "message: %s\n", decl->u.static_assrt.message);
            }
        } else {
            if (decl->u.empty.specifiers) {
                print_indent(fd, level + 1);
                fprintf(fd, "specifiers:\n");
                export_decl_spec(fd, decl->u.empty.specifiers, level + 2);
            }
            print_indent(fd, level + 1);
            fprintf(fd, "type:\n");
            export_type(fd, decl->u.empty.type, level + 2);
        }
        decl = decl->next;
    }
}

static void export_literal(FILE *fd, const Literal *lit, int level)
{
    print_indent(fd, level);
    fprintf(fd, "kind: ");
    switch (lit->kind) {
    case LITERAL_INT:
        fprintf(fd, "int\n");
        break;
    case LITERAL_LONG:
        fprintf(fd, "long\n");
        break;
    case LITERAL_LONG_LONG:
        fprintf(fd, "long_long\n");
        break;
    case LITERAL_UINT:
        fprintf(fd, "uint\n");
        break;
    case LITERAL_ULONG:
        fprintf(fd, "ulong\n");
        break;
    case LITERAL_ULONG_LONG:
        fprintf(fd, "ulong_long\n");
        break;
    case LITERAL_FLOAT:
        fprintf(fd, "float\n");
        break;
    case LITERAL_DOUBLE:
        fprintf(fd, "double\n");
        break;
    case LITERAL_LONG_DOUBLE:
        fprintf(fd, "long_double\n");
        break;
    case LITERAL_CHAR:
        fprintf(fd, "char\n");
        break;
    case LITERAL_STRING:
        fprintf(fd, "string\n");
        break;
    case LITERAL_ENUM:
        fprintf(fd, "enum\n");
        break;
    }
    print_indent(fd, level);
    fprintf(fd, "value: ");
    switch (lit->kind) {
    case LITERAL_INT:
        fprintf(fd, "%" PRId64 "\n", lit->u.int_val);
        break;
    case LITERAL_LONG:
        fprintf(fd, "%ld\n", lit->u.long_val);
        break;
    case LITERAL_LONG_LONG:
        fprintf(fd, "%lld\n", lit->u.long_long_val);
        break;
    case LITERAL_UINT:
        fprintf(fd, "%" PRIu64 "\n", lit->u.uint_val);
        break;
    case LITERAL_ULONG:
        fprintf(fd, "%lu\n", lit->u.ulong_val);
        break;
    case LITERAL_ULONG_LONG:
        fprintf(fd, "%llu\n", lit->u.ulong_long_val);
        break;
    case LITERAL_FLOAT:
        fprintf(fd, "%a\n", lit->u.real_val);
        break;
    case LITERAL_DOUBLE:
        fprintf(fd, "%a\n", lit->u.real_val);
        break;
    case LITERAL_LONG_DOUBLE: {
        char buf[F128_BUFSIZE];
        fprintf(fd, "%s\n", f128_format(lit->u.long_double_val, buf));
        break;
    }
    case LITERAL_CHAR:
        fprintf(fd, "'%c'\n", lit->u.char_val);
        break;
    case LITERAL_STRING:
        fprintf(fd, "%s\n", lit->u.string_val);
        break;
    case LITERAL_ENUM:
        fprintf(fd, "%s\n", lit->u.enum_const);
        break;
    }
}

static void export_unary_op(FILE *fd, UnaryOp kind, int level)
{
    print_indent(fd, level);
    fprintf(fd, "op: ");
    switch (kind) {
    case UNARY_ADDRESS:
        fprintf(fd, "address\n");
        break;
    case UNARY_DEREF:
        fprintf(fd, "deref\n");
        break;
    case UNARY_PLUS:
        fprintf(fd, "plus\n");
        break;
    case UNARY_NEG:
        fprintf(fd, "neg\n");
        break;
    case UNARY_BIT_NOT:
        fprintf(fd, "bit_not\n");
        break;
    case UNARY_LOG_NOT:
        fprintf(fd, "log_not\n");
        break;
    case UNARY_PRE_INC:
        fprintf(fd, "pre_inc\n");
        break;
    case UNARY_PRE_DEC:
        fprintf(fd, "pre_dec\n");
        break;
    }
}

static void export_binary_op(FILE *fd, BinaryOp kind, int level)
{
    print_indent(fd, level);
    fprintf(fd, "op: ");
    switch (kind) {
    case BINARY_MUL:
        fprintf(fd, "mul\n");
        break;
    case BINARY_DIV:
        fprintf(fd, "div\n");
        break;
    case BINARY_MOD:
        fprintf(fd, "mod\n");
        break;
    case BINARY_ADD:
        fprintf(fd, "add\n");
        break;
    case BINARY_SUB:
        fprintf(fd, "sub\n");
        break;
    case BINARY_LEFT_SHIFT:
        fprintf(fd, "left_shift\n");
        break;
    case BINARY_RIGHT_SHIFT:
        fprintf(fd, "right_shift\n");
        break;
    case BINARY_LT:
        fprintf(fd, "lt\n");
        break;
    case BINARY_GT:
        fprintf(fd, "gt\n");
        break;
    case BINARY_LE:
        fprintf(fd, "le\n");
        break;
    case BINARY_GE:
        fprintf(fd, "ge\n");
        break;
    case BINARY_EQ:
        fprintf(fd, "eq\n");
        break;
    case BINARY_NE:
        fprintf(fd, "ne\n");
        break;
    case BINARY_BIT_AND:
        fprintf(fd, "bit_and\n");
        break;
    case BINARY_BIT_XOR:
        fprintf(fd, "bit_xor\n");
        break;
    case BINARY_BIT_OR:
        fprintf(fd, "bit_or\n");
        break;
    case BINARY_LOG_AND:
        fprintf(fd, "log_and\n");
        break;
    case BINARY_LOG_OR:
        fprintf(fd, "log_or\n");
        break;
    case BINARY_COMMA:
        fprintf(fd, "comma\n");
        break;
    }
}

static void export_assign_op(FILE *fd, AssignOp kind, int level)
{
    print_indent(fd, level);
    fprintf(fd, "op: ");
    switch (kind) {
    case ASSIGN_SIMPLE:
        fprintf(fd, "simple\n");
        break;
    case ASSIGN_MUL:
        fprintf(fd, "mul\n");
        break;
    case ASSIGN_DIV:
        fprintf(fd, "div\n");
        break;
    case ASSIGN_MOD:
        fprintf(fd, "mod\n");
        break;
    case ASSIGN_ADD:
        fprintf(fd, "add\n");
        break;
    case ASSIGN_SUB:
        fprintf(fd, "sub\n");
        break;
    case ASSIGN_LEFT:
        fprintf(fd, "left\n");
        break;
    case ASSIGN_RIGHT:
        fprintf(fd, "right\n");
        break;
    case ASSIGN_AND:
        fprintf(fd, "and\n");
        break;
    case ASSIGN_XOR:
        fprintf(fd, "xor\n");
        break;
    case ASSIGN_OR:
        fprintf(fd, "or\n");
        break;
    }
}

static void export_generic_assoc(FILE *fd, GenericAssoc *ga, int level)
{
    while (ga) {
        print_indent(fd, level);
        fprintf(fd, "- assoc:\n");
        print_indent(fd, level + 1);
        fprintf(fd, "kind: %s\n", ga->kind == GENERIC_ASSOC_TYPE ? "type" : "default");
        if (ga->kind == GENERIC_ASSOC_TYPE) {
            print_indent(fd, level + 1);
            fprintf(fd, "type:\n");
            export_type(fd, ga->u.type_assoc.type, level + 2);
            print_indent(fd, level + 1);
            fprintf(fd, "expr:\n");
            export_expr(fd, ga->u.type_assoc.expr, level + 2);
        } else {
            print_indent(fd, level + 1);
            fprintf(fd, "expr:\n");
            export_expr(fd, ga->u.default_assoc, level + 2);
        }
        ga = ga->next;
    }
}

static void export_expr(FILE *fd, Expr *expr, int level)
{
    if (!expr)
        return;
    print_indent(fd, level);
    fprintf(fd, "- expr:\n");
    print_indent(fd, level + 1);
    fprintf(fd, "kind: ");
    switch (expr->kind) {
    case EXPR_LITERAL:
        fprintf(fd, "literal\n");
        export_literal(fd, expr->u.literal, level + 1);
        break;
    case EXPR_VAR:
        fprintf(fd, "var\n");
        export_ident(fd, expr->u.var, level + 1);
        break;
    case EXPR_UNARY_OP:
        fprintf(fd, "unary_op\n");
        export_unary_op(fd, expr->u.unary_op.op, level + 1);
        print_indent(fd, level + 1);
        fprintf(fd, "expr:\n");
        export_expr(fd, expr->u.unary_op.expr, level + 2);
        break;
    case EXPR_BINARY_OP:
        fprintf(fd, "binary_op\n");
        export_binary_op(fd, expr->u.binary_op.op, level + 1);
        print_indent(fd, level + 1);
        fprintf(fd, "left:\n");
        export_expr(fd, expr->u.binary_op.left, level + 2);
        print_indent(fd, level + 1);
        fprintf(fd, "right:\n");
        export_expr(fd, expr->u.binary_op.right, level + 2);
        break;
    case EXPR_ASSIGN:
        fprintf(fd, "assign\n");
        export_assign_op(fd, expr->u.assign.op, level + 1);
        print_indent(fd, level + 1);
        fprintf(fd, "target:\n");
        export_expr(fd, expr->u.assign.target, level + 2);
        print_indent(fd, level + 1);
        fprintf(fd, "value:\n");
        export_expr(fd, expr->u.assign.value, level + 2);
        break;
    case EXPR_COND:
        fprintf(fd, "cond\n");
        print_indent(fd, level + 1);
        fprintf(fd, "condition:\n");
        export_expr(fd, expr->u.cond.condition, level + 2);
        print_indent(fd, level + 1);
        fprintf(fd, "then:\n");
        export_expr(fd, expr->u.cond.then_expr, level + 2);
        print_indent(fd, level + 1);
        fprintf(fd, "else:\n");
        export_expr(fd, expr->u.cond.else_expr, level + 2);
        break;
    case EXPR_CAST:
        fprintf(fd, "cast\n");
        print_indent(fd, level + 1);
        fprintf(fd, "type:\n");
        export_type(fd, expr->u.cast.type, level + 2);
        print_indent(fd, level + 1);
        fprintf(fd, "expr:\n");
        export_expr(fd, expr->u.cast.expr, level + 2);
        break;
    case EXPR_CALL:
        fprintf(fd, "call\n");
        print_indent(fd, level + 1);
        fprintf(fd, "func:\n");
        export_expr(fd, expr->u.call.func, level + 2);
        if (expr->u.call.args) {
            print_indent(fd, level + 1);
            fprintf(fd, "args:\n");
            Expr *arg = expr->u.call.args;
            while (arg) {
                export_expr(fd, arg, level + 2);
                arg = arg->next;
            }
        }
        break;
    case EXPR_COMPOUND:
        fprintf(fd, "compound\n");
        print_indent(fd, level + 1);
        fprintf(fd, "type:\n");
        export_type(fd, expr->u.compound_literal.type, level + 2);
        if (expr->u.compound_literal.init) {
            print_indent(fd, level + 1);
            fprintf(fd, "init:\n");
            export_init_declarator(fd, (InitDeclarator *)expr->u.compound_literal.init, level + 2);
        }
        break;
    case EXPR_SUBSCRIPT:
        fprintf(fd, "subscript\n");
        print_indent(fd, level + 1);
        fprintf(fd, "left:\n");
        export_expr(fd, expr->u.subscript.left, level + 2);
        print_indent(fd, level + 1);
        fprintf(fd, "right:\n");
        export_expr(fd, expr->u.subscript.right, level + 2);
        break;
    case EXPR_FIELD_ACCESS:
        fprintf(fd, "field_access\n");
        print_indent(fd, level + 1);
        fprintf(fd, "expr:\n");
        export_expr(fd, expr->u.field_access.expr, level + 2);
        export_ident(fd, expr->u.field_access.field, level + 1);
        break;
    case EXPR_PTR_ACCESS:
        fprintf(fd, "ptr_access\n");
        print_indent(fd, level + 1);
        fprintf(fd, "expr:\n");
        export_expr(fd, expr->u.ptr_access.expr, level + 2);
        export_ident(fd, expr->u.ptr_access.field, level + 1);
        break;
    case EXPR_POST_INC:
        fprintf(fd, "post_inc\n");
        export_expr(fd, expr->u.post_inc, level + 1);
        break;
    case EXPR_POST_DEC:
        fprintf(fd, "post_dec\n");
        export_expr(fd, expr->u.post_dec, level + 1);
        break;
    case EXPR_SIZEOF_EXPR:
        fprintf(fd, "sizeof_expr\n");
        print_indent(fd, level + 1);
        fprintf(fd, "expr:\n");
        export_expr(fd, expr->u.sizeof_expr, level + 2);
        break;
    case EXPR_SIZEOF_TYPE:
        fprintf(fd, "sizeof_type\n");
        print_indent(fd, level + 1);
        fprintf(fd, "type:\n");
        export_type(fd, expr->u.sizeof_type, level + 2);
        break;
    case EXPR_ALIGNOF:
        fprintf(fd, "alignof\n");
        print_indent(fd, level + 1);
        fprintf(fd, "type:\n");
        export_type(fd, expr->u.align_of, level + 2);
        break;
    case EXPR_VA_CLASS:
        fprintf(fd, "va_class\n");
        print_indent(fd, level + 1);
        fprintf(fd, "type:\n");
        export_type(fd, expr->u.va_class, level + 2);
        break;
    case EXPR_GENERIC:
        fprintf(fd, "generic\n");
        print_indent(fd, level + 1);
        fprintf(fd, "controlling_expr:\n");
        export_expr(fd, expr->u.generic.controlling_expr, level + 2);
        if (expr->u.generic.associations) {
            print_indent(fd, level + 1);
            fprintf(fd, "associations:\n");
            export_generic_assoc(fd, expr->u.generic.associations, level + 2);
        }
        break;
    case EXPR_YIELD:
        fprintf(fd, "yield\n");
        if (expr->u.yield_expr) {
            print_indent(fd, level + 1);
            fprintf(fd, "expr:\n");
            export_expr(fd, expr->u.yield_expr, level + 2);
        }
        break;
    case EXPR_AWAIT:
        fprintf(fd, "await\n");
        print_indent(fd, level + 1);
        fprintf(fd, "expr:\n");
        export_expr(fd, expr->u.await_expr, level + 2);
        break;
    case EXPR_CO_OP:
        fprintf(fd, "co_op\n");
        print_indent(fd, level + 1);
        fprintf(fd, "op: %s\n", co_op_name[expr->u.co_op.op]);
        if (expr->u.co_op.args) {
            print_indent(fd, level + 1);
            fprintf(fd, "args:\n");
            for (Expr *arg = expr->u.co_op.args; arg; arg = arg->next)
                export_expr(fd, arg, level + 2);
        }
        break;
    }
    if (expr->type) {
        print_indent(fd, level + 1);
        fprintf(fd, "type:\n");
        export_type(fd, expr->type, level + 2);
    }
}

static void export_decl_or_stmt(FILE *fd, DeclOrStmt *dos, int level)
{
    while (dos) {
        print_indent(fd, level);
        fprintf(fd, "- %s:\n", dos->kind == DECL_OR_STMT_DECL ? "decl" : "stmt");
        if (dos->kind == DECL_OR_STMT_DECL) {
            export_decl(fd, dos->u.decl, level + 1);
        } else {
            export_stmt(fd, dos->u.stmt, level + 1);
        }
        dos = dos->next;
    }
}

static void export_for_init(FILE *fd, ForInit *fi, int level)
{
    print_indent(fd, level);
    fprintf(fd, "kind: %s\n", fi->kind == FOR_INIT_EXPR ? "expr" : "decl");
    if (fi->kind == FOR_INIT_EXPR) {
        print_indent(fd, level);
        fprintf(fd, "expr:\n");
        export_expr(fd, fi->u.expr, level + 1);
    } else {
        print_indent(fd, level);
        fprintf(fd, "decl:\n");
        export_decl(fd, fi->u.decl, level + 1);
    }
}

static void export_stmt(FILE *fd, Stmt *stmt, int level)
{
    if (!stmt)
        return;
    print_indent(fd, level);
    fprintf(fd, "kind: ");
    switch (stmt->kind) {
    case STMT_EXPR:
        fprintf(fd, "expr\n");
        break;
    case STMT_COMPOUND:
        fprintf(fd, "compound\n");
        break;
    case STMT_IF:
        fprintf(fd, "if\n");
        break;
    case STMT_SWITCH:
        fprintf(fd, "switch\n");
        break;
    case STMT_WHILE:
        fprintf(fd, "while\n");
        break;
    case STMT_DO_WHILE:
        fprintf(fd, "do_while\n");
        break;
    case STMT_FOR:
        fprintf(fd, "for\n");
        break;
    case STMT_GOTO:
        fprintf(fd, "goto\n");
        break;
    case STMT_CONTINUE:
        fprintf(fd, "continue\n");
        break;
    case STMT_BREAK:
        fprintf(fd, "break\n");
        break;
    case STMT_RETURN:
        fprintf(fd, "return\n");
        break;
    case STMT_LABELED:
        fprintf(fd, "labeled\n");
        break;
    case STMT_CASE:
        fprintf(fd, "case\n");
        break;
    case STMT_DEFAULT:
        fprintf(fd, "default\n");
        break;
    case STMT_DEFER:
        fprintf(fd, "defer\n");
        break;
    }
    switch (stmt->kind) {
    case STMT_EXPR:
        if (stmt->u.expr) {
            print_indent(fd, level);
            fprintf(fd, "expr:\n");
            export_expr(fd, stmt->u.expr, level + 1);
        }
        break;
    case STMT_COMPOUND:
        if (stmt->u.compound) {
            print_indent(fd, level);
            fprintf(fd, "body:\n");
            export_decl_or_stmt(fd, stmt->u.compound, level + 1);
        }
        break;
    case STMT_IF:
        print_indent(fd, level);
        fprintf(fd, "condition:\n");
        export_expr(fd, stmt->u.if_stmt.condition, level + 1);
        print_indent(fd, level);
        fprintf(fd, "then:\n");
        export_stmt(fd, stmt->u.if_stmt.then_stmt, level + 1);
        if (stmt->u.if_stmt.else_stmt) {
            print_indent(fd, level);
            fprintf(fd, "else:\n");
            export_stmt(fd, stmt->u.if_stmt.else_stmt, level + 1);
        }
        break;
    case STMT_SWITCH:
        print_indent(fd, level);
        fprintf(fd, "expr:\n");
        export_expr(fd, stmt->u.switch_stmt.expr, level + 1);
        print_indent(fd, level);
        fprintf(fd, "body:\n");
        export_stmt(fd, stmt->u.switch_stmt.body, level + 1);
        break;
    case STMT_WHILE:
        print_indent(fd, level);
        fprintf(fd, "condition:\n");
        export_expr(fd, stmt->u.while_stmt.condition, level + 1);
        print_indent(fd, level);
        fprintf(fd, "body:\n");
        export_stmt(fd, stmt->u.while_stmt.body, level + 1);
        break;
    case STMT_DO_WHILE:
        print_indent(fd, level);
        fprintf(fd, "body:\n");
        export_stmt(fd, stmt->u.do_while.body, level + 1);
        print_indent(fd, level);
        fprintf(fd, "condition:\n");
        export_expr(fd, stmt->u.do_while.condition, level + 1);
        break;
    case STMT_FOR:
        print_indent(fd, level);
        fprintf(fd, "init:\n");
        export_for_init(fd, stmt->u.for_stmt.init, level + 1);
        if (stmt->u.for_stmt.condition) {
            print_indent(fd, level);
            fprintf(fd, "condition:\n");
            export_expr(fd, stmt->u.for_stmt.condition, level + 1);
        }
        if (stmt->u.for_stmt.update) {
            print_indent(fd, level);
            fprintf(fd, "update:\n");
            export_expr(fd, stmt->u.for_stmt.update, level + 1);
        }
        print_indent(fd, level);
        fprintf(fd, "body:\n");
        export_stmt(fd, stmt->u.for_stmt.body, level + 1);
        break;
    case STMT_GOTO:
        export_ident(fd, stmt->u.goto_label, level);
        break;
    case STMT_LABELED:
        export_ident(fd, stmt->u.labeled.label, level);
        print_indent(fd, level);
        fprintf(fd, "stmt:\n");
        export_stmt(fd, stmt->u.labeled.stmt, level + 1);
        break;
    case STMT_CASE:
        print_indent(fd, level);
        fprintf(fd, "expr:\n");
        export_expr(fd, stmt->u.case_stmt.expr, level + 1);
        print_indent(fd, level);
        fprintf(fd, "stmt:\n");
        export_stmt(fd, stmt->u.case_stmt.stmt, level + 1);
        break;
    case STMT_DEFAULT:
        print_indent(fd, level);
        fprintf(fd, "stmt:\n");
        export_stmt(fd, stmt->u.default_stmt, level + 1);
        break;
    case STMT_DEFER:
        print_indent(fd, level);
        fprintf(fd, "stmt:\n");
        export_stmt(fd, stmt->u.defer_stmt, level + 1);
        break;
    case STMT_RETURN:
        if (stmt->u.expr) {
            print_indent(fd, level);
            fprintf(fd, "expr:\n");
            export_expr(fd, stmt->u.expr, level + 1);
        }
        break;
    default:
        break;
    }
}

void export_yaml(FILE *fd, Program *program)
{
    if (!program || !fd)
        return;
    fprintf(fd, "program:\n");
    ExternalDecl *decl = program->decls;
    while (decl) {
        print_indent(fd, 1);
        fprintf(fd, "- external_decl:\n");
        print_indent(fd, 2);
        fprintf(fd, "kind: %s\n",
                decl->kind == EXTERNAL_DECL_FUNCTION ? "function" : "declaration");
        if (decl->kind == EXTERNAL_DECL_FUNCTION) {
            print_indent(fd, 2);
            fprintf(fd, "type:\n");
            export_type(fd, decl->u.function.type, 3);
            export_ident(fd, decl->u.function.name, 2);
            if (decl->u.function.specifiers) {
                print_indent(fd, 2);
                fprintf(fd, "specifiers:\n");
                export_decl_spec(fd, decl->u.function.specifiers, 3);
            }
            if (decl->u.function.param_decls) {
                print_indent(fd, 2);
                fprintf(fd, "param_decls:\n");
                export_decl(fd, decl->u.function.param_decls, 3);
            }
            if (decl->u.function.body) {
                print_indent(fd, 2);
                fprintf(fd, "body:\n");
                export_stmt(fd, decl->u.function.body, 3);
            }
        } else {
            print_indent(fd, 2);
            fprintf(fd, "declaration:\n");
            export_decl(fd, decl->u.declaration, 3);
        }
        decl = decl->next;
    }
}
