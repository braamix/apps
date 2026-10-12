//
// Register allocation over TAC variables (regalloc.h).
//
#include "regalloc.h"

#include <string.h>

#include "bitops.h"
#include "xalloc.h"

typedef struct {
    const RegAlloc_Target *t;
    Flow *flow;
    int n;     // flow variables
    int N;     // nodes: n, then the high words of long longs
    int words; // per adjacency row
    bool *cand;
    bool *fp;
    Flow_Set *adj; // N rows of `words`
    int *alias;
    double *cost;
    int *color;  // register, or 0
    int *hint;   // preferred register, or 0
    bool *cross; // live across a call: callee-saved registers only
} Alloc;

static Flow_Set *row(const Alloc *a, int v)
{
    return a->adj + (size_t)v * a->words;
}

// The high-word node of variable v, or -1.
static int hi_node(const Alloc *a, int v)
{
    return v < a->n && a->cand[v + a->n] ? v + a->n : -1;
}

// cppcheck-suppress constParameterPointer ; adds to the graph, through row()
static void add_edge(Alloc *a, int x, int y)
{
    if (x == y || !a->cand[x] || !a->cand[y] || a->fp[x] != a->fp[y])
        return;
    flow_add(row(a, x), y);
    flow_add(row(a, y), x);
}

static int degree(const Alloc *a, int v)
{
    int d = 0;
    for (int w = 0; w < a->words; w++)
        d += popcount64(row(a, v)[w]);
    return d;
}

static int find(const Alloc *a, int v)
{
    while (a->alias[v] != v)
        v = a->alias[v];
    return v;
}

static int k_of(const Alloc *a, int v)
{
    const RegAlloc_Target *t = a->t;
    return a->fp[v] ? t->nfp - (a->cross[v] ? t->fp_narg : 0)
                    : t->nint - (a->cross[v] ? t->int_narg : 0);
}

static const int *pool_of(const Alloc *a, int v)
{
    const RegAlloc_Target *t = a->t;
    return a->fp[v] ? t->fp_pool + (a->cross[v] ? t->fp_narg : 0)
                    : t->int_pool + (a->cross[v] ? t->int_narg : 0);
}

static void exclude_name(Alloc *a, const char *name)
{
    int v = flow_var(a->flow, name);
    if (v >= 0)
        a->cand[v] = false;
}

// Candidates: scalars never in memory, nor named as an aggregate by a member access.
static void find_candidates(Alloc *a)
{
    const Flow *f = a->flow;
    RegAlloc_Class *cls =
        xalloc((a->n ? a->n : 1) * sizeof(RegAlloc_Class), __func__, __FILE__, __LINE__);
    bool *agg = xalloc((a->n ? a->n : 1) * sizeof(bool), __func__, __FILE__, __LINE__);
    for (int v = 0; v < a->n; v++) {
        const Tac_Type *t = f->types[v];
        cls[v]            = t ? a->t->classify(a->t->arg, t) : REGALLOC_NONE;
        agg[v]            = t && (t->kind == TAC_TYPE_STRUCTURE || t->kind == TAC_TYPE_ARRAY);
        if (agg[v])
            a->cand[v] = cls[v] == REGALLOC_PAIR && a->t->aggregate_ok &&
                         !flow_has(f->addressed, v) && a->t->aggregate_ok(a->t->arg, f, v);
        else
            a->cand[v] = cls[v] != REGALLOC_NONE && !flow_has(f->in_memory, v);
        a->fp[v] = a->cand[v] && cls[v] == REGALLOC_FP;
    }
    for (int i = 0; i < f->ninstrs; i++) {
        const Tac_Instruction *in = f->instrs[i];
        const char *name          = NULL;
        if (in->kind == TAC_INSTRUCTION_COPY_TO_OFFSET ||
            in->kind == TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET)
            name = in->u.copy_to_offset.dst;
        if (in->kind == TAC_INSTRUCTION_COPY_FROM_OFFSET ||
            in->kind == TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET)
            name = in->u.copy_from_offset.src;
        int v = name ? flow_var(f, name) : -1;
        if (name && (v < 0 || !agg[v]))
            exclude_name(a, name);
    }
    xfree(agg);
    for (int v = 0; v < a->n; v++)
        a->cand[v + a->n] = a->cand[v] && cls[v] == REGALLOC_PAIR;
    xfree(cls);
}

