#ifndef WASM_CODEGEN_H
#define WASM_CODEGEN_H

#include <stdbool.h>
#include <stdio.h>

#include "tac.h"

#ifdef __cplusplus
extern "C" {
#endif

// Structured control flow (on by default): off, every function gets the dispatch
// skeleton (genwasm --no-structure).
extern bool wasm_structure;
// A dispatch for each irreducible region (on by default): off, a function with one gets
// the whole-function skeleton (genwasm --no-regional).
extern bool wasm_regional;
// The rewrites of the finished code (on by default): the peephole rules, keeping values
// on the operand stack (stackify), and sharing locals (coalescing); genwasm
// --no-peephole, --no-stackify, --no-coalesce.
extern bool wasm_peephole;
extern bool wasm_stackify;
extern bool wasm_coalesce;

// Translate one TAC toplevel to WebAssembly assembly (the LLVM assembler's syntax) on
// `out`.  `program` heads the whole translation unit, lowered with -t wasm32.
void wasm_codegen(const Tac_TopLevel *program, const Tac_TopLevel *tl, FILE *out);

#ifdef __cplusplus
}
#endif

#endif // WASM_CODEGEN_H
