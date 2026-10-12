#include <inttypes.h>
#include <string.h>

#include "ast.h"
#include "internal.h"

// Helper function to print indentation
static coro(braam_call *) void print_indent(FILE *fd, int indent)
{
    await fprintf(fd, "%*s", indent, "");
}

// Print an _Alignas specifier, if any.
static coro(braam_call *) void print_alignment_spec(FILE *fd, const AlignmentSpec *as, int indent)
{
    if (!as)
        return;
    await print_indent(fd, indent);
    await fprintf(fd, "AlignSpec: ");
    if (as->kind == ALIGN_SPEC_TYPE) {
        await fprintf(fd, "type\n");
        await print_type(fd, as->u.type, indent + 2);
    } else {
        await fprintf(fd, "expr\n");
        await print_expression(fd, as->u.expr, indent + 2);
    }
}

// Enum-to-string mappings
static const char *expr_kind_str[] = {
    [EXPR_LITERAL]      = "Literal",
    [EXPR_VAR]          = "Var",
    [EXPR_UNARY_OP]     = "UnaryOp",
    [EXPR_BINARY_OP]    = "BinaryOp",
    [EXPR_SUBSCRIPT]    = "Subscript",
    [EXPR_ASSIGN]       = "Assign",
    [EXPR_COND]         = "Cond",
    [EXPR_CAST]         = "Cast",
    [EXPR_CALL]         = "Call",
    [EXPR_COMPOUND]     = "Compound",
    [EXPR_FIELD_ACCESS] = "FieldAccess",
    [EXPR_PTR_ACCESS]   = "PtrAccess",
    [EXPR_POST_INC]     = "PostInc",
    [EXPR_POST_DEC]     = "PostDec",
    [EXPR_SIZEOF_EXPR]  = "SizeofExpr",
    [EXPR_SIZEOF_TYPE]  = "SizeofType",
    [EXPR_ALIGNOF]      = "Alignof",
    [EXPR_GENERIC]      = "Generic",
    [EXPR_VA_CLASS]     = "VaClass",
    [EXPR_YIELD]        = "Yield",
    [EXPR_AWAIT]        = "Await",
    [EXPR_CO_OP]        = "CoOp",
};

const char *co_op_name[] = {
    [CO_OP_INIT] = "__co_init",       [CO_OP_ALLOCA] = "__co_alloca",
    [CO_OP_RESUME] = "__co_resume",   [CO_OP_CANCEL] = "__co_cancel",
    [CO_OP_DESTROY] = "__co_destroy", [CO_OP_DONE] = "__co_done",
    [CO_OP_VALUE] = "__co_value",     [CO_OP_RESULT] = "__co_result",
    [CO_OP_SIZEOF] = "__co_sizeof",   [CO_OP_ALIGNOF] = "__co_alignof",
};

static const char *stmt_kind_str[] = { [STMT_EXPR] = "Expression",  [STMT_IF] = "If",
                                       [STMT_SWITCH] = "Switch",    [STMT_WHILE] = "While",
                                       [STMT_DO_WHILE] = "DoWhile", [STMT_FOR] = "For",
                                       [STMT_GOTO] = "Goto",        [STMT_CONTINUE] = "Continue",
                                       [STMT_BREAK] = "Break",      [STMT_RETURN] = "Return",
                                       [STMT_LABELED] = "Labeled",  [STMT_CASE] = "Case",
                                       [STMT_DEFAULT] = "Default",  [STMT_COMPOUND] = "Compound",
                                       [STMT_DEFER] = "Defer" };

const char *type_kind_str[] = {
    [TYPE_VOID]         = "void",
    [TYPE_BOOL]         = "_Bool",
    [TYPE_CHAR]         = "char",
    [TYPE_SCHAR]        = "signed char",
    [TYPE_UCHAR]        = "unsigned char",
    [TYPE_SHORT]        = "short",
    [TYPE_USHORT]       = "unsigned short",
    [TYPE_INT]          = "int",
    [TYPE_UINT]         = "unsigned int",
    [TYPE_LONG]         = "long",
    [TYPE_ULONG]        = "unsigned long",
    [TYPE_LONG_LONG]    = "long long",
    [TYPE_ULONG_LONG]   = "unsigned long long",
    [TYPE_SIGNED]       = "signed",
    [TYPE_UNSIGNED]     = "unsigned",
    [TYPE_FLOAT]        = "float",
    [TYPE_DOUBLE]       = "double",
    [TYPE_LONG_DOUBLE]  = "long double",
    [TYPE_COMPLEX]      = "_Complex",
    [TYPE_IMAGINARY]    = "_Imaginary",
    [TYPE_POINTER]      = "ptr",
    [TYPE_ARRAY]        = "array",
    [TYPE_FUNCTION]     = "func",
    [TYPE_STRUCT]       = "struct",
    [TYPE_UNION]        = "union",
    [TYPE_ENUM]         = "enum",
    [TYPE_TYPEDEF_NAME] = "typedef",
    [TYPE_ATOMIC]       = "_Atomic",
};

