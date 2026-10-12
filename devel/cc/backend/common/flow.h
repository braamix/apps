#ifndef BACKEND_FLOW_H
#define BACKEND_FLOW_H

#include <stdbool.h>
#include <stdint.h>

#include "string_map.h"
#include "tac.h"

#ifdef __cplusplus
extern "C" {
#endif

//
// Control-flow graph and liveness over a function's TAC body, for code generators.
// The body is only read.  The variables tracked are the function's parameters and
// locals (temporaries included).  One whose address is taken, or that is an
// ALLOCATE_LOCAL object, is "in memory": a pointer may read or write it, so its
// liveness means nothing.  A member store (COPY_TO_OFFSET) counts as a use of the
// aggregate, not a definition, since the other members survive it.
//

// A set of variables, `words` 64-bit words long.
typedef uint64_t Flow_Set;

typedef struct {
    int first, last; // indices into Flow.instrs, inclusive
    int nsucc;
    int *succ;          // nsucc of them: two at most, but for a jump table
    Flow_Set *use;      // read before any write in the block
    Flow_Set *def;      // written in the block
    Flow_Set *live_in;  // live on entry
    Flow_Set *live_out; // live on exit
} Flow_Block;

typedef struct Flow {
    const Tac_TopLevel *fn;
    int ninstrs;
    const Tac_Instruction **instrs; // the body, in order
    int nvars;
    const char **names;     // variable index → name
    const Tac_Type **types; // variable index → type (may be NULL)
    StringMap index;        // name → variable index
    int words;              // words per set
    Flow_Set *in_memory;    // address taken, ALLOCATE_LOCAL or volatile
    Flow_Set *addressed;    // in memory but for ALLOCATE_LOCAL: address taken or volatile
    int nblocks;
    Flow_Block *blocks; // [0] is the entry
} Flow;

// Build the CFG of function `fn` and solve liveness.
Flow *flow_build(const Tac_TopLevel *fn);
void flow_free(Flow *f);

// The index of tracked variable `name`, or -1.
int flow_var(const Flow *f, const char *name);

// Call `fn` for each tracked variable instruction `in` reads, or writes.
typedef void (*Flow_Visitor)(int var, void *arg);
void flow_uses(const Flow *f, const Tac_Instruction *in, Flow_Visitor fn, void *arg);
void flow_defs(const Flow *f, const Tac_Instruction *in, Flow_Visitor fn, void *arg);

// Turn the set live after `in` into the set live before it.
void flow_step(const Flow *f, const Tac_Instruction *in, Flow_Set *live);

Flow_Set *flow_set_new(const Flow *f);
static inline bool flow_has(const Flow_Set *s, int i)
{
    return (s[i / 64] >> (i % 64)) & 1;
}
static inline void flow_add(Flow_Set *s, int i)
{
    s[i / 64] |= (uint64_t)1 << (i % 64);
}
static inline void flow_remove(Flow_Set *s, int i)
{
    s[i / 64] &= ~((uint64_t)1 << (i % 64));
}

#ifdef __cplusplus
}
#endif

#endif // BACKEND_FLOW_H
