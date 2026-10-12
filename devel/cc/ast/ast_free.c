#include <stdlib.h>

#include "ast.h"
#include "internal.h"
#include "xalloc.h"

void free_program(Program *program)
{
    if (program == NULL)
        return;
    free_external_decl(program->decls);
    xfree(program);
}

void free_type(Type *type)
{
    if (type == NULL)
        return;
    switch (type->kind) {
    case TYPE_COMPLEX:
    case TYPE_IMAGINARY:
        free_type(type->u.complex.base);
        break;
    case TYPE_POINTER:
        free_type(type->u.pointer.target);
        free_type_qualifier(type->u.pointer.qualifiers);
        break;
    case TYPE_ARRAY:
        free_type(type->u.array.element);
        free_expression(type->u.array.size);
        free_type_qualifier(type->u.array.qualifiers);
        break;
    case TYPE_FUNCTION:
        free_type(type->u.function.return_type);
        free_param(type->u.function.params);
        break;
    case TYPE_STRUCT:
    case TYPE_UNION:
        xfree(type->u.struct_t.name);
        free_field(type->u.struct_t.fields);
        free_type(type->u.struct_t.frame_yield);
        free_type(type->u.struct_t.frame_result);
        break;
    case TYPE_ENUM:
        xfree(type->u.enum_t.name);
        free_enumerator(type->u.enum_t.enumerators);
        break;
    case TYPE_TYPEDEF_NAME:
        xfree(type->u.typedef_name.name);
        break;
    case TYPE_ATOMIC:
        free_type(type->u.atomic.base);
        break;
    default:
        break; /* No nested allocations for basic types */
    }
    free_type_qualifier(type->qualifiers);
    xfree(type);
}

void free_type_qualifier(TypeQualifier *qual)
{
    while (qual != NULL) {
        TypeQualifier *next = qual->next;
        xfree(qual);
        qual = next;
    }
}

void free_field(Field *field)
{
    while (field != NULL) {
        Field *next = field->next;
        switch (field->kind) {
        case FIELD_MEMBER:
            free_type(field->u.member.type);
            xfree(field->u.member.name);
            free_expression(field->u.member.bitfield);
            free_alignment_spec(field->u.member.align_spec);
            break;
        case FIELD_STATIC_ASSERT:
            free_expression(field->u.static_assrt.condition);
            xfree(field->u.static_assrt.message);
            break;
        }
        xfree(field);
        field = next;
    }
}

void free_enumerator(Enumerator *enumerator)
{
    while (enumerator != NULL) {
        Enumerator *next = enumerator->next;
        xfree(enumerator->name);
        free_expression(enumerator->value);
        xfree(enumerator);
        enumerator = next;
    }
}

void free_param(Param *param)
{
    while (param != NULL) {
        Param *next = param->next;
        xfree(param->name);
        free_type(param->type);
        free_decl_spec(param->specifiers);
        xfree(param);
        param = next;
    }
}

void free_declaration(Declaration *decl)
{
    while (decl != NULL) {
        Declaration *next = decl->next;
        switch (decl->kind) {
        case DECL_VAR:
            free_decl_spec(decl->u.var.specifiers);
            free_init_declarator(decl->u.var.declarators);
            break;
        case DECL_STATIC_ASSERT:
            free_expression(decl->u.static_assrt.condition);
            xfree(decl->u.static_assrt.message);
            break;
        case DECL_EMPTY:
            free_decl_spec(decl->u.empty.specifiers);
            free_type(decl->u.empty.type);
            break;
        }
        xfree(decl);
        decl = next;
    }
}

void free_decl_spec(DeclSpec *spec)
{
    if (spec == NULL)
        return;
    free_type_qualifier(spec->qualifiers);
    free_function_spec(spec->func_specs);
    free_alignment_spec(spec->align_spec);
    xfree(spec);
}

void free_function_spec(FunctionSpec *fs)
{
    while (fs != NULL) {
        FunctionSpec *next = fs->next;
        free_type(fs->yield_type);
        xfree(fs);
        fs = next;
    }
}

void free_alignment_spec(AlignmentSpec *as)
{
    if (as == NULL)
        return;
    if (as->kind == ALIGN_SPEC_TYPE) {
        free_type(as->u.type);
    } else {
        free_expression(as->u.expr);
    }
    xfree(as);
}

void free_init_declarator(InitDeclarator *init_decl)
{
    while (init_decl != NULL) {
        InitDeclarator *next = init_decl->next;
        free_type(init_decl->type);
        xfree(init_decl->name);
        free_initializer(init_decl->init);
        xfree(init_decl);
        init_decl = next;
    }
}

void free_initializer(Initializer *init)
{
    if (init == NULL)
        return;
    switch (init->kind) {
    case INITIALIZER_SINGLE:
        free_expression(init->u.expr);
        break;
    case INITIALIZER_COMPOUND:
        free_init_item(init->u.items);
        break;
    }
    free_type(init->type);
    xfree(init);
}

