//
// Control flow.  TAC jumps to labels; wasm only branches out of an enclosing block (to
// its end) or loop (to its start).  A reducible graph becomes blocks, loops and ifs by
// Ramsey's translation ("Beyond Relooper", ICFP 2022); any graph, an irreducible one
// included, the dispatch skeleton.
//
// Ramsey's translation walks the dominator tree, the blocks numbered in reverse
// postorder.  A block with a backward jump into it heads a loop around its subtree.  A
// merge node, a block with two forward jumps into it or more, follows a wasm block
// around the code of the earlier parts of its dominator's subtree, the latest one
// outermost, so a jump to it leaves that block.  Any other block has a single forward
// predecessor, which dominates it, and is placed right at that jump.  A conditional
// jump is an if whose arms are the two jumps.  A jump finds its target in the context,
// the stack of enclosing constructs: a backward one continues its loop, a forward one
// to a merge node leaves its block.
//
// An irreducible graph is made reducible first, as LLVM's FixIrreducibleControlFlow
// does: each strongly connected region with several entries gets a dispatch node of its
// own, and every jump to one of those entries goes through it, setting the state local
// to the entry's index; the dispatch node is a br_table on the state.  The region is
// then a loop headed by the dispatch node, and the regions inside it are fixed the same
// way.  Ramsey's translation runs over that graph, where each entry of a dispatch node
// counts as a merge node, so the br_table leaves the block in front of it.
//
// The skeleton: the basic blocks in their order, block i's code just after the end of
// a wasm block B_i, with B_i enclosing every earlier block's code:
//     loop                        only with a backward jump
//       block B_n-1
//         ...
//           block B_0             only with a backward jump
//             local.get state
//             br_table {0, 1, ..., n-1, 0}
//           end_block
//           <code of block 0>
//         end_block               B_1
//         <code of block 1>
//       ...
//       end_block                 B_n-1
//       <code of block n-1>
//     end_loop
//
// A forward jump from block i to block j leaves B_j, which encloses i's code: br j-i-1.
// A backward one sets the state to j and restarts the loop, br n-1-i, whose br_table
// leaves B_j.  Falling through is running into the next block's code.
//
#include <stdlib.h>

#include "flow.h"
#include "internal.h"
#include "xalloc.h"

bool wasm_structure = true;
bool wasm_regional  = true;

// The block a jump to label `target` goes to.
static int target_block(const Gen *g, const char *target)
{
    intptr_t b;
    if (!map_get(&g->labels, target, &b))
        internal_error("wasm: %s: no label %s", g->fn->name, target);
    return (int)b;
}

void gen_branch_setup(Gen *g, const char *target)
{
    int j = target_block(g, target);
    if (j <= g->cur) {
        wasm_append(g->fn, WASM_I32_CONST)->imm = j;
        wasm_append(g->fn, WASM_LOCAL_SET)->imm = g->state;
    }
}

void gen_branch(Gen *g, const char *target, bool conditional)
{
    int j     = target_block(g, target);
    int depth = j > g->cur ? j - g->cur - 1 : g->nblocks - 1 - g->cur;
    wasm_append(g->fn, conditional ? WASM_BR_IF : WASM_BR)->imm = depth;
}