static const char *binary_op_kind_str[] = {
    [BINARY_MUL] = "*",          [BINARY_DIV] = "/",      [BINARY_MOD] = "%",
    [BINARY_ADD] = "+",          [BINARY_SUB] = "-",      [BINARY_LEFT_SHIFT] = "<<",
    [BINARY_RIGHT_SHIFT] = ">>", [BINARY_LT] = "<",       [BINARY_GT] = ">",
    [BINARY_LE] = "<=",          [BINARY_GE] = ">=",      [BINARY_EQ] = "==",
    [BINARY_NE] = "!=",          [BINARY_BIT_AND] = "&",  [BINARY_BIT_XOR] = "^",
    [BINARY_BIT_OR] = "|",       [BINARY_LOG_AND] = "&&", [BINARY_LOG_OR] = "||",
    [BINARY_COMMA] = ","
};

static const char *assign_op_kind_str[] = {
    [ASSIGN_SIMPLE] = "=", [ASSIGN_MUL] = "*=", [ASSIGN_DIV] = "/=",   [ASSIGN_MOD] = "%=",
    [ASSIGN_ADD] = "+=",   [ASSIGN_SUB] = "-=", [ASSIGN_LEFT] = "<<=", [ASSIGN_RIGHT] = ">>=",
    [ASSIGN_AND] = "&=",   [ASSIGN_XOR] = "^=", [ASSIGN_OR] = "|=",
};

static const char *unary_op_kind_str[] = {
    [UNARY_ADDRESS] = "&", [UNARY_DEREF] = "*",   [UNARY_PLUS] = "+",     [UNARY_NEG] = "-",
    [UNARY_BIT_NOT] = "~", [UNARY_LOG_NOT] = "!", [UNARY_PRE_INC] = "++", [UNARY_PRE_DEC] = "--",
};

// Forward declarations
static coro(braam_call *) void print_decl_spec(FILE *fd, const DeclSpec *spec, int indent);

// Print Field structure
coro(braam_call *) void print_field(FILE *fd, const Field *field, int indent)
{
    await print_indent(fd, indent);
    if (!field) {
        await fprintf(fd, "Field: NULL\n");
        return;
    }
    switch (field->kind) {
    case FIELD_MEMBER:
        await fprintf(fd, "Field: %s\n",
                      field->u.member.name ? field->u.member.name : "(anonymous)");
        await print_type(fd, field->u.member.type, indent + 2);
        if (field->u.member.bitfield) {
            await print_indent(fd, indent + 2);
            await fprintf(fd, "Bitfield:\n");
            await print_expression(fd, field->u.member.bitfield, indent + 4);
        }
        await print_alignment_spec(fd, field->u.member.align_spec, indent + 2);
        break;
    case FIELD_STATIC_ASSERT:
        await fprintf(fd, "StaticAssert:\n");
        await print_expression(fd, field->u.static_assrt.condition, indent + 2);
        if (field->u.static_assrt.message) {
            await print_indent(fd, indent + 2);
            await fprintf(fd, "Message: %s\n", field->u.static_assrt.message);
        }
        break;
    }
}

// Print Param structure
coro(braam_call *) void print_param(FILE *fd, Param *params, int indent)
{
    if (!params) {
        await print_indent(fd, indent);
        await fprintf(fd, "Param: NULL\n");
        return;
    }

    for (Param *p = params; p; p = p->next) {
        await print_indent(fd, indent);
        await fprintf(fd, "Param: %s\n", p->name ? p->name : "(no name)");
        await print_type(fd, p->type, indent + 4);
        await print_decl_spec(fd, p->specifiers, indent + 4);
    }
}