void free_init_item(InitItem *item)
{
    while (item != NULL) {
        InitItem *next = item->next;
        free_designator(item->designators);
        free_initializer(item->init);
        xfree(item);
        item = next;
    }
}

void free_designator(Designator *design)
{
    while (design != NULL) {
        Designator *next = design->next;
        if (design->kind == DESIGNATOR_ARRAY) {
            free_expression(design->u.expr);
        } else {
            xfree(design->u.name);
        }
        xfree(design);
        design = next;
    }
}

void free_expression(Expr *expr)
{
    while (expr != NULL) {
        Expr *next = expr->next;
        switch (expr->kind) {
        case EXPR_LITERAL:
            free_literal(expr->u.literal);
            break;
        case EXPR_VAR:
            xfree(expr->u.var);
            break;
        case EXPR_UNARY_OP:
            free_expression(expr->u.unary_op.expr);
            break;
        case EXPR_BINARY_OP:
            free_expression(expr->u.binary_op.left);
            free_expression(expr->u.binary_op.right);
            break;
        case EXPR_ASSIGN:
            free_expression(expr->u.assign.target);
            free_expression(expr->u.assign.value);
            break;
        case EXPR_COND:
            free_expression(expr->u.cond.condition);
            free_expression(expr->u.cond.then_expr);
            free_expression(expr->u.cond.else_expr);
            break;
        case EXPR_CAST:
            free_type(expr->u.cast.type);
            free_expression(expr->u.cast.expr);
            break;
        case EXPR_CALL:
            free_expression(expr->u.call.func);
            free_expression(expr->u.call.args);
            break;
        case EXPR_COMPOUND:
            free_type(expr->u.compound_literal.type);
            free_init_item(expr->u.compound_literal.init);
            break;
        case EXPR_SUBSCRIPT:
            free_expression(expr->u.subscript.left);
            free_expression(expr->u.subscript.right);
            break;
        case EXPR_FIELD_ACCESS:
        case EXPR_PTR_ACCESS:
            free_expression(expr->u.field_access.expr);
            xfree(expr->u.field_access.field);
            free_type(expr->u.field_access.member_type);
            break;
        case EXPR_POST_INC:
        case EXPR_POST_DEC:
            free_expression(expr->u.post_inc);
            break;
        case EXPR_SIZEOF_EXPR:
            free_expression(expr->u.sizeof_expr);
            break;
        case EXPR_SIZEOF_TYPE:
        case EXPR_ALIGNOF:
        case EXPR_VA_CLASS:
            free_type(expr->u.sizeof_type);
            break;
        case EXPR_GENERIC:
            free_expression(expr->u.generic.controlling_expr);
            free_generic_assoc(expr->u.generic.associations);
            break;
        case EXPR_YIELD:
            free_expression(expr->u.yield_expr);
            break;
        case EXPR_AWAIT:
            free_expression(expr->u.await_expr);
            break;
        case EXPR_CO_OP:
            free_expression(expr->u.co_op.args);
            break;
        }
        free_type(expr->type);
        xfree(expr);
        expr = next;
    }
}

void free_literal(Literal *lit)
{
    if (lit == NULL)
        return;
    switch (lit->kind) {
    case LITERAL_STRING:
        xfree(lit->u.string_val);
        break;
    case LITERAL_ENUM:
        xfree(lit->u.enum_const);
        break;
    default:
        break; /* No allocations for int, float, char */
    }
    xfree(lit);
}

void free_generic_assoc(GenericAssoc *assoc)
{
    while (assoc != NULL) {
        GenericAssoc *next = assoc->next;
        if (assoc->kind == GENERIC_ASSOC_TYPE) {
            free_type(assoc->u.type_assoc.type);
            free_expression(assoc->u.type_assoc.expr);
        } else {
            free_expression(assoc->u.default_assoc);
        }
        xfree(assoc);
        assoc = next;
    }
}