// The source variable of a copy between candidates of one type, or -1.
static int move_source(const Alloc *a, const Tac_Instruction *in, int *dst)
{
    if (in->kind != TAC_INSTRUCTION_COPY || in->u.copy.src->kind != TAC_VAL_VAR)
        return -1;
    int s = flow_var(a->flow, in->u.copy.src->u.var_name);
    int d = flow_var(a->flow, in->u.copy.dst->u.var_name);
    if (s < 0 || d < 0 || !a->cand[s] || !a->cand[d] ||
        (hi_node(a, s) >= 0) != (hi_node(a, d) >= 0))
        return -1;
    // The two kinds of long long are the same pair of words.
    if (a->flow->types[s]->kind != a->flow->types[d]->kind && hi_node(a, s) < 0)
        return -1;
    *dst = d;
    return s;
}

typedef struct {
    Alloc *a;
    const Flow_Set *live;
    int skip; // the source of a move: no edge
} DefArg;

// Edges between every word of variables x and y.
static void add_var_edge(Alloc *a, int x, int y)
{
    const int xs[2] = { x, hi_node(a, x) }, ys[2] = { y, hi_node(a, y) };
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++)
            if (xs[i] >= 0 && ys[j] >= 0)
                add_edge(a, xs[i], ys[j]);
}

// A copy's destination interferes with what is live after it, but its source; of a
// pair, a word of the destination still interferes with the other word of the source.
static void interfere_def(int d, void *arg)
{
    DefArg *da = arg;
    Alloc *a   = da->a;
    for (int v = 0; v < a->n; v++) {
        if (!flow_has(da->live, v))
            continue;
        if (v != da->skip) {
            add_var_edge(a, d, v);
        } else if (hi_node(a, v) >= 0) {
            add_edge(a, d, hi_node(a, v));
            add_edge(a, hi_node(a, d), v);
        }
    }
}

typedef struct {
    Alloc *a;
    double weight;
} CostArg;

static void add_cost(int v, void *arg)
{
    CostArg *ca = arg;
    ca->a->cost[v] += ca->weight;
}

// Loop depth of each block: one per back edge spanning it, as laid out.
static int *loop_depths(const Flow *f)
{
    int *depth = xalloc((f->nblocks ? f->nblocks : 1) * sizeof(int), __func__, __FILE__, __LINE__);
    memset(depth, 0, (f->nblocks ? f->nblocks : 1) * sizeof(int));
    for (int b = 0; b < f->nblocks; b++)
        for (int k = 0; k < f->blocks[b].nsucc; k++)
            for (int h = f->blocks[b].succ[k]; h <= b; h++)
                depth[h]++;
    return depth;
}

// Whether `in` makes a call, explicit or to the runtime; its result in *res.
static bool makes_call(const Alloc *a, const Tac_Instruction *in, const Tac_Val **res)
{
    if (in->kind == TAC_INSTRUCTION_FUN_CALL || in->kind == TAC_INSTRUCTION_FUN_CALL_NORETURN) {
        *res = in->u.fun_call.dst;
        return !a->t->inline_call || !a->t->inline_call(a->t->arg, in);
    }
    return a->t->runtime_call(a->t->arg, a->flow, in, res);
}

static void build(Alloc *a)
{
    const Flow *f  = a->flow;
    Flow_Set *live = flow_set_new(f);
    int *depth     = loop_depths(f);
    for (int b = 0; b < f->nblocks; b++) {
        const Flow_Block *blk = &f->blocks[b];
        memcpy(live, blk->live_out, f->words * sizeof(Flow_Set));
        CostArg ca = { a, 1 };
        for (int k = 0; k < depth[b] && k < 4; k++)
            ca.weight *= 10;
        for (int i = blk->last; i >= blk->first; i--) {
            const Tac_Instruction *in = f->instrs[i];
            int dst;
            DefArg da = { a, live, move_source(a, in, &dst) };
            flow_defs(f, in, interfere_def, &da);
            const Tac_Val *res;
            if (makes_call(a, in, &res)) {
                int r = res ? flow_var(f, res->u.var_name) : -1;
                for (int v = 0; v < a->n; v++)
                    if (v != r && flow_has(live, v))
                        a->cross[v] = true;
            }
            flow_defs(f, in, add_cost, &ca);
            flow_uses(f, in, add_cost, &ca);
            flow_step(f, in, live);
        }
    }
    // The parameters are all written on entry, but for those dead there, when the
    // target leaves their incoming values where they are.
    if (f->nblocks > 0) {
        DefArg da = { a, f->blocks[0].live_in, -1 };
        for (const Tac_Param *p = f->fn->u.function.params; p; p = p->next) {
            int v = flow_var(f, p->name);
            if (v >= 0 && (flow_has(f->blocks[0].live_in, v) || !a->t->dead_param))
                interfere_def(v, &da);
        }
    }
    for (int v = 0; v < a->n; v++) {
        int h = hi_node(a, v);
        if (h >= 0) {
            add_edge(a, v, h);
            a->cross[h] = a->cross[v];
            a->cost[h]  = a->cost[v];
        }
    }
    xfree(depth);
    xfree(live);
}

