//
// WebAssembly backend internals: the state of one function's translation and the
// functions of the backend's files.
//
#ifndef WASM_INTERNAL_H
#define WASM_INTERNAL_H

#include "codegen.h"
#include "string_map.h"
#include "wasm_ir.h"

#ifdef __cplusplus
extern "C" {
#endif

// A function's wasm signature, as the C ABI maps its type.
typedef struct {
    Wasm_ValType params[64];
    int nparams;
    Wasm_ValType result;
} Wasm_Sig;

typedef struct {
    Wasm_Func *fn;
    const Tac_TopLevel *program; // the translation unit
    const Tac_TopLevel *tl;      // the function
    StringMap locals;            // name → its local's index + 1
    StringMap types;             // name → const Tac_Type * of a frame-resident name
    struct Flow *flow;           // the body's CFG and liveness
    StringMap slots;             // name → its frame slot's offset + 1
    int frame_size;              // bytes of the frame on the shadow stack, 16-aligned
    int fp;                      // the local holding the frame's address
    StringMap labels;            // label → its basic block
    int nblocks;                 // basic blocks
    int cur;                     // the block being translated
    int state;                   // the dispatch loop's local: the block to go to
    int sret;                    // the parameter holding the result's address, or -1
    StringMap refs;              // name → the local holding its address + 1 (by reference)
    StringMap pindex;            // parameter → its first wasm parameter + 1
    int va_param;                // the parameter holding the variable arguments' address
    int call_area;               // the offset of the calls' area in the frame
    int scratch;                 // an i32 local for an address used twice + 1, or 0
    struct {
        const char *name;
        char *sig;
    } helpers[32]; // the runtime routines called, to declare
    int nhelpers;
    bool barrier; // translating a volatile access
} Gen;

// How a value travels in a call, as clang's wasm32 passes it.
typedef enum {
    WASM_PASS_NONE,  // an empty aggregate: not at all
    WASM_PASS_VALUE, // one value, of the scalar type
    WASM_PASS_PAIR,  // a long double, or an aggregate of one: two i64, the low half first
    WASM_PASS_REF,   // the address of a copy
} Wasm_Pass;

//
// Types and locals (frame.c)
//
int wasm_type_size(const Tac_Type *t);
int wasm_type_align(const Tac_Type *t);
// The value type a scalar of type t is held in.
Wasm_ValType wasm_valtype(const Tac_Type *t);
// How a value of type t travels, with the scalar it travels as in *scalar (when not
// NULL): the type itself, or the one scalar of an aggregate.
Wasm_Pass wasm_pass(const Tac_Type *t, const Tac_Type **scalar);
// Whether a result of type ret goes through a hidden first parameter, its address.
bool wasm_sret(const Tac_Type *ret);
// The signature of a function of type fun_type (a FUN_TYPE).
void wasm_signature(const Tac_Type *fun_type, Wasm_Sig *sig);
void gen_init(Gen *g, const Tac_TopLevel *program, const Tac_TopLevel *tl);
void gen_done(Gen *g);
// The type of frame-resident name, or NULL.
const Tac_Type *var_type(const Gen *g, const char *name);
// The local holding name; fails when there is none.
int var_local(const Gen *g, const char *name);
// The local holding name, or -1.
int find_local(const Gen *g, const char *name);
// The local holding the address of name, passed by reference, or -1.
int var_ref(const Gen *g, const char *name);
// The offset of name's frame slot, or -1.
int var_slot(const Gen *g, const char *name);
// The frame on the shadow stack: allocated on entry (where a parameter living in a
// slot is stored), released before each return.
void gen_prologue(Gen *g);
void gen_epilogue(Gen *g);
// The load and store of a scalar of type t.
Wasm_Op wasm_load_op(const Tac_Type *t);
Wasm_Op wasm_store_op(const Tac_Type *t);
// The type of static object `name` (a variable, constant or block-scope static of the
// unit, or an extern), or NULL.
const Tac_Type *global_type(const Gen *g, const char *name);

//
// Instruction selection (instr.c)
//
void gen_instr(Gen *g, const Tac_Instruction *in);
Wasm_Instr *emit(Gen *g, Wasm_Op op);
void emit_imm(Gen *g, Wasm_Op op, int64_t imm);
// Push integer v as a value of type t (i32 or i64).
void push_int(Gen *g, Wasm_ValType t, int64_t v);
// The type of variable `name`, wherever it lives.
const Tac_Type *type_of(const Gen *g, const char *name);
// The type of value v: a constant's by its kind.
const Tac_Type *any_type(const Gen *g, const Tac_Val *v);
// The value type of value v; a constant's is its kind's.
Wasm_ValType val_valtype(const Gen *g, const Tac_Val *v);
// Push value v, of value type t.
void push_val(Gen *g, const Tac_Val *v, Wasm_ValType t);
// Push value v as a value of type t, extended again when narrow.
void push_val_as(Gen *g, const Tac_Val *v, const Tac_Type *t);
// Bring an i32 holding a value of type t to its canonical form.
void narrow(Gen *g, const Tac_Type *t);
// The base of an access to named object `name`, which *sym and *off complete in the
// access's immediate; emit_access then loads or stores there.
void push_base(Gen *g, const char *name, const char **sym, int64_t *off);
void emit_access(Gen *g, Wasm_Op op, const char *sym, int64_t off);
// Push the address of named object `name`.
void push_addr(Gen *g, const char *name);
// Around the computation of the value of `dst`: its address first, when in memory,
// then the store of the value on top of the stack.
void begin_dst(Gen *g, const Tac_Val *dst);
void end_dst(Gen *g, const Tac_Val *dst);
bool is_aggregate(const Tac_Type *t);
// Push an i32, nonzero when a JUMP_IF_ZERO (if_zero) or JUMP_IF_NOT_ZERO on `cond` jumps.
void push_condition(Gen *g, const Tac_Val *cond, bool if_zero);
// Whether a value of type t lives only in memory, moved by its bytes: an aggregate or a
// long double.
bool is_memory_type(const Tac_Type *t);

// A place in memory: a named object, where a pointer value points, or where a local
// points, plus a byte offset.
typedef struct {
    const char *name;
    const Tac_Val *ptr;
    int local;
    int offset;
} Place;
Place place_named(const char *name, int offset);
Place place_local(int local, int offset);
// Push the address of place p.
void place_addr(Gen *g, const Place *p);
// Store value v as a value of type t at place p: a scalar by its store, a memory type
// by a copy of its bytes (a long double constant by its two halves).
void store_value(Gen *g, const Place *p, const Tac_Val *v, const Tac_Type *t);
// Push the scalar of type t at place p.
void load_value(Gen *g, const Place *p, const Tac_Type *t);

//
// Calls (call.c)
//
void gen_call(Gen *g, const Tac_Instruction *in, bool noreturn);
// __builtin_stack_save, __builtin_stack_restore, __builtin_alloca: nonzero for one of
// them, which a call expands in place.
int wasm_stack_builtin(const char *name);
bool wasm_uses_alloca(const Tac_TopLevel *tl);
void gen_return(Gen *g, const Tac_Val *src);
// A call of runtime routine `name` taking args[0..n-1] of types[0..n-1], its result of
// type ret into dst, or left on the stack when dst is NULL.
void gen_runtime(Gen *g, const char *name, const Tac_Type *ret, const Tac_Val *const *args,
                 const Tac_Type *const *types, int n, const Tac_Val *dst);
// Whether instruction `in` is a long double operation, a call of the runtime that may
// need the calls' area for its result.
bool is_ld_op(const Gen *g, const Tac_Instruction *in);
// The bytes call `in` needs in the calls' area of the frame.
int call_area_size(const Gen *g, const Tac_Instruction *in);

//
// Control flow (structure.c)
//
// Translate the function's body: into blocks, loops and ifs when its graph is
// reducible or can be made so with a dispatch per irreducible region (and
// wasm_structure is on), else into the dispatch skeleton.
void gen_body(Gen *g);
// A jump to label `target`: what goes before its condition, then the branch.
void gen_branch_setup(Gen *g, const char *target);
void gen_branch(Gen *g, const char *target, bool conditional);

//
// The finished code (peephole.c, locals.c)
//
// Stackify, the peephole rules and coalescing, as switched on.
void wasm_optimize(Wasm_Func *fn);
// Share locals whose values are never live at once.
void wasm_coalesce_locals(Wasm_Func *fn);

//
// Static data (data.c)
//
void emit_static_variable(FILE *out, const char *name, bool global, const Tac_Type *type,
                          const Tac_StaticInit *init, bool readonly, int alignment);

//
// Symbols and assembly (emit.c)
//
// The assembler's name of a symbol: `name` itself, but for those it cannot take.
const char *wasm_name(const char *name);
// The assembler's name of TAC name `name`: main as clang names it.
const char *wasm_symbol(const Tac_TopLevel *program, const char *name);
// The declarations a translation unit starts with: the target's features and the
// signature of every function it defines or references.
void wasm_emit_unit_begin(FILE *out, const Tac_TopLevel *program);
void wasm_emit_func(FILE *out, const Wasm_Func *fn);
// The signature as call_indirect spells it, (i32, i64) -> (f64), in a new string.
char *wasm_sig_string(const Wasm_Sig *sig);
// main(void)'s two other names, as clang emits them: the main(argc, argv) that calls it
// and __main_void, which crt0 calls.
void wasm_emit_main_aliases(FILE *out);

#ifdef __cplusplus
}
#endif

#endif // WASM_INTERNAL_H
