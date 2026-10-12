#include <setjmp.h>
#include <stdio.h>

#include "parser_internal.h"
#include "xalloc.h"

//
// statement
//     : labeled_statement
//     | compound_statement
//     | expression_statement
//     | selection_statement
//     | iteration_statement
//     | jump_statement
//     | defer_statement           (vcc extension)
//     ;
//
Stmt *parse_statement()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    SrcLoc loc = diag_loc; // of its first token
    Stmt *stmt;
    if (current_token == TOKEN_IDENTIFIER && next_token() == TOKEN_COLON) {
        stmt = parse_labeled_statement();
    } else if (current_token == TOKEN_CASE || current_token == TOKEN_DEFAULT) {
        stmt = parse_labeled_statement();
    } else if (current_token == TOKEN_LBRACE) {
        stmt = parse_compound_statement();
    } else if (current_token == TOKEN_IF || current_token == TOKEN_SWITCH) {
        stmt = parse_selection_statement();
    } else if (current_token == TOKEN_WHILE || current_token == TOKEN_DO ||
               current_token == TOKEN_FOR) {
        stmt = parse_iteration_statement();
    } else if (current_token == TOKEN_GOTO || current_token == TOKEN_CONTINUE ||
               current_token == TOKEN_BREAK || current_token == TOKEN_RETURN) {
        stmt = parse_jump_statement();
    } else if (current_token == TOKEN_DEFER) {
        stmt = parse_defer_statement();
    } else {
        stmt = parse_expression_statement();
    }
    if (stmt) {
        stmt->loc = loc;
    }
    return stmt;
}

//
// labeled_statement
//     : IDENTIFIER ':' statement
//     | CASE constant_expression ':' statement
//     | DEFAULT ':' statement
//     ;
//
Stmt *parse_labeled_statement()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_IDENTIFIER) {
        Ident label = xstrdup(current_lexeme);
        advance_token();
        expect_token(TOKEN_COLON);
        Stmt *stmt               = parse_statement();
        Stmt *labeled            = new_stmt(STMT_LABELED);
        labeled->u.labeled.label = label;
        labeled->u.labeled.stmt  = stmt;
        return labeled;
    } else if (current_token == TOKEN_CASE) {
        advance_token();
        Expr *expr = parse_constant_expression();
        expect_token(TOKEN_COLON);
        Stmt *stmt                  = parse_statement();
        Stmt *case_stmt             = new_stmt(STMT_CASE);
        case_stmt->u.case_stmt.expr = expr;
        case_stmt->u.case_stmt.stmt = stmt;
        return case_stmt;
    } else if (current_token != TOKEN_DEFAULT) {
        internal_error("expected labeled statement");
    }
    advance_token();
    expect_token(TOKEN_COLON);
    Stmt *stmt                   = parse_statement();
    Stmt *default_stmt           = new_stmt(STMT_DEFAULT);
    default_stmt->u.default_stmt = stmt;
    return default_stmt;
}

//
// defer_statement
//     : DEFER statement
//     ;
//
// The statement runs when control leaves the block that contains the defer
// (docs/Coroutines_in_C.md, section 1).
//
Stmt *parse_defer_statement()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    expect_token(TOKEN_DEFER);
    Stmt *stmt         = new_stmt(STMT_DEFER);
    stmt->u.defer_stmt = parse_statement();
    return stmt;
}

