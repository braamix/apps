#include <stdio.h>
#include <stdlib.h>

#include "ast.h"
#include "tags.h"
#include "wio.h"

int export_debug; // Enable manually for debug

void export_type(WFILE *fd, Type *type);
void export_type_qualifier(WFILE *fd, const TypeQualifier *qual);
void export_field(WFILE *fd, Field *field);
void export_enumerator(WFILE *fd, Enumerator *enumr);
void export_param(WFILE *fd, Param *param);
void export_declaration(WFILE *fd, Declaration *decl);
void export_decl_spec(WFILE *fd, DeclSpec *spec);
void export_function_spec(WFILE *fd, const FunctionSpec *fspec);
void export_alignment_spec(WFILE *fd, AlignmentSpec *aspec);
void export_init_declarator(WFILE *fd, InitDeclarator *idecl);
void export_initializer(WFILE *fd, Initializer *init);
void export_init_item(WFILE *fd, InitItem *item);
void export_designator(WFILE *fd, Designator *desg);
void export_expr(WFILE *fd, Expr *expr);
void export_literal(WFILE *fd, const Literal *lit);
void export_generic_assoc(WFILE *fd, GenericAssoc *gasc);
void export_stmt(WFILE *fd, Stmt *stmt);
void export_decl_or_stmt(WFILE *fd, DeclOrStmt *dost);
void export_for_init(WFILE *fd, ForInit *finit);
void export_external_decl(WFILE *fd, ExternalDecl *exdecl);

// The file of the last location written: a location names its file only when it
// differs, so a reader keeps the same state (import_loc).
static const char *last_file;

//
// A source location: line, column, then 1 and the file name when the file is not
// the one of the location before, else 0.
//
static void export_loc(WFILE *fd, SrcLoc loc)
{
    wputw(loc.line, fd);
    wputw(loc.col, fd);
    if (loc.file != last_file) {
        wputw(1, fd);
        wputstr(loc.file ? loc.file : "", fd);
        last_file = loc.file;
    } else {
        wputw(0, fd);
    }
}

void export_ast(int fildes, Program *program)
{
    if (export_debug) {
        printf("--- %s()\n", __func__);
    }
    WFILE fd;
    if (wdopen(&fd, fildes, "a") < 0) {
        fprintf(stderr, "Error exporting AST: cannot open file descriptor #%d\n", fildes);
        exit(1);
    }
    wputw(TAG_PROGRAM, &fd);
    last_file = NULL;
    if (program) {
        for (ExternalDecl *decl = program->decls; decl; decl = decl->next) {
            export_external_decl(&fd, decl);
        }
    }
    wputw(TAG_EOL, &fd);
    wclose(&fd);
}

