// ============================================================================
// liveness.c — backward liveness of frame-resident names over a CFG, shared by
// dead-store elimination (dead_store.c) and the coroutine split pass
// (translator/coro.c).  See dead_store.c for the analysis itself.
// ============================================================================

#include "liveness.h"

#include <stdbool.h>
#include <string.h>

#include "optimize.h"
#include "srcloc.h"
#include "xalloc.h"

// ============================================================================
// Live sets are bit vectors over the names of one function. Every name the
// transfer can touch is numbered once, in strcmp order, so that walking a set's
// bits visits the names in the order a StringMap of them would. The index owns
// the names, so a set stays valid across the instruction frees the removal walk
// does.
// ============================================================================

#define WORD_BITS (8 * (int)sizeof(LiveWord))

static int bit_of(const OptLive *lv, const char *name)
{
    intptr_t v;
    if (!map_get((StringMap *)&lv->index, name, &v))
        internal_error("liveness: '%s' is not among the function's names", name);
    return (int)v - 1;
}

static void set_bit(LiveWord *set, int k)
{
    set[k / WORD_BITS] |= (LiveWord)1 << (k % WORD_BITS);
}

LiveWord *opt_live_new(const OptLive *lv)
{
    LiveWord *set = xalloc((lv->nwords + 1) * sizeof(LiveWord), __func__, __FILE__, __LINE__);
    memset(set, 0, (lv->nwords + 1) * sizeof(LiveWord));
    return set;
}

void opt_live_copy(const OptLive *lv, LiveWord *dst, const LiveWord *src)
{
    memcpy(dst, src, lv->nwords * sizeof(LiveWord));
}

bool opt_live_has(const OptLive *lv, const LiveWord *set, const char *name)
{
    int k = bit_of(lv, name);
    return (set[k / WORD_BITS] >> (k % WORD_BITS)) & 1;
}

void opt_live_remove(const OptLive *lv, LiveWord *set, const char *name)
{
    if (!name)
        return;
    int k = bit_of(lv, name);
    set[k / WORD_BITS] &= ~((LiveWord)1 << (k % WORD_BITS));
}

void opt_live_each(const OptLive *lv, const LiveWord *set, void (*f)(const char *name, void *arg),
                   void *arg)
{
    for (int w = 0; w < lv->nwords; w++)
        for (LiveWord bits = set[w]; bits; bits &= bits - 1) {
            int k = w * WORD_BITS;
            for (LiveWord b = bits & -bits; b > 1; b >>= 1)
                k++;
            f(lv->names[k], arg);
        }
}

static void add_name(const OptLive *lv, LiveWord *set, const char *name)
{
    if (name)
        set_bit(set, bit_of(lv, name));
}

// ============================================================================
// opt_live_def: variable defined by this instruction (or NULL).
// STORE and FUN_CALL are side-effecting: not killed.
// COPY_TO_OFFSET.dst is char* (direct name, not Tac_Val*).
// ============================================================================