// The dispatch skeleton.
static void gen_dispatch(Gen *g)
{
    const Flow *f = g->flow;
    g->nblocks    = f->nblocks;
    map_init(&g->labels);
    for (int b = 0; b < f->nblocks; b++)
        for (int i = f->blocks[b].first; i <= f->blocks[b].last; i++)
            if (f->instrs[i]->kind == TAC_INSTRUCTION_LABEL)
                map_insert(&g->labels, f->instrs[i]->u.label.name, b, 0);

    // A backward jump needs the loop and the state.
    bool loop = false;
    for (int b = 0; b < f->nblocks; b++)
        for (int k = 0; k < f->blocks[b].nsucc; k++)
            if (f->blocks[b].succ[k] <= b)
                loop = true;
    Wasm_Func *fn = g->fn;
    if (loop) {
        g->state = wasm_add_local(fn, WASM_I32);
        wasm_append(fn, WASM_LOOP);
    }
    for (int b = f->nblocks - 1; b >= (loop ? 0 : 1); b--)
        wasm_append(fn, WASM_BLOCK);
    if (loop) {
        wasm_append(fn, WASM_LOCAL_GET)->imm = g->state;
        Wasm_Instr *in                       = wasm_append(fn, WASM_BR_TABLE);
        in->ntable                           = f->nblocks + 1;
        in->table = xalloc(in->ntable * sizeof(int), __func__, __FILE__, __LINE__);
        for (int b = 0; b < f->nblocks; b++)
            in->table[b] = b;
        in->table[f->nblocks] = 0;
    }
    for (int b = 0; b < f->nblocks; b++) {
        if (b > 0 || loop)
            wasm_append(fn, WASM_END_BLOCK);
        g->cur = b;
        for (int i = f->blocks[b].first; i <= f->blocks[b].last; i++)
            gen_instr(g, f->instrs[i]);
    }
    if (loop)
        wasm_append(fn, WASM_END_LOOP);
    map_destroy(&g->labels);
}

//
// Ramsey's translation.
//
enum { CTX_IF, CTX_LOOP, CTX_BLOCK };

typedef struct {
    int from, to;   // a jump from block `from` to block `to`
    int via, index; // goes to dispatch node `via` with the state `index`
} Redirect;

typedef struct {
    Gen *g;
    const Flow *f;
    int n; // the basic blocks, 0..n-1; dispatch nodes follow, up to N-1
    int N, cap;
    int *nsucc; // node → its successors in the graph translated
    int **succ;
    Redirect *rd;
    int nrd, caprd;
    bool every;   // redirect every jump to an entry, not only those from outside or back
    int *rpo;     // block → its number in reverse postorder, -1 when unreachable
    int *order;   // number → block
    int count;    // reachable blocks
    int *idom;    // block → its immediate dominator
    int *nfwd;    // block → forward jumps into it
    bool *header; // block → a backward jump into it: a loop header
    struct {
        int kind, block;
    } *ctx;
    int nctx;
} Structure;

static int intersect(const Structure *s, int a, int b)
{
    while (a != b) {
        while (s->rpo[a] > s->rpo[b])
            a = s->idom[a];
        while (s->rpo[b] > s->rpo[a])
            b = s->idom[b];
    }
    return a;
}

static bool dominates(const Structure *s, int d, int b)
{
    for (;;) {
        if (b == d)
            return true;
        if (b == 0)
            return false;
        b = s->idom[b];
    }
}

