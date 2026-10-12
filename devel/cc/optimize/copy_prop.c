// ============================================================================
// copy_prop.c — copy propagation via reaching-copies analysis.
//
// After `Copy(src, dst)`, later uses of `dst` can be replaced by `src` — but
// only where we know `dst` has not been reassigned on any path reaching the use.
// We establish that with a forward dataflow analysis, "reaching copies":
//
//   - Lattice element: a set of (dst → src) copy pairs that hold on *every* path
//     reaching the program point.
//   - Initial value at entry: empty.
//   - Meet at merge points: intersection — a copy reaches only if it holds on
//     all incoming paths (and agrees on the same src).
//   - Transfer for one instruction:
//       Gen:  a Copy(src, dst) adds the pair (dst → src).
//       Kill: assigning to a variable v removes every pair mentioning v as
//             either dst or src (the old value is no longer valid).
//
// Iterating the transfer over the CFG to a fixed point yields, at each point,
// the copies guaranteed valid there. We then substitute: a use of x is replaced
// by src when a reaching copy (x → src) is in effect. Substitution can create
// Copy(x, x) self-copies, which are no-ops and removed on the spot.
//
// Conservatism around aliasing (see alias.c): static-duration and address-taken
// variables may be changed behind our back. A FunCall may touch any static or
// address-taken variable, so it kills all copies involving them; a Store writes
// through a pointer, which may point at either, so it kills the same copies.
// Temporaries (%N) are never address-taken and propagate freely.
//
// See docs/TAC_Optimization.md §"Copy propagation".
// ============================================================================

#include <string.h>

#include "alias.h"
#include "cfg.h"
#include "dataflow.h"
#include "optimize.h"
#include "string_map.h"
#include "tac.h"
#include "target.h"
#include "xalloc.h"

// ============================================================================
// CopyPair — one reaching copy (dst → src). A copy set is a StringMap keyed by
// the destination variable name; the CopyPair is stored as the map's intptr_t
// value. The destination name is mirrored inside the pair because map_iterate
// callbacks receive only the value, not the key.
// ============================================================================

typedef struct {
    char *name;   // owned (xstrdup); the destination name, == the map key
    Tac_Val *src; // owned (dup_val) — an independent copy, not a borrow into the
                  // instruction stream, which substitution/self-copy removal frees
} CopyPair;

static Tac_Val *dup_val(const Tac_Val *v);

static void pair_free(intptr_t value)
{
    CopyPair *p = (CopyPair *)value;
    if (!p)
        return;
    xfree(p->name);
    tac_free_val(p->src);
    xfree(p);
}

static void copy_set_destroy(StringMap *cs)
{
    map_destroy_free(cs, pair_free);
}

// ============================================================================
// copy_set_copy: deep-copy src into dst (dst must be a freshly initialised
// empty map). Each CopyPair is duplicated (its name re-xstrdup'd); the borrowed
// `src` Tac_Val pointer is shared, since it points into the instruction stream.
// Used to seed a block's in-set from a predecessor's out-set.
// ============================================================================

typedef struct {
    StringMap *dst;
} CopyCtx;

static void copy_cb(const char *key, intptr_t value, const void *arg)
{
    (void)key;
    const CopyCtx *ctx = (const CopyCtx *)arg;
    const CopyPair *sp = (const CopyPair *)value;
    CopyPair *np       = xalloc(sizeof(CopyPair), __func__, __FILE__, __LINE__);
    np->name           = xstrdup(sp->name);
    np->src            = dup_val(sp->src);
    map_insert_free(ctx->dst, np->name, (intptr_t)np, 0, pair_free);
}

static void copy_set_copy(StringMap *dst, const StringMap *src)
{
    CopyCtx ctx = { dst };
    map_iterate((StringMap *)src, copy_cb, &ctx);
}

// ============================================================================
// kill_name: the Kill rule for a single variable. Assigning to `name`
// invalidates every copy that mentions it — both (name → ...) where it is the
// destination and (... → Var(name)) where it is the propagated source.
// ============================================================================

typedef struct {
    KeyBuf *kb;
    const char *name;
} KillNameCtx;

static void kill_name_cb(const char *key, intptr_t value, const void *arg)
{
    (void)key;
    const KillNameCtx *ctx = (const KillNameCtx *)arg;
    const CopyPair *pair   = (const CopyPair *)value;
    if (strcmp(pair->name, ctx->name) == 0 ||
        (pair->src->kind == TAC_VAL_VAR && strcmp(pair->src->u.var_name, ctx->name) == 0)) {
        keybuf_push(ctx->kb, pair->name);
    }
}