// Print TypeQualifier list
coro(braam_call *) void print_type_qualifiers(FILE *fd, const TypeQualifier *qualifiers, int indent)
{
    if (!qualifiers)
        return;
    await print_indent(fd, indent);
    await fprintf(fd, "Qualifiers:\n");
    for (const TypeQualifier *q = qualifiers; q; q = q->next) {
        await print_indent(fd, indent + 2);
        switch (q->kind) {
        case TYPE_QUALIFIER_CONST:
            await fprintf(fd, "const ");
            break;
        case TYPE_QUALIFIER_RESTRICT:
            await fprintf(fd, "restrict ");
            break;
        case TYPE_QUALIFIER_VOLATILE:
            await fprintf(fd, "volatile ");
            break;
        case TYPE_QUALIFIER_ATOMIC:
            await fprintf(fd, "_Atomic ");
            break;
        default:
            await fprintf(fd, "unknown ");
            break;
        }
    }
    await fprintf(fd, "\n");
}

// Print Type
coro(braam_call *) void print_type(FILE *fd, const Type *type, int indent)
{
    if (!type) {
        await print_indent(fd, indent);
        await fprintf(fd, "Type: NULL\n");
        return;
    }

    await print_indent(fd, indent);
    await fprintf(fd, "Type: ");
    switch (type->kind) {
    case TYPE_VOID:
        await fprintf(fd, "void\n");
        break;
    case TYPE_BOOL:
        await fprintf(fd, "_Bool\n");
        break;
    case TYPE_CHAR:
        await fprintf(fd, "char\n");
        break;
    case TYPE_SCHAR:
        await fprintf(fd, "char (signed)\n");
        break;
    case TYPE_UCHAR:
        await fprintf(fd, "char (unsigned)\n");
        break;
    case TYPE_SHORT:
        await fprintf(fd, "short (signed)\n");
        break;
    case TYPE_USHORT:
        await fprintf(fd, "short (unsigned)\n");
        break;
    case TYPE_INT:
        await fprintf(fd, "int (signed)\n");
        break;
    case TYPE_UINT:
        await fprintf(fd, "int (unsigned)\n");
        break;
    case TYPE_LONG:
        await fprintf(fd, "long (signed)\n");
        break;
    case TYPE_ULONG:
        await fprintf(fd, "long (unsigned)\n");
        break;
    case TYPE_LONG_LONG:
        await fprintf(fd, "long long (signed)\n");
        break;
    case TYPE_ULONG_LONG:
        await fprintf(fd, "long long (unsigned)\n");
        break;
    case TYPE_FLOAT:
        await fprintf(fd, "float\n");
        break;
    case TYPE_DOUBLE:
        await fprintf(fd, "double\n");
        break;
    case TYPE_COMPLEX:
        await fprintf(fd, "complex\n");
        await print_indent(fd, indent + 1);
        await fprintf(fd, "Base:\n");
        await print_type(fd, type->u.complex.base, indent + 2);
        break;
    case TYPE_IMAGINARY:
        await fprintf(fd, "imaginary\n");
        await print_indent(fd, indent + 1);
        await fprintf(fd, "Base:\n");
        await print_type(fd, type->u.complex.base, indent + 2);
        break;
    case TYPE_POINTER:
        await fprintf(fd, "pointer\n");
        await print_indent(fd, indent + 1);
        await fprintf(fd, "Target:\n");
        await print_type(fd, type->u.pointer.target, indent + 2);
        await print_type_qualifiers(fd, type->u.pointer.qualifiers, indent + 1);
        break;
    case TYPE_ARRAY:
        await fprintf(fd, "array\n");
        await print_indent(fd, indent + 1);
        await fprintf(fd, "Element:\n");
        await print_type(fd, type->u.array.element, indent + 2);
        if (type->u.array.size) {
            await print_indent(fd, indent + 1);
            await fprintf(fd, "Size:\n");
            await print_expression(fd, type->u.array.size, indent + 2);
        }
        await print_type_qualifiers(fd, type->u.array.qualifiers, indent + 1);
        await print_indent(fd, indent + 1);
        await fprintf(fd, "Static: %s\n", type->u.array.is_static ? "yes" : "no");
        break;
    case TYPE_FUNCTION:
        await fprintf(fd, "function\n");
        await print_indent(fd, indent + 1);
        await fprintf(fd, "ReturnType:\n");
        await print_type(fd, type->u.function.return_type, indent + 2);
        await print_indent(fd, indent + 1);
        await fprintf(fd, "Parameters:\n");
        await print_param(fd, type->u.function.params, indent + 2);
        await print_indent(fd, indent + 1);
        await fprintf(fd, "Variadic: %s\n", type->u.function.variadic ? "yes" : "no");
        break;
    case TYPE_STRUCT:
        await fprintf(fd, "struct %s\n",
                      type->u.struct_t.name ? type->u.struct_t.name : "(anonymous)");
        if (type->u.struct_t.fields) {
            await print_indent(fd, indent + 1);
            await fprintf(fd, "Fields:\n");
            for (const Field *f = type->u.struct_t.fields; f; f = f->next) {
                await print_field(fd, f, indent + 2);
            }
        }
        if (type->u.struct_t.frame_yield) {
            await print_indent(fd, indent + 1);
            await fprintf(fd, "FrameYield:\n");
            await print_type(fd, type->u.struct_t.frame_yield, indent + 2);
            await print_indent(fd, indent + 1);
            await fprintf(fd, "FrameResult:\n");
            await print_type(fd, type->u.struct_t.frame_result, indent + 2);
        }
        break;
    case TYPE_UNION:
        await fprintf(fd, "union %s\n",
                      type->u.struct_t.name ? type->u.struct_t.name : "(anonymous)");
        if (type->u.struct_t.fields) {
            await print_indent(fd, indent + 1);
            await fprintf(fd, "Fields:\n");
            for (const Field *f = type->u.struct_t.fields; f; f = f->next) {
                await print_field(fd, f, indent + 2);
            }
        }
        break;
    case TYPE_ENUM:
        await fprintf(fd, "enum %s\n", type->u.enum_t.name ? type->u.enum_t.name : "(anonymous)");
        if (type->u.enum_t.enumerators) {
            await print_indent(fd, indent + 1);
            await fprintf(fd, "Enumerators:\n");
            for (Enumerator *e = type->u.enum_t.enumerators; e; e = e->next) {
                await print_indent(fd, indent + 2);
                await fprintf(fd, "%s", e->name ? e->name : "(null)");
                if (e->value) {
                    await fprintf(fd, " = ");
                    await print_expression(fd, e->value, indent + 3);
                } else {
                    await fprintf(fd, "\n");
                }
            }
        }
        break;
    case TYPE_TYPEDEF_NAME:
        await fprintf(fd, "typedef %s\n",
                      type->u.typedef_name.name ? type->u.typedef_name.name : "(null)");
        break;
    case TYPE_ATOMIC:
        await fprintf(fd, "atomic\n");
        await print_indent(fd, indent + 1);
        await fprintf(fd, "Base:\n");
        await print_type(fd, type->u.atomic.base, indent + 2);
        break;
    default:
        await fprintf(fd, "unknown\n");
        break;
    }

    /* Print qualifiers for the type itself */
    await print_type_qualifiers(fd, type->qualifiers, indent);
}

