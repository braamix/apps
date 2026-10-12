#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"

static int node_id = 0;

static int gen_node_id()
{
    return node_id++;
}

static void export_type(FILE *fd, Type *type, int parent_id);
static void export_expr(FILE *fd, Expr *expr, int parent_id);
static void export_stmt(FILE *fd, Stmt *stmt, int parent_id);
static void export_decl(FILE *fd, Declaration *decl, int parent_id);
static void export_decl_spec(FILE *fd, DeclSpec *ds, int parent_id);

static void export_ident(FILE *fd, Ident ident, int parent_id, const char *label)
{
    if (!ident)
        return;
    int id = gen_node_id();
    fprintf(fd, "  n%d [label=\"%s: %s\", shape=box];\n", id, label, ident);
    fprintf(fd, "  n%d -> n%d;\n", parent_id, id);
}

static void export_type_qualifier(FILE *fd, const TypeQualifier *qual, int parent_id)
{
    while (qual) {
        int id = gen_node_id();
        fprintf(fd, "  n%d [label=\"TypeQualifier: ", id);
        switch (qual->kind) {
        case TYPE_QUALIFIER_CONST:
            fprintf(fd, "const");
            break;
        case TYPE_QUALIFIER_RESTRICT:
            fprintf(fd, "restrict");
            break;
        case TYPE_QUALIFIER_VOLATILE:
            fprintf(fd, "volatile");
            break;
        case TYPE_QUALIFIER_ATOMIC:
            fprintf(fd, "atomic");
            break;
        }
        fprintf(fd, "\", shape=box];\n");
        fprintf(fd, "  n%d -> n%d [label=\"qualifier\"];\n", parent_id, id);
        qual = qual->next;
    }
}

static void export_alignment_spec(FILE *fd, AlignmentSpec *as, int parent_id);

static void export_field(FILE *fd, Field *field, int parent_id)
{
    while (field) {
        int id = gen_node_id();
        switch (field->kind) {
        case FIELD_MEMBER:
            fprintf(fd, "  n%d [label=\"Field\", shape=box];\n", id);
            fprintf(fd, "  n%d -> n%d [label=\"field\"];\n", parent_id, id);
            export_type(fd, field->u.member.type, id);
            export_ident(fd, field->u.member.name, id, "name");
            if (field->u.member.bitfield) {
                int expr_id = gen_node_id();
                fprintf(fd, "  n%d [label=\"Bitfield\", shape=box];\n", expr_id);
                fprintf(fd, "  n%d -> n%d [label=\"bitfield\"];\n", id, expr_id);
                export_expr(fd, field->u.member.bitfield, expr_id);
            }
            export_alignment_spec(fd, field->u.member.align_spec, id);
            break;
        case FIELD_STATIC_ASSERT:
            fprintf(fd, "  n%d [label=\"StaticAssert\", shape=box];\n", id);
            fprintf(fd, "  n%d -> n%d [label=\"field\"];\n", parent_id, id);
            export_expr(fd, field->u.static_assrt.condition, id);
            if (field->u.static_assrt.message) {
                int mid = gen_node_id();
                fprintf(fd, "  n%d [label=\"Msg: %s\", shape=box];\n", mid,
                        field->u.static_assrt.message);
                fprintf(fd, "  n%d -> n%d [label=\"message\"];\n", id, mid);
            }
            break;
        }
        field = field->next;
    }
}

static void export_enumerator(FILE *fd, Enumerator *enumr, int parent_id)
{
    while (enumr) {
        int id = gen_node_id();
        fprintf(fd, "  n%d [label=\"Enumerator\", shape=box];\n", id);
        fprintf(fd, "  n%d -> n%d [label=\"enumerator\"];\n", parent_id, id);
        export_ident(fd, enumr->name, id, "name");
        if (enumr->value) {
            int expr_id = gen_node_id();
            fprintf(fd, "  n%d [label=\"Value\", shape=box];\n", expr_id);
            fprintf(fd, "  n%d -> n%d [label=\"value\"];\n", id, expr_id);
            export_expr(fd, enumr->value, expr_id);
        }
        enumr = enumr->next;
    }
}

static void export_param(FILE *fd, Param *param, int parent_id)
{
    while (param) {
        int id = gen_node_id();
        fprintf(fd, "  n%d [label=\"Param\", shape=box];\n", id);
        fprintf(fd, "  n%d -> n%d [label=\"param\"];\n", parent_id, id);
        export_ident(fd, param->name, id, "name");
        export_type(fd, param->type, id);
        export_decl_spec(fd, param->specifiers, id);
        param = param->next;
    }
}