static void kill_name(StringMap *cs, const char *name)
{
    KeyBuf kb       = { 0 };
    KillNameCtx ctx = { &kb, name };
    map_iterate(cs, kill_name_cb, &ctx);
    keybuf_flush(&kb, cs, pair_free);
}

// ============================================================================
// kill_alias_set: the Kill rule for a whole class of variables at once — every
// copy whose dst or src-var is in `alias` is removed. Used at FunCall (alias =
// static names and address-taken names) and Store (alias = address-taken names),
// where a variable may have been modified out from under us.
// ============================================================================

typedef struct {
    KeyBuf *kb;
    const StringMap *alias;
} KillAliasCtx;

static void kill_alias_cb(const char *key, intptr_t value, const void *arg)
{
    (void)key;
    const KillAliasCtx *ctx = (const KillAliasCtx *)arg;
    const CopyPair *pair    = (const CopyPair *)value;
    bool kill               = map_get((StringMap *)ctx->alias, pair->name, NULL);
    if (!kill && pair->src->kind == TAC_VAL_VAR)
        kill = map_get((StringMap *)ctx->alias, pair->src->u.var_name, NULL);
    if (kill)
        keybuf_push(ctx->kb, pair->name);
}

static void kill_alias_set(StringMap *cs, const StringMap *alias)
{
    KeyBuf kb        = { 0 };
    KillAliasCtx ctx = { &kb, alias };
    map_iterate(cs, kill_alias_cb, &ctx);
    keybuf_flush(&kb, cs, pair_free);
}

// ============================================================================
// apply_transfer: the reaching-copies transfer function for one instruction,
// updating copy-set `cs` in place. This is both the Gen and Kill of the lattice.
// ============================================================================

// The value class of a type, as the verifier sees it: integer, pointer, each
// floating type, aggregate. A copy is propagated only between names of one class.
static int value_class(const Tac_Type *t)
{
    switch (t->kind) {
    case TAC_TYPE_POINTER:
        return 1;
    case TAC_TYPE_FLOAT:
        return 2;
    case TAC_TYPE_DOUBLE:
        return 3;
    case TAC_TYPE_LONG_DOUBLE:
        return 4;
    case TAC_TYPE_ARRAY:
    case TAC_TYPE_STRUCTURE:
        return 5;
    default:
        return 0;
    }
}

// A scalar type a store through a pointer to it writes in its own width and register
// file.
static bool is_scalar_pointee(const Tac_Type *t)
{
    return t && t->kind < TAC_TYPE_VOID;
}

// May the copy `dst = src` be propagated? Not when it changes the value class: a
// same-size integer copied into a pointer (a cast, lowered as a COPY on a 32-bit
// target) must not be forwarded into the pointer operand of an ADD_PTR or LOAD.
// Nor when it is a cast between pointers to different scalars: a STORE is as wide as
// its pointer's pointee, so `*(unsigned long *)&d = 0` must not become a store
// through the `double *`. Names of unknown type (globals, untyped hand-built TAC) are
// let through.
static bool same_class(const StringMap *types, const char *dst, const Tac_Val *src)
{
    intptr_t td = 0, ts = 0;
    if (!types || src->kind != TAC_VAL_VAR || !map_get((StringMap *)types, dst, &td) ||
        !map_get((StringMap *)types, src->u.var_name, &ts) || !td || !ts)
        return true;
    const Tac_Type *a = (const Tac_Type *)td, *b = (const Tac_Type *)ts;
    if (value_class(a) != value_class(b))
        return false;
    if (a->kind == TAC_TYPE_POINTER && b->kind == TAC_TYPE_POINTER) {
        const Tac_Type *pa = a->u.pointer.target_type, *pb = b->u.pointer.target_type;
        if (is_scalar_pointee(pa) && is_scalar_pointee(pb) && pa->kind != pb->kind)
            return false;
    }
    return true;
}