// Print Literal
static coro(braam_call *) void print_literal(FILE *fd, const Literal *lit, int indent)
{
    if (!lit) {
        await print_indent(fd, indent);
        await fprintf(fd, "Literal: null\n");
        return;
    }
    await print_indent(fd, indent);
    await fprintf(fd, "Literal: ");
    switch (lit->kind) {
    case LITERAL_INT:
        await fprintf(fd, "int %" PRId64 "\n", lit->u.int_val);
        break;
    case LITERAL_LONG:
        await fprintf(fd, "long %" PRId64 "\n", lit->u.long_val);
        break;
    case LITERAL_LONG_LONG:
        await fprintf(fd, "long long %lld\n", lit->u.long_long_val);
        break;
    case LITERAL_UINT:
        await fprintf(fd, "unsigned int %" PRIu64 "\n", lit->u.uint_val);
        break;
    case LITERAL_ULONG:
        await fprintf(fd, "unsigned long %" PRIu64 "\n", lit->u.ulong_val);
        break;
    case LITERAL_ULONG_LONG:
        await fprintf(fd, "unsigned long long %llu\n", lit->u.ulong_long_val);
        break;
    case LITERAL_FLOAT:
        await fprintf(fd, "float %a\n", lit->u.real_val);
        break;
    case LITERAL_DOUBLE:
        await fprintf(fd, "double %a\n", lit->u.real_val);
        break;
    case LITERAL_LONG_DOUBLE: {
        char buf[F128_BUFSIZE];
        await fprintf(fd, "long double %s\n", f128_format(lit->u.long_double_val, buf));
        break;
    }
    case LITERAL_STRING:
        await fprintf(fd, "string \"%s\"\n", lit->u.string_val);
        break;
    case LITERAL_CHAR:
        await fprintf(fd, "char '%c'\n", lit->u.char_val);
        break;
    case LITERAL_ENUM:
        await fprintf(fd, "enum %s\n", lit->u.enum_const);
        break;
    }
}