static void export_type(FILE *fd, Type *type, int parent_id)
{
    if (!type)
        return;
    int id = gen_node_id();
    fprintf(fd, "  n%d [label=\"Type: ", id);
    switch (type->kind) {
    case TYPE_VOID:
        fprintf(fd, "void");
        break;
    case TYPE_BOOL:
        fprintf(fd, "bool");
        break;
    case TYPE_CHAR:
        fprintf(fd, "char");
        break;
    case TYPE_SCHAR:
        fprintf(fd, "signed_char");
        break;
    case TYPE_UCHAR:
        fprintf(fd, "unsigned_char");
        break;
    case TYPE_SHORT:
        fprintf(fd, "short");
        break;
    case TYPE_USHORT:
        fprintf(fd, "unsigned_short");
        break;
    case TYPE_INT:
        fprintf(fd, "int");
        break;
    case TYPE_UINT:
        fprintf(fd, "unsigned_int");
        break;
    case TYPE_LONG:
        fprintf(fd, "long");
        break;
    case TYPE_ULONG:
        fprintf(fd, "unsigned_long");
        break;
    case TYPE_LONG_LONG:
        fprintf(fd, "long_long");
        break;
    case TYPE_ULONG_LONG:
        fprintf(fd, "unsigned_long_long");
        break;
    case TYPE_SIGNED:
        fprintf(fd, "signed");
        break;
    case TYPE_UNSIGNED:
        fprintf(fd, "unsigned");
        break;
    case TYPE_FLOAT:
        fprintf(fd, "float");
        break;
    case TYPE_DOUBLE:
        fprintf(fd, "double");
        break;
    case TYPE_LONG_DOUBLE:
        fprintf(fd, "long_double");
        break;
    case TYPE_COMPLEX:
        fprintf(fd, "complex");
        break;
    case TYPE_IMAGINARY:
        fprintf(fd, "imaginary");
        break;
    case TYPE_POINTER:
        fprintf(fd, "pointer");
        break;
    case TYPE_ARRAY:
        fprintf(fd, "array");
        break;
    case TYPE_FUNCTION:
        fprintf(fd, "function");
        break;
    case TYPE_STRUCT:
        fprintf(fd, "struct");
        break;
    case TYPE_UNION:
        fprintf(fd, "union");
        break;
    case TYPE_ENUM:
        fprintf(fd, "enum");
        break;
    case TYPE_TYPEDEF_NAME:
        fprintf(fd, "typedef_name");
        break;
    case TYPE_ATOMIC:
        fprintf(fd, "atomic");
        break;
    }
    fprintf(fd, "\", shape=oval];\n");
    fprintf(fd, "  n%d -> n%d [label=\"type\"];\n", parent_id, id);
    if (type->qualifiers) {
        export_type_qualifier(fd, type->qualifiers, id);
    }
    switch (type->kind) {
    case TYPE_CHAR:
        fprintf(fd, "  n%d [label=\"Char\", shape=box];\n", gen_node_id());
        fprintf(fd, "  n%d -> n%d;\n", id, node_id - 1);
        break;
    case TYPE_SCHAR:
    case TYPE_SHORT:
    case TYPE_INT:
    case TYPE_LONG:
        fprintf(fd, "  n%d [label=\"Signed\", shape=box];\n", gen_node_id());
        fprintf(fd, "  n%d -> n%d [label=\"signed\"];\n", id, node_id - 1);
        break;
    case TYPE_UCHAR:
    case TYPE_USHORT:
    case TYPE_UINT:
    case TYPE_ULONG:
        fprintf(fd, "  n%d [label=\"Unsigned\", shape=box];\n", gen_node_id());
        fprintf(fd, "  n%d -> n%d [label=\"unsigned\"];\n", id, node_id - 1);
        break;
    case TYPE_COMPLEX:
    case TYPE_IMAGINARY:
        export_type(fd, type->u.complex.base, id);
        break;
    case TYPE_POINTER:
        export_type(fd, type->u.pointer.target, id);
        if (type->u.pointer.qualifiers) {
            export_type_qualifier(fd, type->u.pointer.qualifiers, id);
        }
        break;
    case TYPE_ARRAY:
        export_type(fd, type->u.array.element, id);
        if (type->u.array.size) {
            int expr_id = gen_node_id();
            fprintf(fd, "  n%d [label=\"Size\", shape=box];\n", expr_id);
            fprintf(fd, "  n%d -> n%d [label=\"size\"];\n", id, expr_id);
            export_expr(fd, type->u.array.size, expr_id);
        }
        if (type->u.array.qualifiers) {
            export_type_qualifier(fd, type->u.array.qualifiers, id);
        }
        fprintf(fd, "  n%d [label=\"Static: %s\", shape=box];\n", gen_node_id(),
                type->u.array.is_static ? "true" : "false");
        fprintf(fd, "  n%d -> n%d [label=\"is_static\"];\n", id, node_id - 1);
        break;
    case TYPE_FUNCTION:
        export_type(fd, type->u.function.return_type, id);
        if (type->u.function.params) {
            export_param(fd, type->u.function.params, id);
        }
        fprintf(fd, "  n%d [label=\"Variadic: %s\", shape=box];\n", gen_node_id(),
                type->u.function.variadic ? "true" : "false");
        fprintf(fd, "  n%d -> n%d [label=\"variadic\"];\n", id, node_id - 1);
        break;
    case TYPE_STRUCT:
    case TYPE_UNION:
        export_ident(fd, type->u.struct_t.name, id, "name");
        if (type->u.struct_t.fields) {
            export_field(fd, type->u.struct_t.fields, id);
        }
        if (type->u.struct_t.frame_yield) {
            export_type(fd, type->u.struct_t.frame_yield, id);
            export_type(fd, type->u.struct_t.frame_result, id);
        }
        break;
    case TYPE_ENUM:
        export_ident(fd, type->u.enum_t.name, id, "name");
        if (type->u.enum_t.enumerators) {
            export_enumerator(fd, type->u.enum_t.enumerators, id);
        }
        break;
    case TYPE_TYPEDEF_NAME:
        export_ident(fd, type->u.typedef_name.name, id, "name");
        break;
    case TYPE_ATOMIC:
        export_type(fd, type->u.atomic.base, id);
        break;
    default:
        break;
    }
}