static void apply_transfer(StringMap *cs, const Tac_Instruction *ins, const StringMap *static_names,
                           const StringMap *address_taken, const StringMap *types)
{
    if (ins->kind == TAC_INSTRUCTION_COPY) {
        // Copy(src, dst): Kill old copies mentioning dst, then Gen (dst → src).
        const Tac_Val *dst = ins->u.copy.dst;
        if (dst->kind == TAC_VAL_VAR) {
            OPT_TRACE("[copy-prop] kill copies involving %s\n", dst->u.var_name);
            kill_name(cs, dst->u.var_name);
            // A volatile copy must re-execute its exact read on every use, so it
            // is not a propagatable copy: kill, but do not Gen a (dst → src) pair.
            // Nor is one that changes the value class (see same_class).
            if (ins->is_volatile || !same_class(types, dst->u.var_name, ins->u.copy.src))
                return;
            CopyPair *p = xalloc(sizeof(CopyPair), __func__, __FILE__, __LINE__);
            p->name     = xstrdup(dst->u.var_name);
            p->src      = dup_val(ins->u.copy.src);
            map_insert_free(cs, p->name, (intptr_t)p, 0, pair_free);
            OPT_TRACE(
                "[copy-prop] gen copy: %s → %s\n", dst->u.var_name,
                ins->u.copy.src->kind == TAC_VAL_VAR ? ins->u.copy.src->u.var_name : "<const>");
        }
        return;
    }

    if (ins->kind == TAC_INSTRUCTION_FUN_CALL || ins->kind == TAC_INSTRUCTION_FUN_CALL_NORETURN) {
        // A call may read or write any static or address-taken variable, so all
        // copies involving them become invalid; also kill the call's own result.
        OPT_TRACE("[copy-prop] fun-call %s: kill static+address-taken copies\n",
                  ins->u.fun_call.fun_name);
        kill_alias_set(cs, static_names);
        kill_alias_set(cs, address_taken);
        const Tac_Val *dst = ins->u.fun_call.dst;
        if (dst && dst->kind == TAC_VAL_VAR) {
            OPT_TRACE("[copy-prop] fun-call: kill dst %s\n", dst->u.var_name);
            kill_name(cs, dst->u.var_name);
        }
        return;
    }

    if (ins->kind == TAC_INSTRUCTION_STORE || ins->kind == TAC_INSTRUCTION_STORE_BYTE) {
        // A store writes through a pointer, which may point at any address-taken
        // variable — or at any static one, whose address another function may
        // have taken: kill copies involving them. (Store defines no named var.)
        OPT_TRACE("[copy-prop] store: kill static+address-taken copies\n");
        kill_alias_set(cs, static_names);
        kill_alias_set(cs, address_taken);
        return;
    }

    if (ins->kind == TAC_INSTRUCTION_COPY_TO_OFFSET ||
        ins->kind == TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET) {
        // A member write changes the aggregate named by dst.
        kill_name(cs, ins->u.copy_to_offset.dst);
        return;
    }

    // Every other defining instruction just Kills copies mentioning its dst.
    const Tac_Val *dst = opt_defining_dst(ins);
    if (dst && dst->kind == TAC_VAL_VAR) {
        OPT_TRACE("[copy-prop] kill copies involving %s\n", dst->u.var_name);
        kill_name(cs, dst->u.var_name);
    }
}

// ============================================================================
// copy_set_intersect: the meet operator. Restrict `result` to entries also
// present in `other` with the *same* src; drop any entry that is missing from
// `other` or that disagrees on its src. This is how copies from multiple
// predecessors are combined — a copy survives only if every path agrees on it.
// ============================================================================

typedef struct {
    KeyBuf *kb;
    const StringMap *other;
} IntersectCtx;

static void intersect_cb(const char *key, intptr_t value, const void *arg)
{
    (void)key;
    const IntersectCtx *ctx = (const IntersectCtx *)arg;
    const CopyPair *pair    = (const CopyPair *)value;
    intptr_t oval           = 0;
    if (!map_get((StringMap *)ctx->other, pair->name, &oval)) {
        keybuf_push(ctx->kb, pair->name);
        return;
    }
    const CopyPair *op = (const CopyPair *)oval;
    if (!tac_compare_val(pair->src, op->src))
        keybuf_push(ctx->kb, pair->name);
}

static void copy_set_intersect(StringMap *result, const StringMap *other)
{
    KeyBuf kb        = { 0 };
    IntersectCtx ctx = { &kb, other };
    map_iterate(result, intersect_cb, &ctx);
    keybuf_flush(&kb, result, pair_free);
}

// ============================================================================
// copy_set_equal: returns true iff a and b have identical (name, src) entries.
// The fixed-point test for the dataflow loop — iteration stops when a block's
// out-set stops changing. Checked in both directions to catch entries present
// in one set but not the other.
// ============================================================================

