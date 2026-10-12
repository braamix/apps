#pragma once
#include <stdint.h>

#include "cfg.h"
#include "string_map.h"
#include "tac.h"

// Scaffolding shared by the forward dataflow passes (copy propagation, common-
// subexpression elimination): deferred key removal, predecessor lists, and the
// destination an instruction defines.

// A growable list of keys to delete *after* iterating. StringMap is an AVL tree;
// removing nodes from it while map_iterate is walking it would corrupt the
// traversal. So the kill/intersect callbacks only *collect* keys here, and
// keybuf_flush performs the actual removals once iteration has finished.
typedef struct {
    char **keys;
    int count;
    int cap;
} KeyBuf;

void keybuf_push(KeyBuf *kb, char *key);

// Remove all collected keys from `map` and release each removed value with
// `free_value`. A key may point into its own value (freed last), since the map
// keeps its own copy of the key string.
void keybuf_flush(KeyBuf *kb, StringMap *map, void (*free_value)(intptr_t));

// Predecessor lists, built by inverting the CFG's successor edges: block i has
// npreds[i] predecessors, the block ids preds[i][0 .. npreds[i]-1].
typedef struct {
    int n;
    int *npreds;
    int **preds;
} OptPreds;

void opt_preds_build(OptPreds *p, const OptCfg *cfg);
void opt_preds_free(OptPreds *p);

// The dst operand of an instruction that defines a value: the arithmetic and
// conversion kinds, address and pointer operations, LOAD and COPY_FROM_OFFSET.
// COPY, FUN_CALL and the stores are left to the caller, which handles them
// specially; NULL for everything else.
const Tac_Val *opt_defining_dst(const Tac_Instruction *ins);