static void export_storage_class(FILE *fd, StorageClass kind, int parent_id)
{
    if (!kind)
        return;
    int id = gen_node_id();
    fprintf(fd, "  n%d [label=\"StorageClass: ", id);
    switch (kind) {
    case STORAGE_CLASS_TYPEDEF:
        fprintf(fd, "typedef");
        break;
    case STORAGE_CLASS_EXTERN:
        fprintf(fd, "extern");
        break;
    case STORAGE_CLASS_STATIC:
        fprintf(fd, "static");
        break;
    case STORAGE_CLASS_THREAD_LOCAL:
        fprintf(fd, "thread_local");
        break;
    case STORAGE_CLASS_AUTO:
        fprintf(fd, "auto");
        break;
    case STORAGE_CLASS_REGISTER:
        fprintf(fd, "register");
        break;
    case STORAGE_CLASS_NONE:
        fprintf(fd, "none");
        break;
    }
    fprintf(fd, "\", shape=box];\n");
    fprintf(fd, "  n%d -> n%d [label=\"storage_class\"];\n", parent_id, id);
}

static void export_function_spec(FILE *fd, const FunctionSpec *fs, int parent_id)
{
    while (fs) {
        int id = gen_node_id();
        fprintf(fd, "  n%d [label=\"FunctionSpec: ", id);
        switch (fs->kind) {
        case FUNC_SPEC_INLINE:
            fprintf(fd, "inline");
            break;
        case FUNC_SPEC_NORETURN:
            fprintf(fd, "noreturn");
            break;
        case FUNC_SPEC_CORO:
            fprintf(fd, "coro");
            break;
        }
        fprintf(fd, "\", shape=box];\n");
        fprintf(fd, "  n%d -> n%d [label=\"func_spec\"];\n", parent_id, id);
        if (fs->yield_type)
            export_type(fd, fs->yield_type, id);
        fs = fs->next;
    }
}

static void export_alignment_spec(FILE *fd, AlignmentSpec *as, int parent_id)
{
    if (!as)
        return;
    int id = gen_node_id();
    fprintf(fd, "  n%d [label=\"AlignmentSpec: %s\", shape=box];\n", id,
            as->kind == ALIGN_SPEC_TYPE ? "type" : "expr");
    fprintf(fd, "  n%d -> n%d [label=\"alignment\"];\n", parent_id, id);
    if (as->kind == ALIGN_SPEC_TYPE) {
        export_type(fd, as->u.type, id);
    } else {
        export_expr(fd, as->u.expr, id);
    }
}