void free_statement(Stmt *stmt)
{
    if (stmt == NULL)
        return;
    switch (stmt->kind) {
    case STMT_EXPR:
        free_expression(stmt->u.expr);
        break;
    case STMT_COMPOUND:
        free_decl_or_stmt(stmt->u.compound);
        break;
    case STMT_IF:
        free_expression(stmt->u.if_stmt.condition);
        free_statement(stmt->u.if_stmt.then_stmt);
        free_statement(stmt->u.if_stmt.else_stmt);
        break;
    case STMT_SWITCH:
        free_expression(stmt->u.switch_stmt.expr);
        free_statement(stmt->u.switch_stmt.body);
        break;
    case STMT_WHILE:
        free_expression(stmt->u.while_stmt.condition);
        free_statement(stmt->u.while_stmt.body);
        break;
    case STMT_DO_WHILE:
        free_statement(stmt->u.do_while.body);
        free_expression(stmt->u.do_while.condition);
        break;
    case STMT_FOR:
        free_for_init(stmt->u.for_stmt.init);
        free_expression(stmt->u.for_stmt.condition);
        free_expression(stmt->u.for_stmt.update);
        free_statement(stmt->u.for_stmt.body);
        break;
    case STMT_GOTO:
        xfree(stmt->u.goto_label);
        break;
    case STMT_CONTINUE:
    case STMT_BREAK:
        break; /* No allocations */
    case STMT_RETURN:
        free_expression(stmt->u.expr);
        break;
    case STMT_LABELED:
        xfree(stmt->u.labeled.label);
        free_statement(stmt->u.labeled.stmt);
        break;
    case STMT_CASE:
        free_expression(stmt->u.case_stmt.expr);
        free_statement(stmt->u.case_stmt.stmt);
        break;
    case STMT_DEFAULT:
        free_statement(stmt->u.default_stmt);
        break;
    case STMT_DEFER:
        free_statement(stmt->u.defer_stmt);
        break;
    }
    xfree(stmt->loop_end_label);
    xfree(stmt->loop_continue_label);
    xfree(stmt->branch_target_label);
    xfree(stmt);
}

void free_decl_or_stmt(DeclOrStmt *ds)
{
    while (ds != NULL) {
        DeclOrStmt *next = ds->next;
        if (ds->kind == DECL_OR_STMT_DECL) {
            free_declaration(ds->u.decl);
        } else {
            free_statement(ds->u.stmt);
        }
        xfree(ds);
        ds = next;
    }
}

void free_for_init(ForInit *fi)
{
    if (fi == NULL)
        return;
    if (fi->kind == FOR_INIT_EXPR) {
        free_expression(fi->u.expr);
    } else {
        free_declaration(fi->u.decl);
    }
    xfree(fi);
}

void free_external_decl(ExternalDecl *ext_decl)
{
    while (ext_decl != NULL) {
        ExternalDecl *next = ext_decl->next;
        if (ext_decl->kind == EXTERNAL_DECL_FUNCTION) {
            free_type(ext_decl->u.function.type);
            xfree(ext_decl->u.function.name);
            free_decl_spec(ext_decl->u.function.specifiers);
            free_declaration(ext_decl->u.function.param_decls);
            free_statement(ext_decl->u.function.body);
        } else {
            free_declaration(ext_decl->u.declaration);
        }
        xfree(ext_decl);
        ext_decl = next;
    }
}

void free_type_spec(TypeSpec *ts)
{
    while (ts != NULL) {
        TypeSpec *next = ts->next;
        switch (ts->kind) {
        case TYPE_SPEC_BASIC:
            free_type(ts->u.basic);
            break;
        case TYPE_SPEC_STRUCT:
        case TYPE_SPEC_UNION:
            xfree(ts->u.struct_spec.name);
            free_field(ts->u.struct_spec.fields);
            break;
        case TYPE_SPEC_ENUM:
            xfree(ts->u.enum_spec.name);
            free_enumerator(ts->u.enum_spec.enumerators);
            break;
        case TYPE_SPEC_TYPEDEF_NAME:
            xfree(ts->u.typedef_name.name);
            break;
        case TYPE_SPEC_ATOMIC:
        case TYPE_SPEC_CORO_FRAME:
            free_type(ts->u.atomic.type);
            break;
        }
        free_type_qualifier(ts->qualifiers);
        xfree(ts);
        ts = next;
    }
}

void free_declarator(Declarator *decl)
{
    while (decl != NULL) {
        Declarator *next = decl->next;
        xfree(decl->name);
        free_pointer(decl->pointers);
        free_declarator_suffix(decl->suffixes);
        xfree(decl);
        decl = next;
    }
}

void free_pointer(Pointer *ptr)
{
    while (ptr != NULL) {
        Pointer *next = ptr->next;
        free_type_qualifier(ptr->qualifiers);
        xfree(ptr);
        ptr = next;
    }
}

void free_declarator_suffix(DeclaratorSuffix *suffix)
{
    while (suffix != NULL) {
        DeclaratorSuffix *next = suffix->next;
        switch (suffix->kind) {
        case SUFFIX_ARRAY:
            free_expression(suffix->u.array.size);
            free_type_qualifier(suffix->u.array.qualifiers);
            break;
        case SUFFIX_FUNCTION:
            free_param(suffix->u.function.params);
            break;
        case SUFFIX_POINTER:
            free_pointer(suffix->u.pointer.pointers);
            free_declarator_suffix(suffix->u.pointer.suffix);
            break;
        }
        xfree(suffix);
        suffix = next;
    }
}
