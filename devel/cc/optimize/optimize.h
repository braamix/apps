#pragma once
#include <stdbool.h>
#include <stdio.h>

#include "tac.h"

typedef struct {
    bool unreachable_elim; // --no-unreachable disables
    bool copy_propagation; // --no-copy-prop disables
    bool cse;              // --no-cse disables
    bool dead_store_elim;  // --no-dead-store disables
    bool loop_rotate;      // --no-loop-rotate disables
    bool ivsr;             // --no-ivsr disables induction-variable strength reduction
    bool debug;            // --opt-debug enables the optimizer trace
    int max_iterations;    // --opt-max-iter: rounds at most; 0 = until a fixed point
} OptFlags;

// A fixed point is always reached well before this many rounds; see optimize_function.
enum { OPT_MAX_ROUNDS = 64 };

OptFlags opt_flags_default(void);

// Process-global trace switch, modelled on translator_debug. The pass entry
// points do not take OptFlags (some are called directly by the test fixture), so
// the trace is gated by this global instead. optimize_function sets it from
// OptFlags.debug at entry; tests may set it directly. Trace output goes to stdout.
extern int optimize_debug;

// Gated trace print: emits to stdout only when optimize_debug is set.
#define OPT_TRACE(...)           \
    do {                         \
        if (optimize_debug)      \
            printf(__VA_ARGS__); \
    } while (0)

// Dump one instruction (via tac_print_instruction) under `prefix`, gated by
// optimize_debug. Used to show an instruction before/after a rewrite.
void opt_trace_instr(const char *prefix, const Tac_Instruction *ins);

// Returns the optimized body in place (modifies the list).
// Caller owns the result; caller freed the original list.
// `fn` is the function's own toplevel — its params and automatic locals let the
// CFG passes tell private locals from observable globals. Pass NULL when no such
// context is available (the optimizer then makes no global-vs-local distinction).
Tac_Instruction *optimize_function(Tac_Instruction *body, OptFlags flags, Tac_TopLevel *fn);

// Drop from fn's locals every name its body no longer mentions (temporaries and
// variables whose last use a pass removed), so params + locals stay exactly the
// typed symbols of the body.  Run after optimize_function.
void optimize_prune_locals(Tac_TopLevel *fn);