typedef struct {
    bool *equal;
    const StringMap *other;
} EqualCtx;

static void equal_cb(const char *key, intptr_t value, const void *arg)
{
    (void)key;
    const EqualCtx *ctx = (const EqualCtx *)arg;
    if (!*ctx->equal)
        return;
    const CopyPair *pair = (const CopyPair *)value;
    intptr_t oval        = 0;
    if (!map_get((StringMap *)ctx->other, pair->name, &oval)) {
        *ctx->equal = false;
        return;
    }
    const CopyPair *op = (const CopyPair *)oval;
    if (!tac_compare_val(pair->src, op->src))
        *ctx->equal = false;
}

static bool copy_set_equal(const StringMap *a, const StringMap *b)
{
    bool eq      = true;
    EqualCtx ctx = { &eq, b };
    map_iterate((StringMap *)a, equal_cb, &ctx);
    if (!eq)
        return false;
    ctx.other = a;
    map_iterate((StringMap *)b, equal_cb, &ctx);
    return eq;
}

// ============================================================================
// dup_val: deep-copy a single Tac_Val node without following .next. Needed when
// substituting, because the copy's src is borrowed and may be propagated into
// several use sites — each site must own an independent Tac_Val.
// ============================================================================

static Tac_Val *dup_val(const Tac_Val *v)
{
    Tac_Val *nv = tac_new_val(v->kind);
    nv->next    = NULL; // tac_new_val leaves .next uninitialized
    if (v->kind == TAC_VAL_CONSTANT) {
        Tac_Const *nc  = tac_new_const(v->u.constant->kind);
        *nc            = *v->u.constant;
        nv->u.constant = nc;
    } else {
        nv->u.var_name = xstrdup(v->u.var_name);
    }
    return nv;
}

// ============================================================================
// subst_val: the substitution step. If *vp is Var(x) and a copy (x → src)
// reaches here, replace the operand with a fresh duplicate of src and free the
// old Var node. This is where copy propagation actually rewrites the code.
// ============================================================================

static void subst_val(Tac_Val **vp, const StringMap *cs)
{
    Tac_Val *v = *vp;
    if (!v || v->kind != TAC_VAL_VAR)
        return;
    intptr_t oval = 0;
    if (!map_get((StringMap *)cs, v->u.var_name, &oval))
        return;
    const CopyPair *p = (const CopyPair *)oval;
    Tac_Val *repl     = dup_val(p->src);
    v->next           = NULL; // isolate before freeing; tac_free_val follows .next
    tac_free_val(v);
    *vp = repl;
}

// ============================================================================
// subst_args: subst_val applied to each element of a FunCall's args list. Done
// in-place, relinking the list when an argument node is replaced.
// ============================================================================

static void subst_args(Tac_Val **head, const StringMap *cs)
{
    while (*head) {
        Tac_Val *arg = *head;
        if (arg->kind == TAC_VAL_VAR) {
            intptr_t oval = 0;
            if (map_get((StringMap *)cs, arg->u.var_name, &oval)) {
                const CopyPair *p = (const CopyPair *)oval;
                Tac_Val *repl     = dup_val(p->src);
                repl->next        = arg->next;
                *head             = repl;
                arg->next         = NULL;
                tac_free_val(arg);
                head = &repl->next;
                continue;
            }
        }
        head = &(*head)->next;
    }
}

// ============================================================================
// subst_instruction: substitute all source operands (not dst) of one instruction.
// GET_ADDRESS.src is intentionally excluded: it is address-taken, not a value use.
// ============================================================================