void export_type(WFILE *fd, Type *type)
{
    if (export_debug) {
        printf("--- %s()\n", __func__);
    }
    if (!type) {
        wputw(TAG_EOL, fd);
        return;
    }
    wputw(TAG_TYPE + type->kind, fd);
    switch (type->kind) {
    case TYPE_VOID:
    case TYPE_BOOL:
    case TYPE_CHAR:
    case TYPE_SCHAR:
    case TYPE_UCHAR:
    case TYPE_SHORT:
    case TYPE_USHORT:
    case TYPE_INT:
    case TYPE_UINT:
    case TYPE_LONG:
    case TYPE_ULONG:
    case TYPE_LONG_LONG:
    case TYPE_ULONG_LONG:
    case TYPE_SIGNED:
    case TYPE_UNSIGNED:
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
    case TYPE_LONG_DOUBLE:
        break;
    case TYPE_COMPLEX:
    case TYPE_IMAGINARY:
        export_type(fd, type->u.complex.base);
        break;
    case TYPE_POINTER:
        export_type(fd, type->u.pointer.target);
        for (const TypeQualifier *q = type->u.pointer.qualifiers; q; q = q->next) {
            export_type_qualifier(fd, q);
        }
        wputw(TAG_EOL, fd);
        break;
    case TYPE_ARRAY:
        export_type(fd, type->u.array.element);
        export_expr(fd, type->u.array.size);
        for (const TypeQualifier *q = type->u.array.qualifiers; q; q = q->next) {
            export_type_qualifier(fd, q);
        }
        wputw(TAG_EOL, fd);
        wputw((size_t)type->u.array.is_static, fd);
        break;
    case TYPE_FUNCTION:
        export_type(fd, type->u.function.return_type);
        for (Param *p = type->u.function.params; p; p = p->next) {
            export_param(fd, p);
        }
        wputw(TAG_EOL, fd);
        wputw((size_t)type->u.function.variadic, fd);
        break;
    case TYPE_STRUCT:
    case TYPE_UNION:
        wputstr(type->u.struct_t.name, fd);
        for (Field *f = type->u.struct_t.fields; f; f = f->next) {
            export_field(fd, f);
        }
        wputw(TAG_EOL, fd);
        export_type(fd, type->u.struct_t.frame_yield);
        export_type(fd, type->u.struct_t.frame_result);
        break;
    case TYPE_ENUM:
        wputstr(type->u.enum_t.name, fd);
        for (Enumerator *e = type->u.enum_t.enumerators; e; e = e->next) {
            export_enumerator(fd, e);
        }
        wputw(TAG_EOL, fd);
        break;
    case TYPE_TYPEDEF_NAME:
        wputstr(type->u.typedef_name.name, fd);
        break;
    case TYPE_ATOMIC:
        export_type(fd, type->u.atomic.base);
        break;
    }
    for (const TypeQualifier *q = type->qualifiers; q; q = q->next) {
        export_type_qualifier(fd, q);
    }
    wputw(TAG_EOL, fd);
}

void export_type_qualifier(WFILE *fd, const TypeQualifier *qual)
{
    if (export_debug) {
        printf("--- %s()\n", __func__);
    }
    if (!qual) {
        wputw(TAG_EOL, fd);
        return;
    }
    wputw(TAG_TYPEQUALIFIER + qual->kind, fd);
}

void export_field(WFILE *fd, Field *field)
{
    if (export_debug) {
        printf("--- %s()\n", __func__);
    }
    if (!field) {
        wputw(TAG_EOL, fd);
        return;
    }
    switch (field->kind) {
    case FIELD_MEMBER:
        wputw(TAG_FIELD, fd);
        export_type(fd, field->u.member.type);
        wputstr(field->u.member.name, fd);
        export_expr(fd, field->u.member.bitfield);
        export_alignment_spec(fd, field->u.member.align_spec);
        break;
    case FIELD_STATIC_ASSERT:
        wputw(TAG_STATIC_ASSERT, fd);
        export_expr(fd, field->u.static_assrt.condition);
        wputstr(field->u.static_assrt.message, fd);
        break;
    }
}

void export_enumerator(WFILE *fd, Enumerator *enumr)
{
    if (export_debug) {
        printf("--- %s()\n", __func__);
    }
    if (!enumr) {
        wputw(TAG_EOL, fd);
        return;
    }
    wputw(TAG_ENUMERATOR, fd);
    wputstr(enumr->name, fd);
    export_expr(fd, enumr->value);
}

void export_param(WFILE *fd, Param *param)
{
    if (export_debug) {
        printf("--- %s()\n", __func__);
    }
    if (!param) {
        wputw(TAG_EOL, fd);
        return;
    }
    wputw(TAG_PARAM, fd);
    export_loc(fd, param->loc);
    wputstr(param->name, fd);
    export_type(fd, param->type);
    export_decl_spec(fd, param->specifiers);
}