// Number the blocks, find dominators, loop headers and merge nodes; false when the
// graph is irreducible (a backward jump to a block that does not dominate its source).
static bool analyze(Structure *s)
{
    int n = s->N;
    for (int b = 0; b < n; b++)
        s->rpo[b] = s->idom[b] = -1;

    // Postorder by an explicit depth-first walk, then reversed.
    int *stack = xalloc((n + 1) * sizeof(int), __func__, __FILE__, __LINE__);
    int *next  = xalloc((n + 1) * sizeof(int), __func__, __FILE__, __LINE__);
    int *post  = xalloc((n + 1) * sizeof(int), __func__, __FILE__, __LINE__);
    bool *seen = xalloc((n + 1) * sizeof(bool), __func__, __FILE__, __LINE__);
    for (int b = 0; b < n; b++)
        seen[b] = false;
    int sp = 0, np = 0;
    stack[sp]  = 0;
    next[sp++] = 0;
    seen[0]    = true;
    while (sp) {
        int b = stack[sp - 1];
        if (next[sp - 1] < s->nsucc[b]) {
            int t = s->succ[b][next[sp - 1]++];
            if (!seen[t]) {
                seen[t]    = true;
                stack[sp]  = t;
                next[sp++] = 0;
            }
        } else {
            post[np++] = b;
            sp--;
        }
    }
    s->count = np;
    for (int i = 0; i < np; i++) {
        s->order[i]              = post[np - 1 - i];
        s->rpo[post[np - 1 - i]] = i;
    }
    xfree(stack);
    xfree(next);
    xfree(post);
    xfree(seen);

    // Dominators (Cooper, Harvey and Kennedy), over the predecessors.
    int *start = xalloc((n + 1) * sizeof(int), __func__, __FILE__, __LINE__);
    int nedges = 0;
    for (int b = 0; b < n; b++)
        nedges += s->nsucc[b];
    int *preds = xalloc((nedges + 1) * sizeof(int), __func__, __FILE__, __LINE__);
    for (int b = 0; b <= n; b++)
        start[b] = 0;
    for (int i = 0; i < s->count; i++)
        for (int k = 0; k < s->nsucc[s->order[i]]; k++)
            start[s->succ[s->order[i]][k] + 1]++;
    for (int b = 0; b < n; b++)
        start[b + 1] += start[b];
    int *fill = xalloc((n + 1) * sizeof(int), __func__, __FILE__, __LINE__);
    for (int b = 0; b < n; b++)
        fill[b] = start[b];
    for (int i = 0; i < s->count; i++) {
        int p = s->order[i];
        for (int k = 0; k < s->nsucc[p]; k++)
            preds[fill[s->succ[p][k]]++] = p;
    }
    xfree(fill);
    s->idom[0]   = 0;
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 1; i < s->count; i++) {
            int b = s->order[i], nd = -1;
            for (int j = start[b]; j < start[b + 1]; j++) {
                int p = preds[j];
                if (s->idom[p] >= 0)
                    nd = nd < 0 ? p : intersect(s, p, nd);
            }
            if (nd != s->idom[b]) {
                s->idom[b] = nd;
                changed    = true;
            }
        }
    }
    xfree(start);
    xfree(preds);

    // Loop headers and merge nodes.
    for (int b = 0; b < n; b++) {
        s->nfwd[b]   = 0;
        s->header[b] = false;
    }
    for (int i = 0; i < s->count; i++) {
        int p = s->order[i];
        for (int k = 0; k < s->nsucc[p]; k++) {
            int t = s->succ[p][k];
            if (s->rpo[t] <= s->rpo[p]) {
                if (!dominates(s, t, p))
                    return false;
                s->header[t] = true;
            } else {
                s->nfwd[t]++;
            }
        }
    }
    // The entries of a dispatch node are left by its br_table, as merge nodes are, and
    // so are the targets of a jump table.
    for (int d = 0; d < n; d++) {
        if (d < s->n && s->f->instrs[s->f->blocks[d].last]->kind != TAC_INSTRUCTION_JUMP_TABLE)
            continue;
        for (int k = 0; k < s->nsucc[d]; k++)
            if (s->nfwd[s->succ[d][k]] < 2)
                s->nfwd[s->succ[d][k]] = 2;
    }
    return true;
}

//
// Making the graph reducible.
//

// The redirection of the jump from block `from` to block `to`, or NULL.
static const Redirect *redirect_of(const Structure *s, int from, int to)
{
    for (int i = 0; i < s->nrd; i++)
        if (s->rd[i].from == from && s->rd[i].to == to)
            return &s->rd[i];
    return NULL;
}

// Where the jump from block `from` to block `to` goes in the graph translated.
static int effective(const Structure *s, int from, int to)
{
    const Redirect *r = redirect_of(s, from, to);
    return r ? r->via : to;
}

typedef struct {
    Structure *s;
    const bool *in; // the region
    int header;     // its header, whose incoming edges are left out, or -1
    int *index, *low, *stack, *comp;
    bool *on;
    int sp, counter, ncomp;
} Scc;

static bool edge_in(const Scc *c, int t)
{
    return c->in[t] && t != c->header;
}