static void subst_instruction(Tac_Instruction *ins, const StringMap *cs)
{
    // A volatile access must read its exact operand from memory every time; never
    // rewrite its operands with a propagated value.
    if (ins->is_volatile)
        return;
    switch (ins->kind) {
    case TAC_INSTRUCTION_RETURN:
        if (ins->u.return_.src)
            subst_val(&ins->u.return_.src, cs);
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
        subst_val(&ins->u.sign_extend.src, cs);
        break;
    case TAC_INSTRUCTION_UNARY:
        subst_val(&ins->u.unary.src, cs);
        break;
    case TAC_INSTRUCTION_BINARY:
        subst_val(&ins->u.binary.src1, cs);
        subst_val(&ins->u.binary.src2, cs);
        break;
    case TAC_INSTRUCTION_COPY:
        subst_val(&ins->u.copy.src, cs);
        break;
    case TAC_INSTRUCTION_GET_ADDRESS:
    case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
    case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
        break;
    case TAC_INSTRUCTION_LOAD:
    case TAC_INSTRUCTION_LOAD_BYTE:
        subst_val(&ins->u.load.src_ptr, cs);
        break;
    case TAC_INSTRUCTION_STORE:
    case TAC_INSTRUCTION_STORE_BYTE:
        subst_val(&ins->u.store.src, cs);
        subst_val(&ins->u.store.dst_ptr, cs);
        break;
    case TAC_INSTRUCTION_ADD_PTR:
        subst_val(&ins->u.add_ptr.ptr, cs);
        subst_val(&ins->u.add_ptr.index, cs);
        break;
    case TAC_INSTRUCTION_PTR_DIFF:
        subst_val(&ins->u.ptr_diff.ptr_a, cs);
        subst_val(&ins->u.ptr_diff.ptr_b, cs);
        break;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET:
        subst_val(&ins->u.copy_to_offset.src, cs);
        break;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
        break;
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
        subst_val(&ins->u.jump_if_zero.condition, cs);
        break;
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
        subst_val(&ins->u.jump_if_not_zero.condition, cs);
        break;
    case TAC_INSTRUCTION_JUMP_TABLE:
        subst_val(&ins->u.jump_table.index, cs);
        break;
    case TAC_INSTRUCTION_FUN_CALL:
    case TAC_INSTRUCTION_FUN_CALL_NORETURN:
        subst_args(&ins->u.fun_call.args, cs);
        break;
    default:
        break;
    }
}

// ============================================================================
// propagate_copies: entry point. Runs the three stages — alias pre-analysis,
// the forward reaching-copies fixpoint, then substitution — and frees all the
// dataflow scaffolding before returning.
// ============================================================================