void export_declaration(WFILE *fd, Declaration *decl)
{
    if (export_debug) {
        printf("--- %s()\n", __func__);
    }
    if (!decl) {
        wputw(TAG_EOL, fd);
        return;
    }
    wputw(TAG_DECLARATION + decl->kind, fd);
    export_loc(fd, decl->loc);
    switch (decl->kind) {
    case DECL_VAR:
        export_decl_spec(fd, decl->u.var.specifiers);
        for (InitDeclarator *id = decl->u.var.declarators; id; id = id->next) {
            export_init_declarator(fd, id);
        }
        wputw(TAG_EOL, fd);
        break;
    case DECL_STATIC_ASSERT:
        export_expr(fd, decl->u.static_assrt.condition);
        wputstr(decl->u.static_assrt.message, fd);
        break;
    case DECL_EMPTY:
        export_decl_spec(fd, decl->u.empty.specifiers);
        export_type(fd, decl->u.empty.type);
        break;
    }
}

void export_decl_spec(WFILE *fd, DeclSpec *spec)
{
    if (export_debug) {
        printf("--- %s()\n", __func__);
    }
    if (!spec) {
        wputw(TAG_EOL, fd);
        return;
    }
    wputw(TAG_DECLSPEC, fd);
    for (const TypeQualifier *q = spec->qualifiers; q; q = q->next) {
        export_type_qualifier(fd, q);
    }
    wputw(TAG_EOL, fd);
    wputw(spec->storage, fd);
    for (const FunctionSpec *fs = spec->func_specs; fs; fs = fs->next) {
        export_function_spec(fd, fs);
    }
    wputw(TAG_EOL, fd);
    export_alignment_spec(fd, spec->align_spec);
}

void export_function_spec(WFILE *fd, const FunctionSpec *fspec)
{
    if (export_debug) {
        printf("--- %s()\n", __func__);
    }
    if (!fspec) {
        wputw(TAG_EOL, fd);
        return;
    }
    wputw(TAG_FUNCTIONSPEC + fspec->kind, fd);
    if (fspec->kind == FUNC_SPEC_CORO)
        export_type(fd, fspec->yield_type);
}

void export_alignment_spec(WFILE *fd, AlignmentSpec *aspec)
{
    if (export_debug) {
        printf("--- %s()\n", __func__);
    }
    if (!aspec) {
        wputw(TAG_EOL, fd);
        return;
    }
    wputw(TAG_ALIGNMENTSPEC + aspec->kind, fd);
    switch (aspec->kind) {
    case ALIGN_SPEC_TYPE:
        export_type(fd, aspec->u.type);
        break;
    case ALIGN_SPEC_EXPR:
        export_expr(fd, aspec->u.expr);
        break;
    }
}

void export_init_declarator(WFILE *fd, InitDeclarator *idecl)
{
    if (export_debug) {
        printf("--- %s()\n", __func__);
    }
    if (!idecl) {
        wputw(TAG_EOL, fd);
        return;
    }
    wputw(TAG_INITDECLARATOR, fd);
    export_loc(fd, idecl->loc);
    export_type(fd, idecl->type);
    wputstr(idecl->name, fd);
    export_initializer(fd, idecl->init);
}

void export_initializer(WFILE *fd, Initializer *init)
{
    if (export_debug) {
        printf("--- %s()\n", __func__);
    }
    if (!init) {
        wputw(TAG_EOL, fd);
        return;
    }
    wputw(TAG_INITIALIZER + init->kind, fd);
    export_loc(fd, init->loc);
    switch (init->kind) {
    case INITIALIZER_SINGLE:
        export_expr(fd, init->u.expr);
        break;
    case INITIALIZER_COMPOUND:
        for (InitItem *item = init->u.items; item; item = item->next) {
            export_init_item(fd, item);
        }
        wputw(TAG_EOL, fd);
        break;
    }
}

void export_init_item(WFILE *fd, InitItem *item)
{
    if (export_debug) {
        printf("--- %s()\n", __func__);
    }
    if (!item) {
        wputw(TAG_EOL, fd);
        return;
    }
    wputw(TAG_INITITEM, fd);
    for (Designator *d = item->designators; d; d = d->next) {
        export_designator(fd, d);
    }
    wputw(TAG_EOL, fd);
    export_initializer(fd, item->init);
}

