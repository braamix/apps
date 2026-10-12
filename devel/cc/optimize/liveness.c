// ============================================================================
// liveness.c — backward liveness of frame-resident names over a CFG, shared by
// dead-store elimination (dead_store.c) and the coroutine split pass
// (translator/coro.c).  See dead_store.c for the analysis itself.
// ============================================================================

#include "liveness.h"

#include <stdbool.h>

#include "optimize.h"
#include "xalloc.h"

// ============================================================================
// Live-set primitives. A live set is a StringMap whose key is a variable name;
// membership = the variable is live. The value is an unused placeholder, and
// StringMap owns its keys, so a live set never points into the instruction
// stream — it stays valid across the instruction frees the removal walk does.
// ============================================================================

void opt_live_add(StringMap *ls, const char *name)
{
    if (name)
        map_insert(ls, name, 0, 0);
}

void opt_live_add_val(StringMap *ls, const Tac_Val *v)
{
    if (v && v->kind == TAC_VAL_VAR)
        opt_live_add(ls, v->u.var_name);
}

// map_remove_key is a no-op when the name is absent, so no membership pre-check.
void opt_live_remove(StringMap *ls, const char *name)
{
    map_remove_key(ls, name);
}

// ============================================================================
// opt_live_copy / opt_live_union
// ============================================================================

typedef struct {
    StringMap *dst;
} LiveCtx;

static void live_copy_cb(const char *key, intptr_t value, const void *arg)
{
    (void)value;
    opt_live_add(((const LiveCtx *)arg)->dst, key);
}

void opt_live_copy(StringMap *dst, const StringMap *src)
{
    LiveCtx ctx = { dst };
    map_iterate((StringMap *)src, live_copy_cb, &ctx);
}

// The meet operator: result ∪= other. Used to combine successors' in-sets into
// a block's out-set, and to seed the Exit out-set with the aliased variables.
void opt_live_union(StringMap *result, const StringMap *other)
{
    LiveCtx ctx = { result };
    map_iterate((StringMap *)other, live_copy_cb, &ctx);
}

// ============================================================================
// opt_live_equal: set equality, used as the fixed-point test for the backward
// dataflow loop. Checked in both directions.
// ============================================================================

typedef struct {
    bool *equal;
    const StringMap *other;
} LiveEqualCtx;

static void live_equal_cb(const char *key, intptr_t value, const void *arg)
{
    const LiveEqualCtx *ctx = (const LiveEqualCtx *)arg;
    (void)value;
    if (!*ctx->equal)
        return;
    if (!map_get((StringMap *)ctx->other, key, NULL))
        *ctx->equal = false;
}

