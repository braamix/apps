#pragma once

#include <stdbool.h>

#include "cfg.h"
#include "string_map.h"
#include "tac.h"

// Backward liveness of names over a CFG (liveness.c).  A live set is a bit vector
// over the names of one function, numbered in strcmp order, so that opt_live_each
// visits them in the order a StringMap of them would.
typedef unsigned LiveWord;

typedef struct {
    int nnames, nwords;
    const char **names; // bit k is names[k]
    StringMap index;    // name -> bit + 1; owns the names
    LiveWord *observed; // the static and address-taken names
    LiveWord *in, *out; // per block, nwords words each
} OptLive;

// A set of none of lv's names, from xalloc.
LiveWord *opt_live_new(const OptLive *lv);
void opt_live_copy(const OptLive *lv, LiveWord *dst, const LiveWord *src);
bool opt_live_has(const OptLive *lv, const LiveWord *set, const char *name);
void opt_live_remove(const OptLive *lv, LiveWord *set, const char *name); // NULL: nothing
// Each name of a set, in strcmp order.
void opt_live_each(const OptLive *lv, const LiveWord *set, void (*f)(const char *name, void *arg),
                   void *arg);
// The variable an instruction defines, or NULL.  STORE and FUN_CALL define none here:
// a call's result is not killed (a conservative choice dead-store elimination relies on).
const char *opt_live_def(const Tac_Instruction *ins);
// The transfer of one instruction, walking backward: kill its definition, then make
// its operands live.  A call also makes live every static and address-taken name,
// which it may read.
void opt_live_transfer(const OptLive *lv, LiveWord *set, const Tac_Instruction *ins);
// Solve liveness over cfg, numbering its names.  The exit blocks' out-sets are seeded
// with the static and address-taken names.  With only_reachable, blocks not marked
// reachable are skipped.
void opt_live_solve(OptLive *lv, const OptCfg *cfg, const StringMap *static_names,
                    const StringMap *address_taken, bool only_reachable);
// The live set at the end of a block.
const LiveWord *opt_live_out(const OptLive *lv, int block);
void opt_live_free(OptLive *lv);