// Print Initializer
coro(braam_call *) void print_initializer(FILE *fd, const Initializer *init, int indent)
{
    await print_indent(fd, indent);
    if (!init) {
        await fprintf(fd, "Initializer: null\n");
        return;
    }
    await fprintf(fd, "Initializer:\n");
    if (init->kind == INITIALIZER_SINGLE) {
        await print_expression(fd, init->u.expr, indent + 2);
    } else {
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Compound:\n");
        for (const InitItem *item = init->u.items; item; item = item->next) {
            await print_initializer(fd, item->init, indent + 4);
        }
    }
}

// Print GenericAssoc
static coro(braam_call *) void print_generic_assoc(FILE *fd, const GenericAssoc *assoc, int indent)
{
    await print_indent(fd, indent);
    if (!assoc) {
        await fprintf(fd, "Assoc: null\n");
        return;
    }
    await fprintf(fd, "Assoc:\n");
    if (assoc->kind == GENERIC_ASSOC_TYPE) {
        await print_type(fd, assoc->u.type_assoc.type, indent + 2);
        await print_expression(fd, assoc->u.type_assoc.expr, indent + 2);
    } else {
        await print_expression(fd, assoc->u.default_assoc, indent + 2);
    }
}

// Print Expr
coro(braam_call *) void print_expression(FILE *fd, const Expr *expr, int indent)
{
    if (!expr) {
        await print_indent(fd, indent);
        await fprintf(fd, "Expr: null\n");
        return;
    }
    await print_indent(fd, indent);
    await fprintf(fd, "Expr (%s):\n", expr_kind_str[expr->kind]);
    if (expr->type) {
        await print_type(fd, expr->type, indent + 2);
    }
    switch (expr->kind) {
    case EXPR_VAR:
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Name: \"%s\"\n", expr->u.var);
        break;
    case EXPR_LITERAL:
        await print_literal(fd, expr->u.literal, indent + 2);
        break;
    case EXPR_BINARY_OP:
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Operator: %s\n", binary_op_kind_str[expr->u.binary_op.op]);
        await print_expression(fd, expr->u.binary_op.left, indent + 2);
        await print_expression(fd, expr->u.binary_op.right, indent + 2);
        break;
    case EXPR_UNARY_OP:
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Operator: %s\n", unary_op_kind_str[expr->u.unary_op.op]);
        await print_expression(fd, expr->u.unary_op.expr, indent + 2);
        break;
    case EXPR_POST_INC:
    case EXPR_POST_DEC:
        await print_expression(fd, expr->u.post_inc, indent + 2);
        break;
    case EXPR_CALL:
        await print_expression(fd, expr->u.call.func, indent + 2);
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Arguments:\n");
        for (const Expr *arg = expr->u.call.args; arg; arg = arg->next) {
            await print_expression(fd, arg, indent + 4);
        }
        break;
    case EXPR_CAST:
        await print_type(fd, expr->u.cast.type, indent + 2);
        await print_expression(fd, expr->u.cast.expr, indent + 2);
        break;
    case EXPR_COMPOUND:
        for (const InitItem *init = expr->u.compound_literal.init; init; init = init->next) {
            await print_initializer(fd, init->init, indent + 4);
        }
        break;
    case EXPR_SIZEOF_EXPR:
        await print_expression(fd, expr->u.sizeof_expr, indent + 2);
        break;
    case EXPR_SIZEOF_TYPE:
        await print_type(fd, expr->u.sizeof_type, indent + 2);
        break;
    case EXPR_ALIGNOF:
        await print_type(fd, expr->u.align_of, indent + 2);
        break;
    case EXPR_VA_CLASS:
        await print_type(fd, expr->u.va_class, indent + 2);
        break;
    case EXPR_YIELD:
        if (expr->u.yield_expr)
            await print_expression(fd, expr->u.yield_expr, indent + 2);
        break;
    case EXPR_AWAIT:
        await print_expression(fd, expr->u.await_expr, indent + 2);
        break;
    case EXPR_CO_OP:
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Op: %s\n", co_op_name[expr->u.co_op.op]);
        for (const Expr *arg = expr->u.co_op.args; arg; arg = arg->next) {
            await print_expression(fd, arg, indent + 4);
        }
        break;
    case EXPR_GENERIC:
        await print_expression(fd, expr->u.generic.controlling_expr, indent + 2);
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Associations:\n");
        for (const GenericAssoc *assoc = expr->u.generic.associations; assoc; assoc = assoc->next) {
            await print_generic_assoc(fd, assoc, indent + 4);
        }
        break;
    case EXPR_ASSIGN:
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Assign: %s\n", assign_op_kind_str[expr->u.assign.op]);
        await print_expression(fd, expr->u.assign.target, indent + 2);
        await print_expression(fd, expr->u.assign.value, indent + 2);
        break;
    case EXPR_COND:
        await print_expression(fd, expr->u.cond.condition, indent + 2);
        await print_expression(fd, expr->u.cond.then_expr, indent + 2);
        await print_expression(fd, expr->u.cond.else_expr, indent + 2);
        break;
    case EXPR_SUBSCRIPT:
        await print_expression(fd, expr->u.subscript.left, indent + 2);
        await print_expression(fd, expr->u.subscript.right, indent + 2);
        break;
    case EXPR_FIELD_ACCESS:
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Field: .%s\n", expr->u.field_access.field);
        await print_expression(fd, expr->u.field_access.expr, indent + 2);
        break;
    case EXPR_PTR_ACCESS:
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Field: ->%s\n", expr->u.ptr_access.field);
        await print_expression(fd, expr->u.ptr_access.expr, indent + 2);
        break;
    }
    if (expr->next) {
        await print_indent(fd, indent);
        await fprintf(fd, "Next Expr:\n");
        await print_expression(fd, expr->next, indent + 2);
    }
}

