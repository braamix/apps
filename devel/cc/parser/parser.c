#include "parser.h"

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"
#include "parser_internal.h"
#include "scanner.h"
#include "xalloc.h"

/* Global lexer state; diag_loc holds where the current token starts */
int current_token;
static int peek_token;
static SrcLoc peek_loc;
const char *current_lexeme;
static char lexeme_buffer[1024]; // Buffer for current lexeme

// Name of the function being defined, for __func__; NULL outside a function body.
const char *current_function_name;

// Enable debug output
int parser_debug;

bool parser_recovery;

// Braces consumed so far and not yet closed.
int brace_depth;

// Level of scope for nested compound operators, from semantic.
extern int scope_level;

//
// Helpers for fatal_message.
//
int parser_get_token()
{
    return current_token;
}

const char *parser_get_lexeme()
{
    return current_lexeme;
}

//
// In case of identifier, look in the parser's symbol table.
// When it's a previously defined typedef - return TOKEN_TYPEDEF_NAME.
// When a previously defined enumerator - return TOKEN_ENUMERATION_CONSTANT.
//
static int token_translation(int token)
{
    // Check identifier type
    if (token == TOKEN_IDENTIFIER) {
        token = nametab_find(get_yytext());
        if (!token) {
            token = TOKEN_IDENTIFIER;
        }
    }
    if (parser_debug) {
        printf("--- token %d '%s'\n", token, get_yytext());
    }
    return token;
}

/* Token handling */
void advance_token()
{
    if (current_token == TOKEN_LBRACE) {
        brace_depth++;
    } else if (current_token == TOKEN_RBRACE && brace_depth > 0) {
        brace_depth--;
    }
    if (peek_token > 0) {
        current_token = peek_token;
        peek_token    = 0;
        diag_loc      = peek_loc;
    } else {
        current_token = token_translation(yylex());
        diag_loc      = scanner_token_loc;
    }
    current_lexeme = get_yytext();
}

// Is current token valid but different from the given one?
bool current_token_is_not(int token)
{
    return (current_token != TOKEN_EOF) && (current_token != token);
}

// Does this token have something valuable in yytext?
static bool has_yytext(int token)
{
    return token == TOKEN_IDENTIFIER || token == TOKEN_I_CONSTANT || token == TOKEN_F_CONSTANT ||
           token == TOKEN_ENUMERATION_CONSTANT || token == TOKEN_STRING_LITERAL ||
           token == TOKEN_TYPEDEF_NAME;
}

// Peek next token, without advancing the parser.
int next_token()
{
    if (peek_token == 0) {
        if (has_yytext(current_token) && current_lexeme != lexeme_buffer) {
            // Make a copy of current lexeme.
            strcpy(lexeme_buffer, current_lexeme);
            current_lexeme = lexeme_buffer;
        }
        peek_token = token_translation(yylex());
        peek_loc   = scanner_token_loc;
    }
    return peek_token;
}

//
// After a syntax error, skip to where parsing can resume at the given brace depth:
// past a ';' at that depth, or past the '}' that brings it back there (and a ';'
// right after, as in `struct S { ... };`). A '}' at that depth closes the enclosing
// block, whose own parser consumes it; at the top level there is no enclosing block,
// so a stray '}' is skipped. At the end of the file there is nothing left to find,
// and further errors there would only echo the first one.
//
void parser_sync(int depth, bool top_level)
{
    for (;;) {
        if (current_token == TOKEN_EOF) {
            exit(1);
        }
        if (current_token == TOKEN_SEMICOLON && brace_depth <= depth) {
            advance_token();
            return;
        }
        if (current_token == TOKEN_RBRACE && brace_depth <= depth + 1) {
            if (brace_depth <= depth && !top_level) {
                return;
            }
            advance_token();
            if (current_token == TOKEN_SEMICOLON) {
                advance_token();
            }
            return;
        }
        advance_token();
    }
}

const char *parser_where(void)
{
    static char buf[80];
    if (current_token == TOKEN_EOF)
        return "at end of file";
    if (has_yytext(current_token) && current_lexeme && current_lexeme[0]) {
        size_t len = strlen(current_lexeme);
        if (len > 40)
            snprintf(buf, sizeof(buf), "before '%.37s...'", current_lexeme);
        else
            snprintf(buf, sizeof(buf), "before '%s'", current_lexeme);
    } else {
        snprintf(buf, sizeof(buf), "before %s", token_name(current_token));
    }
    return buf;
}

void expect_token(int expected)
{
    if (current_token != expected) {
        fatal_error("expected %s %s", token_name(expected), parser_where());
    }
    advance_token();
}

// Is this token a type specifier?
bool is_type_specifier(int token)
{
    return token == TOKEN_VOID || token == TOKEN_CHAR || token == TOKEN_SHORT ||
           token == TOKEN_INT || token == TOKEN_LONG || token == TOKEN_FLOAT ||
           token == TOKEN_DOUBLE || token == TOKEN_SIGNED || token == TOKEN_UNSIGNED ||
           token == TOKEN_BOOL || token == TOKEN_COMPLEX || token == TOKEN_IMAGINARY ||
           token == TOKEN_STRUCT || token == TOKEN_UNION || token == TOKEN_ENUM ||
           token == TOKEN_TYPEDEF_NAME || token == TOKEN_CORO_FRAME || token == TOKEN_CORO_PTR;
}

