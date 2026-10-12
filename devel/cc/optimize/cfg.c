// ============================================================================
// cfg.c — control-flow graph construction and flattening.
//
// A basic block starts at a Label (or the function entry) and ends at a Jump,
// conditional jump, or Return. Building the CFG is a single linear scan; the
// graph has an implicit Entry (block 0) and an implicit Exit (the target of
// every Return, represented as a block with zero successors). Edges:
//   - Jump(target)          → one edge, to the block beginning Label(target).
//   - JumpIfZero/NotZero     → two edges: the target block, and the fall-through
//                              (immediately following) block.
//   - Return                 → no successors (edge to Exit).
//   - any other terminator-less block → fall through to the next block.
//
// See docs/TAC_Optimization.md §"Control-flow graphs".
// ============================================================================

#include "cfg.h"

#include "optimize.h"
#include "string_map.h"
#include "xalloc.h"

// A "terminal" instruction ends a basic block: control leaves the block here.
static bool is_terminal(Tac_InstructionKind k)
{
    return k == TAC_INSTRUCTION_JUMP || k == TAC_INSTRUCTION_JUMP_IF_ZERO ||
           k == TAC_INSTRUCTION_JUMP_IF_NOT_ZERO || k == TAC_INSTRUCTION_RETURN ||
           k == TAC_INSTRUCTION_JUMP_TABLE;
}

// Count the basic blocks in `body` so cfg_build can size its array up front.
// A new block begins after any terminal instruction and at every Label.
static int count_blocks(const Tac_Instruction *body)
{
    int n              = 1;
    bool prev_terminal = is_terminal(body->kind);
    for (const Tac_Instruction *i = body->next; i; i = i->next) {
        if (prev_terminal || i->kind == TAC_INSTRUCTION_LABEL)
            n++;
        prev_terminal = is_terminal(i->kind);
    }
    return n;
}