// Tarjan's strongly connected components of the region.
static void strongconnect(Scc *c, int v)
{
    Structure *s = c->s;
    c->index[v] = c->low[v] = c->counter++;
    c->stack[c->sp++]       = v;
    c->on[v]                = true;
    for (int k = 0; k < s->nsucc[v]; k++) {
        int w = s->succ[v][k];
        if (!edge_in(c, w))
            continue;
        if (c->index[w] < 0) {
            strongconnect(c, w);
            if (c->low[w] < c->low[v])
                c->low[v] = c->low[w];
        } else if (c->on[w] && c->index[w] < c->low[v]) {
            c->low[v] = c->index[w];
        }
    }
    if (c->low[v] == c->index[v]) {
        int w;
        do {
            w          = c->stack[--c->sp];
            c->on[w]   = false;
            c->comp[w] = c->ncomp;
        } while (w != v);
        c->ncomp++;
    }
}

static bool fix_region(Structure *s, const bool *in, int header);

// The jumps of region `scc` that a depth-first walk from node v finds going back:
// back[base[u] + j] for the j-th successor of node u.
static void find_back(const Structure *s, const bool *scc, int v, char *mark, const int *base,
                      bool *back)
{
    mark[v] = 1;
    for (int j = 0; j < s->nsucc[v]; j++) {
        int w = s->succ[v][j];
        if (!scc[w])
            continue;
        if (mark[w] == 1)
            back[base[v] + j] = true;
        else if (mark[w] == 0)
            find_back(s, scc, w, mark, base, back);
    }
    mark[v] = 2;
}

// A node for a dispatch on the state to entries es[0..k-1] of region `scc`, the jumps to
// them from the rest of `region` redirected to it, and those inside the region that go
// back to one (with s->every, all of them).  A jump forward inside the region may stay:
// the dispatch node dominates its target all the same.  False when the node cannot be
// made (a jump to redirect leaves a dispatch node, or the region is entered at the
// function's start).
static bool add_dispatch(Structure *s, const bool *region, bool *scc, const int *es, int k)
{
    if (s->N == s->cap)
        return false;
    int d       = s->N++;
    s->nsucc[d] = k;
    s->succ[d]  = xalloc(k * sizeof(int), __func__, __FILE__, __LINE__);
    for (int i = 0; i < k; i++)
        s->succ[d][i] = es[i];
    scc[d]     = true;
    char *mark = xalloc(s->cap, __func__, __FILE__, __LINE__);
    int *base  = xalloc((s->N + 1) * sizeof(int), __func__, __FILE__, __LINE__);
    base[0]    = 0;
    for (int v = 0; v < s->N; v++)
        base[v + 1] = base[v] + s->nsucc[v];
    bool *back = xalloc((base[s->N] + 1) * sizeof(bool), __func__, __FILE__, __LINE__);
    for (int v = 0; v < s->cap; v++)
        mark[v] = 0;
    for (int j = 0; j < base[s->N]; j++)
        back[j] = false;
    find_back(s, scc, d, mark, base, back);
    xfree(mark);
    bool ok = true;
    for (int i = 0; i < k && ok; i++) {
        int e = es[i];
        if (e == 0) {
            ok = false;
            break;
        }
        for (int u = 0; u < d && ok; u++) {
            if (!region[u])
                continue;
            bool found = false;
            for (int j = 0; j < s->nsucc[u]; j++)
                if (s->succ[u][j] == e && (!scc[u] || s->every || back[base[u] + j])) {
                    s->succ[u][j] = d;
                    found         = true;
                }
            if (!found)
                continue;
            if (u >= s->n) {
                ok = false;
                break;
            }
            if (s->nrd == s->caprd) {
                s->caprd    = s->caprd ? 2 * s->caprd : 16;
                Redirect *r = xalloc(s->caprd * sizeof *r, __func__, __FILE__, __LINE__);
                for (int j = 0; j < s->nrd; j++)
                    r[j] = s->rd[j];
                xfree(s->rd);
                s->rd = r;
            }
            s->rd[s->nrd++] = (Redirect){ u, e, d, i };
        }
    }
    xfree(back);
    xfree(base);
    return ok && fix_region(s, scc, d);
}

