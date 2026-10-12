#ifndef BACKEND_REGALLOC_H
#define BACKEND_REGALLOC_H

#include <stdbool.h>

#include "flow.h"
#include "string_map.h"
#include "tac.h"

#ifdef __cplusplus
extern "C" {
#endif

//
// Register allocation over TAC variables, for code generators: graph colouring with
// conservative (Briggs) coalescing of copies, and optimistic spilling.  A candidate is
// a scalar parameter or local that is never in memory, nor named as an aggregate by a
// member access.  Each candidate is of one register class, integer or FP, whose pool
// is the class's argument registers followed by its callee-saved registers.  A value
// live across a call takes a callee-saved one; any other may take an argument
// register too, first.  A parameter prefers the register it arrives in, a call
// argument its argument register, a returned value or a call's result the result
// register.  A parameter dead on entry (unused, or written before it is read) takes
// no register from the others then.  A spilled candidate stays in its frame slot.
//
// An aggregate is a candidate only where the target says so (aggregate_ok): a struct
// of two words, say, held in a pair of registers.
//
// A value two integer registers wide (long long on a 32-bit target) has its high word
// as a node of its own (flow variable v + n), interfering with all the low word does
// and with the low word.  A copy of one is coalesced as a pair, low word with low and
// high with high, or not at all.  It gets registers only when both halves do.
//
// The target describes itself with a RegAlloc_Target; `arg` is passed to every hook.
//

typedef enum {
    REGALLOC_NONE, // never in a register
    REGALLOC_INT,  // one integer register
    REGALLOC_FP,   // one FP register
    REGALLOC_PAIR, // two integer registers
} RegAlloc_Class;

typedef struct {
    const int *int_pool; // argument registers, then callee-saved
    int nint, int_narg;
    const int *fp_pool;
    int nfp, fp_narg;
    int ret_int, ret_int_hi, ret_fp; // where a returned value comes back
    void *arg;
    // The class of a variable of type `t`.
    RegAlloc_Class (*classify)(void *arg, const Tac_Type *t);
    // Whether `in`, not a FUN_CALL, calls the runtime (clobbering the argument
    // registers); its result in *res.
    bool (*runtime_call)(void *arg, const Flow *f, const Tac_Instruction *in, const Tac_Val **res);
    // The incoming register of each parameter passed in a register of its own class,
    // by name; of a pair's high word in `hints_hi`.
    void (*param_hints)(void *arg, StringMap *hints, StringMap *hints_hi);
    // Hints for a call: each argument variable its argument register, the result the
    // result register; only where hint[var] is still 0.  A pair's high word at
    // hint[var + f->nvars].
    void (*call_hints)(void *arg, const Flow *f, const Tac_Instruction *in, int *hint);
    // Variable `name` gets register `reg`, and its high word `hi` (else 0).
    void (*assign)(void *arg, const char *name, int reg, int hi);
    // Optional: whether FUN_CALL `in` is expanded in place (a stack builtin), so it
    // clobbers nothing and takes no argument registers.
    bool (*inline_call)(void *arg, const Tac_Instruction *in);
    // Optional: whether aggregate variable `v`, of class REGALLOC_PAIR, may be a
    // candidate: then neither its declaration nor its member accesses keep it in
    // memory, only its address taken.  Without this hook no aggregate is a candidate.
    bool (*aggregate_ok)(void *arg, const Flow *f, int v);
    // Optional: parameter `name`, just assigned, is dead on entry.  Its register may
    // then hold another parameter on entry, so its incoming value must not be moved or
    // loaded there.  Without this hook every parameter interferes with what is live on
    // entry, as if its value were kept.
    void (*dead_param)(void *arg, const char *name);
} RegAlloc_Target;

// Allocate registers for the variables of function `fn`.
void regalloc(const RegAlloc_Target *t, const Tac_TopLevel *fn);

#ifdef __cplusplus
}
#endif

#endif // BACKEND_REGALLOC_H