bool opt_live_equal(const StringMap *a, const StringMap *b)
{
    bool eq          = true;
    LiveEqualCtx ctx = { &eq, b };
    map_iterate((StringMap *)a, live_equal_cb, &ctx);
    if (!eq)
        return false;
    ctx.other = a;
    map_iterate((StringMap *)b, live_equal_cb, &ctx);
    return eq;
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
// opt_live_transfer: the liveness transfer function for one instruction,
// updating live set `ls` in place. Caller walks instructions last-to-first.
// The order is kill-def then gen-use, so an instruction like `x = x + 1` keeps
// x live: x is removed as the def, then re-added as a use.
// ============================================================================

void opt_live_transfer(StringMap *ls, const Tac_Instruction *ins, const StringMap *static_names,
                       const StringMap *address_taken)
{
    if (ins->kind == TAC_INSTRUCTION_FUN_CALL || ins->kind == TAC_INSTRUCTION_FUN_CALL_NORETURN) {
        // Callee may read any static or address-taken variable.
        opt_live_union(ls, static_names);
        opt_live_union(ls, address_taken);
        for (const Tac_Val *a = ins->u.fun_call.args; a; a = a->next)
            opt_live_add_val(ls, a);
        if (ins->u.fun_call.indirect)
            opt_live_add(ls, ins->u.fun_call.fun_name); // the callee is read out of this var
        // FUN_CALL: no defined variable to kill.
        return;
    }

    opt_live_remove(ls, opt_live_def(ins));

    switch (ins->kind) {
    case TAC_INSTRUCTION_RETURN:
        opt_live_add_val(ls, ins->u.return_.src);
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
        opt_live_add_val(ls, ins->u.sign_extend.src);
        break;
    case TAC_INSTRUCTION_UNARY:
        opt_live_add_val(ls, ins->u.unary.src);
        break;
    case TAC_INSTRUCTION_BINARY:
        opt_live_add_val(ls, ins->u.binary.src1);
        opt_live_add_val(ls, ins->u.binary.src2);
        break;
    case TAC_INSTRUCTION_COPY:
        opt_live_add_val(ls, ins->u.copy.src);
        break;
    case TAC_INSTRUCTION_GET_ADDRESS:
    case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
    case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
        opt_live_add_val(ls, ins->u.get_address.src);
        break;
    case TAC_INSTRUCTION_LOAD:
    case TAC_INSTRUCTION_LOAD_BYTE:
        opt_live_add_val(ls, ins->u.load.src_ptr);
        break;
    case TAC_INSTRUCTION_STORE:
    case TAC_INSTRUCTION_STORE_BYTE:
        opt_live_add_val(ls, ins->u.store.src);
        opt_live_add_val(ls, ins->u.store.dst_ptr);
        break;
    case TAC_INSTRUCTION_ADD_PTR:
        opt_live_add_val(ls, ins->u.add_ptr.ptr);
        opt_live_add_val(ls, ins->u.add_ptr.index);
        break;
    case TAC_INSTRUCTION_PTR_DIFF:
        opt_live_add_val(ls, ins->u.ptr_diff.ptr_a);
        opt_live_add_val(ls, ins->u.ptr_diff.ptr_b);
        break;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET:
        opt_live_add_val(ls, ins->u.copy_to_offset.src);
        break;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
        opt_live_add(ls, ins->u.copy_from_offset.src); // char*, not Tac_Val*
        break;
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
        opt_live_add_val(ls, ins->u.jump_if_zero.condition);
        break;
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
        opt_live_add_val(ls, ins->u.jump_if_not_zero.condition);
        break;
    case TAC_INSTRUCTION_JUMP_TABLE:
        opt_live_add_val(ls, ins->u.jump_table.index);
        break;
    default:
        break;
    }
}

// ============================================================================
// opt_live_solve: the backward fixpoint.  in[] and out[] (n entries each) are
// initialized here; the caller destroys them with opt_live_free.
// ============================================================================

void opt_live_solve(const OptCfg *cfg, const StringMap *static_names,
                    const StringMap *address_taken, bool only_reachable, StringMap *in_sets,
                    StringMap *out_sets)
{
    int n = cfg->nblocks;
    for (int i = 0; i < n; i++) {
        map_init(&in_sets[i]);
        map_init(&out_sets[i]);
    }

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
    bool changed = true;
    int iter     = 0;
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

            // Meet: out[b] = union of in[s] for all successors s.
            StringMap new_out;
            map_init(&new_out);
            for (int j = 0; j < b->nsucc; j++)
                opt_live_union(&new_out, &in_sets[b->succs[j]->id]);
            // Exit block (no successors): seed with variables observable after return.
            if (b->nsucc == 0) {
                opt_live_union(&new_out, static_names);
                opt_live_union(&new_out, address_taken);
            }

            // Transfer: in[b] = backward_transfer(out[b]).
            StringMap new_in;
            map_init(&new_in);
            opt_live_copy(&new_in, &new_out);
            for (int j = block_nins[i] - 1; j >= 0; j--)
                opt_live_transfer(&new_in, block_insts[i][j], static_names, address_taken);

            if (!opt_live_equal(&new_in, &in_sets[i]))
                changed = true;

            map_destroy(&in_sets[i]);
            map_destroy(&out_sets[i]);
            in_sets[i]  = new_in;
            out_sets[i] = new_out;
        }
    }
    OPT_TRACE("[liveness] fixpoint converged after %d iteration(s)\n", iter);
    for (int i = 0; i < n; i++)
        xfree(block_insts[i]);
    xfree(block_insts);
    xfree(block_nins);
}

void opt_live_free(int n, StringMap *in_sets, StringMap *out_sets)
{
    for (int i = 0; i < n; i++) {
        map_destroy(&in_sets[i]);
        map_destroy(&out_sets[i]);
    }
}