void export_designator(WFILE *fd, Designator *desg)
{
    if (export_debug) {
        printf("--- %s()\n", __func__);
    }
    if (!desg) {
        wputw(TAG_EOL, fd);
        return;
    }
    wputw(TAG_DESIGNATOR + desg->kind, fd);
    switch (desg->kind) {
    case DESIGNATOR_ARRAY:
        export_expr(fd, desg->u.expr);
        break;
    case DESIGNATOR_FIELD:
        wputstr(desg->u.name, fd);
        break;
    }
}

void export_expr(WFILE *fd, Expr *expr)
{
    if (export_debug) {
        printf("--- %s()\n", __func__);
    }
    if (!expr) {
        wputw(TAG_EOL, fd);
        return;
    }
    wputw(TAG_EXPR + expr->kind, fd);
    export_loc(fd, expr->loc);
    switch (expr->kind) {
    case EXPR_LITERAL:
        export_literal(fd, expr->u.literal);
        break;
    case EXPR_VAR:
        wputstr(expr->u.var, fd);
        break;
    case EXPR_UNARY_OP:
        wputw(expr->u.unary_op.op, fd);
        export_expr(fd, expr->u.unary_op.expr);
        break;
    case EXPR_BINARY_OP:
        wputw(expr->u.binary_op.op, fd);
        export_expr(fd, expr->u.binary_op.left);
        export_expr(fd, expr->u.binary_op.right);
        break;
    case EXPR_ASSIGN:
        wputw(expr->u.assign.op, fd);
        export_expr(fd, expr->u.assign.target);
        export_expr(fd, expr->u.assign.value);
        break;
    case EXPR_COND:
        export_expr(fd, expr->u.cond.condition);
        export_expr(fd, expr->u.cond.then_expr);
        export_expr(fd, expr->u.cond.else_expr);
        break;
    case EXPR_CAST:
        export_type(fd, expr->u.cast.type);
        export_expr(fd, expr->u.cast.expr);
        break;
    case EXPR_CALL:
        export_expr(fd, expr->u.call.func);
        for (Expr *arg = expr->u.call.args; arg; arg = arg->next) {
            export_expr(fd, arg);
        }
        wputw(TAG_EOL, fd);
        break;
    case EXPR_COMPOUND:
        export_type(fd, expr->u.compound_literal.type);
        for (InitItem *item = expr->u.compound_literal.init; item; item = item->next) {
            export_init_item(fd, item);
        }
        wputw(TAG_EOL, fd);
        break;
    case EXPR_SUBSCRIPT:
        export_expr(fd, expr->u.subscript.left);
        export_expr(fd, expr->u.subscript.right);
        break;
    case EXPR_FIELD_ACCESS:
        export_expr(fd, expr->u.field_access.expr);
        wputstr(expr->u.field_access.field, fd);
        break;
    case EXPR_PTR_ACCESS:
        export_expr(fd, expr->u.ptr_access.expr);
        wputstr(expr->u.ptr_access.field, fd);
        break;
    case EXPR_POST_INC:
        export_expr(fd, expr->u.post_inc);
        break;
    case EXPR_POST_DEC:
        export_expr(fd, expr->u.post_dec);
        break;
    case EXPR_SIZEOF_EXPR:
        export_expr(fd, expr->u.sizeof_expr);
        break;
    case EXPR_SIZEOF_TYPE:
        export_type(fd, expr->u.sizeof_type);
        break;
    case EXPR_ALIGNOF:
        export_type(fd, expr->u.align_of);
        break;
    case EXPR_VA_CLASS:
        export_type(fd, expr->u.va_class);
        break;
    case EXPR_GENERIC:
        export_expr(fd, expr->u.generic.controlling_expr);
        for (GenericAssoc *ga = expr->u.generic.associations; ga; ga = ga->next) {
            export_generic_assoc(fd, ga);
        }
        wputw(TAG_EOL, fd);
        break;
    case EXPR_YIELD:
        export_expr(fd, expr->u.yield_expr);
        break;
    case EXPR_AWAIT:
        export_expr(fd, expr->u.await_expr);
        break;
    case EXPR_CO_OP:
        wputw(expr->u.co_op.op, fd);
        for (Expr *arg = expr->u.co_op.args; arg; arg = arg->next) {
            export_expr(fd, arg);
        }
        wputw(TAG_EOL, fd);
        break;
    }
    export_type(fd, expr->type);
}

