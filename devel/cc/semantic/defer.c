//
// Checks for `defer` (docs/Coroutines_in_C.md, section 1).
//
// A deferred statement runs when control leaves the block holding the `defer`.  The
// compiler lowers it inline on every exit edge (translator/stmt.c), which needs the
// set of defers registered at any point to be known statically.  So:
//   - a deferred statement may only be left by completing it: no return, no goto out
//     of it, and nothing jumps into it (a break or continue that would leave it is
//     label_loops' error: the deferred statement is a barrier to it);
//   - no goto, case or default label may jump into a block past a defer in it.
// A goto may still leave blocks, or jump back over a defer of its own block: the
// translator runs the defers it leaves behind.
//
// A co_alloca is a defer for this purpose: its frame is released at the end of its
// block, ordered with the block's defers.  It is registered after the declaration or
// expression statement holding it, or, in the head of an if or switch, before the
// statement's body (none may be in a loop's head: expressions.c).
//
#include "defer.h"

#include "semantic.h"
#include "xalloc.h"

typedef struct {
    DeferScope *stack; // blocks from the root down to the statement being walked
    int depth, cap;
    const Stmt *defer; // innermost deferred statement being walked, or NULL
    int switch_base;   // depth at the innermost switch inside it, or -1
    StringMap *labels; // label name -> DeferPos *
    bool check_gotos;  // second walk: labels known, check every goto
} Walk;

static void walk_stmt(Walk *w, const Stmt *s);

static int count_co_alloca(const Expr *e);

static int count_co_alloca_list(const Expr *e)
{
    int n = 0;
    for (; e; e = e->next)
        n += count_co_alloca(e);
    return n;
}

static int count_co_alloca_init(const Initializer *init)
{
    if (!init)
        return 0;
    if (init->kind == INITIALIZER_SINGLE)
        return count_co_alloca(init->u.expr);
    int n = 0;
    for (const InitItem *item = init->u.items; item; item = item->next)
        n += count_co_alloca_init(item->init);
    return n;
}

// The co_alloca operations an expression evaluates (not those of a sizeof operand).
static int count_co_alloca(const Expr *e)
{
    if (!e)
        return 0;
    switch (e->kind) {
    case EXPR_UNARY_OP:
        return count_co_alloca(e->u.unary_op.expr);
    case EXPR_BINARY_OP:
        return count_co_alloca(e->u.binary_op.left) + count_co_alloca(e->u.binary_op.right);
    case EXPR_SUBSCRIPT:
        return count_co_alloca(e->u.subscript.left) + count_co_alloca(e->u.subscript.right);
    case EXPR_ASSIGN:
        return count_co_alloca(e->u.assign.target) + count_co_alloca(e->u.assign.value);
    case EXPR_COND:
        return count_co_alloca(e->u.cond.condition) + count_co_alloca(e->u.cond.then_expr) +
               count_co_alloca(e->u.cond.else_expr);
    case EXPR_CAST:
        return count_co_alloca(e->u.cast.expr);
    case EXPR_CALL:
        return count_co_alloca(e->u.call.func) + count_co_alloca_list(e->u.call.args);
    case EXPR_COMPOUND: {
        int n = 0;
        for (const InitItem *item = e->u.compound_literal.init; item; item = item->next)
            n += count_co_alloca_init(item->init);
        return n;
    }
    case EXPR_FIELD_ACCESS:
    case EXPR_PTR_ACCESS:
        return count_co_alloca(e->u.field_access.expr);
    case EXPR_POST_INC:
    case EXPR_POST_DEC:
        return count_co_alloca(e->u.post_inc);
    case EXPR_GENERIC: {
        int n = count_co_alloca(e->u.generic.controlling_expr);
        for (const GenericAssoc *ga = e->u.generic.associations; ga; ga = ga->next)
            n += count_co_alloca(ga->kind == GENERIC_ASSOC_TYPE ? ga->u.type_assoc.expr
                                                                : ga->u.default_assoc);
        return n;
    }
    case EXPR_YIELD:
        return count_co_alloca(e->u.yield_expr);
    case EXPR_AWAIT:
        return count_co_alloca(e->u.await_expr);
    case EXPR_CO_OP:
        return (e->u.co_op.op == CO_OP_ALLOCA) + count_co_alloca_list(e->u.co_op.args);
    default:
        return 0; // a literal, a name, sizeof, _Alignof, __builtin_va_class
    }
}