static void export_init_declarator(FILE *fd, InitDeclarator *id, int parent_id)
{
    while (id) {
        int decl_id = gen_node_id();
        fprintf(fd, "  n%d [label=\"InitDeclarator\", shape=box];\n", decl_id);
        fprintf(fd, "  n%d -> n%d [label=\"declarator\"];\n", parent_id, decl_id);
        export_type(fd, id->type, decl_id);
        export_ident(fd, id->name, decl_id, "name");
        if (id->init) {
            int init_id = gen_node_id();
            fprintf(fd, "  n%d [label=\"Initializer: %s\", shape=box];\n", init_id,
                    id->init->kind == INITIALIZER_SINGLE ? "single" : "compound");
            fprintf(fd, "  n%d -> n%d [label=\"init\"];\n", decl_id, init_id);
            if (id->init->kind == INITIALIZER_SINGLE) {
                export_expr(fd, id->init->u.expr, init_id);
            } else {
                InitItem *item = id->init->u.items;
                while (item) {
                    int item_id = gen_node_id();
                    fprintf(fd, "  n%d [label=\"InitItem\", shape=box];\n", item_id);
                    fprintf(fd, "  n%d -> n%d [label=\"item\"];\n", init_id, item_id);
                    Designator *des = item->designators;
                    while (des) {
                        int des_id = gen_node_id();
                        fprintf(fd, "  n%d [label=\"Designator: %s\", shape=box];\n", des_id,
                                des->kind == DESIGNATOR_ARRAY ? "array" : "field");
                        fprintf(fd, "  n%d -> n%d [label=\"designator\"];\n", item_id, des_id);
                        if (des->kind == DESIGNATOR_ARRAY) {
                            export_expr(fd, des->u.expr, des_id);
                        } else {
                            export_ident(fd, des->u.name, des_id, "name");
                        }
                        des = des->next;
                    }
                    int sub_init_id = gen_node_id();
                    fprintf(fd, "  n%d [label=\"Initializer: %s\", shape=box];\n", sub_init_id,
                            item->init->kind == INITIALIZER_SINGLE ? "single" : "compound");
                    fprintf(fd, "  n%d -> n%d [label=\"init\"];\n", item_id, sub_init_id);
                    if (item->init->kind == INITIALIZER_SINGLE) {
                        export_expr(fd, item->init->u.expr, sub_init_id);
                    } else {
                        export_init_declarator(fd, (InitDeclarator *)item->init->u.items,
                                               sub_init_id);
                    }
                    item = item->next;
                }
            }
        }
        id = id->next;
    }
}

static void export_decl_spec(FILE *fd, DeclSpec *ds, int parent_id)
{
    if (!ds)
        return;
    int id = gen_node_id();
    fprintf(fd, "  n%d [label=\"DeclSpec\", shape=box];\n", id);
    fprintf(fd, "  n%d -> n%d [label=\"specifiers\"];\n", parent_id, id);
    export_type_qualifier(fd, ds->qualifiers, id);
    export_storage_class(fd, ds->storage, id);
    export_function_spec(fd, ds->func_specs, id);
    export_alignment_spec(fd, ds->align_spec, id);
}

static void export_decl(FILE *fd, Declaration *decl, int parent_id)
{
    while (decl) {
        int id = gen_node_id();
        fprintf(fd, "  n%d [label=\"Declaration: ", id);
        switch (decl->kind) {
        case DECL_VAR:
            fprintf(fd, "var");
            break;
        case DECL_STATIC_ASSERT:
            fprintf(fd, "static_assert");
            break;
        case DECL_EMPTY:
            fprintf(fd, "empty");
            break;
        }
        fprintf(fd, "\", shape=box];\n");
        fprintf(fd, "  n%d -> n%d [label=\"declaration\"];\n", parent_id, id);
        if (decl->kind == DECL_VAR) {
            export_decl_spec(fd, decl->u.var.specifiers, id);
            if (decl->u.var.declarators) {
                export_init_declarator(fd, decl->u.var.declarators, id);
            }
        } else if (decl->kind == DECL_STATIC_ASSERT) {
            int expr_id = gen_node_id();
            fprintf(fd, "  n%d [label=\"Condition\", shape=box];\n", expr_id);
            fprintf(fd, "  n%d -> n%d [label=\"condition\"];\n", id, expr_id);
            export_expr(fd, decl->u.static_assrt.condition, expr_id);
            if (decl->u.static_assrt.message) {
                int msg_id = gen_node_id();
                fprintf(fd, "  n%d [label=\"Message: %s\", shape=box];\n", msg_id,
                        decl->u.static_assrt.message);
                fprintf(fd, "  n%d -> n%d [label=\"message\"];\n", id, msg_id);
            }
        } else {
            export_decl_spec(fd, decl->u.empty.specifiers, id);
            export_type(fd, decl->u.empty.type, id);
        }
        decl = decl->next;
    }
}

static void export_string(FILE *fd, const char *str)
{
    while (*str) {
        if (*str == '"' || *str == '\\') {
            fputc('\\', fd);
        }
        fputc(*str, fd);
        str++;
    }
}

