//
// Internal header: cross-file declarations for the typecheck implementation.
// Included by typecheck.c, expressions.c, initializers.c, statements.c, declarations.c.
// Not part of the public API — use semantic.h for external callers.
//
#ifndef TYPECHECK_H
#define TYPECHECK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ast.h"
#include "tac.h"

// Scope management — typecheck.c
void scope_increment(void);
void scope_decrement(void);

// Type system utilities — typecheck.c
void validate_type(const Type *t);

// Reject a struct/union tag reference whose keyword disagrees with an existing tag of the same
// name (e.g. using `union x` when `struct x` is in scope) — declarations.c.
void check_tag_kind(const Type *t);

// Register struct/union definitions embedded in a type tree — declarations.c.
void register_inline_struct_defs(const Type *t);

// Resolve and validate a type name, registering any struct/union it defines — expressions.c.
Type *check_type_name(Type *t);
size_t get_array_size(const Type *t);
void set_array_size(Type *t, size_t size);

// Type conversion — typecheck.c
Expr *convert_to_type(Expr *e, const Type *target_type);
Expr *convert_to_kind(Expr *e, TypeKind kind);
const Type *get_common_type(const Type *t1, const Type *t2);
bool is_zero_int(const Literal *c);
bool is_null_pointer_constant(const Expr *e);
Type *common_pointer_type(const Expr *e1, const Expr *e2);
bool compatible_type(const Type *target, const Type *src);
// Convert e as by assignment to target_type; the context ("assigning", "returning",
// "passing argument 2 of 'f'", ...) completes the message when it cannot be converted.
Expr *coerce_for_assignment(Expr *e, const Type *target_type, const char *context);

// Const evaluation — typecheck.c
bool try_eval_const_int(const Expr *e, long *out);

// An integer constant converted to the integer type `t` of the active target: wrapped
// to its value width, as C11 §6.3.1.3 converts it.
long narrow_const_int(long val, const Type *t);
bool try_eval_const_real(const Expr *e, double *out);
bool try_eval_const_ld(const Expr *e, Float128 *out);

// Expression type-checking — expressions.c
Expr *typecheck_string(Expr *e);
Expr *typecheck_call_args(const Type *fn_type, Expr *args, const char *name);
Expr *typecheck_and_decay(Expr *e);
Expr *typecheck_scalar(Expr *e);

// Initializer type-checking — initializers.c
Tac_StaticInit *build_static_init(Type *var_type, Initializer **init);
Initializer *typecheck_init(Type *target_type, Initializer *init);

// Initializer normalization — init_normalize.c
typedef enum {
    INIT_AUTOMATIC, // automatic storage / compound literal: typecheck the leaves
    INIT_STATIC,    // static storage: leave the leaves raw
} InitMode;
Initializer *normalize_init(Type *type, Initializer *init, InitMode mode);

// Statement type-checking — statements.c
DeclOrStmt *typecheck_block(const Type *ret_type, DeclOrStmt *block);
Stmt *typecheck_statement(const Type *ret_type, Stmt *s);

// Declaration spec helpers — declarations.c
bool has_storage(const DeclSpec *spec);

// Declaration type-checking — declarations.c
void typecheck_local_decl(Declaration *d);
void typecheck_global_decl(ExternalDecl *d);

// The type of the function whose body is being checked, or NULL.
extern const Type *typecheck_function;

// Coroutines — coroutines.c
struct Symbol;
extern int coro_defer_depth;                 // deferred statements around the expression checked
extern int coro_loop_head_depth;             // loop heads around it: no co_alloca there
bool is_frame_type(const Type *t);           // _Coro_frame(Y, T)
bool is_coro_struct(const Type *t);          // _Coro_frame(Y, T) or the struct of _Coro_ptr(Y, T)
const Type *coro_desc_target(const Type *t); // a _Coro_ptr(Y, T)'s struct, else NULL
bool coroutine_has_coro_ptr(const Type *fn_type); // takes (void) or (void *)
bool coroutine_value(Expr *e,
                     const struct Symbol *sym); // a coroutine's name as a value: its coro_ptr
const Type *typecheck_coro_ptr_call(Expr *call, const Type *desc); // await p(arg): the result type
bool same_frame_type(const Type *a, const Type *b);                // two frame types: Y and T agree
void check_frame_type(const Type *t);
void reject_coro_spec(const DeclSpec *spec, const char *name);
// The yield type of a function declared _Coro(Y), its declaration checked; else NULL.
const Type *check_coroutine_decl(const char *name, const DeclSpec *spec, const Type *fn_type);
// A redeclaration agrees with `existing` on being a coroutine, and on Y.
void agree_coroutine(const struct Symbol *existing, const Type *yield, const char *name);
void coro_begin_body(const Type *yield); // NULL for an ordinary function
void coro_end_body(void);
void check_coroutine_name(const struct Symbol *sym); // a coroutine named as a value
bool coroutine_call_allowed(const Expr *call);       // the call an arena await makes
void check_alloca_call(void);                        // a call of __builtin_alloca
// The lint for frames in automatic storage (coroutines.c).
void coro_lint_function(const char *name);                          // the function checked next
void coro_lint_bind(const char *var, int level, const Expr *value); // var (NULL: wider) = value
void coro_lint_statement(const Expr *e);                            // an expression statement
void coro_lint_scope_exit(int level);                               // the block at `level` ends
// Drop the per-function state of each module, for semantic_recover().
void coro_recover(void);
void declarations_recover(void);
void statements_recover(void);
Expr *typecheck_yield(Expr *e);
Expr *typecheck_await(Expr *e);
Expr *typecheck_co_op(Expr *e);

#ifdef __cplusplus
}
#endif

#endif /* TYPECHECK_H */