// Print Pointer
coro(braam_call *) void print_pointer(FILE *fd, const Pointer *pointer, int indent)
{
    await print_indent(fd, indent);
    await fprintf(fd, "Pointer\n");
    await print_type_qualifiers(fd, pointer->qualifiers, indent + 2);
}

// Print DeclaratorSuffix
coro(braam_call *) void print_declarator_suffix(FILE *fd, DeclaratorSuffix *suffix, int indent)
{
    await print_indent(fd, indent);
    await fprintf(fd, "Suffix: ");
    switch (suffix->kind) {
    case SUFFIX_ARRAY:
        await fprintf(fd, "Array\n");
        if (suffix->u.array.size) {
            await print_expression(fd, suffix->u.array.size, indent + 2);
        }
        break;
    case SUFFIX_FUNCTION:
        await fprintf(fd, "Function\n");
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Parameters:\n");
        if (!suffix->u.function.params) {
            await print_indent(fd, indent + 4);
            await fprintf(fd, "Empty\n");
        } else {
            await print_param(fd, suffix->u.function.params, indent + 4);
        }
        break;
    case SUFFIX_POINTER:
        await fprintf(fd, "Pointer\n");
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Indirections:\n");
        for (const Pointer *pointer = suffix->u.pointer.pointers; pointer;
             pointer                = pointer->next) {
            await print_pointer(fd, pointer, indent + 4);
        }
        await print_declarator_suffix(fd, suffix->u.pointer.suffix, indent + 2);
        break;
    }
}

// Print Declarator
coro(braam_call *) void print_declarator(FILE *fd, Declarator *decl, int indent)
{
    await print_indent(fd, indent);
    if (!decl) {
        await fprintf(fd, "Declarator: null\n");
        return;
    }
    await fprintf(fd, "Declarator: %s\n", decl->name ? decl->name : "(abstract)");
    for (const Pointer *pointer = decl->pointers; pointer; pointer = pointer->next) {
        await print_pointer(fd, pointer, indent + 4);
    }
    for (DeclaratorSuffix *suffix = decl->suffixes; suffix; suffix = suffix->next) {
        await print_declarator_suffix(fd, suffix, indent + 2);
    }
}

// Print TypeSpec
coro(braam_call *) void print_type_spec(FILE *fd, const TypeSpec *spec, int indent)
{
    switch (spec->kind) {
    case TYPE_SPEC_BASIC:
        await fprintf(fd, "%s\n", type_kind_str[spec->u.basic->kind]);
        break;
    case TYPE_SPEC_STRUCT:
        await fprintf(fd, "struct %s\n",
                      spec->u.struct_spec.name ? spec->u.struct_spec.name : "(anonymous)");
        for (const Field *field = spec->u.struct_spec.fields; field; field = field->next) {
            await print_field(fd, field, indent + 4);
        }
        break;
    case TYPE_SPEC_UNION:
        await fprintf(fd, "union %s\n",
                      spec->u.struct_spec.name ? spec->u.struct_spec.name : "(anonymous)");
        for (const Field *field = spec->u.struct_spec.fields; field; field = field->next) {
            await print_field(fd, field, indent + 4);
        }
        break;
    case TYPE_SPEC_ENUM:
        await fprintf(fd, "enum %s\n",
                      spec->u.enum_spec.name ? spec->u.enum_spec.name : "(anonymous)");
        for (const Enumerator *e = spec->u.enum_spec.enumerators; e; e = e->next) {
            await print_indent(fd, indent + 4);
            await fprintf(fd, "Enumerator: \"%s\"\n", e->name);
            if (e->value) {
                await print_expression(fd, e->value, indent + 6);
            }
        }
        break;
    case TYPE_SPEC_TYPEDEF_NAME:
        await fprintf(fd, "typedef %s\n", spec->u.typedef_name.name);
        break;
    case TYPE_SPEC_ATOMIC:
        await fprintf(fd, "_Atomic\n");
        await print_type(fd, spec->u.atomic.type, indent + 4);
        break;
    case TYPE_SPEC_CORO_FRAME:
        await fprintf(fd, "_Coro_frame\n");
        await print_type(fd, spec->u.atomic.type, indent + 4);
        break;
    }
}