static void export_literal(FILE *fd, const Literal *lit, int parent_id)
{
    int id = gen_node_id();
    fprintf(fd, "  n%d [label=\"Literal: ", id);
    switch (lit->kind) {
    case LITERAL_INT:
        fprintf(fd, "int: %" PRId64, lit->u.int_val);
        break;
    case LITERAL_LONG:
        fprintf(fd, "long: %ld", lit->u.long_val);
        break;
    case LITERAL_LONG_LONG:
        fprintf(fd, "long long: %lld", lit->u.long_long_val);
        break;
    case LITERAL_UINT:
        fprintf(fd, "unsigned int: %" PRIu64, lit->u.uint_val);
        break;
    case LITERAL_ULONG:
        fprintf(fd, "unsigned long: %lu", lit->u.ulong_val);
        break;
    case LITERAL_ULONG_LONG:
        fprintf(fd, "unsigned long long: %llu", lit->u.ulong_long_val);
        break;
    case LITERAL_FLOAT:
        fprintf(fd, "float: %a", lit->u.real_val);
        break;
    case LITERAL_DOUBLE:
        fprintf(fd, "double: %a", lit->u.real_val);
        break;
    case LITERAL_LONG_DOUBLE: {
        char buf[F128_BUFSIZE];
        fprintf(fd, "long double: %s", f128_format(lit->u.long_double_val, buf));
        break;
    }
    case LITERAL_CHAR:
        fprintf(fd, "char: '%c'", lit->u.char_val);
        break;
    case LITERAL_STRING:
        fprintf(fd, "string: ");
        export_string(fd, lit->u.string_val);
        break;
    case LITERAL_ENUM:
        fprintf(fd, "enum: %s", lit->u.enum_const);
        break;
    }
    fprintf(fd, "\", shape=box];\n");
    fprintf(fd, "  n%d -> n%d [label=\"literal\"];\n", parent_id, id);
}

static void export_unary_op(FILE *fd, UnaryOp kind, int parent_id)
{
    int id = gen_node_id();
    fprintf(fd, "  n%d [label=\"UnaryOp: ", id);
    switch (kind) {
    case UNARY_ADDRESS:
        fprintf(fd, "address");
        break;
    case UNARY_DEREF:
        fprintf(fd, "deref");
        break;
    case UNARY_PLUS:
        fprintf(fd, "plus");
        break;
    case UNARY_NEG:
        fprintf(fd, "neg");
        break;
    case UNARY_BIT_NOT:
        fprintf(fd, "bit_not");
        break;
    case UNARY_LOG_NOT:
        fprintf(fd, "log_not");
        break;
    case UNARY_PRE_INC:
        fprintf(fd, "pre_inc");
        break;
    case UNARY_PRE_DEC:
        fprintf(fd, "pre_dec");
        break;
    }
    fprintf(fd, "\", shape=box];\n");
    fprintf(fd, "  n%d -> n%d [label=\"op\"];\n", parent_id, id);
}

static void export_binary_op(FILE *fd, BinaryOp kind, int parent_id)
{
    int id = gen_node_id();
    fprintf(fd, "  n%d [label=\"BinaryOp: ", id);
    switch (kind) {
    case BINARY_MUL:
        fprintf(fd, "mul");
        break;
    case BINARY_DIV:
        fprintf(fd, "div");
        break;
    case BINARY_MOD:
        fprintf(fd, "mod");
        break;
    case BINARY_ADD:
        fprintf(fd, "add");
        break;
    case BINARY_SUB:
        fprintf(fd, "sub");
        break;
    case BINARY_LEFT_SHIFT:
        fprintf(fd, "left_shift");
        break;
    case BINARY_RIGHT_SHIFT:
        fprintf(fd, "right_shift");
        break;
    case BINARY_LT:
        fprintf(fd, "lt");
        break;
    case BINARY_GT:
        fprintf(fd, "gt");
        break;
    case BINARY_LE:
        fprintf(fd, "le");
        break;
    case BINARY_GE:
        fprintf(fd, "ge");
        break;
    case BINARY_EQ:
        fprintf(fd, "eq");
        break;
    case BINARY_NE:
        fprintf(fd, "ne");
        break;
    case BINARY_BIT_AND:
        fprintf(fd, "bit_and");
        break;
    case BINARY_BIT_XOR:
        fprintf(fd, "bit_xor");
        break;
    case BINARY_BIT_OR:
        fprintf(fd, "bit_or");
        break;
    case BINARY_LOG_AND:
        fprintf(fd, "log_and");
        break;
    case BINARY_LOG_OR:
        fprintf(fd, "log_or");
        break;
    case BINARY_COMMA:
        fprintf(fd, "comma");
        break;
    }
    fprintf(fd, "\", shape=box];\n");
    fprintf(fd, "  n%d -> n%d [label=\"op\"];\n", parent_id, id);
}

