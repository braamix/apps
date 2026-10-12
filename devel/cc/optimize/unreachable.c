// ============================================================================
// unreachable.c — unreachable-code elimination.
//
// Constant folding may have turned conditional jumps into unconditional ones,
// leaving some blocks with no path from the entry. This pass removes them:
//
//   1. Mark every block reachable from the entry by a breadth-first traversal
//      of the CFG's successor edges.
//   2. Free the instructions of every unmarked (unreachable) block.
//   3. Two cleanups tighten the result:
//        - remove_useless_jumps:  a Jump to the immediately following block is a
//          no-op once intervening dead blocks are gone — delete it.
//        - remove_unused_labels:  a Label that is no longer any jump's target
//          (and is not the entry label) is removed for readability.
//
// A common beneficiary is dead code following an unconditional `return`/`jump`:
// when a block already ends in a return, any straight-line instructions after it
// are unreachable and are removed here automatically, with no special case anywhere.
//
// See docs/TAC_Optimization.md §"Unreachable code elimination".
// ============================================================================

#include <string.h>

#include "cfg.h"
#include "optimize.h"
#include "string_map.h"
#include "xalloc.h"

// Cleanup 1: delete every reachable block's trailing Jump when its target is the
// next reachable block (the jump would just fall through anyway). Intervening
// unreachable blocks are skipped when locating that "next" block, so jumps that
// only existed to hop over now-removed code become useless and are dropped.
static void remove_useless_jumps(OptCfg *cfg)
{
    for (int i = 0; i < cfg->nblocks; i++) {
        OptBlock *b = cfg->blocks[i];
        if (!b->reachable || !b->last)
            continue;
        if (b->last->kind != TAC_INSTRUCTION_JUMP)
            continue;
        const char *target = b->last->u.jump.target;

        int next = i + 1;
        while (next < cfg->nblocks && !cfg->blocks[next]->reachable)
            next++;
        if (next >= cfg->nblocks || !cfg->blocks[next]->first)
            continue;
        const OptBlock *nb = cfg->blocks[next];
        if (nb->first->kind != TAC_INSTRUCTION_LABEL)
            continue;
        if (strcmp(nb->first->u.label.name, target) != 0)
            continue;

        // Unlink the trailing Jump. If it was the block's only instruction the
        // block becomes empty; otherwise walk to its predecessor and re-terminate.
        OPT_TRACE("[unreach] block %d: dropping useless jump to %s\n", i, target);
        Tac_Instruction *jmp = b->last;
        if (b->first == b->last) {
            b->first = b->last = NULL;
        } else {
            Tac_Instruction *prev = b->first;
            while (prev->next != jmp)
                prev = prev->next;
            prev->next = NULL;
            b->last    = prev;
        }
        jmp->next = NULL;
        tac_free_instruction(jmp);
    }
}

// Cleanup 2: remove Labels that no surviving jump targets. Labels emit no
// machine code, so this does not change code size or speed — it only makes the
// instruction stream easier to read and debug.
static void remove_unused_labels(OptCfg *cfg)
{
    // First gather the set of all label names still referenced by some jump in a
    // reachable block.
    StringMap targets;
    map_init(&targets);
    for (int i = 0; i < cfg->nblocks; i++) {
        OptBlock *b = cfg->blocks[i];
        if (!b->reachable)
            continue;
        for (Tac_Instruction *ins = b->first; ins; ins = ins->next) {
            const char *t = NULL;
            if (ins->kind == TAC_INSTRUCTION_JUMP)
                t = ins->u.jump.target;
            else if (ins->kind == TAC_INSTRUCTION_JUMP_IF_ZERO)
                t = ins->u.jump_if_zero.target;
            else if (ins->kind == TAC_INSTRUCTION_JUMP_IF_NOT_ZERO)
                t = ins->u.jump_if_not_zero.target;
            else if (ins->kind == TAC_INSTRUCTION_JUMP_TABLE) {
                for (int k = 0; k < ins->u.jump_table.count; k++)
                    map_insert(&targets, ins->u.jump_table.targets[k], 1, 0);
                t = ins->u.jump_table.default_target;
            }
            if (t) {
                intptr_t dummy;
                if (!map_get(&targets, t, &dummy))
                    map_insert(&targets, t, 1, 0);
            }
        }
    }

    // The function's entry label is always kept even if nothing jumps to it.
    const Tac_Instruction *entry = cfg->blocks[0]->first;

    // Drop the leading Label of any reachable block whose name is not a target.
    for (int i = 0; i < cfg->nblocks; i++) {
        OptBlock *b = cfg->blocks[i];
        if (!b->reachable || !b->first)
            continue;
        if (b->first->kind != TAC_INSTRUCTION_LABEL)
            continue;
        Tac_Instruction *lbl = b->first;
        if (lbl == entry)
            continue;
        intptr_t dummy;
        if (map_get(&targets, lbl->u.label.name, &dummy))
            continue;

        OPT_TRACE("[unreach] block %d: dropping unused label %s\n", i, lbl->u.label.name);
        Tac_Instruction *new_first = lbl->next;
        lbl->next                  = NULL;
        tac_free_instruction(lbl);
        b->first = new_first;
        if (new_first == NULL)
            b->last = NULL;
    }

    map_destroy(&targets);
}

// Entry point: mark reachable blocks (BFS from the entry), free the rest, then
// run the two cleanups.
void eliminate_unreachable(OptCfg *cfg)
{
    // Breadth-first traversal from the entry block, using a simple ring-free
    // queue sized to the block count (each block is enqueued at most once).
    OptBlock **queue = xalloc(cfg->nblocks * sizeof(OptBlock *), __func__, __FILE__, __LINE__);
    int head = 0, tail = 0;

    cfg->blocks[0]->reachable = true;
    queue[tail++]             = cfg->blocks[0];

    while (head < tail) {
        OptBlock *b = queue[head++];
        OPT_TRACE("[unreach] reachable: block %d\n", b->id);
        for (int i = 0; i < b->nsucc; i++) {
            OptBlock *succ = b->succs[i];
            if (!succ->reachable) {
                succ->reachable = true;
                queue[tail++]   = succ;
            }
        }
    }
    xfree(queue);

    // Free the instructions of every block that was never marked reachable.
    // (tac_free_instruction follows ->next, freeing the whole block sub-list,
    // which is safe because cfg_build severed the list at block boundaries.)
    for (int i = 0; i < cfg->nblocks; i++) {
        OptBlock *b = cfg->blocks[i];
        if (!b->reachable) {
            OPT_TRACE("[unreach] freeing unreachable block %d\n", i);
            tac_free_instruction(b->first);
            b->first = NULL;
            b->last  = NULL;
        }
    }

    remove_useless_jumps(cfg);
    remove_unused_labels(cfg);
}
