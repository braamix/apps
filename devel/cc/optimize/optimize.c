// ============================================================================
// optimize.c — the machine-independent TAC optimization pipeline.
//
// No single pass is sufficient on its own; the five passes form a virtuous
// cycle and amplify one another:
//
//   - Constant folding produces constants that copy propagation can substitute
//     into expressions, which constant folding can then evaluate again.
//   - Constant folding turns conditional jumps into unconditional ones, creating
//     unreachable blocks that unreachable-code elimination can remove.
//   - Common-subexpression elimination turns a recomputation into a copy,
//     which copy propagation forwards and dead-store elimination removes.
//   - Copy propagation eliminates the variable in a copy's destination, turning
//     the copy into a dead store that dead-store elimination can remove.
//   - Dead-store elimination removes instructions, which may make previously
//     reachable blocks empty, which unreachable-code elimination can clean up.
//
// Because the passes feed each other, the optimizer runs them in a loop until
// the instruction list stops changing (a fixed point). Within one iteration the
// pass order is fixed: constant folding runs first — it is the only pass that
// works on the flat instruction list and needs no CFG — and the remaining four
// run on the CFG in the order unreachable → cse → copy-prop → dead-store, so
// each can exploit what the previous one produced in the same iteration.
//
// See docs/TAC_Optimization.md §"The optimization pipeline".
// ============================================================================

#include "optimize.h"

#include <stdlib.h>
#include <string.h>

#include "cfg.h"
#include "srcloc.h"
#include "string_map.h"
#include "target.h"
#include "xalloc.h"

// Pass entry points, implemented in the sibling translation units.
Tac_Instruction *constant_fold(Tac_Instruction *body);
Tac_Instruction *constant_fold_typed(Tac_Instruction *body, const Tac_TopLevel *fn);
void eliminate_unreachable(OptCfg *cfg);
void propagate_copies(OptCfg *cfg, const Tac_TopLevel *fn);
void eliminate_common_subexpressions(OptCfg *cfg, const Tac_TopLevel *fn);
void eliminate_dead_stores(const OptCfg *cfg, const Tac_TopLevel *fn);
bool reduce_induction_variables(OptCfg *cfg, Tac_TopLevel *fn);

_Noreturn void fatal_error(const char *fmt, ...);

// Process-global trace switch (see optimize.h). Default off.
int optimize_debug;

// Print one instruction under `prefix`, gated by optimize_debug. tac_print_instruction
// emits its own trailing newline, so the line reads "<prefix> <instruction>".
void opt_trace_instr(const char *prefix, const Tac_Instruction *ins)
{
    if (!optimize_debug)
        return;
    printf("%s ", prefix);
    if (ins)
        tac_print_instruction(stdout, ins, 0);
    else
        printf("(null)\n");
}

// Default flags: every CLI-toggleable pass is enabled, tracing off. Constant
// folding has no flag — it always runs, to keep the downstream code generators
// simpler.
OptFlags opt_flags_default(void)
{
    return (OptFlags){
        .unreachable_elim = true,
        .copy_propagation = true,
        .cse              = true,
        .dead_store_elim  = true,
        .loop_rotate      = true,
        .ivsr             = true,
        .debug            = false,
        .max_iterations   = 0,
    };
}

// The instruction list spelled as YAML: the fixed-point test compares the
// spelling before and after a round. The result is xalloc'ed.
static char *snapshot(const Tac_Instruction *body)
{
    char *buf  = NULL;
    size_t len = 0;
    FILE *f    = open_memstream(&buf, &len);
    if (!f)
        internal_error("optimizer: open_memstream failed");
    tac_export_yaml_instruction_list(f, body, 0);
    fclose(f);
    char *copy = xstrdup(buf);
    free(buf);
    return copy;
}

