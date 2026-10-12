//
// Internal types for translator.
//
#ifndef TRANSLATE_H
#define TRANSLATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ast.h"
#include "defer.h"
#include "optimize.h"
#include "semantic.h"
#include "string_map.h"
#include "symtab.h"
#include "tac.h"

//
// What runs when control leaves a block (docs/Coroutines_in_C.md, section 1): a
// deferred statement, or the release of a co_alloca's frame.
//
typedef enum { EXIT_DEFER, EXIT_CO_RELEASE } ExitKind;

typedef struct {
    ExitKind kind;
    Stmt *stmt;  // EXIT_DEFER: the deferred statement
    char *frame; // EXIT_CO_RELEASE: the variable holding the frame, null when skipped
    char *sp;    // EXIT_CO_RELEASE: the variable holding the stack pointer before it;
                 // NULL in a coroutine, whose co_alloca takes the arena
} ExitAction;

// Where an exit that runs a block's shared cleanup goes when it is done: a label, a
// return (of the function's one return variable, when it has a value), or the end of a
// coroutine with its final state.
typedef enum { DEST_LABEL, DEST_RETURN, DEST_FINISH } DestKind;

typedef struct {
    DestKind kind;
    char *label;    // DEST_LABEL
    bool value;     // DEST_RETURN: returns the return variable
    unsigned state; // DEST_FINISH
    int id;         // the value of the "where next" variable that picks it
} ExitDest;

// A block being lowered, and the exit actions registered in it so far.  When an exit
// shares its cleanup (stmt.c), the block's actions are also lowered once more at its end
// as a chain: an entry label in front of each, then a test of the "where next" variable
// for the exits that end here, then on to the next block out that has actions.
typedef struct {
    const Stmt *key; // as in DeferScope (semantic/defer.h)
    ExitAction *actions;
    int count, cap;
    Tac_Instruction *entry; // the last instruction before the block, NULL at the start
    bool chained;           // some exit goes through the chain
    bool continues;         // and some goes on past this block
    char **entries;         // entries[j]: the label in front of action j, or NULL
    int nentries;
    ExitDest *dests; // the exits that end after this block's chain
    int ndests, dests_cap;
} TacScope;

// The coroutine being lowered (translator/coro.c): its frame pointer parameter, and
// where the value it yields and its result lie in the frame.
typedef struct {
    const char *fp;
    const Type *yield, *result;
    int value_off, result_off;
} TacCoro;

// A loop or switch being lowered: where its break and continue go, and how many
// blocks were open outside it (those a break or continue does not leave).
typedef struct {
    const char *break_label;
    const char *cont_label; // NULL for a switch
    int depth;
} TacBreak;

//
// TAC generation context — one per function being lowered.
//
typedef struct {
    Tac_Instruction *head;
    Tac_Instruction *tail;
    int temp_id;
    Tac_TopLevel *static_constants; // strings accumulated during body lowering
    Tac_TopLevel *externs;          // block-scope extern declarations, as EXTERN toplevels
    Tac_Param *locals;              // automatic locals and temporaries, with their types
    Tac_Param *locals_tail;         // tail of `locals` for O(1) append
    Tac_Param *array_locals;        // names of local arrays (block-scope symbols are
                                    // purged before lowering, so value decay needs this)
    const char *sret_name;          // hidden return-pointer param name when the current
                                    // function returns a multi-word struct by value; else NULL
    StringMap user_labels;          // source label name -> unique %L<n> TAC name, per function
    // defer: the blocks open at the point being lowered, innermost last, and the loops
    // and switches around it.  While a deferred statement is lowered, they are its own.
    TacScope *scopes;
    int nscopes, scopes_cap;
    TacBreak *breaks;
    int nbreaks, breaks_cap;
    StringMap label_pos;  // label -> DeferPos *, for a goto that leaves defers behind
    bool label_pos_ready; // label_pos collected (only once a goto needs it)
    Stmt *body;           // the function body, where label_pos comes from
    int defer_depth;      // deferred statements being lowered, one inside another
    const TacCoro *coro;  // the coroutine being lowered, or NULL
    char *where;          // the "where next" variable of the shared cleanups, made when needed
    char *ret_var;        // the value a return through a shared cleanup returns
    int ndest_ids;        // the values `where` has taken
} TacCtx;

