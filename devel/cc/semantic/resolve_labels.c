//
// Goto/label resolution
//
// Per-function validation of labeled statements and goto targets:
//   - every label name must be unique within the function;
//   - every goto must target a label defined in the same function.
// Labels and variables occupy separate namespaces, so `goto a;` with only a
// variable `a` in scope is an undefined-label error.
//
#include "semantic.h"
#include "string_map.h"

static void collect_labels(const Stmt *stmt, StringMap *labels);
static void validate_gotos(const Stmt *stmt, const StringMap *labels);

//
// Validate labels and gotos in one function.
//
void resolve_labels(const ExternalDecl *ast)
{
    if (!ast || ast->kind != EXTERNAL_DECL_FUNCTION || !ast->u.function.body) {
        return;
    }
    StringMap labels;
    map_init(&labels);
    collect_labels(ast->u.function.body, &labels);
    validate_gotos(ast->u.function.body, &labels);
    map_destroy(&labels);
}

//
// First pass: gather every label name, rejecting duplicates.
//
static void collect_labels(const Stmt *stmt, StringMap *labels);

static void collect_labels_at(const Stmt *stmt, StringMap *labels)
{
    if (!stmt) {
        return;
    }
    switch (stmt->kind) {
    case STMT_COMPOUND:
        for (const DeclOrStmt *ds = stmt->u.compound; ds; ds = ds->next) {
            if (ds->kind == DECL_OR_STMT_STMT) {
                collect_labels(ds->u.stmt, labels);
            }
        }
        break;
    case STMT_IF:
        collect_labels(stmt->u.if_stmt.then_stmt, labels);
        collect_labels(stmt->u.if_stmt.else_stmt, labels);
        break;
    case STMT_WHILE:
        collect_labels(stmt->u.while_stmt.body, labels);
        break;
    case STMT_DO_WHILE:
        collect_labels(stmt->u.do_while.body, labels);
        break;
    case STMT_FOR:
        collect_labels(stmt->u.for_stmt.body, labels);
        break;
    case STMT_SWITCH:
        collect_labels(stmt->u.switch_stmt.body, labels);
        break;
    case STMT_LABELED:
        if (map_get(labels, stmt->u.labeled.label, NULL)) {
            diag_enter(stmt->loc);
            fatal_error("redefinition of label '%s'", stmt->u.labeled.label);
        }
        map_insert(labels, stmt->u.labeled.label, 1, 0);
        collect_labels(stmt->u.labeled.stmt, labels);
        break;
    case STMT_CASE:
        collect_labels(stmt->u.case_stmt.stmt, labels);
        break;
    case STMT_DEFAULT:
        collect_labels(stmt->u.default_stmt, labels);
        break;
    case STMT_DEFER:
        collect_labels(stmt->u.defer_stmt, labels);
        break;
    default: // leaf statements: nothing to collect
        break;
    }
}

// collect_labels with diag_loc at the node, for the errors found in it.
static void collect_labels(const Stmt *stmt, StringMap *labels)
{
    SrcLoc saved = diag_enter(stmt ? stmt->loc : diag_loc);
    collect_labels_at(stmt, labels);
    diag_loc = saved;
}

//
// Second pass: check that every goto targets a defined label.
//
static void validate_gotos(const Stmt *stmt, const StringMap *labels)
{
    if (!stmt) {
        return;
    }
    switch (stmt->kind) {
    case STMT_COMPOUND:
        for (const DeclOrStmt *ds = stmt->u.compound; ds; ds = ds->next) {
            if (ds->kind == DECL_OR_STMT_STMT) {
                validate_gotos(ds->u.stmt, labels);
            }
        }
        break;
    case STMT_IF:
        validate_gotos(stmt->u.if_stmt.then_stmt, labels);
        validate_gotos(stmt->u.if_stmt.else_stmt, labels);
        break;
    case STMT_WHILE:
        validate_gotos(stmt->u.while_stmt.body, labels);
        break;
    case STMT_DO_WHILE:
        validate_gotos(stmt->u.do_while.body, labels);
        break;
    case STMT_FOR:
        validate_gotos(stmt->u.for_stmt.body, labels);
        break;
    case STMT_SWITCH:
        validate_gotos(stmt->u.switch_stmt.body, labels);
        break;
    case STMT_LABELED:
        validate_gotos(stmt->u.labeled.stmt, labels);
        break;
    case STMT_CASE:
        validate_gotos(stmt->u.case_stmt.stmt, labels);
        break;
    case STMT_DEFAULT:
        validate_gotos(stmt->u.default_stmt, labels);
        break;
    case STMT_DEFER:
        validate_gotos(stmt->u.defer_stmt, labels);
        break;
    case STMT_GOTO:
        if (!map_get(labels, stmt->u.goto_label, NULL)) {
            diag_enter(stmt->loc); // the goto, not the function
            fatal_error("use of undeclared label '%s'", stmt->u.goto_label);
        }
        break;
    default: // leaf statements: nothing to validate
        break;
    }
}