// Give each region of `in` with several entries a dispatch node, the edges into
// `header` left out; then the same inside each region.
static bool fix_region(Structure *s, const bool *in, int header)
{
    int cap = s->cap;
    Scc c   = { .s = s, .in = in, .header = header };
    c.index = xalloc(cap * sizeof(int), __func__, __FILE__, __LINE__);
    c.low   = xalloc(cap * sizeof(int), __func__, __FILE__, __LINE__);
    c.stack = xalloc(cap * sizeof(int), __func__, __FILE__, __LINE__);
    c.comp  = xalloc(cap * sizeof(int), __func__, __FILE__, __LINE__);
    c.on    = xalloc(cap * sizeof(bool), __func__, __FILE__, __LINE__);
    int n   = s->N; // the nodes the components are of: those made below are not
    for (int v = 0; v < n; v++) {
        c.index[v] = -1;
        c.on[v]    = false;
        c.comp[v]  = -1;
    }
    for (int v = 0; v < n; v++)
        if (in[v] && v != header && c.index[v] < 0)
            strongconnect(&c, v);

    bool ok   = true;
    bool *scc = xalloc(cap * sizeof(bool), __func__, __FILE__, __LINE__);
    int *es   = xalloc(cap * sizeof(int), __func__, __FILE__, __LINE__);
    for (int k = 0; k < c.ncomp && ok; k++) {
        int size = 0, ne = 0;
        bool cycle = false;
        for (int v = 0; v < cap; v++)
            scc[v] = v < n && c.comp[v] == k;
        for (int v = 0; v < n; v++) {
            if (!scc[v])
                continue;
            size++;
            for (int j = 0; j < s->nsucc[v]; j++)
                if (s->succ[v][j] == v)
                    cycle = true;
        }
        if (size < 2 && !cycle)
            continue;
        // Its entries: entered from the rest of the region, or the function's start.
        for (int v = 0; v < n; v++) {
            if (!scc[v])
                continue;
            bool entry = v == 0;
            for (int u = 0; u < n && !entry; u++)
                if (in[u] && !scc[u])
                    for (int j = 0; j < s->nsucc[u]; j++)
                        if (s->succ[u][j] == v)
                            entry = true;
            if (entry)
                es[ne++] = v;
        }
        if (ne == 1)
            ok = fix_region(s, scc, es[0]);
        else
            ok = add_dispatch(s, in, scc, es, ne);
    }
    xfree(scc);
    xfree(es);
    xfree(c.index);
    xfree(c.low);
    xfree(c.stack);
    xfree(c.comp);
    xfree(c.on);
    return ok;
}

// The graph as the flow has it, with no dispatch node.
static void reset_graph(Structure *s)
{
    for (int b = 0; b < s->n; b++)
        for (int k = 0; k < s->nsucc[b]; k++)
            s->succ[b][k] = s->f->blocks[b].succ[k];
    for (int d = s->n; d < s->N; d++)
        xfree(s->succ[d]);
    s->N   = s->n;
    s->nrd = 0;
}

// The graph, made reducible: false when that fails.  Redirecting only the jumps from
// outside a region and those back is enough but for a region inside it that a dispatch
// node enters by two ways or more; then every jump to an entry is redirected.
static bool make_reducible(Structure *s)
{
    bool *in = xalloc(s->cap * sizeof(bool), __func__, __FILE__, __LINE__);
    for (int b = 0; b < s->cap; b++)
        in[b] = b < s->n && s->rpo[b] >= 0; // reachable
    bool ok = fix_region(s, in, -1);
    if (!ok) {
        reset_graph(s);
        s->every = true;
        ok       = fix_region(s, in, -1);
    }
    xfree(in);
    return ok;
}

static void push_ctx(Structure *s, int kind, int block)
{
    s->ctx[s->nctx].kind    = kind;
    s->ctx[s->nctx++].block = block;
}

// The depth of a br to the construct of `kind` for `block` in the context.
static int ctx_depth(const Structure *s, int kind, int block)
{
    for (int i = s->nctx - 1; i >= 0; i--)
        if (s->ctx[i].kind == kind && s->ctx[i].block == block)
            return s->nctx - 1 - i;
    internal_error("wasm: %s: no enclosing construct for block %d", s->g->fn->name, block);
}