//
// Switch case tracking — built by collect_cases(), consumed by gen_stmt STMT_SWITCH.
//
typedef struct CaseEntry {
    Expr *expr;        // case constant expression
    const char *label; // non-owning ptr to stmt->branch_target_label
    struct CaseEntry *next;
} CaseEntry;

typedef struct {
    CaseEntry *head;
    CaseEntry **tail;
    const char *default_label; // non-owning ptr, or NULL
} CaseList;

// Enable debug output
extern int translator_debug;
// Nonzero: check every translated function with tac_verify_function and stop on a
// problem.  Always on in a build without NDEBUG.
extern int translate_verify;
extern int translate_rotate_loops; // set by translate() from OptFlags.loop_rotate
extern bool translate_cond_jumps;  // && || ! in a condition as jumps (expr.c); --no-cond-jumps
extern bool
    translate_shared_cleanup; // an exit may share a large cleanup (stmt.c); --no-shared-cleanup
extern int import_debug;
extern int export_debug;
extern int wio_debug;
extern int xalloc_debug;

//
// Low-level TAC-building helpers (translate.c)
//
void tac_append(TacCtx *ctx, Tac_Instruction *instr);
// A fresh label name.
char *new_temp(TacCtx *ctx);
// A fresh temporary holding a value of `type`, recorded in the function's symbol list
// (takes ownership of `type`).
char *new_typed_temp(TacCtx *ctx, Tac_Type *type);
void tac_record_local(TacCtx *ctx, const char *name, const Type *type);
void tac_record_array_local(TacCtx *ctx, const char *name);
bool tac_is_array_local(const TacCtx *ctx, const char *name);
// Record a block-scope declaration of an external object or function.  Its symbol is
// purged before lowering, so it travels as an EXTERN toplevel ahead of the function.
void tac_record_extern(TacCtx *ctx, const char *name, const Type *type);
// The same with a TAC type (takes ownership): a runtime routine, or a name the
// coroutine split defines (f$init, f$resume, f$co), which has no symbol.
void tac_record_extern_tac(TacCtx *ctx, const char *name, Tac_Type *type);
Tac_Val *val_int(int64_t v);
Tac_Val *val_long(long v);
Tac_Val *val_long_long(long long v);
Tac_Val *val_uint(uint64_t v);
Tac_Val *val_ulong(unsigned long v);
Tac_Val *val_ulong_long(unsigned long long v);
Tac_Val *val_size(uint64_t v);
Tac_Val *val_float(float v);
Tac_Val *val_double(double v);
Tac_Val *val_long_double(Float128 v);
Tac_Val *val_var(const char *name);
// A Var naming a fresh temporary of `type` (takes ownership of `type`).
Tac_Val *new_var_val(TacCtx *ctx, Tac_Type *type);
Tac_Val *dup_val(const Tac_Val *v);
// A zero constant of scalar type `t`: floating-point for a floating type.
Tac_Val *val_zero(const Type *t);
Tac_Val *emit_cast(TacCtx *ctx, Tac_Val *src, const Type *from, const Type *to);
// "src != 0" — the C11 §6.3.1.2 conversion of a scalar to _Bool.  Used by emit_cast for
// every ordinary conversion, and directly by ++/--, which never builds a cast.  Returns
// the instruction-owned destination Val (see the definition).
Tac_Val *emit_bool_normalize(TacCtx *ctx, Tac_Val *src, const Type *from, const Type *to);
void emit_jump(TacCtx *ctx, const char *target);
void emit_label(TacCtx *ctx, const char *name);
const char *user_label_name(TacCtx *ctx, const char *src);
void free_user_labels(StringMap *labels);

