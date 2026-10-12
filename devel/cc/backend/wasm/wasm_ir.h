//
// WebAssembly backend IR: a function is a flat list of stack-machine instructions, the
// structured ones (block, loop, if, else, end) among them, as the LLVM assembler takes
// them.  An instruction has at most one immediate: a local index, a constant, a symbol
// plus offset, a memory access's offset and alignment, a branch depth, or a call's
// callee or signature.  Only what the backend emits is modelled.
//
#ifndef WASM_IR_H
#define WASM_IR_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// A value type, and the empty type of a function returning nothing.
typedef enum {
    WASM_VOID,
    WASM_I32,
    WASM_I64,
    WASM_F32,
    WASM_F64,
} Wasm_ValType;

// The immediate an opcode takes, which the emitter prints.
typedef enum {
    WASM_FORM_NONE,     // nothing: i32.add, return, end_block
    WASM_FORM_LOCAL,    // a local's index
    WASM_FORM_GLOBAL,   // a global's symbol
    WASM_FORM_I32,      // a 32-bit constant, or sym+off (a relocated address)
    WASM_FORM_I64,      // a 64-bit constant
    WASM_FORM_F32,      // a binary32 constant, by its bits
    WASM_FORM_F64,      // a binary64 constant, by its bits
    WASM_FORM_MEM,      // offset (or sym+off) and alignment: a load or store
    WASM_FORM_DEPTH,    // a branch depth: br, br_if
    WASM_FORM_TABLE,    // a list of depths, the last the default: br_table
    WASM_FORM_CALL,     // the callee's symbol
    WASM_FORM_INDIRECT, // the callee's signature: call_indirect
    WASM_FORM_BLOCK,    // a block type, WASM_VOID for none: block, loop, if
    WASM_FORM_MEMORY,   // memory index 0: memory.size, memory.grow, memory.fill
    WASM_FORM_MEMORY2,  // memory indices 0, 0: memory.copy
} Wasm_Form;