static int count_co_alloca_decl(const Declaration *d)
{
    int n = 0;
    if (d && d->kind == DECL_VAR) {
        for (const InitDeclarator *id = d->u.var.declarators; id; id = id->next)
            n += count_co_alloca_init(id->init);
    }
    return n;
}

static void push(Walk *w, const Stmt *key)
{
    if (w->depth == w->cap) {
        w->cap          = w->cap ? 2 * w->cap : 16;
        DeferScope *new = xalloc(w->cap * sizeof *new, __func__, __FILE__, __LINE__);
        for (int i = 0; i < w->depth; i++)
            new[i] = w->stack[i];
        xfree(w->stack);
        w->stack = new;
    }
    w->stack[w->depth].key   = key;
    w->stack[w->depth].count = 0;
    w->depth++;
}

// A substatement: a block of its own, unless it is a compound statement, which is one.
static void walk_sub(Walk *w, const Stmt *s)
{
    if (!s)
        return;
    if (s->kind == STMT_COMPOUND) {
        walk_stmt(w, s);
        return;
    }
    push(w, s);
    walk_stmt(w, s);
    w->depth--;
}

static void record_label(Walk *w, const char *name)
{
    DeferPos *pos = xalloc(sizeof *pos, __func__, __FILE__, __LINE__);
    pos->depth    = w->depth;
    pos->scopes   = xalloc(w->depth * sizeof *pos->scopes + 1, __func__, __FILE__, __LINE__);
    pos->defer    = w->defer;
    for (int i = 0; i < w->depth; i++)
        pos->scopes[i] = w->stack[i];
    map_insert(w->labels, name, (intptr_t)pos, 0);
}

static void check_goto(const Walk *w, const char *name)
{
    intptr_t v;
    if (!map_get(w->labels, name, &v))
        return; // undefined: resolve_labels reports it
    const DeferPos *to = (const DeferPos *)v;
    if (to->defer != w->defer) {
        fatal_error("'goto %s' jumps into or out of a deferred statement", name);
    }
    int k = 0;
    while (k < w->depth && k < to->depth && w->stack[k].key == to->scopes[k].key)
        k++;
    for (int i = k; i < to->depth; i++) {
        if (to->scopes[i].count > 0)
            fatal_error("'goto %s' jumps into a block past a defer or co_alloca", name);
    }
    if (k > 0 && to->scopes[k - 1].count > w->stack[k - 1].count) {
        fatal_error("'goto %s' jumps forward past a defer or co_alloca", name);
    }
}

// Register n co_allocas, like as many defers, in the innermost block.
static void register_co_alloca(Walk *w, int n)
{
    if (n > 0 && w->depth > 0)
        w->stack[w->depth - 1].count += n;
}

static void check_case(const Walk *w, const char *what)
{
    if (w->switch_base < 0) {
        fatal_error(
            "'%s' label inside a deferred statement cannot belong to a switch "
            "outside it",
            what);
    }
    for (int i = w->switch_base; i < w->depth; i++) {
        if (w->stack[i].count > 0)
            fatal_error("'%s' label past a defer or co_alloca in its switch", what);
    }
}

static void walk_stmt(Walk *w, const Stmt *s);