// Is this token a type qualifier?
// Except _Atomic.
bool is_type_qualifier(int token)
{
    return token == TOKEN_CONST || token == TOKEN_RESTRICT || token == TOKEN_VOLATILE;
}

// Is this token a storage class specifier?
bool is_storage_class_specifier(int token)
{
    return token == TOKEN_TYPEDEF || token == TOKEN_EXTERN || token == TOKEN_STATIC ||
           token == TOKEN_THREAD_LOCAL || token == TOKEN_AUTO || token == TOKEN_REGISTER;
}

//
// translation_unit
//     : external_declaration
//     | translation_unit external_declaration
//     ;
//
Program *parse_translation_unit()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Program *program = new_program();
    jmp_buf here;
    if (parser_recovery) {
        if (setjmp(here)) {
            // Drop the declaration and what it opened.
            scope_level = 0;
            nametab_purge(0);
            current_function_name = NULL;
            parser_sync(0, true);
        }
        diag_recover = &here;
    }
    while (current_token != TOKEN_EOF) {
        ExternalDecl *decl = parse_external_declaration();
        append_list(&program->decls, decl);
    }
    diag_recover = NULL;
    return program;
}

//
// external_declaration
//     : function_definition
//     | declaration
//     ;
//
ExternalDecl *parse_external_declaration()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    SrcLoc loc = diag_loc; // of its first token
    if (current_token == TOKEN_STATIC_ASSERT) {
        // Static assert.
        ExternalDecl *ed  = new_external_decl(EXTERNAL_DECL_DECLARATION);
        ed->loc           = loc;
        ed->u.declaration = parse_static_assert_declaration();
        return ed;
    }

    // Parse declaration_specifiers (common to both function_definition and declaration).
    Type *base_type = NULL;
    DeclSpec *spec  = parse_declaration_specifiers(&base_type);

    // Check if it's a declaration (ends with ';').
    if (current_token == TOKEN_SEMICOLON) {
        // Empty declaration.
        advance_token();
        Declaration *decl        = new_declaration(DECL_EMPTY);
        decl->loc                = loc;
        decl->u.empty.specifiers = spec;
        decl->u.empty.type       = base_type;

        ExternalDecl *ed  = new_external_decl(EXTERNAL_DECL_DECLARATION);
        ed->loc           = loc;
        ed->u.declaration = decl;
        return ed;
    }

    // Parse declarator (required for both function_definition and declaration with
    // init_declarator_list).
    Declarator *decl = parse_declarator();

    // Check if it's a declaration with init_declarator_list.
    if (current_token == TOKEN_SEMICOLON || current_token == TOKEN_COMMA ||
        current_token == TOKEN_ASSIGN) {
        // Declaration of variables.
        ExternalDecl *ed       = new_external_decl(EXTERNAL_DECL_DECLARATION);
        ed->loc                = loc;
        ed->u.declaration      = new_declaration(DECL_VAR);
        ed->u.declaration->loc = loc;

        ed->u.declaration->u.var.specifiers  = spec;
        ed->u.declaration->u.var.declarators = parse_init_declarator_list(decl, base_type);
        if (is_typedef(spec)) {
            define_typedef(ed->u.declaration->u.var.declarators);
        }
        expect_token(TOKEN_SEMICOLON);
        free_type(base_type);
        return ed;
    }

    // Must be a function_definition.
    Declaration *decl_list = NULL;
    if (current_token_is_not(TOKEN_LBRACE)) {
        decl_list = parse_declaration_list();
    }

    ExternalDecl *ed          = new_external_decl(EXTERNAL_DECL_FUNCTION);
    ed->loc                   = decl->loc; // of the function's name
    ed->u.function.specifiers = spec;
    ed->u.function.name       = xstrdup(decl->name);
    ed->u.function.type =
        type_apply_suffixes(type_apply_pointers(base_type, decl->pointers), decl->suffixes);
    ed->u.function.param_decls = decl_list;
    current_function_name      = ed->u.function.name;
    ed->u.function.body        = parse_compound_statement();
    current_function_name      = NULL;
    free_declarator(decl);
    return ed;
}

//
// declaration_list
//     : declaration
//     | declaration_list declaration
//     ;
//
Declaration *parse_declaration_list()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Declaration *decl = parse_declaration();
    if (current_token_is_not(TOKEN_LBRACE)) {
        decl->next = parse_declaration_list();
    }
    return decl;
}

/* Main parsing function */
Program *parse(FILE *input)
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    init_scanner(input);
    peek_token  = 0;
    scope_level = 0;
    brace_depth = 0;
    reset_anon_tag_counter(); // synthetic anonymous tags are numbered per translation unit
    current_function_name = NULL;
    advance_token();
    Program *program = parse_translation_unit();
    if (current_token != TOKEN_EOF) {
        internal_error("expected end of file");
    }
    if (diag_errors) {
        exit(1); // all reported; no AST from a broken source
    }
    return program;
}