static void export_assign_op(FILE *fd, AssignOp kind, int parent_id)
{
    int id = gen_node_id();
    fprintf(fd, "  n%d [label=\"AssignOp: ", id);
    switch (kind) {
    case ASSIGN_SIMPLE:
        fprintf(fd, "simple");
        break;
    case ASSIGN_MUL:
        fprintf(fd, "mul");
        break;
    case ASSIGN_DIV:
        fprintf(fd, "div");
        break;
    case ASSIGN_MOD:
        fprintf(fd, "mod");
        break;
    case ASSIGN_ADD:
        fprintf(fd, "add");
        break;
    case ASSIGN_SUB:
        fprintf(fd, "sub");
        break;
    case ASSIGN_LEFT:
        fprintf(fd, "left");
        break;
    case ASSIGN_RIGHT:
        fprintf(fd, "right");
        break;
    case ASSIGN_AND:
        fprintf(fd, "and");
        break;
    case ASSIGN_XOR:
        fprintf(fd, "xor");
        break;
    case ASSIGN_OR:
        fprintf(fd, "or");
        break;
    }
    fprintf(fd, "\", shape=box];\n");
    fprintf(fd, "  n%d -> n%d [label=\"op\"];\n", parent_id, id);
}

static void export_generic_assoc(FILE *fd, GenericAssoc *ga, int parent_id)
{
    while (ga) {
        int id = gen_node_id();
        fprintf(fd, "  n%d [label=\"GenericAssoc: %s\", shape=box];\n", id,
                ga->kind == GENERIC_ASSOC_TYPE ? "type" : "default");
        fprintf(fd, "  n%d -> n%d [label=\"assoc\"];\n", parent_id, id);
        if (ga->kind == GENERIC_ASSOC_TYPE) {
            export_type(fd, ga->u.type_assoc.type, id);
            export_expr(fd, ga->u.type_assoc.expr, id);
        } else {
            export_expr(fd, ga->u.default_assoc, id);
        }
        ga = ga->next;
    }
}

static void export_expr(FILE *fd, Expr *expr, int parent_id)
{
    if (!expr)
        return;
    int id = gen_node_id();
    fprintf(fd, "  n%d [label=\"Expr: ", id);
    switch (expr->kind) {
    case EXPR_LITERAL:
        fprintf(fd, "literal");
        break;
    case EXPR_VAR:
        fprintf(fd, "var");
        break;
    case EXPR_UNARY_OP:
        fprintf(fd, "unary_op");
        break;
    case EXPR_BINARY_OP:
        fprintf(fd, "binary_op");
        break;
    case EXPR_ASSIGN:
        fprintf(fd, "assign");
        break;
    case EXPR_COND:
        fprintf(fd, "cond");
        break;
    case EXPR_CAST:
        fprintf(fd, "cast");
        break;
    case EXPR_CALL:
        fprintf(fd, "call");
        break;
    case EXPR_COMPOUND:
        fprintf(fd, "compound");
        break;
    case EXPR_FIELD_ACCESS:
        fprintf(fd, "field_access");
        break;
    case EXPR_SUBSCRIPT:
        fprintf(fd, "subscript");
        break;
    case EXPR_PTR_ACCESS:
        fprintf(fd, "ptr_access");
        break;
    case EXPR_POST_INC:
        fprintf(fd, "post_inc");
        break;
    case EXPR_POST_DEC:
        fprintf(fd, "post_dec");
        break;
    case EXPR_SIZEOF_EXPR:
        fprintf(fd, "sizeof_expr");
        break;
    case EXPR_SIZEOF_TYPE:
        fprintf(fd, "sizeof_type");
        break;
    case EXPR_ALIGNOF:
        fprintf(fd, "alignof");
        break;
    case EXPR_VA_CLASS:
        fprintf(fd, "va_class");
        break;
    case EXPR_GENERIC:
        fprintf(fd, "generic");
        break;
    case EXPR_YIELD:
        fprintf(fd, "yield");
        break;
    case EXPR_AWAIT:
        fprintf(fd, "await");
        break;
    case EXPR_CO_OP:
        fprintf(fd, "%s", co_op_name[expr->u.co_op.op]);
        break;
    }
    fprintf(fd, "\", shape=oval];\n");
    fprintf(fd, "  n%d -> n%d [label=\"expr\"];\n", parent_id, id);
    switch (expr->kind) {
    case EXPR_LITERAL:
        export_literal(fd, expr->u.literal, id);
        break;
    case EXPR_VAR:
        export_ident(fd, expr->u.var, id, "var");
        break;
    case EXPR_UNARY_OP:
        export_unary_op(fd, expr->u.unary_op.op, id);
        export_expr(fd, expr->u.unary_op.expr, id);
        break;
    case EXPR_BINARY_OP:
        export_binary_op(fd, expr->u.binary_op.op, id);
        export_expr(fd, expr->u.binary_op.left, id);
        export_expr(fd, expr->u.binary_op.right, id);
        break;
    case EXPR_ASSIGN:
        export_assign_op(fd, expr->u.assign.op, id);
        export_expr(fd, expr->u.assign.target, id);
        export_expr(fd, expr->u.assign.value, id);
        break;
    case EXPR_COND:
        export_expr(fd, expr->u.cond.condition, id);
        export_expr(fd, expr->u.cond.then_expr, id);
        export_expr(fd, expr->u.cond.else_expr, id);
        break;
    case EXPR_CAST:
        export_type(fd, expr->u.cast.type, id);
        export_expr(fd, expr->u.cast.expr, id);
        break;
    case EXPR_CALL:
        export_expr(fd, expr->u.call.func, id);
        if (expr->u.call.args) {
            Expr *arg = expr->u.call.args;
            while (arg) {
                export_expr(fd, arg, id);
                arg = arg->next;
            }
        }
        break;
    case EXPR_COMPOUND:
        export_type(fd, expr->u.compound_literal.type, id);
        if (expr->u.compound_literal.init) {
            export_init_declarator(fd, (InitDeclarator *)expr->u.compound_literal.init, id);
        }
        break;
    case EXPR_SUBSCRIPT:
        export_expr(fd, expr->u.subscript.left, id);
        export_expr(fd, expr->u.subscript.right, id);
        break;
    case EXPR_FIELD_ACCESS:
        export_expr(fd, expr->u.field_access.expr, id);
        export_ident(fd, expr->u.field_access.field, id, "field");
        break;
    case EXPR_PTR_ACCESS:
        export_expr(fd, expr->u.ptr_access.expr, id);
        export_ident(fd, expr->u.ptr_access.field, id, "field");
        break;
    case EXPR_POST_INC:
        export_expr(fd, expr->u.post_inc, id);
        break;
    case EXPR_POST_DEC:
        export_expr(fd, expr->u.post_dec, id);
        break;
    case EXPR_SIZEOF_EXPR:
        export_expr(fd, expr->u.sizeof_expr, id);
        break;
    case EXPR_SIZEOF_TYPE:
        export_type(fd, expr->u.sizeof_type, id);
        break;
    case EXPR_ALIGNOF:
        export_type(fd, expr->u.align_of, id);
        break;
    case EXPR_VA_CLASS:
        export_type(fd, expr->u.va_class, id);
        break;
    case EXPR_GENERIC:
        export_expr(fd, expr->u.generic.controlling_expr, id);
        if (expr->u.generic.associations) {
            export_generic_assoc(fd, expr->u.generic.associations, id);
        }
        break;
    case EXPR_YIELD:
        if (expr->u.yield_expr)
            export_expr(fd, expr->u.yield_expr, id);
        break;
    case EXPR_AWAIT:
        export_expr(fd, expr->u.await_expr, id);
        break;
    case EXPR_CO_OP:
        for (Expr *arg = expr->u.co_op.args; arg; arg = arg->next)
            export_expr(fd, arg, id);
        break;
    }
    if (expr->type) {
        export_type(fd, expr->type, id);
    }
}