// Build the CFG in two passes over the instruction list.
OptCfg *cfg_build(Tac_Instruction *body)
{
    int nblocks = count_blocks(body);
    OPT_TRACE("[cfg] block count: %d\n", nblocks);

    OptCfg *cfg  = xalloc(sizeof(OptCfg), __func__, __FILE__, __LINE__);
    cfg->nblocks = nblocks;
    cfg->blocks  = xalloc(nblocks * sizeof(OptBlock *), __func__, __FILE__, __LINE__);

    for (int i = 0; i < nblocks; i++) {
        OptBlock *b    = xalloc(sizeof(OptBlock), __func__, __FILE__, __LINE__);
        b->id          = i;
        b->first       = NULL;
        b->last        = NULL;
        b->succs       = NULL;
        b->nsucc       = 0;
        b->reachable   = false;
        cfg->blocks[i] = b;
    }

    // Pass 1: split the instruction list into blocks and record, for each
    // label, the id of the block it begins. The split severs the list at every
    // boundary (prev->next = NULL) so each block owns a self-contained sub-list.
    StringMap label_map;
    map_init(&label_map);

    int bid               = 0;
    cfg->blocks[0]->first = body;
    if (body->kind == TAC_INSTRUCTION_LABEL)
        map_insert(&label_map, body->u.label.name, (intptr_t)0, 0);

    Tac_Instruction *prev = body;
    for (Tac_Instruction *instr = body->next; instr; instr = instr->next) {
        // A boundary falls after a terminator and before a label.
        if (is_terminal(prev->kind) || instr->kind == TAC_INSTRUCTION_LABEL) {
            cfg->blocks[bid]->last = prev;
            prev->next             = NULL; // sever: end the current block's sub-list
            bid++;
            cfg->blocks[bid]->first = instr;
        }
        if (instr->kind == TAC_INSTRUCTION_LABEL)
            map_insert(&label_map, instr->u.label.name, (intptr_t)bid, 0);
        prev = instr;
    }
    cfg->blocks[bid]->last = prev;

    // Pass 2: wire successor edges from each block's terminator using label_map.
    for (int i = 0; i < nblocks; i++) {
        Tac_Instruction *term = cfg->blocks[i]->last;
        if (term->kind == TAC_INSTRUCTION_JUMP) {
            // Unconditional jump: single edge to the target label's block.
            intptr_t target_id;
            map_get(&label_map, term->u.jump.target, &target_id);
            cfg->blocks[i]->succs    = xalloc(sizeof(OptBlock *), __func__, __FILE__, __LINE__);
            cfg->blocks[i]->succs[0] = cfg->blocks[target_id];
            cfg->blocks[i]->nsucc    = 1;
            OPT_TRACE("[cfg] block %d -[jump]-> block %d\n", i, (int)target_id);
        } else if (term->kind == TAC_INSTRUCTION_JUMP_IF_ZERO ||
                   term->kind == TAC_INSTRUCTION_JUMP_IF_NOT_ZERO) {
            // Conditional jump: two edges — the branch target (condition met)
            // and the fall-through to the immediately following block.
            const char *target = (term->kind == TAC_INSTRUCTION_JUMP_IF_ZERO)
                                     ? term->u.jump_if_zero.target
                                     : term->u.jump_if_not_zero.target;
            intptr_t target_id;
            map_get(&label_map, target, &target_id);
            // The last block falls through to the Exit: a void function may end
            // in a loop's conditional jump, its trailing return removed.
            cfg->blocks[i]->succs    = xalloc(2 * sizeof(OptBlock *), __func__, __FILE__, __LINE__);
            cfg->blocks[i]->succs[0] = cfg->blocks[target_id];
            cfg->blocks[i]->nsucc    = 1;
            OPT_TRACE("[cfg] block %d -[cond-taken]-> block %d\n", i, (int)target_id);
            if (i + 1 < nblocks) {
                cfg->blocks[i]->succs[1] = cfg->blocks[i + 1];
                cfg->blocks[i]->nsucc    = 2;
                OPT_TRACE("[cfg] block %d -[cond-fallthru]-> block %d\n", i, i + 1);
            }
        } else if (term->kind == TAC_INSTRUCTION_JUMP_TABLE) {
            // A jump table: an edge to each distinct target, the default among them.
            int n                 = term->u.jump_table.count + 1;
            cfg->blocks[i]->succs = xalloc(n * sizeof(OptBlock *), __func__, __FILE__, __LINE__);
            cfg->blocks[i]->nsucc = 0;
            for (int k = 0; k < n; k++) {
                const char *t =
                    k < n - 1 ? term->u.jump_table.targets[k] : term->u.jump_table.default_target;
                intptr_t target_id;
                map_get(&label_map, t, &target_id);
                OptBlock *to = cfg->blocks[target_id];
                bool seen    = false;
                for (int j = 0; j < cfg->blocks[i]->nsucc; j++)
                    seen |= cfg->blocks[i]->succs[j] == to;
                if (!seen)
                    cfg->blocks[i]->succs[cfg->blocks[i]->nsucc++] = to;
            }
        } else if (term->kind == TAC_INSTRUCTION_RETURN) {
            // Return: no successors — this is an edge to the implicit Exit.
            cfg->blocks[i]->nsucc = 0;
            OPT_TRACE("[cfg] block %d -[return]-> exit\n", i);
        } else if (i + 1 < nblocks) {
            // No terminator (block ended only because a label followed):
            // fall through to the next block.
            cfg->blocks[i]->succs    = xalloc(sizeof(OptBlock *), __func__, __FILE__, __LINE__);
            cfg->blocks[i]->succs[0] = cfg->blocks[i + 1];
            cfg->blocks[i]->nsucc    = 1;
            OPT_TRACE("[cfg] block %d -[fallthru]-> block %d\n", i, i + 1);
        }
    }

    map_destroy(&label_map);
    return cfg;
}

// Rejoin the blocks into one flat list in block order, skipping blocks that
// passes have emptied (first == NULL). Re-links each block's `last` to the next
// non-empty block's `first`; returns the head, or NULL if everything is empty.
Tac_Instruction *cfg_flatten(OptCfg *cfg)
{
    Tac_Instruction *head      = NULL;
    Tac_Instruction *tail_last = NULL;

    for (int i = 0; i < cfg->nblocks; i++) {
        if (!cfg->blocks[i]->first) {
            OPT_TRACE("[cfg] flatten: block %d is empty, skipping\n", i);
            continue;
        }
        if (!head)
            head = cfg->blocks[i]->first;
        else
            tail_last->next = cfg->blocks[i]->first;
        tail_last = cfg->blocks[i]->last;
    }
    if (tail_last)
        tail_last->next = NULL;
    return head;
}

// Free the CFG scaffolding only. The instructions are not freed here; ownership
// has passed back to the list returned by cfg_flatten.
void cfg_free(OptCfg *cfg)
{
    for (int i = 0; i < cfg->nblocks; i++) {
        xfree(cfg->blocks[i]->succs);
        xfree(cfg->blocks[i]);
    }
    xfree(cfg->blocks);
    xfree(cfg);
}