const char *opt_live_def(const Tac_Instruction *ins)
{
    switch (ins->kind) {
    case TAC_INSTRUCTION_COPY:
        if (ins->u.copy.dst->kind == TAC_VAL_VAR)
            return ins->u.copy.dst->u.var_name;
        return NULL;
    case TAC_INSTRUCTION_UNARY:
        if (ins->u.unary.dst->kind == TAC_VAL_VAR)
            return ins->u.unary.dst->u.var_name;
        return NULL;
    case TAC_INSTRUCTION_BINARY:
        if (ins->u.binary.dst->kind == TAC_VAL_VAR)
            return ins->u.binary.dst->u.var_name;
        return NULL;
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
        if (ins->u.sign_extend.dst->kind == TAC_VAL_VAR)
            return ins->u.sign_extend.dst->u.var_name;
        return NULL;
    case TAC_INSTRUCTION_GET_ADDRESS:
    case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
    case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
        if (ins->u.get_address.dst->kind == TAC_VAL_VAR)
            return ins->u.get_address.dst->u.var_name;
        return NULL;
    case TAC_INSTRUCTION_LOAD:
    case TAC_INSTRUCTION_LOAD_BYTE:
        if (ins->u.load.dst->kind == TAC_VAL_VAR)
            return ins->u.load.dst->u.var_name;
        return NULL;
    case TAC_INSTRUCTION_ADD_PTR:
        if (ins->u.add_ptr.dst->kind == TAC_VAL_VAR)
            return ins->u.add_ptr.dst->u.var_name;
        return NULL;
    case TAC_INSTRUCTION_PTR_DIFF:
        if (ins->u.ptr_diff.dst->kind == TAC_VAL_VAR)
            return ins->u.ptr_diff.dst->u.var_name;
        return NULL;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
        if (ins->u.copy_from_offset.dst->kind == TAC_VAL_VAR)
            return ins->u.copy_from_offset.dst->u.var_name;
        return NULL;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET:
        return ins->u.copy_to_offset.dst; // char* directly
    default:
        return NULL;
    }
}

// ============================================================================
// The names an instruction reads, each handed to `use`; a call's are its
// arguments and, called through a pointer, the pointer.
// ============================================================================

typedef void (*UseFn)(const OptLive *lv, LiveWord *set, const char *name);

static void use_val(UseFn use, const OptLive *lv, LiveWord *set, const Tac_Val *v)
{
    if (v && v->kind == TAC_VAL_VAR)
        use(lv, set, v->u.var_name);
}

static void uses(const Tac_Instruction *ins, UseFn use, const OptLive *lv, LiveWord *set)
{
    switch (ins->kind) {
    case TAC_INSTRUCTION_FUN_CALL:
    case TAC_INSTRUCTION_FUN_CALL_NORETURN:
        for (const Tac_Val *a = ins->u.fun_call.args; a; a = a->next)
            use_val(use, lv, set, a);
        if (ins->u.fun_call.indirect)
            use(lv, set, ins->u.fun_call.fun_name); // the callee is read out of this var
        break;
    case TAC_INSTRUCTION_RETURN:
        use_val(use, lv, set, ins->u.return_.src);
        break;
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
        use_val(use, lv, set, ins->u.sign_extend.src);
        break;
    case TAC_INSTRUCTION_UNARY:
        use_val(use, lv, set, ins->u.unary.src);
        break;
    case TAC_INSTRUCTION_BINARY:
        use_val(use, lv, set, ins->u.binary.src1);
        use_val(use, lv, set, ins->u.binary.src2);
        break;
    case TAC_INSTRUCTION_COPY:
        use_val(use, lv, set, ins->u.copy.src);
        break;
    case TAC_INSTRUCTION_GET_ADDRESS:
    case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
    case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
        use_val(use, lv, set, ins->u.get_address.src);
        break;
    case TAC_INSTRUCTION_LOAD:
    case TAC_INSTRUCTION_LOAD_BYTE:
        use_val(use, lv, set, ins->u.load.src_ptr);
        break;
    case TAC_INSTRUCTION_STORE:
    case TAC_INSTRUCTION_STORE_BYTE:
        use_val(use, lv, set, ins->u.store.src);
        use_val(use, lv, set, ins->u.store.dst_ptr);
        break;
    case TAC_INSTRUCTION_ADD_PTR:
        use_val(use, lv, set, ins->u.add_ptr.ptr);
        use_val(use, lv, set, ins->u.add_ptr.index);
        break;
    case TAC_INSTRUCTION_PTR_DIFF:
        use_val(use, lv, set, ins->u.ptr_diff.ptr_a);
        use_val(use, lv, set, ins->u.ptr_diff.ptr_b);
        break;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET:
        use_val(use, lv, set, ins->u.copy_to_offset.src);
        break;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
        if (ins->u.copy_from_offset.src)
            use(lv, set, ins->u.copy_from_offset.src); // char*, not Tac_Val*
        break;
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
        use_val(use, lv, set, ins->u.jump_if_zero.condition);
        break;
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
        use_val(use, lv, set, ins->u.jump_if_not_zero.condition);
        break;
    case TAC_INSTRUCTION_JUMP_TABLE:
        use_val(use, lv, set, ins->u.jump_table.index);
        break;
    default:
        break;
    }
}