// Run the pipeline on one function body to a fixed point and return the
// optimized list. The body is transformed in place across iterations; the
// caller owns the returned list. Each TAC_TOPLEVEL_FUNCTION is optimized
// independently (intraprocedural); `fn` is the function's own toplevel, supplying
// its params + locals so the CFG passes can tell private locals from globals.
Tac_Instruction *optimize_function(Tac_Instruction *body, OptFlags flags, Tac_TopLevel *fn)
{
    if (!body)
        return NULL;

    optimize_debug = flags.debug ? 1 : 0;

    int iter = 0;
    for (;;) {
        iter++;
        OPT_TRACE("[optimize] iteration %d\n", iter);

        char *before = snapshot(body);

        // Constant folding first, on the flat list (no CFG required).
        OPT_TRACE("[optimize] running pass: const-fold\n");
        body = constant_fold_typed(body, fn);

        // Split into basic blocks for the three CFG-based passes.
        OptCfg *cfg = cfg_build(body);
        OPT_TRACE("[optimize] cfg built: %d blocks\n", cfg->nblocks);

        if (flags.unreachable_elim) {
            OPT_TRACE("[optimize] running pass: unreachable-elim\n");
            eliminate_unreachable(cfg);
        } else {
            OPT_TRACE("[optimize] pass unreachable-elim: skipped (disabled)\n");
        }
        if (flags.cse) {
            OPT_TRACE("[optimize] running pass: cse\n");
            eliminate_common_subexpressions(cfg, fn);
        } else {
            OPT_TRACE("[optimize] pass cse: skipped (disabled)\n");
        }
        if (flags.copy_propagation) {
            OPT_TRACE("[optimize] running pass: copy-prop\n");
            propagate_copies(cfg, fn);
        } else {
            OPT_TRACE("[optimize] pass copy-prop: skipped (disabled)\n");
        }
        if (flags.dead_store_elim) {
            OPT_TRACE("[optimize] running pass: dead-store-elim\n");
            eliminate_dead_stores(cfg, fn);
        } else {
            OPT_TRACE("[optimize] pass dead-store-elim: skipped (disabled)\n");
        }

        // Rejoin the (possibly modified) blocks into a flat list.
        Tac_Instruction *new_body = cfg_flatten(cfg);
        cfg_free(cfg);

        // An empty result is also a terminal condition: nothing left to iterate.
        if (!new_body) {
            xfree(before);
            OPT_TRACE("[optimize] converged (empty body) after %d iteration(s)\n", iter);
            return new_body;
        }

        // Fixed point: the passes rewrite the list in place, so the round is
        // compared against a snapshot of the list taken before it.
        char *after    = snapshot(new_body);
        bool unchanged = strcmp(before, after) == 0;
        xfree(before);
        xfree(after);
        // At the fixed point of the scalar passes, the loop optimizations run on code
        // they have simplified as far as it goes: a loop bound, say, is invariant only
        // once CSE and copy propagation have found its one computation. When they
        // change something, the scalar passes run again. They are off where the target
        // opts out of them (BESM-6).
        if (unchanged && flags.ivsr && !target_config->no_loop_opt) {
            OPT_TRACE("[optimize] running pass: ivsr\n");
            OptCfg *lcfg = cfg_build(new_body);
            unchanged    = !reduce_induction_variables(lcfg, fn);
            new_body     = cfg_flatten(lcfg);
            cfg_free(lcfg);
        }
        if (unchanged) {
            OPT_TRACE("[optimize] fixed point reached after %d iteration(s)\n", iter);
            return new_body;
        }
        if (flags.max_iterations > 0 && iter >= flags.max_iterations) {
            OPT_TRACE("[optimize] stopped after %d iteration(s)\n", iter);
            return new_body;
        }
        if (iter >= OPT_MAX_ROUNDS) {
            // Every pass only ever removes or simplifies, so the passes cannot
            // undo each other forever: running into the cap means a bug.
#ifndef NDEBUG
            internal_error("optimizer: %s does not converge in %d rounds",
                           fn ? fn->u.function.name : "?", OPT_MAX_ROUNDS);
#else
            OPT_TRACE("[optimize] no fixed point after %d rounds; stopping\n", iter);
            return new_body;
#endif
        }
        OPT_TRACE("[optimize] body changed; iterating\n");
        body = new_body;
    }
}

static void note_used(const char *name, void *arg)
{
    map_insert((StringMap *)arg, name, 1, 0);
}

void optimize_prune_locals(Tac_TopLevel *fn)
{
    StringMap used;
    map_init(&used);
    for (const Tac_Instruction *in = fn->u.function.body; in; in = in->next)
        tac_visit_names(in, note_used, &used);

    Tac_Param **pp = &fn->u.function.locals;
    while (*pp) {
        Tac_Param *p = *pp;
        if (map_get(&used, p->name, NULL)) {
            pp = &p->next;
            continue;
        }
        *pp     = p->next;
        p->next = NULL;
        tac_free_param(p);
    }
    map_destroy(&used);
}
