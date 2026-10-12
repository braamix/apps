#include <stdlib.h>

#include "ast.h"
#include "internal.h"
#include "xalloc.h"

/* Helper functions for AST construction */
Type *new_type(TypeKind kind, const char *funcname, const char *filename, unsigned lineno)
{
    Type *t = xalloc(sizeof(Type), funcname, filename, lineno);
    t->kind = kind;
    return t;
}

TypeQualifier *new_type_qualifier(TypeQualifierKind kind)
{
    TypeQualifier *q = xalloc(sizeof(TypeQualifier), __func__, __FILE__, __LINE__);
    q->kind          = kind;
    return q;
}

Field *new_field(FieldKind kind)
{
    Field *f = (Field *)xalloc(sizeof(Field), __func__, __FILE__, __LINE__);
    f->kind  = kind;
    return f;
}

Enumerator *new_enumerator(Ident name, Expr *value)
{
    Enumerator *e = xalloc(sizeof(Enumerator), __func__, __FILE__, __LINE__);
    e->name       = name;
    e->value      = value;
    return e;
}

Param *new_param()
{
    Param *p = xalloc(sizeof(Param), __func__, __FILE__, __LINE__);
    p->loc   = diag_loc;
    return p;
}

Declaration *new_declaration(DeclarationKind kind)
{
    Declaration *d = xalloc(sizeof(Declaration), __func__, __FILE__, __LINE__);
    d->loc         = diag_loc;
    d->kind        = kind;
    return d;
}

DeclSpec *new_decl_spec()
{
    DeclSpec *ds = xalloc(sizeof(DeclSpec), __func__, __FILE__, __LINE__);
    return ds;
}

TypeSpec *new_type_spec(TypeSpecKind kind)
{
    TypeSpec *ts = xalloc(sizeof(TypeSpec), __func__, __FILE__, __LINE__);
    ts->kind     = kind;
    return ts;
}

FunctionSpec *new_function_spec(FunctionSpecKind kind)
{
    FunctionSpec *fs = xalloc(sizeof(FunctionSpec), __func__, __FILE__, __LINE__);
    fs->kind         = kind;
    return fs;
}

AlignmentSpec *new_alignment_spec(AlignmentSpecKind kind)
{
    AlignmentSpec *as = xalloc(sizeof(AlignmentSpec), __func__, __FILE__, __LINE__);
    as->kind          = kind;
    return as;
}

InitDeclarator *new_init_declarator()
{
    InitDeclarator *id = xalloc(sizeof(InitDeclarator), __func__, __FILE__, __LINE__);
    id->loc            = diag_loc;
    return id;
}

Declarator *new_declarator()
{
    Declarator *d = xalloc(sizeof(Declarator), __func__, __FILE__, __LINE__);
    return d;
}

Pointer *new_pointer()
{
    Pointer *p = xalloc(sizeof(Pointer), __func__, __FILE__, __LINE__);
    return p;
}

DeclaratorSuffix *new_declarator_suffix(DeclaratorSuffixKind kind)
{
    DeclaratorSuffix *ds = xalloc(sizeof(DeclaratorSuffix), __func__, __FILE__, __LINE__);
    ds->kind             = kind;
    return ds;
}

Initializer *new_initializer(InitializerKind kind)
{
    Initializer *i = xalloc(sizeof(Initializer), __func__, __FILE__, __LINE__);
    i->loc         = diag_loc;
    i->kind        = kind;
    return i;
}

InitItem *new_init_item(Designator *designators, Initializer *init)
{
    InitItem *ii    = xalloc(sizeof(InitItem), __func__, __FILE__, __LINE__);
    ii->designators = designators;
    ii->init        = init;
    ii->offset      = 0;
    return ii;
}

Designator *new_designator(DesignatorKind kind)
{
    Designator *d = xalloc(sizeof(Designator), __func__, __FILE__, __LINE__);
    d->kind       = kind;
    return d;
}

Expr *new_expression(ExprKind kind)
{
    Expr *e = xalloc(sizeof(Expr), __func__, __FILE__, __LINE__);
    e->loc  = diag_loc;
    e->kind = kind;
    return e;
}

Literal *new_literal(LiteralKind kind)
{
    Literal *l = xalloc(sizeof(Literal), __func__, __FILE__, __LINE__);
    l->kind    = kind;
    return l;
}

GenericAssoc *new_generic_assoc(GenericAssocKind kind)
{
    GenericAssoc *ga = xalloc(sizeof(GenericAssoc), __func__, __FILE__, __LINE__);
    ga->kind         = kind;
    return ga;
}

Stmt *new_stmt(StmtKind kind)
{
    Stmt *s                = xalloc(sizeof(Stmt), __func__, __FILE__, __LINE__);
    s->loc                 = diag_loc;
    s->kind                = kind;
    s->loop_end_label      = NULL;
    s->loop_continue_label = NULL;
    s->branch_target_label = NULL;
    return s;
}

DeclOrStmt *new_decl_or_stmt(DeclOrStmtKind kind)
{
    DeclOrStmt *ds = xalloc(sizeof(DeclOrStmt), __func__, __FILE__, __LINE__);
    ds->kind       = kind;
    return ds;
}

ForInit *new_for_init(ForInitKind kind)
{
    ForInit *fi = xalloc(sizeof(ForInit), __func__, __FILE__, __LINE__);
    fi->kind    = kind;
    return fi;
}

ExternalDecl *new_external_decl(ExternalDeclKind kind)
{
    ExternalDecl *ed = xalloc(sizeof(ExternalDecl), __func__, __FILE__, __LINE__);
    ed->loc          = diag_loc;
    ed->kind         = kind;
    return ed;
}

Program *new_program()
{
    Program *p = xalloc(sizeof(Program), __func__, __FILE__, __LINE__);
    return p;
}