// ============================================================================
// opt_live_transfer: one instruction, backward. A call makes live every static
// and address-taken name, which the callee may read, and kills nothing.
// ============================================================================

void opt_live_transfer(const OptLive *lv, LiveWord *set, const Tac_Instruction *ins)
{
    if (ins->kind == TAC_INSTRUCTION_FUN_CALL || ins->kind == TAC_INSTRUCTION_FUN_CALL_NORETURN) {
        for (int w = 0; w < lv->nwords; w++)
            set[w] |= lv->observed[w];
    } else {
        opt_live_remove(lv, set, opt_live_def(ins));
    }
    uses(ins, add_name, lv, set);
}

// ============================================================================
// The function's names: every name an instruction defines or reads, a call's
// result, and the static and address-taken names; numbered in strcmp order.
// ============================================================================

static void note(const OptLive *lv, LiveWord *set, const char *name)
{
    (void)set;
    if (name)
        map_insert((StringMap *)&lv->index, name, 0, 0);
}

static void note_key(const char *key, intptr_t value, const void *arg)
{
    (void)value;
    note(arg, NULL, key);
}

static void count_key(const char *key, intptr_t value, const void *arg)
{
    (void)key, (void)value;
    ((OptLive *)arg)->nnames++;
}

static void name_key(const char *key, intptr_t value, const void *arg)
{
    (void)value;
    OptLive *lv             = (OptLive *)arg;
    lv->names[lv->nnames++] = key;
}

static void observe(const char *key, intptr_t value, const void *arg)
{
    (void)value;
    const OptLive *lv = arg;
    add_name(lv, lv->observed, key);
}

static void number_names(OptLive *lv, const OptCfg *cfg, const StringMap *static_names,
                         const StringMap *address_taken)
{
    map_init(&lv->index);
    for (int i = 0; i < cfg->nblocks; i++)
        for (const Tac_Instruction *ins = cfg->blocks[i]->first; ins; ins = ins->next) {
            note(lv, NULL, opt_live_def(ins));
            if (ins->kind == TAC_INSTRUCTION_FUN_CALL && ins->u.fun_call.dst &&
                ins->u.fun_call.dst->kind == TAC_VAL_VAR)
                note(lv, NULL, ins->u.fun_call.dst->u.var_name);
            uses(ins, note, lv, NULL);
        }
    map_iterate((StringMap *)static_names, note_key, lv);
    map_iterate((StringMap *)address_taken, note_key, lv);

    // A key lives in its node, which keeps its place while only its value changes.
    lv->nnames = 0;
    map_iterate(&lv->index, count_key, lv);
    lv->names  = xalloc((lv->nnames + 1) * sizeof(char *), __func__, __FILE__, __LINE__);
    lv->nnames = 0;
    map_iterate(&lv->index, name_key, lv);
    for (int k = 0; k < lv->nnames; k++)
        map_insert(&lv->index, lv->names[k], k + 1, 0);
    lv->nwords = (lv->nnames + WORD_BITS - 1) / WORD_BITS;

    lv->observed = opt_live_new(lv);
    map_iterate((StringMap *)static_names, observe, lv);
    map_iterate((StringMap *)address_taken, observe, lv);
}

// ============================================================================
// opt_live_solve: the backward fixpoint.  Release with opt_live_free.
// ============================================================================