static void export_decl_or_stmt(FILE *fd, DeclOrStmt *dos, int parent_id)
{
    while (dos) {
        int id = gen_node_id();
        fprintf(fd, "  n%d [label=\"%s\", shape=box];\n", id,
                dos->kind == DECL_OR_STMT_DECL ? "DeclOrStmt: decl" : "DeclOrStmt: stmt");
        fprintf(fd, "  n%d -> n%d [label=\"decl_or_stmt\"];\n", parent_id, id);
        if (dos->kind == DECL_OR_STMT_DECL) {
            export_decl(fd, dos->u.decl, id);
        } else {
            export_stmt(fd, dos->u.stmt, id);
        }
        dos = dos->next;
    }
}

static void export_for_init(FILE *fd, ForInit *fi, int parent_id)
{
    int id = gen_node_id();
    fprintf(fd, "  n%d [label=\"ForInit: %s\", shape=box];\n", id,
            fi->kind == FOR_INIT_EXPR ? "expr" : "decl");
    fprintf(fd, "  n%d -> n%d [label=\"init\"];\n", parent_id, id);
    if (fi->kind == FOR_INIT_EXPR) {
        export_expr(fd, fi->u.expr, id);
    } else {
        export_decl(fd, fi->u.decl, id);
    }
}

static void export_stmt(FILE *fd, Stmt *stmt, int parent_id)
{
    if (!stmt)
        return;
    int id = gen_node_id();
    fprintf(fd, "  n%d [label=\"Stmt: ", id);
    switch (stmt->kind) {
    case STMT_EXPR:
        fprintf(fd, "expr");
        break;
    case STMT_COMPOUND:
        fprintf(fd, "compound");
        break;
    case STMT_IF:
        fprintf(fd, "if");
        break;
    case STMT_SWITCH:
        fprintf(fd, "switch");
        break;
    case STMT_WHILE:
        fprintf(fd, "while");
        break;
    case STMT_DO_WHILE:
        fprintf(fd, "do_while");
        break;
    case STMT_FOR:
        fprintf(fd, "for");
        break;
    case STMT_GOTO:
        fprintf(fd, "goto");
        break;
    case STMT_CONTINUE:
        fprintf(fd, "continue");
        break;
    case STMT_BREAK:
        fprintf(fd, "break");
        break;
    case STMT_RETURN:
        fprintf(fd, "return");
        break;
    case STMT_LABELED:
        fprintf(fd, "labeled");
        break;
    case STMT_CASE:
        fprintf(fd, "case");
        break;
    case STMT_DEFAULT:
        fprintf(fd, "default");
        break;
    case STMT_DEFER:
        fprintf(fd, "defer");
        break;
    }
    fprintf(fd, "\", shape=oval];\n");
    fprintf(fd, "  n%d -> n%d [label=\"stmt\"];\n", parent_id, id);
    switch (stmt->kind) {
    case STMT_EXPR:
        if (stmt->u.expr) {
            export_expr(fd, stmt->u.expr, id);
        }
        break;
    case STMT_COMPOUND:
        if (stmt->u.compound) {
            export_decl_or_stmt(fd, stmt->u.compound, id);
        }
        break;
    case STMT_IF:
        export_expr(fd, stmt->u.if_stmt.condition, id);
        export_stmt(fd, stmt->u.if_stmt.then_stmt, id);
        if (stmt->u.if_stmt.else_stmt) {
            export_stmt(fd, stmt->u.if_stmt.else_stmt, id);
        }
        break;
    case STMT_SWITCH:
        export_expr(fd, stmt->u.switch_stmt.expr, id);
        export_stmt(fd, stmt->u.switch_stmt.body, id);
        break;
    case STMT_WHILE:
        export_expr(fd, stmt->u.while_stmt.condition, id);
        export_stmt(fd, stmt->u.while_stmt.body, id);
        break;
    case STMT_DO_WHILE:
        export_stmt(fd, stmt->u.do_while.body, id);
        export_expr(fd, stmt->u.do_while.condition, id);
        break;
    case STMT_FOR:
        export_for_init(fd, stmt->u.for_stmt.init, id);
        if (stmt->u.for_stmt.condition) {
            export_expr(fd, stmt->u.for_stmt.condition, id);
        }
        if (stmt->u.for_stmt.update) {
            export_expr(fd, stmt->u.for_stmt.update, id);
        }
        export_stmt(fd, stmt->u.for_stmt.body, id);
        break;
    case STMT_GOTO:
        export_ident(fd, stmt->u.goto_label, id, "label");
        break;
    case STMT_LABELED:
        export_ident(fd, stmt->u.labeled.label, id, "label");
        export_stmt(fd, stmt->u.labeled.stmt, id);
        break;
    case STMT_CASE:
        export_expr(fd, stmt->u.case_stmt.expr, id);
        export_stmt(fd, stmt->u.case_stmt.stmt, id);
        break;
    case STMT_DEFAULT:
        export_stmt(fd, stmt->u.default_stmt, id);
        break;
    case STMT_DEFER:
        export_stmt(fd, stmt->u.defer_stmt, id);
        break;
    case STMT_RETURN:
        if (stmt->u.expr) {
            export_expr(fd, stmt->u.expr, id);
        }
        break;
    default:
        break;
    }
}