// Merge y into x.
static void merge(Alloc *a, int x, int y)
{
    for (int v = 0; v < a->N; v++) {
        if (flow_has(row(a, y), v)) {
            flow_remove(row(a, v), y);
            add_edge(a, x, v);
        }
    }
    memset(row(a, y), 0, a->words * sizeof(Flow_Set));
    a->alias[y] = x;
    a->cost[x] += a->cost[y];
    if (!a->hint[x])
        a->hint[x] = a->hint[y];
    a->cross[x] = a->cross[x] || a->cross[y];
}

// Briggs: the merged node has fewer than K neighbours of significant degree.
static bool can_merge(const Alloc *a, int x, int y)
{
    bool cross  = a->cross[x];
    a->cross[x] = cross || a->cross[y];
    int k = k_of(a, x), significant = 0;
    a->cross[x] = cross;
    for (int v = 0; v < a->N; v++) {
        bool nx = flow_has(row(a, x), v), ny = flow_has(row(a, y), v);
        if (!nx && !ny)
            continue;
        int d = degree(a, v) - (nx && ny);
        if (d >= k)
            significant++;
    }
    return significant < k;
}

static void coalesce(Alloc *a)
{
    const Flow *f = a->flow;
    bool changed  = true;
    while (changed) {
        changed = false;
        for (int i = 0; i < f->ninstrs; i++) {
            int dst, src = move_source(a, f->instrs[i], &dst);
            if (src < 0)
                continue;
            int x = find(a, dst), y = find(a, src);
            if (x == y || flow_has(row(a, x), y) || !can_merge(a, x, y))
                continue;
            int hd = hi_node(a, dst);
            if (hd >= 0) {
                int hx = find(a, hd), hy = find(a, hi_node(a, src));
                if (flow_has(row(a, hx), hy) || !can_merge(a, hx, hy))
                    continue;
                merge(a, hx, hy);
            }
            merge(a, x, y);
            changed = true;
        }
    }
}

static bool in_pool(const int *pool, int k, int reg)
{
    for (int i = 0; i < k; i++)
        if (pool[i] == reg)
            return true;
    return false;
}

static void color(Alloc *a)
{
    int n      = a->N;
    int *deg   = xalloc((n ? n : 1) * sizeof(int), __func__, __FILE__, __LINE__);
    int *stack = xalloc((n ? n : 1) * sizeof(int), __func__, __FILE__, __LINE__);
    bool *gone = xalloc((n ? n : 1) * sizeof(bool), __func__, __FILE__, __LINE__);
    int nstack = 0, active = 0;
    for (int v = 0; v < n; v++) {
        gone[v] = !a->cand[v] || find(a, v) != v;
        deg[v]  = gone[v] ? 0 : degree(a, v);
        active += !gone[v];
    }
    while (active > 0) {
        int pick = -1;
        for (int v = 0; v < n && pick < 0; v++)
            if (!gone[v] && deg[v] < k_of(a, v))
                pick = v;
        if (pick < 0) {
            // Optimistically push the cheapest per neighbour; it may still find a colour.
            double best = 0;
            for (int v = 0; v < n; v++) {
                if (gone[v])
                    continue;
                double c = a->cost[v] / (deg[v] + 1);
                if (pick < 0 || c < best) {
                    pick = v;
                    best = c;
                }
            }
        }
        gone[pick]      = true;
        stack[nstack++] = pick;
        active--;
        for (int v = 0; v < n; v++)
            if (flow_has(row(a, pick), v))
                deg[v]--;
    }
    // Parameters first (or what they were coalesced into), in the registers they
    // arrive in, unless two coalesced parameters want different ones.
    for (const Tac_Param *p = a->flow->fn->u.function.params; p; p = p->next) {
        int pv = flow_var(a->flow, p->name);
        if (pv < 0)
            continue;
        const int words[2] = { pv, hi_node(a, pv) };
        for (int i = 0; i < 2; i++) {
            int v = words[i], r = v >= 0 ? find(a, v) : -1;
            if (v < 0 || !a->cand[v] || a->color[r] ||
                !in_pool(pool_of(a, r), k_of(a, r), a->hint[v]))
                continue;
            bool ok = true;
            for (int u = 0; u < n && ok; u++)
                ok = !(flow_has(row(a, r), u) && a->color[u] == a->hint[v]);
            if (ok)
                a->color[r] = a->hint[v];
        }
    }
    while (nstack > 0) {
        int v           = stack[--nstack];
        const int *pool = pool_of(a, v);
        for (int c = -1; c < k_of(a, v) && !a->color[v]; c++) {
            int reg = c < 0 ? a->hint[v] : pool[c];
            bool ok = c >= 0 || in_pool(pool, k_of(a, v), reg);
            for (int u = 0; u < n && ok; u++)
                ok = !(flow_has(row(a, v), u) && a->color[u] == reg);
            if (ok)
                a->color[v] = reg;
        }
    }
    xfree(gone);
    xfree(stack);
    xfree(deg);
}

