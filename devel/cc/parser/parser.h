//
// Internal types for parser, hidden from consumers of AST.
//
#ifndef PARSER_H
#define PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ast.h"

typedef struct Declarator Declarator;

// Enable debug output
extern int parser_debug;

// Go on after a syntax error, to report several per run; set by the parse tool.
extern bool parser_recovery;
extern int import_debug;
extern int export_debug;
extern int wio_debug;

//
// Parse
//
Program *parse(FILE *input);
Declarator *parse_declarator(void);
int parser_get_token(void);
const char *parser_get_lexeme(void);

// Where the parser stands, for a message: "before ';'", "before 'foo'", or
// "at end of file".
const char *parser_where(void);

//
// Error reporting, provided by caller
//
#ifdef __cplusplus
[[noreturn]]
#else
_Noreturn
#endif
void fatal_error(const char *message, ...);

//
// Name table
//
int nametab_find(const char *name);
void nametab_define(const char *name, int token, int level);
void nametab_remove(const char *name);
void nametab_purge(int level);
void nametab_destroy(void);

#ifdef GTEST_API_
void advance_token(void);
Expr *parse_primary_expression(void);
Expr *parse_expression(void);
Stmt *parse_statement(void);
Type *parse_type_name(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* PARSER_H */