//
// Struct-by-value support (translate.c)
//
// One target machine word, in bytes (the unit a scalar return value occupies).
int target_word_bytes(void);
// True when `t` is a struct/union too large to return by value on this target
// (Target.struct_return_max), so it uses the hidden-pointer (sret) calling convention.
bool type_is_byval_sret(const Type *t);
// True when `t` is a struct/union argument this target passes as consecutive words
// (Target.struct_args_split).
bool type_is_split_arg(const Type *t);
// True when a `?:` of type `t` merges its arms in a frame slot by aggregate copies
// rather than by a COPY: a struct/union wider than a word, or any struct/union on a
// byte-addressed target.
bool type_needs_slot(const Type *t);
// One side of an aggregate copy: a named frame/global aggregate `name` at byte `offset`
// (reached by COPY_*_OFFSET), or else the address held in variable `ptr` (ADD_PTR +
// LOAD/STORE).
typedef struct {
    const char *name;
    int offset;
    const char *ptr;
} AggPlace;
// The unit of an aggregate copy: the type's alignment, at most one machine word.  A size
// is a multiple of the alignment, so the chunks cover the object exactly.
int aggregate_chunk(const Type *t);
// Copy a whole object of `type` from `src` to `dst`, chunk by chunk.
void gen_aggregate_copy(TacCtx *ctx, const AggPlace *dst, const AggPlace *src, const Type *type);
// Copy a whole struct/union value from named aggregate `src_name` into `dst_name` at
// byte offset `dst_off`.
void gen_struct_assign(TacCtx *ctx, const char *dst_name, int dst_off, const char *src_name,
                       const Type *type);
// Initialize a whole aggregate (named destination base `dst_name`+`dst_off`) from a value
// expression, which may also be reached through a pointer (unlike gen_struct_assign,
// which assumes the source is a named aggregate base).  Lives in expr.c.
void gen_aggregate_init_from_expr(TacCtx *ctx, const char *dst_name, int dst_off, Expr *value,
                                  const Type *type);

//
// Type conversion (translate.c)
//
// ast_type_to_tac_type is semantic's (semantic.h).
// Pointer to `target` (takes ownership).
Tac_Type *tac_type_ptr(Tac_Type *target);
// Pointer to the TAC form of AST type `t`.
Tac_Type *tac_type_ptr_to(const Type *t);
// Byte sizes of the active target, for tac_verify.
void tac_layout_of_target(Tac_Layout *layout);
// Plain char, with the target's signedness.
Tac_Type *tac_type_char(void);
// The unsigned integer of one machine word (pointer size): the unit of word copies.
Tac_Type *tac_type_word(void);

// ptrdiff_t: the type of a pointer difference.
Tac_Type *tac_type_ptrdiff(void);

//
// Expression and statement lowering (translate_expr.c, translate_stmt.c)
//
Tac_Val *gen_expr(TacCtx *ctx, Expr *e);
Tac_Val *gen_string_constant(TacCtx *ctx, const char *s, size_t len); // its address
// Initialize bit-field `bf` of type `type`, in the storage unit at `offset` of named
// aggregate `var`, to owned value `v`.
void gen_bitfield_init(TacCtx *ctx, const char *var, int offset, const BitField *bf,
                       const Type *type, Tac_Val *v);
Tac_Val *gen_cond_val(TacCtx *ctx, Expr *cond);
bool cond_jumps(void);
void gen_cond_jump(TacCtx *ctx, Expr *cond, bool if_true, const char *target);
bool is_simple_cond(const Expr *e);
bool is_logical(const Expr *e);
void gen_stmt(TacCtx *ctx, Stmt *stmt);
void gen_compound_init(TacCtx *ctx, const char *var_name, int base_offset, const Initializer *init);
void gen_aggregate_init(TacCtx *ctx, const char *var_name, const Initializer *init, int bytes);
void gen_string_array_init(TacCtx *ctx, const char *var_name, const Expr *str_expr, int bytes);