// Hints: a parameter's incoming register, the result register for a returned variable
// (two for a pair).
static void find_hints(Alloc *a)
{
    const Flow *f = a->flow;
    StringMap params, params_hi;
    map_init(&params);
    map_init(&params_hi);
    a->t->param_hints(a->t->arg, &params, &params_hi);
    for (int v = 0; v < a->n; v++) {
        intptr_t r;
        if (map_get(&params, f->names[v], &r))
            a->hint[v] = (int)r;
        if (hi_node(a, v) >= 0 && map_get(&params_hi, f->names[v], &r))
            a->hint[hi_node(a, v)] = (int)r;
    }
    map_destroy(&params_hi);
    map_destroy(&params);
    for (int i = 0; i < f->ninstrs; i++) {
        const Tac_Instruction *in = f->instrs[i];
        if ((in->kind == TAC_INSTRUCTION_FUN_CALL ||
             in->kind == TAC_INSTRUCTION_FUN_CALL_NORETURN) &&
            (!a->t->inline_call || !a->t->inline_call(a->t->arg, in)))
            a->t->call_hints(a->t->arg, f, in, a->hint);
        if (in->kind == TAC_INSTRUCTION_RETURN && in->u.return_.src &&
            in->u.return_.src->kind == TAC_VAL_VAR) {
            int v = flow_var(f, in->u.return_.src->u.var_name);
            if (v >= 0 && a->cand[v] && !a->hint[v])
                a->hint[v] = a->fp[v] ? a->t->ret_fp : a->t->ret_int;
            if (v >= 0 && hi_node(a, v) >= 0 && !a->hint[hi_node(a, v)])
                a->hint[hi_node(a, v)] = a->t->ret_int_hi;
        }
    }
}

static bool is_param(const Flow *f, int v)
{
    for (const Tac_Param *p = f->fn->u.function.params; p; p = p->next)
        if (flow_var(f, p->name) == v)
            return true;
    return false;
}

static void *zalloc(size_t size)
{
    void *p = xalloc(size ? size : 1, __func__, __FILE__, __LINE__);
    memset(p, 0, size ? size : 1);
    return p;
}

void regalloc(const RegAlloc_Target *t, const Tac_TopLevel *fn)
{
    Alloc a = { .t = t, .flow = flow_build(fn) };
    int n = a.n = a.flow->nvars;
    int N = a.N = 2 * n;
    a.words     = (N + 63) / 64;
    a.cand      = zalloc(N * sizeof(bool));
    a.fp        = zalloc(N * sizeof(bool));
    a.adj       = zalloc((size_t)N * a.words * sizeof(Flow_Set));
    a.alias     = zalloc(N * sizeof(int));
    a.cost      = zalloc(N * sizeof(double));
    a.color     = zalloc(N * sizeof(int));
    a.hint      = zalloc(N * sizeof(int));
    a.cross     = zalloc(N * sizeof(bool));
    for (int v = 0; v < N; v++)
        a.alias[v] = v;

    find_candidates(&a);
    build(&a);
    find_hints(&a);
    coalesce(&a);
    color(&a);

    for (int v = 0; v < n; v++) {
        int reg = a.cand[v] ? a.color[find(&a, v)] : 0;
        int h   = hi_node(&a, v);
        int hi  = h >= 0 ? a.color[find(&a, h)] : 0;
        if (!reg || (h >= 0 && !hi))
            continue; // a pair gets both registers or neither
        t->assign(t->arg, a.flow->names[v], reg, hi);
        if (t->dead_param && is_param(a.flow, v) && a.flow->nblocks > 0 &&
            !flow_has(a.flow->blocks[0].live_in, v))
            t->dead_param(t->arg, a.flow->names[v]);
    }

    xfree(a.cross);
    xfree(a.hint);
    xfree(a.color);
    xfree(a.cost);
    xfree(a.alias);
    xfree(a.adj);
    xfree(a.fp);
    xfree(a.cand);
    flow_free(a.flow);
}