//
// compound_statement
//     : '{' '}'
//     | '{'  block_item_list '}'
//     ;
//
Stmt *parse_compound_statement()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    SrcLoc loc = diag_loc; // of the '{'
    expect_token(TOKEN_LBRACE);
    scope_level++;

    // On a syntax error in an item, drop it and resume with the next one.
    int level                 = scope_level;
    int depth                 = brace_depth;
    jmp_buf *outer            = diag_recover;
    DeclOrStmt *volatile head = NULL;
    DeclOrStmt *volatile last = NULL;
    jmp_buf here;
    if (outer) {
        if (setjmp(here)) {
            scope_level = level;
            nametab_purge(level);
            parser_sync(depth, false);
        }
        diag_recover = &here;
    }
    while (current_token_is_not(TOKEN_RBRACE)) {
        DeclOrStmt *item = parse_block_item();
        if (last)
            last->next = item;
        else
            head = item;
        last = item;
    }
    diag_recover      = outer;
    DeclOrStmt *items = head;
    expect_token(TOKEN_RBRACE);
    scope_level--;
    nametab_purge(scope_level);

    Stmt *stmt       = new_stmt(STMT_COMPOUND);
    stmt->loc        = loc;
    stmt->u.compound = items;
    return stmt;
}

//
// block_item
//     : declaration
//     | statement
//     ;
//
DeclOrStmt *parse_block_item()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    if (is_storage_class_specifier(current_token) || is_type_specifier(current_token) ||
        is_type_qualifier(current_token) || current_token == TOKEN_ATOMIC ||
        current_token == TOKEN_INLINE || current_token == TOKEN_NORETURN ||
        current_token == TOKEN_CORO || current_token == TOKEN_ALIGNAS ||
        current_token == TOKEN_STATIC_ASSERT) {
        Declaration *decl = parse_declaration();
        DeclOrStmt *ds    = new_decl_or_stmt(DECL_OR_STMT_DECL);
        ds->u.decl        = decl;
        return ds;
    }
    Stmt *stmt     = parse_statement();
    DeclOrStmt *ds = new_decl_or_stmt(DECL_OR_STMT_STMT);
    ds->u.stmt     = stmt;
    return ds;
}

//
// expression_statement
//     : ';'
//     | expression ';'
//     ;
//
Stmt *parse_expression_statement()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *expr = NULL;
    if (current_token_is_not(TOKEN_SEMICOLON)) {
        expr = parse_expression();
    }
    expect_token(TOKEN_SEMICOLON);
    Stmt *stmt   = new_stmt(STMT_EXPR);
    stmt->u.expr = expr;
    return stmt;
}

//
// selection_statement
//     : IF '(' expression ')' statement ELSE statement
//     | IF '(' expression ')' statement
//     | SWITCH '(' expression ')' statement
//     ;
//
Stmt *parse_selection_statement()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_IF) {
        advance_token();
        expect_token(TOKEN_LPAREN);
        Expr *condition = parse_expression();
        expect_token(TOKEN_RPAREN);
        Stmt *then_stmt = parse_statement();
        Stmt *else_stmt = NULL;
        if (current_token == TOKEN_ELSE) {
            advance_token();
            else_stmt = parse_statement();
        }
        Stmt *stmt                = new_stmt(STMT_IF);
        stmt->u.if_stmt.condition = condition;
        stmt->u.if_stmt.then_stmt = then_stmt;
        stmt->u.if_stmt.else_stmt = else_stmt;
        return stmt;
    } else if (current_token != TOKEN_SWITCH) {
        internal_error("expected if or switch");
    }
    advance_token();
    expect_token(TOKEN_LPAREN);
    Expr *expr = parse_expression();
    expect_token(TOKEN_RPAREN);
    Stmt *body               = parse_statement();
    Stmt *stmt               = new_stmt(STMT_SWITCH);
    stmt->u.switch_stmt.expr = expr;
    stmt->u.switch_stmt.body = body;
    return stmt;
}