static void do_tree(Structure *s, int x);

// To node `to` of the graph translated, from block or dispatch node `from`.
static void do_branch(Structure *s, int from, int to)
{
    if (s->rpo[to] <= s->rpo[from])
        wasm_append(s->g->fn, WASM_BR)->imm = ctx_depth(s, CTX_LOOP, to);
    else if (s->nfwd[to] >= 2)
        wasm_append(s->g->fn, WASM_BR)->imm = ctx_depth(s, CTX_BLOCK, to);
    else
        do_tree(s, to);
}

// Whether going from block x to node `to` is a bare br: backward, or to a merge node.
static bool is_br(const Structure *s, int from, int to)
{
    return s->rpo[to] <= s->rpo[from] || s->nfwd[to] >= 2;
}

// The state for a jump from block x to block `to` that goes through a dispatch node.
static void set_state(Structure *s, int x, int to)
{
    const Redirect *r = redirect_of(s, x, to);
    if (r) {
        wasm_append(s->g->fn, WASM_I32_CONST)->imm = r->index;
        wasm_append(s->g->fn, WASM_LOCAL_SET)->imm = s->g->state;
    }
}

// The jump from block x to block `to`.
static void do_jump(Structure *s, int x, int to)
{
    set_state(s, x, to);
    do_branch(s, x, effective(s, x, to));
}

// Running on from the end of block x: into the next, or off the end of the function.
static void do_fall(Structure *s, int x)
{
    if (x + 1 < s->n)
        do_jump(s, x, x + 1);
    else if (s->g->fn->result == WASM_VOID)
        gen_return(s->g, NULL);
    else
        wasm_append(s->g->fn, WASM_UNREACHABLE);
}

static int target_of(const Gen *g, const char *label)
{
    intptr_t b;
    if (!map_get(&g->labels, label, &b))
        internal_error("wasm: %s: no label %s", g->fn->name, label);
    return (int)b;
}

// Block x's jump table: a br_table, each of whose targets is a merge node (analyze).  A
// target that goes through a dispatch node needs the state set on the way, which a
// br_table cannot do: it leaves a block of its own instead, after which the state is
// set and the jump made.
static void jump_table(Structure *s, int x, const Tac_Instruction *in)
{
    Wasm_Func *fn = s->g->fn;
    int count     = in->u.jump_table.count;
    int *tr       = xalloc((count + 1) * sizeof(int), __func__, __FILE__, __LINE__);
    int ntr       = 0;
    for (int i = 0; i <= count; i++) {
        int t = target_of(
            s->g, i < count ? in->u.jump_table.targets[i] : in->u.jump_table.default_target);
        bool seen = false;
        for (int j = 0; j < ntr; j++)
            seen |= tr[j] == t;
        if (!seen && redirect_of(s, x, t))
            tr[ntr++] = t;
    }
    for (int j = 0; j < ntr; j++) {
        wasm_append(fn, WASM_BLOCK);
        push_ctx(s, CTX_IF, x); // a trampoline: no jump looks for it by name
    }
    push_val(s->g, in->u.jump_table.index, WASM_I32);
    Wasm_Instr *bt = wasm_append(fn, WASM_BR_TABLE);
    bt->ntable     = count + 1;
    bt->table      = xalloc(bt->ntable * sizeof(int), __func__, __FILE__, __LINE__);
    for (int i = 0; i <= count; i++) {
        int t = target_of(
            s->g, i < count ? in->u.jump_table.targets[i] : in->u.jump_table.default_target);
        int j = 0;
        while (j < ntr && tr[j] != t)
            j++;
        if (j < ntr) {
            bt->table[i] = ntr - 1 - j;
        } else {
            int to = effective(s, x, t);
            bt->table[i] =
                s->rpo[to] <= s->rpo[x] ? ctx_depth(s, CTX_LOOP, to) : ctx_depth(s, CTX_BLOCK, to);
        }
    }
    for (int j = ntr - 1; j >= 0; j--) {
        wasm_append(fn, WASM_END_BLOCK);
        s->nctx--;
        do_jump(s, x, tr[j]);
    }
    xfree(tr);
}