static void walk_stmt_at(Walk *w, const Stmt *s)
{
    if (!s)
        return;
    switch (s->kind) {
    case STMT_COMPOUND:
        push(w, s);
        for (const DeclOrStmt *ds = s->u.compound; ds; ds = ds->next) {
            if (ds->kind == DECL_OR_STMT_STMT)
                walk_stmt(w, ds->u.stmt);
            else
                register_co_alloca(w, count_co_alloca_decl(ds->u.decl));
        }
        w->depth--;
        break;
    case STMT_EXPR:
        register_co_alloca(w, count_co_alloca(s->u.expr));
        break;
    case STMT_IF:
        register_co_alloca(w, count_co_alloca(s->u.if_stmt.condition));
        walk_sub(w, s->u.if_stmt.then_stmt);
        walk_sub(w, s->u.if_stmt.else_stmt);
        break;
    case STMT_WHILE:
        walk_sub(w, s->u.while_stmt.body);
        break;
    case STMT_DO_WHILE:
        walk_sub(w, s->u.do_while.body);
        break;
    case STMT_FOR:
        walk_sub(w, s->u.for_stmt.body);
        break;
    case STMT_SWITCH: {
        register_co_alloca(w, count_co_alloca(s->u.switch_stmt.expr));
        int base       = w->switch_base;
        w->switch_base = w->depth;
        walk_sub(w, s->u.switch_stmt.body);
        w->switch_base = base;
        break;
    }
    case STMT_LABELED:
        if (!w->check_gotos)
            record_label(w, s->u.labeled.label);
        walk_stmt(w, s->u.labeled.stmt);
        break;
    case STMT_CASE:
        if (!w->check_gotos)
            check_case(w, "case");
        walk_stmt(w, s->u.case_stmt.stmt);
        break;
    case STMT_DEFAULT:
        if (!w->check_gotos)
            check_case(w, "default");
        walk_stmt(w, s->u.default_stmt);
        break;
    case STMT_GOTO:
        if (w->check_gotos)
            check_goto(w, s->u.goto_label);
        break;
    case STMT_RETURN:
        if (w->defer)
            fatal_error("'return' inside a deferred statement");
        break;
    case STMT_DEFER: {
        // The deferred statement is walked as a root of its own: nothing leaves it.
        Walk inner = { NULL, 0, 0, s, -1, w->labels, w->check_gotos };
        walk_sub(&inner, s->u.defer_stmt);
        xfree(inner.stack);
        // Registered from here on, in the block holding it.
        if (w->depth > 0)
            w->stack[w->depth - 1].count++;
        break;
    }
    default:
        break;
    }
}

// walk_stmt with diag_loc at the node, for the errors found in it.
static void walk_stmt(Walk *w, const Stmt *s)
{
    SrcLoc saved = diag_enter(s ? s->loc : diag_loc);
    walk_stmt_at(w, s);
    diag_loc = saved;
}

void defer_collect_labels(const Stmt *body, StringMap *labels)
{
    Walk w = { NULL, 0, 0, NULL, -1, labels, false };
    walk_stmt(&w, body);
    xfree(w.stack);
}

static void free_pos(intptr_t v)
{
    DeferPos *pos = (DeferPos *)v;
    xfree(pos->scopes);
    xfree(pos);
}

void defer_free_labels(StringMap *labels)
{
    map_destroy_free(labels, free_pos);
}

// Does the statement hold a defer or a co_alloca anywhere?  Most functions have none,
// and need no walk.
static bool has_defer(const Stmt *s)
{
    if (!s)
        return false;
    switch (s->kind) {
    case STMT_DEFER:
        return true;
    case STMT_COMPOUND:
        for (const DeclOrStmt *ds = s->u.compound; ds; ds = ds->next) {
            if (ds->kind == DECL_OR_STMT_STMT ? has_defer(ds->u.stmt)
                                              : count_co_alloca_decl(ds->u.decl) > 0)
                return true;
        }
        return false;
    case STMT_EXPR:
        return count_co_alloca(s->u.expr) > 0;
    case STMT_IF:
        return count_co_alloca(s->u.if_stmt.condition) > 0 || has_defer(s->u.if_stmt.then_stmt) ||
               has_defer(s->u.if_stmt.else_stmt);
    case STMT_WHILE:
        return has_defer(s->u.while_stmt.body);
    case STMT_DO_WHILE:
        return has_defer(s->u.do_while.body);
    case STMT_FOR:
        return has_defer(s->u.for_stmt.body);
    case STMT_SWITCH:
        return count_co_alloca(s->u.switch_stmt.expr) > 0 || has_defer(s->u.switch_stmt.body);
    case STMT_LABELED:
        return has_defer(s->u.labeled.stmt);
    case STMT_CASE:
        return has_defer(s->u.case_stmt.stmt);
    case STMT_DEFAULT:
        return has_defer(s->u.default_stmt);
    default:
        return false;
    }
}

void check_defers(const ExternalDecl *ast)
{
    if (!ast || ast->kind != EXTERNAL_DECL_FUNCTION || !has_defer(ast->u.function.body))
        return;
    StringMap labels;
    map_init(&labels);
    defer_collect_labels(ast->u.function.body, &labels);
    Walk w = { NULL, 0, 0, NULL, -1, &labels, true };
    walk_stmt(&w, ast->u.function.body);
    xfree(w.stack);
    defer_free_labels(&labels);
}
