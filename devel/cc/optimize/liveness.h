#pragma once
#include <stdbool.h>

#include "cfg.h"
#include "string_map.h"
#include "tac.h"

// Backward liveness of names over a CFG (liveness.c).  A live set is a StringMap
// whose keys are the live names; its values are unused.

void opt_live_add(StringMap *ls, const char *name);
void opt_live_add_val(StringMap *ls, const Tac_Val *v);
void opt_live_remove(StringMap *ls, const char *name);
void opt_live_copy(StringMap *dst, const StringMap *src);
void opt_live_union(StringMap *result, const StringMap *other);
bool opt_live_equal(const StringMap *a, const StringMap *b);

// The variable an instruction defines, or NULL.  STORE and FUN_CALL define none here:
// a call's result is not killed (a conservative choice dead-store elimination relies on).
const char *opt_live_def(const Tac_Instruction *ins);

// The transfer of one instruction, walking backward: kill its definition, then make
// its operands live.  A call also makes live every static and address-taken name,
// which it may read.
void opt_live_transfer(StringMap *ls, const Tac_Instruction *ins, const StringMap *static_names,
                       const StringMap *address_taken);

// Solve liveness: in[] and out[] (cfg->nblocks entries each) are initialized here.
// The exit blocks' out-sets are seeded with the static and address-taken names.  With
// only_reachable, blocks not marked reachable are skipped.
void opt_live_solve(const OptCfg *cfg, const StringMap *static_names,
                    const StringMap *address_taken, bool only_reachable, StringMap *in_sets,
                    StringMap *out_sets);
void opt_live_free(int n, StringMap *in_sets, StringMap *out_sets);