// Print DeclSpec
static coro(braam_call *) void print_decl_spec(FILE *fd, const DeclSpec *spec, int indent)
{
    if (!spec) {
        return;
    }
    await print_indent(fd, indent);
    await fprintf(fd, "DeclSpec:\n");
    if (spec->storage) {
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Storage: ");
        switch (spec->storage) {
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
            await fprintf(fd, "_Thread_local\n");
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
    if (spec->qualifiers) {
        await print_type_qualifiers(fd, spec->qualifiers, indent + 2);
    }
    if (spec->func_specs) {
        await print_indent(fd, indent + 2);
        await fprintf(fd, "FunctionSpec: ");
        switch (spec->func_specs->kind) {
        case FUNC_SPEC_INLINE:
            await fprintf(fd, "inline\n");
            break;
        case FUNC_SPEC_NORETURN:
            await fprintf(fd, "_Noreturn\n");
            break;
        case FUNC_SPEC_CORO:
            await fprintf(fd, "_Coro\n");
            await print_type(fd, spec->func_specs->yield_type, indent + 4);
            break;
        }
    }
    await print_alignment_spec(fd, spec->align_spec, indent + 2);
}

// Print InitDeclarator
static coro(braam_call *) void print_init_declarator(FILE *fd, InitDeclarator *id, int indent)
{
    if (!id) {
        await print_indent(fd, indent);
        await fprintf(fd, "InitDeclarator: null\n");
        return;
    }
    await print_indent(fd, indent);
    await fprintf(fd, "InitDeclarator: %s\n", id->name ? id->name : "(abstract)");
    await print_type(fd, id->type, indent + 2);
    if (id->init) {
        await print_initializer(fd, id->init, indent + 2);
    }
    if (id->next) {
        await print_indent(fd, indent);
        await fprintf(fd, "Next InitDeclarator:\n");
        await print_init_declarator(fd, id->next, indent + 2);
    }
}

// Print Declaration
coro(braam_call *) void print_declaration(FILE *fd, Declaration *decl, int indent)
{
    if (!decl) {
        await print_indent(fd, indent);
        await fprintf(fd, "Declaration: null\n");
        return;
    }
    await print_indent(fd, indent);
    await fprintf(fd, "Declaration: ");
    switch (decl->kind) {
    case DECL_VAR:
        await fprintf(fd, "Variable\n");
        await print_decl_spec(fd, decl->u.var.specifiers, indent + 2);
        await print_init_declarator(fd, decl->u.var.declarators, indent + 2);
        break;
    case DECL_STATIC_ASSERT:
        await fprintf(fd, "StaticAssert\n");
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Condition:\n");
        await print_expression(fd, decl->u.static_assrt.condition, indent + 4);
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Message: \"%s\"\n", decl->u.static_assrt.message);
        break;
    case DECL_EMPTY:
        await fprintf(fd, "Empty\n");
        await print_decl_spec(fd, decl->u.empty.specifiers, indent + 2);
        await print_type(fd, decl->u.empty.type, indent + 2);
        break;
    }
}

// Print Stmt
coro(braam_call *) void print_statement(FILE *fd, Stmt *stmt, int indent)
{
    if (!stmt) {
        await print_indent(fd, indent);
        await fprintf(fd, "Stmt: null\n");
        return;
    }
    await print_indent(fd, indent);
    await fprintf(fd, "Stmt (%s):\n", stmt_kind_str[stmt->kind]);
    switch (stmt->kind) {
    case STMT_EXPR:
        await print_expression(fd, stmt->u.expr, indent + 2);
        break;
    case STMT_IF:
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Condition:\n");
        await print_expression(fd, stmt->u.if_stmt.condition, indent + 4);
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Then:\n");
        await print_statement(fd, stmt->u.if_stmt.then_stmt, indent + 4);
        if (stmt->u.if_stmt.else_stmt) {
            await print_indent(fd, indent + 2);
            await fprintf(fd, "Else:\n");
            await print_statement(fd, stmt->u.if_stmt.else_stmt, indent + 4);
        }
        break;
    case STMT_SWITCH:
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Expression:\n");
        await print_expression(fd, stmt->u.switch_stmt.expr, indent + 4);
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Body:\n");
        await print_statement(fd, stmt->u.switch_stmt.body, indent + 4);
        break;
    case STMT_WHILE:
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Condition:\n");
        await print_expression(fd, stmt->u.while_stmt.condition, indent + 4);
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Body:\n");
        await print_statement(fd, stmt->u.while_stmt.body, indent + 4);
        break;
    case STMT_DO_WHILE:
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Body:\n");
        await print_statement(fd, stmt->u.do_while.body, indent + 4);
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Condition:\n");
        await print_expression(fd, stmt->u.do_while.condition, indent + 4);
        break;
    case STMT_FOR:
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Init:\n");
        if (stmt->u.for_stmt.init->kind == FOR_INIT_EXPR) {
            await print_expression(fd, stmt->u.for_stmt.init->u.expr, indent + 4);
        } else {
            await print_declaration(fd, stmt->u.for_stmt.init->u.decl, indent + 4);
        }
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Condition:\n");
        await print_expression(fd, stmt->u.for_stmt.condition, indent + 4);
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Update:\n");
        await print_expression(fd, stmt->u.for_stmt.update, indent + 4);
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Body:\n");
        await print_statement(fd, stmt->u.for_stmt.body, indent + 4);
        break;
    case STMT_GOTO:
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Label: \"%s\"\n", stmt->u.goto_label);
        break;
    case STMT_CONTINUE:
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Continue\n");
        break;
    case STMT_BREAK:
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Break\n");
        break;
    case STMT_RETURN:
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Return:\n");
        await print_expression(fd, stmt->u.expr, indent + 4);
        break;
    case STMT_LABELED:
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Label: \"%s\"\n", stmt->u.labeled.label);
        await print_statement(fd, stmt->u.labeled.stmt, indent + 2);
        break;
    case STMT_CASE:
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Case:\n");
        await print_expression(fd, stmt->u.case_stmt.expr, indent + 4);
        await print_statement(fd, stmt->u.case_stmt.stmt, indent + 4);
        break;
    case STMT_DEFAULT:
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Default:\n");
        await print_statement(fd, stmt->u.default_stmt, indent + 4);
        break;
    case STMT_DEFER:
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Defer:\n");
        await print_statement(fd, stmt->u.defer_stmt, indent + 4);
        break;
    case STMT_COMPOUND:
        await print_indent(fd, indent + 2);
        await fprintf(fd, "Compound:\n");
        for (DeclOrStmt *item = stmt->u.compound; item; item = item->next) {
            await print_indent(fd, indent + 4);
            await fprintf(fd, "Item:\n");
            if (item->kind == DECL_OR_STMT_DECL) {
                await print_declaration(fd, item->u.decl, indent + 6);
            } else {
                await print_statement(fd, item->u.stmt, indent + 6);
            }
        }
        break;
    }
}

