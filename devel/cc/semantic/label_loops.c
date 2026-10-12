//
// Loop labeling
//
#include "semantic.h"
#include "xalloc.h"

typedef struct {
    const char *break_lbl;
    const char *cont_lbl;
} LabelFrame;

// Unit-wide loop/switch label counter, owned by the caller and shared with the
// translator's temp counter (see translate.h): loop labels (%L0, %L1, …) and TAC
// temporaries (%2, %3, …) draw from one monotonic sequence so every internal name
// is unique within the translation unit — required by the single-file backends.
static int *label_seq;

static void label_statement(Stmt *stmt, LabelFrame *stack, int *depth);

// Replace a label the statement may already carry: the translator labels a deferred
// statement afresh each time it lowers a copy of it.
static void set_label(char **slot, char *label)
{
    xfree(*slot);
    *slot = label;
}

//
// Annotate loops and break/continue statements.
//
void label_loops(const ExternalDecl *ast, int *seq)
{
    label_seq = seq;
    if (!ast || ast->kind != EXTERNAL_DECL_FUNCTION || !ast->u.function.body) {
        return;
    }
    LabelFrame stack[256];
    int depth = 0;
    label_statement(ast->u.function.body, stack, &depth);
}

//
// Label the loops of one statement anew, as if it were a function body: a deferred
// statement, which no break or continue may leave (check_defers).
//
void label_loops_stmt(Stmt *stmt, int *seq)
{
    label_seq = seq;
    LabelFrame stack[256];
    int depth = 0;
    label_statement(stmt, stack, &depth);
}

static void label_statement(Stmt *stmt, LabelFrame *stack, int *depth);

static void label_statement_at(Stmt *stmt, LabelFrame *stack, int *depth)
{
    if (!stmt) {
        return;
    }
    switch (stmt->kind) {
    case STMT_EXPR:
    case STMT_GOTO:
    case STMT_RETURN:
        break;
    case STMT_CONTINUE: {
        int i = *depth - 1;
        while (i >= 0 && stack[i].cont_lbl == NULL) {
            i--;
        }
        if (i < 0) {
            fatal_error("'continue' statement not in a loop");
        }
        set_label(&stmt->branch_target_label, xstrdup(stack[i].cont_lbl));
        break;
    }
    case STMT_BREAK: {
        if (*depth <= 0) {
            fatal_error("'break' statement not in a loop or switch");
        }
        set_label(&stmt->branch_target_label, xstrdup(stack[*depth - 1].break_lbl));
        break;
    }
    case STMT_COMPOUND: {
        for (DeclOrStmt *ds = stmt->u.compound; ds; ds = ds->next) {
            if (ds->kind == DECL_OR_STMT_STMT) {
                label_statement(ds->u.stmt, stack, depth);
            }
        }
        break;
    }
    case STMT_IF:
        label_statement(stmt->u.if_stmt.then_stmt, stack, depth);
        label_statement(stmt->u.if_stmt.else_stmt, stack, depth);
        break;
    case STMT_SWITCH: {
        char *end = xstruniq("%L", label_seq);
        set_label(&stmt->loop_end_label, end);
        set_label(&stmt->loop_continue_label, NULL);
        stack[*depth].break_lbl = end;
        stack[*depth].cont_lbl  = NULL;
        (*depth)++;
        label_statement(stmt->u.switch_stmt.body, stack, depth);
        (*depth)--;
        break;
    }
    case STMT_WHILE: {
        char *end  = xstruniq("%L", label_seq);
        char *cont = xstruniq("%L", label_seq);
        set_label(&stmt->loop_end_label, end);
        set_label(&stmt->loop_continue_label, cont);
        stack[*depth].break_lbl = end;
        stack[*depth].cont_lbl  = cont;
        (*depth)++;
        label_statement(stmt->u.while_stmt.body, stack, depth);
        (*depth)--;
        break;
    }
    case STMT_DO_WHILE: {
        char *end  = xstruniq("%L", label_seq);
        char *cont = xstruniq("%L", label_seq);
        set_label(&stmt->loop_end_label, end);
        set_label(&stmt->loop_continue_label, cont);
        stack[*depth].break_lbl = end;
        stack[*depth].cont_lbl  = cont;
        (*depth)++;
        label_statement(stmt->u.do_while.body, stack, depth);
        (*depth)--;
        break;
    }
    case STMT_FOR: {
        char *end  = xstruniq("%L", label_seq);
        char *cont = xstruniq("%L", label_seq);
        set_label(&stmt->loop_end_label, end);
        set_label(&stmt->loop_continue_label, cont);
        stack[*depth].break_lbl = end;
        stack[*depth].cont_lbl  = cont;
        (*depth)++;
        label_statement(stmt->u.for_stmt.body, stack, depth);
        (*depth)--;
        break;
    }
    case STMT_LABELED:
        label_statement(stmt->u.labeled.stmt, stack, depth);
        break;
    case STMT_CASE:
        label_statement(stmt->u.case_stmt.stmt, stack, depth);
        break;
    case STMT_DEFAULT:
        label_statement(stmt->u.default_stmt, stack, depth);
        break;
    case STMT_DEFER: {
        // A barrier: a break or continue in a deferred statement binds only to a loop
        // or switch inside it, and is an error otherwise.
        int inner = 0;
        label_statement(stmt->u.defer_stmt, stack + *depth, &inner);
        break;
    }
    default:
        break;
    }
}

// label_statement with diag_loc at the node, for the errors found in it.
static void label_statement(Stmt *stmt, LabelFrame *stack, int *depth)
{
    SrcLoc saved = diag_enter(stmt ? stmt->loc : diag_loc);
    label_statement_at(stmt, stack, depth);
    diag_loc = saved;
}
