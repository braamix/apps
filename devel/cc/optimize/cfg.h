#pragma once
#include <stdbool.h>

#include "tac.h"

// Control-flow graph over a function's TAC body. A basic block is a maximal
// run of instructions entered only at its first instruction and left only at
// its last; the three CFG-based passes (unreachable, copy-prop, dead-store)
// reason over this structure. See docs/TAC_Optimization.md §"Control-flow graphs".

// One basic block. The block owns the instruction sub-list from `first` to
// `last` (the list is severed at block boundaries during cfg_build and rejoined
// by cfg_flatten).
typedef struct OptBlock {
    int id;                  // index into OptCfg.blocks; block 0 is the entry
    Tac_Instruction *first;  // first instruction (NULL once block is emptied)
    Tac_Instruction *last;   // terminator / last instruction of the block
    struct OptBlock **succs; // successor blocks (edges out of this block)
    int nsucc;               // number of successors (0 = edge to Exit)
    bool reachable;          // set by unreachable-code elimination's traversal
} OptBlock;

typedef struct OptCfg {
    OptBlock **blocks; // blocks in original linear order; [0] is entry
    int nblocks;
} OptCfg;

// Build the CFG by splitting `body` into basic blocks and wiring edges. Consumes
// the list (re-links it block-by-block); recover a flat list with cfg_flatten.
OptCfg *cfg_build(Tac_Instruction *body);

// Concatenate the non-empty blocks back into one flat instruction list, in block
// order, and return its head (NULL if every block is empty).
Tac_Instruction *cfg_flatten(OptCfg *cfg);

// Free the CFG scaffolding (blocks and successor arrays). Does not free the
// instructions themselves — those are owned by the flattened list.
void cfg_free(OptCfg *cfg);