// Opcode, mnemonic, form.
#define WASM_OPS(X)                                     \
    X(UNREACHABLE, "unreachable", NONE)                 \
    X(NOP, "nop", NONE)                                 \
    X(BLOCK, "block", BLOCK)                            \
    X(LOOP, "loop", BLOCK)                              \
    X(IF, "if", BLOCK)                                  \
    X(ELSE, "else", NONE)                               \
    X(END_BLOCK, "end_block", NONE)                     \
    X(END_LOOP, "end_loop", NONE)                       \
    X(END_IF, "end_if", NONE)                           \
    X(BR, "br", DEPTH)                                  \
    X(BR_IF, "br_if", DEPTH)                            \
    X(BR_TABLE, "br_table", TABLE)                      \
    X(RETURN, "return", NONE)                           \
    X(CALL, "call", CALL)                               \
    X(CALL_INDIRECT, "call_indirect", INDIRECT)         \
    X(DROP, "drop", NONE)                               \
    X(SELECT, "select", NONE)                           \
    X(LOCAL_GET, "local.get", LOCAL)                    \
    X(LOCAL_SET, "local.set", LOCAL)                    \
    X(LOCAL_TEE, "local.tee", LOCAL)                    \
    X(GLOBAL_GET, "global.get", GLOBAL)                 \
    X(GLOBAL_SET, "global.set", GLOBAL)                 \
    X(I32_LOAD, "i32.load", MEM)                        \
    X(I64_LOAD, "i64.load", MEM)                        \
    X(F32_LOAD, "f32.load", MEM)                        \
    X(F64_LOAD, "f64.load", MEM)                        \
    X(I32_LOAD8_S, "i32.load8_s", MEM)                  \
    X(I32_LOAD8_U, "i32.load8_u", MEM)                  \
    X(I32_LOAD16_S, "i32.load16_s", MEM)                \
    X(I32_LOAD16_U, "i32.load16_u", MEM)                \
    X(I64_LOAD8_S, "i64.load8_s", MEM)                  \
    X(I64_LOAD8_U, "i64.load8_u", MEM)                  \
    X(I64_LOAD16_S, "i64.load16_s", MEM)                \
    X(I64_LOAD16_U, "i64.load16_u", MEM)                \
    X(I64_LOAD32_S, "i64.load32_s", MEM)                \
    X(I64_LOAD32_U, "i64.load32_u", MEM)                \
    X(I32_STORE, "i32.store", MEM)                      \
    X(I64_STORE, "i64.store", MEM)                      \
    X(F32_STORE, "f32.store", MEM)                      \
    X(F64_STORE, "f64.store", MEM)                      \
    X(I32_STORE8, "i32.store8", MEM)                    \
    X(I32_STORE16, "i32.store16", MEM)                  \
    X(I64_STORE8, "i64.store8", MEM)                    \
    X(I64_STORE16, "i64.store16", MEM)                  \
    X(I64_STORE32, "i64.store32", MEM)                  \
    X(MEMORY_SIZE, "memory.size", MEMORY)               \
    X(MEMORY_GROW, "memory.grow", MEMORY)               \
    X(MEMORY_COPY, "memory.copy", MEMORY2)              \
    X(MEMORY_FILL, "memory.fill", MEMORY)               \
    X(I32_CONST, "i32.const", I32)                      \
    X(I64_CONST, "i64.const", I64)                      \
    X(F32_CONST, "f32.const", F32)                      \
    X(F64_CONST, "f64.const", F64)                      \
    X(I32_EQZ, "i32.eqz", NONE)                         \
    X(I32_EQ, "i32.eq", NONE)                           \
    X(I32_NE, "i32.ne", NONE)                           \
    X(I32_LT_S, "i32.lt_s", NONE)                       \
    X(I32_LT_U, "i32.lt_u", NONE)                       \
    X(I32_GT_S, "i32.gt_s", NONE)                       \
    X(I32_GT_U, "i32.gt_u", NONE)                       \
    X(I32_LE_S, "i32.le_s", NONE)                       \
    X(I32_LE_U, "i32.le_u", NONE)                       \
    X(I32_GE_S, "i32.ge_s", NONE)                       \
    X(I32_GE_U, "i32.ge_u", NONE)                       \
    X(I64_EQZ, "i64.eqz", NONE)                         \
    X(I64_EQ, "i64.eq", NONE)                           \
    X(I64_NE, "i64.ne", NONE)                           \
    X(I64_LT_S, "i64.lt_s", NONE)                       \
    X(I64_LT_U, "i64.lt_u", NONE)                       \
    X(I64_GT_S, "i64.gt_s", NONE)                       \
    X(I64_GT_U, "i64.gt_u", NONE)                       \
    X(I64_LE_S, "i64.le_s", NONE)                       \
    X(I64_LE_U, "i64.le_u", NONE)                       \
    X(I64_GE_S, "i64.ge_s", NONE)                       \
    X(I64_GE_U, "i64.ge_u", NONE)                       \
    X(F32_EQ, "f32.eq", NONE)                           \
    X(F32_NE, "f32.ne", NONE)                           \
    X(F32_LT, "f32.lt", NONE)                           \
    X(F32_GT, "f32.gt", NONE)                           \
    X(F32_LE, "f32.le", NONE)                           \
    X(F32_GE, "f32.ge", NONE)                           \
    X(F64_EQ, "f64.eq", NONE)                           \
    X(F64_NE, "f64.ne", NONE)                           \
    X(F64_LT, "f64.lt", NONE)                           \
    X(F64_GT, "f64.gt", NONE)                           \
    X(F64_LE, "f64.le", NONE)                           \
    X(F64_GE, "f64.ge", NONE)                           \
    X(I32_ADD, "i32.add", NONE)                         \
    X(I32_SUB, "i32.sub", NONE)                         \
    X(I32_MUL, "i32.mul", NONE)                         \
    X(I32_DIV_S, "i32.div_s", NONE)                     \
    X(I32_DIV_U, "i32.div_u", NONE)                     \
    X(I32_REM_S, "i32.rem_s", NONE)                     \
    X(I32_REM_U, "i32.rem_u", NONE)                     \
    X(I32_AND, "i32.and", NONE)                         \
    X(I32_OR, "i32.or", NONE)                           \
    X(I32_XOR, "i32.xor", NONE)                         \
    X(I32_SHL, "i32.shl", NONE)                         \
    X(I32_SHR_S, "i32.shr_s", NONE)                     \
    X(I32_SHR_U, "i32.shr_u", NONE)                     \
    X(I64_ADD, "i64.add", NONE)                         \
    X(I64_SUB, "i64.sub", NONE)                         \
    X(I64_MUL, "i64.mul", NONE)                         \
    X(I64_DIV_S, "i64.div_s", NONE)                     \
    X(I64_DIV_U, "i64.div_u", NONE)                     \
    X(I64_REM_S, "i64.rem_s", NONE)                     \
    X(I64_REM_U, "i64.rem_u", NONE)                     \
    X(I64_AND, "i64.and", NONE)                         \
    X(I64_OR, "i64.or", NONE)                           \
    X(I64_XOR, "i64.xor", NONE)                         \
    X(I64_SHL, "i64.shl", NONE)                         \
    X(I64_SHR_S, "i64.shr_s", NONE)                     \
    X(I64_SHR_U, "i64.shr_u", NONE)                     \
    X(F32_NEG, "f32.neg", NONE)                         \
    X(F32_SQRT, "f32.sqrt", NONE)                       \
    X(F32_ADD, "f32.add", NONE)                         \
    X(F32_SUB, "f32.sub", NONE)                         \
    X(F32_MUL, "f32.mul", NONE)                         \
    X(F32_DIV, "f32.div", NONE)                         \
    X(F64_NEG, "f64.neg", NONE)                         \
    X(F64_SQRT, "f64.sqrt", NONE)                       \
    X(F64_ADD, "f64.add", NONE)                         \
    X(F64_SUB, "f64.sub", NONE)                         \
    X(F64_MUL, "f64.mul", NONE)                         \
    X(F64_DIV, "f64.div", NONE)                         \
    X(I32_WRAP_I64, "i32.wrap_i64", NONE)               \
    X(I64_EXTEND_I32_S, "i64.extend_i32_s", NONE)       \
    X(I64_EXTEND_I32_U, "i64.extend_i32_u", NONE)       \
    X(I32_EXTEND8_S, "i32.extend8_s", NONE)             \
    X(I32_EXTEND16_S, "i32.extend16_s", NONE)           \
    X(I64_EXTEND8_S, "i64.extend8_s", NONE)             \
    X(I64_EXTEND16_S, "i64.extend16_s", NONE)           \
    X(I64_EXTEND32_S, "i64.extend32_s", NONE)           \
    X(I32_TRUNC_SAT_F32_S, "i32.trunc_sat_f32_s", NONE) \
    X(I32_TRUNC_SAT_F32_U, "i32.trunc_sat_f32_u", NONE) \
    X(I32_TRUNC_SAT_F64_S, "i32.trunc_sat_f64_s", NONE) \
    X(I32_TRUNC_SAT_F64_U, "i32.trunc_sat_f64_u", NONE) \
    X(I64_TRUNC_SAT_F32_S, "i64.trunc_sat_f32_s", NONE) \
    X(I64_TRUNC_SAT_F32_U, "i64.trunc_sat_f32_u", NONE) \
    X(I64_TRUNC_SAT_F64_S, "i64.trunc_sat_f64_s", NONE) \
    X(I64_TRUNC_SAT_F64_U, "i64.trunc_sat_f64_u", NONE) \
    X(F32_CONVERT_I32_S, "f32.convert_i32_s", NONE)     \
    X(F32_CONVERT_I32_U, "f32.convert_i32_u", NONE)     \
    X(F32_CONVERT_I64_S, "f32.convert_i64_s", NONE)     \
    X(F32_CONVERT_I64_U, "f32.convert_i64_u", NONE)     \
    X(F64_CONVERT_I32_S, "f64.convert_i32_s", NONE)     \
    X(F64_CONVERT_I32_U, "f64.convert_i32_u", NONE)     \
    X(F64_CONVERT_I64_S, "f64.convert_i64_s", NONE)     \
    X(F64_CONVERT_I64_U, "f64.convert_i64_u", NONE)     \
    X(F32_DEMOTE_F64, "f32.demote_f64", NONE)           \
    X(F64_PROMOTE_F32, "f64.promote_f32", NONE)         \
    X(I32_REINTERPRET_F32, "i32.reinterpret_f32", NONE) \
    X(I64_REINTERPRET_F64, "i64.reinterpret_f64", NONE) \
    X(F32_REINTERPRET_I32, "f32.reinterpret_i32", NONE) \
    X(F64_REINTERPRET_I64, "f64.reinterpret_i64", NONE)