//
// iteration_statement
//     : WHILE '(' expression ')' statement
//     | DO statement WHILE '(' expression ')' ';'
//     | FOR '(' expression_statement expression_statement ')' statement
//     | FOR '(' expression_statement expression_statement expression ')' statement
//     | FOR '(' declaration expression_statement ')' statement
//     | FOR '(' declaration expression_statement expression ')' statement
//     ;
//
Stmt *parse_iteration_statement()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_WHILE) {
        advance_token();
        expect_token(TOKEN_LPAREN);
        Expr *condition = parse_expression();
        expect_token(TOKEN_RPAREN);
        Stmt *body                   = parse_statement();
        Stmt *stmt                   = new_stmt(STMT_WHILE);
        stmt->u.while_stmt.condition = condition;
        stmt->u.while_stmt.body      = body;
        return stmt;
    } else if (current_token == TOKEN_DO) {
        advance_token();
        Stmt *body = parse_statement();
        expect_token(TOKEN_WHILE);
        expect_token(TOKEN_LPAREN);
        Expr *condition = parse_expression();
        expect_token(TOKEN_RPAREN);
        expect_token(TOKEN_SEMICOLON);
        Stmt *stmt                 = new_stmt(STMT_DO_WHILE);
        stmt->u.do_while.body      = body;
        stmt->u.do_while.condition = condition;
        return stmt;
    } else if (current_token != TOKEN_FOR) {
        internal_error("expected while, do, or for");
    }
    advance_token();
    expect_token(TOKEN_LPAREN);
    ForInit *init   = NULL;
    Expr *condition = NULL;
    Expr *update    = NULL;
    if (is_storage_class_specifier(current_token) || is_type_specifier(current_token) ||
        is_type_qualifier(current_token) || current_token == TOKEN_ATOMIC ||
        current_token == TOKEN_INLINE || current_token == TOKEN_NORETURN ||
        current_token == TOKEN_CORO || current_token == TOKEN_ALIGNAS ||
        current_token == TOKEN_STATIC_ASSERT) {
        Declaration *decl = parse_declaration();
        init              = new_for_init(FOR_INIT_DECL);
        init->u.decl      = decl;
    } else {
        Stmt *expr_stmt   = parse_expression_statement();
        init              = new_for_init(FOR_INIT_EXPR);
        init->u.expr      = expr_stmt->u.expr;
        expr_stmt->u.expr = NULL;
        free_statement(expr_stmt);
    }
    Stmt *cond_stmt = parse_expression_statement();
    condition       = cond_stmt->u.expr;
    if (current_token_is_not(TOKEN_RPAREN)) {
        update = parse_expression();
    }
    cond_stmt->u.expr = NULL;
    free_statement(cond_stmt);
    expect_token(TOKEN_RPAREN);
    Stmt *body                 = parse_statement();
    Stmt *stmt                 = new_stmt(STMT_FOR);
    stmt->u.for_stmt.init      = init;
    stmt->u.for_stmt.condition = condition;
    stmt->u.for_stmt.update    = update;
    stmt->u.for_stmt.body      = body;
    return stmt;
}

//
// jump_statement
//     : GOTO IDENTIFIER ';'
//     | CONTINUE ';'
//     | BREAK ';'
//     | RETURN ';'
//     | RETURN expression ';'
//     ;
//
Stmt *parse_jump_statement()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_GOTO) {
        advance_token();
        Ident label = xstrdup(current_lexeme);
        expect_token(TOKEN_IDENTIFIER);
        expect_token(TOKEN_SEMICOLON);
        Stmt *stmt         = new_stmt(STMT_GOTO);
        stmt->u.goto_label = label;
        return stmt;
    } else if (current_token == TOKEN_CONTINUE) {
        advance_token();
        expect_token(TOKEN_SEMICOLON);
        return new_stmt(STMT_CONTINUE);
    } else if (current_token == TOKEN_BREAK) {
        advance_token();
        expect_token(TOKEN_SEMICOLON);
        return new_stmt(STMT_BREAK);
    } else if (current_token != TOKEN_RETURN) {
        internal_error("expected jump statement");
    }
    advance_token();
    Expr *expr = NULL;
    if (current_token_is_not(TOKEN_SEMICOLON)) {
        expr = parse_expression();
    }
    expect_token(TOKEN_SEMICOLON);
    Stmt *stmt   = new_stmt(STMT_RETURN);
    stmt->u.expr = expr;
    return stmt;
}