// Blocks and their exit actions (stmt.c), for the coroutines.
void tac_scope_add(TacCtx *ctx, ExitAction action);     // to the innermost block
void tac_scope_entry(TacCtx *ctx, Tac_Instruction *in); // run on entering the innermost block
void gen_exits_all(TacCtx *ctx); // the exit actions of every open block, innermost first
// Leave every open block and end the coroutine with `state`, returning 1: its exit
// actions run through the shared cleanup when there are any.
void gen_finish(TacCtx *ctx, unsigned state);

//
// Coroutines (coro.c; docs/Coroutines_Internals.md §5)
//
// The frame header, the runtime's struct co_header: unsigned state and flags, then the
// resume function and the task, top and limit pointers (24 bytes on ILP32, 40 on LP64).
int co_header_size(void);
// Where the value yielded and the result lie in a frame of co_frame(Y, T), and the
// end of the two, from which the split pass lays out the rest; the frame's alignment so far.
void coro_layout(const Type *yield, const Type *result, int *value_off, int *result_off, int *end,
                 int *align);
Tac_Val *gen_yield(TacCtx *ctx, Expr *e);
Tac_Val *gen_co_op(TacCtx *ctx, Expr *e);
Tac_Val *gen_await(TacCtx *ctx, Expr *e);
void gen_coro_return(TacCtx *ctx, Tac_Val *value, const Type *type); // value may be NULL
void gen_co_release(TacCtx *ctx, const ExitAction *a);
Tac_Val *gen_alloca(TacCtx *ctx, Expr *e);         // a call of __builtin_alloca
void gen_finish_code(TacCtx *ctx, unsigned state); // a coroutine's state set, and return 1

// A coroutine after the optimizer: the split pass (stage 2) makes f$resume a state
// machine over the frame, the user's parameters and every name live across a
// suspension moved into it.  Returns f$init and f$co, to follow it in the unit.
typedef struct {
    const char *name; // f
    bool global;
    int frame_start; // the end of the value and the result: where the rest goes
    int frame_align;
    bool with_ptr; // takes (void) or (void *): f$co holds init and resume too
} CoroSplit;
Tac_TopLevel *coro_split(Tac_TopLevel *fn, const CoroSplit *info);
extern int coro_table_min; // suspension points from which the dispatch is a jump table,
                           // on a target with jump_tables
Tac_Val *gen_coro_ptr(TacCtx *ctx, const char *g,
                      const Type *type); // a coroutine's name as a value

//
// Convert one external declaration to TAC and optimize each function it yields.
// Each function self-describes its params and automatic locals, so the optimizer
// classifies locals vs. globals per function — no whole-program context needed.
//
// Lower one external declaration to TAC.  `label_seq` is a caller-owned counter
// that must persist across every call within one translation unit and be reset to
// 0 at its start: compiler temporaries and branch labels (`%N`, from new_temp) are
// numbered from it, so a per-function reset would make different functions reuse
// `%0`, `%1`, …  That is harmless on the Madlen backend (each function is its own
// `,name,`/`,end,` module, so labels are module-scoped) but corrupts the Unix
// (b6as) and Bemsh backends, which place the whole unit in one file with
// file-scoped labels — a duplicate label silently binds a branch to the wrong
// function.  Threading one counter across the unit keeps every `%N` unique.
Tac_TopLevel *translate(const ExternalDecl *ast, OptFlags flags, int *label_seq);

// Bracket a translation unit.  Between the two calls translate() notes every name the
// unit defines or references; translate_unit_end() returns an EXTERN toplevel for each
// name referenced but not defined (sorted by name), and must run while the file-scope
// symbol table is still alive.
void translate_unit_begin(void);
Tac_TopLevel *translate_unit_end(void);

#ifdef __cplusplus
}
#endif

#endif /* TRANSLATE_H */