typedef enum {
#define WASM_OP_ENUM(op, name, form) WASM_##op,
    WASM_OPS(WASM_OP_ENUM)
#undef WASM_OP_ENUM
        WASM_NUM_OPS
} Wasm_Op;

typedef struct Wasm_Instr {
    struct Wasm_Instr *next, *prev;
    Wasm_Op op;
    int64_t imm;     // a local, a constant (a float by its bits), an offset or a depth
    int align;       // a memory access's alignment in bytes, below its width; 0 = natural
    char *sym;       // owned: a symbol (call, global, sym+off), or a signature; or NULL
    int *table;      // owned: br_table's depths, the default last
    int ntable;      // their count
    Wasm_ValType bt; // a block's result type
    int pops;        // a call's: the values it takes
    int pushes;      // a call's: the values it leaves
    bool barrier;    // part of a volatile access: the optimizer moves nothing across it
} Wasm_Instr;

typedef struct {
    char *name;           // owned: the symbol
    bool global;          // visible outside the unit
    Wasm_ValType *params; // owned: the parameter types
    int nparams;
    Wasm_ValType result;  // WASM_VOID for none
    Wasm_ValType *locals; // owned: the types of the locals after the parameters
    int nlocals;
    int frame; // the local holding the frame's address (16-aligned), or -1
    Wasm_Instr *first, *last;
} Wasm_Func;