void export_dot(FILE *fd, Program *program)
{
    if (!program || !fd)
        return;
    node_id = 0;
    fprintf(fd, "digraph AST {\n");
    fprintf(fd, "  graph [margin=\"0,0\", pad=\"0.1\", ranksep=0.3, nodesep=0.2];\n");
    fprintf(fd, "  node [width=0.3, height=0.3, margin=\"0.02,0.01\"];\n");
    fprintf(fd, "  node [shape=oval];\n");
    // int root_id = gen_node_id();
    ExternalDecl *decl = program->decls;
    while (decl) {
        int decl_id = gen_node_id();
        fprintf(fd, "  n%d [label=\"ExternalDecl: %s\", shape=box];\n", decl_id,
                decl->kind == EXTERNAL_DECL_FUNCTION ? "function" : "declaration");
        if (decl->kind == EXTERNAL_DECL_FUNCTION) {
            export_type(fd, decl->u.function.type, decl_id);
            export_ident(fd, decl->u.function.name, decl_id, "name");
            export_decl_spec(fd, decl->u.function.specifiers, decl_id);
            if (decl->u.function.param_decls) {
                export_decl(fd, decl->u.function.param_decls, decl_id);
            }
            if (decl->u.function.body) {
                export_stmt(fd, decl->u.function.body, decl_id);
            }
        } else {
            export_decl(fd, decl->u.declaration, decl_id);
        }
        decl = decl->next;
    }
    fprintf(fd, "}\n");
}