void export_literal(WFILE *fd, const Literal *lit)
{
    if (export_debug) {
        printf("--- %s()\n", __func__);
    }
    if (!lit) {
        wputw(TAG_EOL, fd);
        return;
    }
    wputw(TAG_LITERAL + lit->kind, fd);
    if (lit->kind <= LITERAL_ULONG_LONG || lit->kind == LITERAL_DOUBLE)
        wputw(lit->spelling, fd);
    if (lit->kind == LITERAL_DOUBLE)
        wputd(lit->single_val, fd);
    switch (lit->kind) {
    case LITERAL_INT:
        wputw((size_t)lit->u.int_val, fd);
        break;
    case LITERAL_LONG:
        wputw((size_t)(unsigned long)lit->u.long_val, fd);
        break;
    case LITERAL_LONG_LONG:
        wputw((size_t)(unsigned long long)lit->u.long_long_val, fd);
        break;
    case LITERAL_UINT:
        wputw((size_t)lit->u.uint_val, fd);
        break;
    case LITERAL_ULONG:
        wputw((size_t)lit->u.ulong_val, fd);
        break;
    case LITERAL_ULONG_LONG:
        wputw((size_t)lit->u.ulong_long_val, fd);
        break;
    case LITERAL_FLOAT:
    case LITERAL_DOUBLE:
        wputd(lit->u.real_val, fd);
        break;
    case LITERAL_LONG_DOUBLE:
        wputf128(lit->u.long_double_val, fd);
        break;
    case LITERAL_CHAR:
        wputw((size_t)lit->u.char_val, fd);
        break;
    case LITERAL_STRING:
        wputstr(lit->u.string_val, fd);
        break;
    case LITERAL_ENUM:
        wputstr(lit->u.enum_const, fd);
        break;
    }
}

void export_generic_assoc(WFILE *fd, GenericAssoc *gasc)
{
    if (export_debug) {
        printf("--- %s()\n", __func__);
    }
    if (!gasc) {
        wputw(TAG_EOL, fd);
        return;
    }
    wputw(TAG_GENERICASSOC + gasc->kind, fd);
    switch (gasc->kind) {
    case GENERIC_ASSOC_TYPE:
        export_type(fd, gasc->u.type_assoc.type);
        export_expr(fd, gasc->u.type_assoc.expr);
        break;
    case GENERIC_ASSOC_DEFAULT:
        export_expr(fd, gasc->u.default_assoc);
        break;
    }
}