void propagate_copies(OptCfg *cfg, const Tac_TopLevel *fn)
{
    if (cfg->nblocks == 0)
        return;

    // Stage 1: identify variables that must be treated conservatively.
    StringMap static_names, address_taken;
    collect_alias_sets(cfg, fn, &static_names, &address_taken);

    // The types of the private names, for the value-class check.
    StringMap types;
    map_init(&types);
    if (fn && fn->kind == TAC_TOPLEVEL_FUNCTION) {
        for (const Tac_Param *p = fn->u.function.params; p; p = p->next)
            if (p->name)
                map_insert(&types, p->name, (intptr_t)p->type, 0);
        for (const Tac_Param *p = fn->u.function.locals; p; p = p->next)
            if (p->name)
                map_insert(&types, p->name, (intptr_t)p->type, 0);
    }

    // Stage 2: the forward reaching-copies dataflow.
    int n = cfg->nblocks;

    // The meet combines predecessors' out-sets.
    OptPreds pr;
    opt_preds_build(&pr, cfg);
    const int *npreds = pr.npreds;
    int *const *preds = pr.preds;

    // Allocate in/out sets, all starting empty (no copies reaching).
    StringMap *in_sets  = xalloc(n * sizeof(StringMap), __func__, __FILE__, __LINE__);
    StringMap *out_sets = xalloc(n * sizeof(StringMap), __func__, __FILE__, __LINE__);
    // Whether a block's out-set has been computed yet. One that has not stands for
    // every copy, the top of the lattice, so a copy made ahead of a loop reaches into
    // it: the back edge's out-set is not yet known when the header is first met.
    // Pessimistic (empty) where the target opts out of the loop optimizations.
    bool *done      = xalloc(n * sizeof(bool), __func__, __FILE__, __LINE__);
    bool optimistic = !target_config->no_loop_opt;
    for (int i = 0; i < n; i++) {
        map_init(&in_sets[i]);
        map_init(&out_sets[i]);
        done[i] = !optimistic;
    }

    // Fixpoint iteration: recompute each block's in/out until nothing changes.
    // Forward analysis, so blocks are visited in ascending (roughly topological)
    // order for faster convergence.
    bool changed = true;
    int cp_iter  = 0;
    while (changed) {
        changed = false;
        cp_iter++;
        OPT_TRACE("[copy-prop] fixpoint iteration %d\n", cp_iter);
        for (int i = 0; i < n; i++) {
            const OptBlock *b = cfg->blocks[i];
            // An empty block passes its in-set on (its successors wait for it).
            if (!b->reachable || (!b->first && !optimistic))
                continue;

            // in[b] = meet (intersection) of out[pred] over all predecessors.
            // Seed from the first predecessor, then intersect the rest. The
            // entry block (i == 0) is the exception: its in-set is the boundary
            // value (empty). Control may enter the function at the entry without
            // traversing any predecessor (e.g. a loop whose header is the entry
            // block), so a copy generated on a back-edge into the entry must not
            // be treated as reaching the entry's instructions.
            StringMap new_in;
            map_init(&new_in);
            if (i != 0 && npreds[i] > 0) {
                bool first = true;
                for (int k = 0; k < npreds[i]; k++) {
                    int p = preds[i][k];
                    if (!done[p] || (optimistic && !cfg->blocks[p]->reachable))
                        continue;
                    if (first)
                        copy_set_copy(&new_in, &out_sets[p]);
                    else
                        copy_set_intersect(&new_in, &out_sets[p]);
                    first = false;
                }
                // Every predecessor still unknown: wait for one, so that the sets only
                // ever shrink from the top.
                if (first) {
                    map_destroy(&new_in);
                    changed = true;
                    continue;
                }
            }

            // out[b] = transfer(in[b]): apply the Gen/Kill rules in order.
            StringMap new_out;
            map_init(&new_out);
            copy_set_copy(&new_out, &new_in);
            for (const Tac_Instruction *ins = b->first; ins; ins = ins->next)
                apply_transfer(&new_out, ins, &static_names, &address_taken, &types);

            if (!done[i] || !copy_set_equal(&new_out, &out_sets[i])) {
                OPT_TRACE("[copy-prop] block %d out-set changed\n", i);
                changed = true;
            }
            done[i] = true;

            copy_set_destroy(&in_sets[i]);
            copy_set_destroy(&out_sets[i]);
            in_sets[i]  = new_in;
            out_sets[i] = new_out;
        }
    }
    OPT_TRACE("[copy-prop] fixpoint converged after %d iteration(s)\n", cp_iter);

    // Stage 3: substitution. Replay the transfer within each block, but now
    // rewrite source operands using the copies in effect at each instruction.
    // `current_in` tracks the reaching set as we move forward through the block,
    // starting from the converged in-set and updated by apply_transfer per step.
    for (int i = 0; i < n; i++) {
        OptBlock *b = cfg->blocks[i];
        if (!b->reachable || !b->first)
            continue;

        StringMap current_in;
        map_init(&current_in);
        copy_set_copy(&current_in, &in_sets[i]);

        Tac_Instruction *prev = NULL;
        Tac_Instruction *ins  = b->first;
        while (ins) {
            Tac_Instruction *next = ins->next;

            opt_trace_instr("[copy-prop] subst before:", ins);
            subst_instruction(ins, &current_in);
            opt_trace_instr("[copy-prop] subst after: ", ins);

            // Substitution may have produced a self-copy Copy(Var(x), Var(x)),
            // a no-op — unlink and free it.
            if (ins->kind == TAC_INSTRUCTION_COPY && ins->u.copy.src->kind == TAC_VAL_VAR &&
                ins->u.copy.dst->kind == TAC_VAL_VAR &&
                strcmp(ins->u.copy.src->u.var_name, ins->u.copy.dst->u.var_name) == 0) {
                OPT_TRACE("[copy-prop] removed self-copy %s = %s\n", ins->u.copy.dst->u.var_name,
                          ins->u.copy.src->u.var_name);
                if (prev)
                    prev->next = next;
                else
                    b->first = next;
                if (b->last == ins)
                    b->last = prev;
                ins->next = NULL;
                tac_free_instruction(ins);
                ins = next;
                continue;
            }

            // Advance the running reaching set past this (post-substitution)
            // instruction before moving on.
            apply_transfer(&current_in, ins, &static_names, &address_taken, &types);
            prev = ins;
            ins  = next;
        }

        copy_set_destroy(&current_in);
    }

    // Free all dataflow scaffolding and the alias maps.
    for (int i = 0; i < n; i++) {
        copy_set_destroy(&in_sets[i]);
        copy_set_destroy(&out_sets[i]);
    }
    xfree(in_sets);
    xfree(out_sets);
    xfree(done);
    opt_preds_free(&pr);
    map_destroy(&static_names);
    map_destroy(&address_taken);
    map_destroy(&types);
}
