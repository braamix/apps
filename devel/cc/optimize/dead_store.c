// ============================================================================
// dead_store.c — dead-store elimination via liveness analysis.
//
// A store is *dead* if it assigns a value that is never read before the variable
// is overwritten again or the function exits. Such stores have no observable
// effect and can be deleted. We find them with a backward dataflow analysis,
// "variable liveness":
//
//   - A variable is live at a point if some path from there reads it before any
//     redefinition.
//   - Lattice element: a set of live variable names.
//   - Initial value at Exit: the static-duration and address-taken variables
//     (they may be observed by the caller after we return); everything else dead.
//   - Meet at merge points: union — live if live on any outgoing path.
//   - Transfer for one instruction, applied backward:
//       kill def(i): the destination is not live just before the instruction;
//       gen use(i):  every operand the instruction reads becomes live.
//
// An instruction is a dead store when its destination is not in the live set
// *after* it — provided the instruction is pure (is_removable). Side-effecting
// or control-flow instructions are kept even when their dst is dead.
//
// Conservatism around aliasing (see alias.c): static-duration and address-taken
// variables are seeded live at Exit, and a FunCall re-livens them (the callee
// may read them) along with its arguments.
//
// See docs/TAC_Optimization.md §"Dead store elimination".
// ============================================================================

#include <stdbool.h>

#include "alias.h"
#include "cfg.h"
#include "liveness.h"
#include "optimize.h"
#include "string_map.h"
#include "tac.h"
#include "xalloc.h"

// ============================================================================
// is_removable: true for pure instructions, whose only effect is to define their
// destination — safe to delete when that destination is dead. Excluded (kept
// even when dst is dead): STORE and COPY_TO_OFFSET (write through a pointer /
// into a struct field — observable), FUN_CALL (arbitrary side effects), and all
// control-flow instructions. GET_ADDRESS, LOAD, ADD_PTR and COPY_FROM_OFFSET are
// pure reads and so are removable when their result is unused.
// ============================================================================

static bool is_removable(Tac_InstructionKind kind)
{
    switch (kind) {
    case TAC_INSTRUCTION_COPY:
    case TAC_INSTRUCTION_UNARY:
    case TAC_INSTRUCTION_BINARY:
    case TAC_INSTRUCTION_SIGN_EXTEND:
    case TAC_INSTRUCTION_TRUNCATE:
    case TAC_INSTRUCTION_ZERO_EXTEND:
    case TAC_INSTRUCTION_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_DOUBLE_TO_UINT:
    case TAC_INSTRUCTION_INT_TO_DOUBLE:
    case TAC_INSTRUCTION_UINT_TO_DOUBLE:
    case TAC_INSTRUCTION_FLOAT_TO_DOUBLE:
    case TAC_INSTRUCTION_DOUBLE_TO_FLOAT:
    case TAC_INSTRUCTION_INT_TO_FLOAT:
    case TAC_INSTRUCTION_UINT_TO_FLOAT:
    case TAC_INSTRUCTION_FLOAT_TO_INT:
    case TAC_INSTRUCTION_FLOAT_TO_UINT:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_UINT:
    case TAC_INSTRUCTION_INT_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_UINT_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_DOUBLE:
    case TAC_INSTRUCTION_DOUBLE_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_FLOAT:
    case TAC_INSTRUCTION_FLOAT_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_PTR_TO_CHAR_PTR:
    case TAC_INSTRUCTION_CHAR_PTR_TO_PTR:
    case TAC_INSTRUCTION_GET_ADDRESS:
    case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
    case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
    case TAC_INSTRUCTION_LOAD:
    case TAC_INSTRUCTION_LOAD_BYTE:
    case TAC_INSTRUCTION_ADD_PTR:
    case TAC_INSTRUCTION_PTR_DIFF:
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
        return true;
    default:
        return false;
    }
}

// ============================================================================
// eliminate_dead_stores: entry point. Runs alias pre-analysis, the backward
// liveness fixpoint, then the removal walk, and frees all scaffolding.
// ============================================================================

void eliminate_dead_stores(const OptCfg *cfg, const Tac_TopLevel *fn)
{
    if (cfg->nblocks == 0)
        return;

    // Stage 1: variables that must be treated as live across calls / at exit.
    StringMap static_names, address_taken;
    collect_alias_sets(cfg, fn, &static_names, &address_taken);

    // Stage 2: the backward liveness dataflow.
    int n               = cfg->nblocks;
    StringMap *in_sets  = xalloc(n * sizeof(StringMap), __func__, __FILE__, __LINE__);
    StringMap *out_sets = xalloc(n * sizeof(StringMap), __func__, __FILE__, __LINE__);
    opt_live_solve(cfg, &static_names, &address_taken, true, in_sets, out_sets);

    // Dead store removal: walk each block backward, pruning instructions whose
    // defined variable is dead on exit and which have no observable side-effects.
    // Backward order lets a single pass cascade: when a dead instruction is
    // removed its sources are not added to live, making earlier defs candidates too.
    for (int i = 0; i < n; i++) {
        OptBlock *b = cfg->blocks[i];
        int nins    = 0;
        for (const Tac_Instruction *ins = b->first; ins; ins = ins->next)
            nins++;
        if (!b->reachable || nins == 0)
            continue;
        Tac_Instruction **insts =
            xalloc(nins * sizeof(Tac_Instruction *), __func__, __FILE__, __LINE__);
        int k = 0;
        for (Tac_Instruction *ins = b->first; ins; ins = ins->next)
            insts[k++] = ins;

        StringMap live;
        map_init(&live);
        opt_live_copy(&live, &out_sets[i]);

        for (int j = nins - 1; j >= 0; j--) {
            Tac_Instruction *ins = insts[j];
            const char *dst      = opt_live_def(ins);
            // Dead store: defines a variable that is not live afterward, and is a
            // pure instruction we may drop. Unlink and free it (and do NOT run
            // the transfer, so its sources are not revived — that is what lets
            // chains of dead defs collapse in this single backward pass).
            if (dst && is_removable(ins->kind) && !ins->is_volatile && !map_get(&live, dst, NULL)) {
                OPT_TRACE("[dead-store] block %d: dst '%s' is dead", i, dst);
                opt_trace_instr(" removing:", ins);
                Tac_Instruction *prev = (j > 0) ? insts[j - 1] : NULL;
                if (prev)
                    prev->next = ins->next;
                else
                    b->first = ins->next;
                if (b->last == ins)
                    b->last = prev;
                ins->next = NULL;
                tac_free_instruction(ins);
            } else {
                opt_live_transfer(&live, ins, &static_names, &address_taken);
            }
        }
        map_destroy(&live);
        xfree(insts);
    }

    // Free all dataflow scaffolding and the alias maps.
    opt_live_free(n, in_sets, out_sets);
    xfree(in_sets);
    xfree(out_sets);
    map_destroy(&static_names);
    map_destroy(&address_taken);
}