// Block x's code and its jumps, inside the blocks of merge nodes ys[0..k-1] (in
// reverse postorder, the last outermost).
static void node_within(Structure *s, int x, const int *ys, int k)
{
    Gen *g        = s->g;
    Wasm_Func *fn = g->fn;
    if (k > 0) {
        int y = ys[k - 1];
        wasm_append(fn, WASM_BLOCK);
        push_ctx(s, CTX_BLOCK, y);
        node_within(s, x, ys, k - 1);
        s->nctx--;
        wasm_append(fn, WASM_END_BLOCK);
        do_tree(s, y);
        return;
    }
    if (x >= s->n) {
        // A dispatch node: to the entry the state names, leaving the block before it.
        wasm_append(fn, WASM_LOCAL_GET)->imm = g->state;
        Wasm_Instr *bt                       = wasm_append(fn, WASM_BR_TABLE);
        bt->ntable                           = s->nsucc[x] + 1;
        bt->table = xalloc(bt->ntable * sizeof(int), __func__, __FILE__, __LINE__);
        for (int i = 0; i < s->nsucc[x]; i++)
            bt->table[i] = ctx_depth(s, CTX_BLOCK, s->succ[x][i]);
        bt->table[s->nsucc[x]] = bt->table[0];
        return;
    }
    const Flow_Block *blk     = &s->f->blocks[x];
    const Tac_Instruction *in = s->f->instrs[blk->last];
    bool jump = in->kind == TAC_INSTRUCTION_JUMP || in->kind == TAC_INSTRUCTION_JUMP_IF_ZERO ||
                in->kind == TAC_INSTRUCTION_JUMP_IF_NOT_ZERO ||
                in->kind == TAC_INSTRUCTION_JUMP_TABLE;
    g->cur    = x;
    for (int i = blk->first; i <= blk->last - (jump ? 1 : 0); i++)
        gen_instr(g, s->f->instrs[i]);
    switch (in->kind) {
    case TAC_INSTRUCTION_JUMP:
        do_jump(s, x, target_of(g, in->u.jump.target));
        return;
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO: {
        int t = target_of(g, in->u.jump_if_zero.target), e = x + 1;
        bool if_zero = in->kind == TAC_INSTRUCTION_JUMP_IF_ZERO;
        if (t == e) { // both ways to one place
            do_jump(s, x, t);
            return;
        }
        bool br_t = is_br(s, x, effective(s, x, t));
        if (br_t || (e < s->n && is_br(s, x, effective(s, x, e)))) {
            // One way is a bare br: a br_if, then the other way.  A state it sets is
            // read by nothing on the other way, which sets its own if it needs one.
            int orig = br_t ? t : e;
            int to   = effective(s, x, orig);
            set_state(s, x, orig);
            push_condition(g, in->u.jump_if_zero.condition, br_t ? if_zero : !if_zero);
            wasm_append(fn, WASM_BR_IF)->imm =
                s->rpo[to] <= s->rpo[x] ? ctx_depth(s, CTX_LOOP, to) : ctx_depth(s, CTX_BLOCK, to);
            if (br_t)
                do_fall(s, x);
            else
                do_jump(s, x, t);
            return;
        }
        push_condition(g, in->u.jump_if_zero.condition, if_zero);
        wasm_append(fn, WASM_IF);
        push_ctx(s, CTX_IF, x);
        do_jump(s, x, t);
        wasm_append(fn, WASM_ELSE);
        do_fall(s, x);
        s->nctx--;
        wasm_append(fn, WASM_END_IF);
        return;
    }
    case TAC_INSTRUCTION_JUMP_TABLE:
        jump_table(s, x, in);
        return;
    case TAC_INSTRUCTION_RETURN:
    case TAC_INSTRUCTION_FUN_CALL_NORETURN:
        return;
    default:
        do_fall(s, x);
        return;
    }
}