void opt_live_solve(OptLive *lv, const OptCfg *cfg, const StringMap *static_names,
                    const StringMap *address_taken, bool only_reachable)
{
    number_names(lv, cfg, static_names, address_taken);
    int n        = cfg->nblocks;
    int nw       = lv->nwords;
    size_t bytes = ((size_t)n * nw + 1) * sizeof(LiveWord);
    lv->in       = xalloc(bytes, __func__, __FILE__, __LINE__);
    lv->out      = xalloc(bytes, __func__, __FILE__, __LINE__);
    memset(lv->in, 0, bytes);
    memset(lv->out, 0, bytes);

    // Per-block instruction arrays, built once: the transfer walks each block backward.
    int *block_nins = xalloc((n + 1) * sizeof(int), __func__, __FILE__, __LINE__);
    const Tac_Instruction ***block_insts =
        xalloc((n + 1) * sizeof(Tac_Instruction **), __func__, __FILE__, __LINE__);
    for (int i = 0; i < n; i++) {
        int cnt = 0;
        for (const Tac_Instruction *ins = cfg->blocks[i]->first; ins; ins = ins->next)
            cnt++;
        block_nins[i] = cnt;
        block_insts[i] =
            cnt ? xalloc(cnt * sizeof(Tac_Instruction *), __func__, __FILE__, __LINE__) : NULL;
        int k = 0;
        for (const Tac_Instruction *ins = cfg->blocks[i]->first; ins; ins = ins->next)
            block_insts[i][k++] = ins;
    }

    // Recompute out[b] then in[b] for every block, in descending order (a backward
    // analysis converges faster bottom-up), until no in-set changes.
    LiveWord *new_in = opt_live_new(lv);
    bool changed     = true;
    int iter         = 0;
    while (changed) {
        changed = false;
        iter++;
        OPT_TRACE("[liveness] fixpoint iteration %d\n", iter);
        for (int i = n - 1; i >= 0; i--) {
            const OptBlock *b = cfg->blocks[i];
            // Skip only truly dead blocks. A *reachable* but empty block (its
            // instructions were emptied by unreachable-elim's jump/label cleanup,
            // yet it still carries a successor edge) must participate as an
            // identity node: with zero instructions the transfer below leaves
            // in[b] == out[b], threading a successor's live-in back to this
            // block's predecessors. Skipping it would strand its in-set empty and
            // let a predecessor wrongly drop a store that is live past the gap.
            if (only_reachable && !b->reachable)
                continue;

            // Meet: out[b] = union of in[s] for all successors s.  Exit block (no
            // successors): the variables observable after return.
            LiveWord *out = lv->out + (size_t)i * nw;
            memset(out, 0, nw * sizeof(LiveWord));
            for (int j = 0; j < b->nsucc; j++) {
                const LiveWord *s = lv->in + (size_t)b->succs[j]->id * nw;
                for (int w = 0; w < nw; w++)
                    out[w] |= s[w];
            }
            if (b->nsucc == 0)
                for (int w = 0; w < nw; w++)
                    out[w] |= lv->observed[w];

            // Transfer: in[b] = backward_transfer(out[b]).
            opt_live_copy(lv, new_in, out);
            for (int j = block_nins[i] - 1; j >= 0; j--)
                opt_live_transfer(lv, new_in, block_insts[i][j]);

            LiveWord *in = lv->in + (size_t)i * nw;
            if (memcmp(new_in, in, nw * sizeof(LiveWord)) != 0) {
                changed = true;
                opt_live_copy(lv, in, new_in);
            }
        }
    }
    OPT_TRACE("[liveness] fixpoint converged after %d iteration(s)\n", iter);
    xfree(new_in);
    for (int i = 0; i < n; i++)
        xfree(block_insts[i]);
    xfree(block_insts);
    xfree(block_nins);
}

const LiveWord *opt_live_out(const OptLive *lv, int block)
{
    return lv->out + (size_t)block * lv->nwords;
}

void opt_live_free(OptLive *lv)
{
    xfree(lv->in);
    xfree(lv->out);
    xfree(lv->observed);
    xfree(lv->names);
    map_destroy(&lv->index);
}
