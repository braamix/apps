//
// Internal declarations shared across parser source files.
// Not part of the public API.
//
#ifndef PARSER_INTERNAL_H
#define PARSER_INTERNAL_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "internal.h"
#include "parser.h"
#include "scanner.h"

/* Shared global state (defined in parser.c) */
extern int current_token;
extern const char *current_lexeme;
extern int scope_level;
extern const char *current_function_name;

void advance_token(void);
extern int brace_depth;
void parser_sync(int depth, bool top_level);
bool current_token_is_not(int token);
int next_token(void);
void expect_token(int expected);
bool is_type_specifier(int token);
bool is_type_qualifier(int token);
bool is_storage_class_specifier(int token);

/* Generic linked-list append (defined in expr.c) */
void append_list(void *head, void *node);

/* Expression parsing (defined in expr.c) */
Expr *parse_primary_expression(void);
Expr *parse_constant(void);
Expr *parse_string(void);
Expr *parse_generic_selection(void);
GenericAssoc *parse_generic_assoc_list(void);
GenericAssoc *parse_generic_association(void);
Expr *parse_postfix_tail(Expr *base);
Expr *parse_postfix_expression(void);
Expr *parse_argument_expression_list(void);
Expr *parse_unary_expression(void);
UnaryOp parse_unary_operator(void);
Expr *parse_cast_expression(void);
Expr *parse_multiplicative_expression(void);
Expr *parse_additive_expression(void);
Expr *parse_shift_expression(void);
Expr *parse_relational_expression(void);
Expr *parse_equality_expression(void);
Expr *parse_and_expression(void);
Expr *parse_exclusive_or_expression(void);
Expr *parse_inclusive_or_expression(void);
Expr *parse_logical_and_expression(void);
Expr *parse_logical_or_expression(void);
Expr *parse_conditional_expression(void);
Expr *parse_assignment_expression(void);
AssignOp parse_assignment_operator(void);
Expr *parse_expression(void);
Expr *parse_constant_expression(void);

/* Declaration helpers (defined in decl.c) */
bool is_typedef(const DeclSpec *specifiers);
void define_typedef(InitDeclarator *decl);

/* Declaration parsing (defined in decl.c) */
void reset_anon_tag_counter(void);
Type *fuse_type_specifiers(const TypeSpec *specs);
Type *type_apply_pointers(Type *type, const Pointer *pointers);
Type *type_apply_suffixes(Type *type, const DeclaratorSuffix *suffixes);
Declaration *parse_declaration(void);
DeclSpec *parse_declaration_specifiers(Type **base_type);
InitDeclarator *parse_init_declarator_list(Declarator *first, const Type *base_type);
InitDeclarator *parse_init_declarator(Declarator *decl, const Type *base_type);
StorageClass parse_storage_class_specifier(void);
TypeSpec *parse_type_specifier(void);
TypeSpec *parse_struct_or_union_specifier(void);
Field *parse_struct_declaration_list(void);
Field *parse_struct_declaration(void);
TypeSpec *parse_specifier_qualifier_list(TypeQualifier **qualifiers, AlignmentSpec **align);
void parse_one_alignment_specifier(AlignmentSpec **slot);
TypeSpec *parse_enum_specifier(void);
Enumerator *parse_enumerator_list(void);
Enumerator *parse_enumerator(void);
Type *parse_atomic_type_specifier(void);
TypeQualifier *parse_type_qualifier(void);
FunctionSpec *parse_function_specifier(void);
Type *parse_coro_frame_specifier(void);
Type *parse_coro_ptr_specifier(void);
AlignmentSpec *parse_alignment_specifier(void);
Declarator *parse_declarator(void);
Declarator *parse_direct_declarator(void);
Pointer *parse_pointer(void);
TypeQualifier *parse_type_qualifier_list(void);
Param *parse_parameter_type_list(bool *variadic_flag);
Param *parse_parameter_list(void);
Param *parse_parameter_declaration(void);
Type *parse_type_name(void);
DeclaratorSuffix *parse_direct_abstract_declarator(Ident *name_out);
Initializer *parse_initializer(void);
InitItem *parse_initializer_list(void);
Designator *parse_designation(void);
Designator *parse_designator_list(void);
Designator *parse_designator(void);
Declaration *parse_static_assert_declaration(void);

/* Statement parsing (defined in stmt.c) */
Stmt *parse_statement(void);
Stmt *parse_labeled_statement(void);
Stmt *parse_compound_statement(void);
DeclOrStmt *parse_block_item(void);
Stmt *parse_expression_statement(void);
Stmt *parse_selection_statement(void);
Stmt *parse_iteration_statement(void);
Stmt *parse_jump_statement(void);
Stmt *parse_defer_statement(void);

/* Translation unit (defined in parser.c) */
Program *parse_translation_unit(void);
ExternalDecl *parse_external_declaration(void);
Declaration *parse_declaration_list(void);

#endif /* PARSER_INTERNAL_H */