void export_stmt(WFILE *fd, Stmt *stmt)
{
    if (export_debug) {
        printf("--- %s()\n", __func__);
    }
    if (!stmt) {
        wputw(TAG_EOL, fd);
        return;
    }
    wputw(TAG_STMT + stmt->kind, fd);
    export_loc(fd, stmt->loc);
    switch (stmt->kind) {
    case STMT_EXPR:
        export_expr(fd, stmt->u.expr);
        break;
    case STMT_COMPOUND:
        for (DeclOrStmt *ds = stmt->u.compound; ds; ds = ds->next) {
            export_decl_or_stmt(fd, ds);
        }
        wputw(TAG_EOL, fd);
        break;
    case STMT_IF:
        export_expr(fd, stmt->u.if_stmt.condition);
        export_stmt(fd, stmt->u.if_stmt.then_stmt);
        export_stmt(fd, stmt->u.if_stmt.else_stmt);
        break;
    case STMT_SWITCH:
        export_expr(fd, stmt->u.switch_stmt.expr);
        export_stmt(fd, stmt->u.switch_stmt.body);
        break;
    case STMT_WHILE:
        export_expr(fd, stmt->u.while_stmt.condition);
        export_stmt(fd, stmt->u.while_stmt.body);
        break;
    case STMT_DO_WHILE:
        export_stmt(fd, stmt->u.do_while.body);
        export_expr(fd, stmt->u.do_while.condition);
        break;
    case STMT_FOR:
        export_for_init(fd, stmt->u.for_stmt.init);
        export_expr(fd, stmt->u.for_stmt.condition);
        export_expr(fd, stmt->u.for_stmt.update);
        export_stmt(fd, stmt->u.for_stmt.body);
        break;
    case STMT_GOTO:
        wputstr(stmt->u.goto_label, fd);
        break;
    case STMT_CONTINUE:
    case STMT_BREAK:
        break;
    case STMT_RETURN:
        export_expr(fd, stmt->u.expr);
        break;
    case STMT_LABELED:
        wputstr(stmt->u.labeled.label, fd);
        export_stmt(fd, stmt->u.labeled.stmt);
        break;
    case STMT_CASE:
        export_expr(fd, stmt->u.case_stmt.expr);
        export_stmt(fd, stmt->u.case_stmt.stmt);
        break;
    case STMT_DEFAULT:
        export_stmt(fd, stmt->u.default_stmt);
        break;
    case STMT_DEFER:
        export_stmt(fd, stmt->u.defer_stmt);
        break;
    }
}

void export_decl_or_stmt(WFILE *fd, DeclOrStmt *dost)
{
    if (export_debug) {
        printf("--- %s()\n", __func__);
    }
    if (!dost) {
        wputw(TAG_EOL, fd);
        return;
    }
    wputw(TAG_DECLORSTMT + dost->kind, fd);
    switch (dost->kind) {
    case DECL_OR_STMT_DECL:
        export_declaration(fd, dost->u.decl);
        break;
    case DECL_OR_STMT_STMT:
        export_stmt(fd, dost->u.stmt);
        break;
    }
}

void export_for_init(WFILE *fd, ForInit *finit)
{
    if (export_debug) {
        printf("--- %s()\n", __func__);
    }
    if (!finit) {
        wputw(TAG_EOL, fd);
        return;
    }
    wputw(TAG_FORINIT + finit->kind, fd);
    switch (finit->kind) {
    case FOR_INIT_EXPR:
        export_expr(fd, finit->u.expr);
        break;
    case FOR_INIT_DECL:
        export_declaration(fd, finit->u.decl);
        break;
    }
}

void export_external_decl(WFILE *fd, ExternalDecl *exdecl)
{
    if (export_debug) {
        printf("--- %s()\n", __func__);
    }
    if (!exdecl) {
        wputw(TAG_EOL, fd);
        return;
    }
    wputw(TAG_EXTERNALDECL + exdecl->kind, fd);
    export_loc(fd, exdecl->loc);
    switch (exdecl->kind) {
    case EXTERNAL_DECL_FUNCTION:
        export_type(fd, exdecl->u.function.type);
        wputstr(exdecl->u.function.name, fd);
        export_decl_spec(fd, exdecl->u.function.specifiers);
        for (Declaration *d = exdecl->u.function.param_decls; d; d = d->next) {
            export_declaration(fd, d);
        }
        wputw(TAG_EOL, fd);
        export_stmt(fd, exdecl->u.function.body);
        break;
    case EXTERNAL_DECL_DECLARATION:
        export_declaration(fd, exdecl->u.declaration);
        break;
    }
}