const char *wasm_op_name(Wasm_Op op);
Wasm_Form wasm_op_form(Wasm_Op op);
const char *wasm_valtype_name(Wasm_ValType t);

Wasm_Func *wasm_new_func(const char *name, bool global);
void wasm_free_func(Wasm_Func *fn);

// Append an instruction to fn and return it, for the caller to fill its immediate.
Wasm_Instr *wasm_append(Wasm_Func *fn, Wasm_Op op);
// Unlink and free one instruction.
void wasm_remove(Wasm_Func *fn, Wasm_Instr *in);
// A new local of type t; its index (after the parameters).
int wasm_add_local(Wasm_Func *fn, Wasm_ValType t);
// A new instruction op before `at` (at the end when NULL).
Wasm_Instr *wasm_insert(Wasm_Func *fn, Wasm_Instr *at, Wasm_Op op);
// Move instructions first..last (a run of fn) in front of `at`.
void wasm_move(Wasm_Func *fn, Wasm_Instr *first, Wasm_Instr *last, Wasm_Instr *at);
// The values instruction `in` takes from the stack and leaves on it; false for a
// structured or branch instruction, whose effect is not a plain one.
bool wasm_stack_effect(const Wasm_Instr *in, int *pops, int *pushes);

#ifndef __cplusplus
#include "srcloc.h"

_Noreturn void fatal_error(const char *fmt, ...);
#endif

#ifdef __cplusplus
}
#endif

#endif // WASM_IR_H