static const int *by_rpo_table;
static int by_rpo(const void *a, const void *b)
{
    return by_rpo_table[*(const int *)a] - by_rpo_table[*(const int *)b];
}

static void do_tree(Structure *s, int x)
{
    // The merge nodes x immediately dominates, in reverse postorder.
    int k   = 0;
    int *ys = xalloc((s->N + 1) * sizeof(int), __func__, __FILE__, __LINE__);
    for (int i = 0; i < s->count; i++) {
        int y = s->order[i];
        if (y != x && s->idom[y] == x && s->nfwd[y] >= 2)
            ys[k++] = y;
    }
    by_rpo_table = s->rpo;
    qsort(ys, k, sizeof(int), by_rpo);
    if (s->header[x]) {
        wasm_append(s->g->fn, WASM_LOOP);
        push_ctx(s, CTX_LOOP, x);
        node_within(s, x, ys, k);
        s->nctx--;
        wasm_append(s->g->fn, WASM_END_LOOP);
    } else {
        node_within(s, x, ys, k);
    }
    xfree(ys);
}

// Ramsey's translation of the body, or false (and nothing emitted) when its graph is
// irreducible.
static bool gen_structured(Gen *g)
{
    const Flow *f = g->flow;
    Structure s   = { .g = g, .f = f, .n = f->nblocks, .N = f->nblocks };
    s.cap         = 2 * s.n + 2; // a dispatch node has two entries or more
    int n         = s.cap;
    s.nsucc       = xalloc(n * sizeof(int), __func__, __FILE__, __LINE__);
    s.succ        = xalloc(n * sizeof(int *), __func__, __FILE__, __LINE__);
    for (int b = 0; b < s.n; b++) {
        s.nsucc[b] = f->blocks[b].nsucc;
        s.succ[b] =
            xalloc((s.nsucc[b] > 2 ? s.nsucc[b] : 2) * sizeof(int), __func__, __FILE__, __LINE__);
        for (int k = 0; k < f->blocks[b].nsucc; k++)
            s.succ[b][k] = f->blocks[b].succ[k];
    }
    s.rpo    = xalloc(n * sizeof(int), __func__, __FILE__, __LINE__);
    s.order  = xalloc(n * sizeof(int), __func__, __FILE__, __LINE__);
    s.idom   = xalloc(n * sizeof(int), __func__, __FILE__, __LINE__);
    s.nfwd   = xalloc(n * sizeof(int), __func__, __FILE__, __LINE__);
    s.header = xalloc(n * sizeof(bool), __func__, __FILE__, __LINE__);
    s.ctx    = xalloc(3 * n * sizeof(*s.ctx), __func__, __FILE__, __LINE__);
    bool ok  = analyze(&s);
    if (!ok && wasm_regional && make_reducible(&s)) {
        ok = analyze(&s);
        if (ok)
            g->state = wasm_add_local(g->fn, WASM_I32);
    }
    if (ok) {
        map_init(&g->labels);
        for (int b = 0; b < s.n; b++)
            for (int i = s.f->blocks[b].first; i <= s.f->blocks[b].last; i++)
                if (s.f->instrs[i]->kind == TAC_INSTRUCTION_LABEL)
                    map_insert(&g->labels, s.f->instrs[i]->u.label.name, b, 0);
        do_tree(&s, 0);
        map_destroy(&g->labels);
    }
    for (int b = 0; b < s.N; b++)
        xfree(s.succ[b]);
    xfree(s.succ);
    xfree(s.nsucc);
    xfree(s.rd);
    xfree(s.rpo);
    xfree(s.order);
    xfree(s.idom);
    xfree(s.nfwd);
    xfree(s.header);
    xfree(s.ctx);
    return ok;
}

void gen_body(Gen *g)
{
    if (!wasm_structure || g->flow->nblocks == 0 || !gen_structured(g))
        gen_dispatch(g);
}