// Print ExternalDecl
coro(braam_call *) void print_external_decl(FILE *fd, ExternalDecl *ext, int indent)
{
    if (!ext) {
        await print_indent(fd, indent);
        await fprintf(fd, "ExternalDecl: null\n");
        return;
    }
    await print_indent(fd, indent);
    await fprintf(fd, "ExternalDecl:\n");
    await print_indent(fd, indent + 2);
    switch (ext->kind) {
    case EXTERNAL_DECL_FUNCTION:
        await fprintf(fd, "Function: %s\n",
                      ext->u.function.name ? ext->u.function.name : "(anonymous)");
        await print_type(fd, ext->u.function.type, indent + 4);
        await print_decl_spec(fd, ext->u.function.specifiers, indent + 4);
        await print_statement(fd, ext->u.function.body, indent + 4);
        break;
    case EXTERNAL_DECL_DECLARATION:
        await fprintf(fd, "Declaration\n");
        await print_declaration(fd, ext->u.declaration, indent + 4);
        break;
    }
}

// Main print function
coro(braam_call *) void print_program(FILE *fd, Program *program)
{
    if (!program) {
        await fprintf(fd, "Program: null\n");
        return;
    }
    await fprintf(fd, "Program:\n");
    for (ExternalDecl *ext = program->decls; ext; ext = ext->next) {
        await print_external_decl(fd, ext, 2);
    }
}
