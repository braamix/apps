//
// Local coalescing: locals whose values are never live at once share one.
//
// Liveness is solved over the finished code itself, instruction by instruction: a
// branch goes to the end of the block or if it names, or to the start of the loop.
// Two locals interfere when one is written while the other is live, but for a copy
// (local.get y; local.set x), which leaves them equal.  At the entry every parameter
// and every local read before it is written (a local starts at zero) is live at once.
// The locals are then coloured greedily, in order, each taking the first local of its
// type no interfering one holds, the parameters keeping their own; the source of a
// copy is tried first, so that the copy disappears.
//
#include <string.h>

#include "internal.h"
#include "xalloc.h"

typedef uint64_t Set;

static bool has(const Set *s, int i)
{
    return (s[i / 64] >> (i % 64)) & 1;
}

static void add(Set *s, int i)
{
    s[i / 64] |= (uint64_t)1 << (i % 64);
}

typedef struct {
    int n;             // instructions
    Wasm_Instr **code; // by index
    int *start;        // instruction i's successors: succ[start[i]..start[i+1]-1]
    int *succ;
} Graph;

static bool opens(Wasm_Op op)
{
    return op == WASM_BLOCK || op == WASM_LOOP || op == WASM_IF;
}

static bool is_end(Wasm_Op op)
{
    return op == WASM_END_BLOCK || op == WASM_END_LOOP || op == WASM_END_IF;
}

// Where a branch to the construct opened at instruction o goes.
static int target(const Graph *g, const int *end, int o)
{
    return g->code[o]->op == WASM_LOOP ? o : end[o];
}

static void build_graph(const Wasm_Func *fn, Graph *g)
{
    int n = 0;
    for (const Wasm_Instr *in = fn->first; in; in = in->next)
        n++;
    g->n    = n;
    g->code = xalloc((n + 1) * sizeof(Wasm_Instr *), __func__, __FILE__, __LINE__);
    int i   = 0;
    for (Wasm_Instr *in = fn->first; in; in = in->next)
        g->code[i++] = in;

    // The end of each construct, and the else of each if.
    int *end   = xalloc((n + 1) * sizeof(int), __func__, __FILE__, __LINE__);
    int *els   = xalloc((n + 1) * sizeof(int), __func__, __FILE__, __LINE__);
    int *stack = xalloc((n + 1) * sizeof(int), __func__, __FILE__, __LINE__);
    int sp     = 0;
    for (i = 0; i < n; i++) {
        Wasm_Op op = g->code[i]->op;
        els[i]     = -1;
        if (opens(op))
            stack[sp++] = i;
        else if (op == WASM_ELSE)
            els[stack[sp - 1]] = i;
        else if (is_end(op))
            end[stack[--sp]] = i;
    }

    // The successors, at most two but for br_table.
    int cap = 2 * n + 1;
    for (i = 0; i < n; i++)
        if (g->code[i]->op == WASM_BR_TABLE)
            cap += g->code[i]->ntable;
    g->start = xalloc((n + 1) * sizeof(int), __func__, __FILE__, __LINE__);
    g->succ  = xalloc(cap * sizeof(int), __func__, __FILE__, __LINE__);
    int k    = 0;
    sp       = 0;
    for (i = 0; i < n; i++) {
        const Wasm_Instr *in = g->code[i];
        g->start[i]          = k;
        switch (in->op) {
        case WASM_BLOCK:
        case WASM_LOOP:
            stack[sp++]  = i;
            g->succ[k++] = i + 1;
            break;
        case WASM_IF:
            stack[sp++]  = i;
            g->succ[k++] = i + 1;
            g->succ[k++] = els[i] >= 0 ? els[i] + 1 : end[i];
            break;
        case WASM_ELSE:
            g->succ[k++] = end[stack[sp - 1]];
            break;
        case WASM_END_BLOCK:
        case WASM_END_LOOP:
        case WASM_END_IF:
            sp--;
            if (i + 1 < n)
                g->succ[k++] = i + 1;
            break;
        case WASM_BR:
            g->succ[k++] = target(g, end, stack[sp - 1 - in->imm]);
            break;
        case WASM_BR_IF:
            g->succ[k++] = target(g, end, stack[sp - 1 - in->imm]);
            g->succ[k++] = i + 1;
            break;
        case WASM_BR_TABLE:
            for (int j = 0; j < in->ntable; j++)
                g->succ[k++] = target(g, end, stack[sp - 1 - in->table[j]]);
            break;
        case WASM_RETURN:
        case WASM_UNREACHABLE:
            break;
        default:
            if (i + 1 < n)
                g->succ[k++] = i + 1;
            break;
        }
    }
    g->start[n] = k;
    xfree(end);
    xfree(els);
    xfree(stack);
}

static void free_graph(Graph *g)
{
    xfree(g->code);
    xfree(g->start);
    xfree(g->succ);
}

// The group of local v.
static int find(int *rep, int v)
{
    while (rep[v] != v)
        v = rep[v] = rep[rep[v]];
    return v;
}

// Whether sets a and b have a member in common.
static bool meets(const Set *a, const Set *b, int words)
{
    for (int w = 0; w < words; w++)
        if (a[w] & b[w])
            return true;
    return false;
}

static bool writes(const Wasm_Instr *in)
{
    return in->op == WASM_LOCAL_SET || in->op == WASM_LOCAL_TEE;
}

void wasm_coalesce_locals(Wasm_Func *fn)
{
    int nv = fn->nparams + fn->nlocals;
    if (fn->nlocals == 0)
        return;
    Graph g;
    build_graph(fn, &g);
    int n = g.n, words = (nv + 63) / 64;

    // Liveness: live[i] before instruction i.
    Set *live = xalloc(((size_t)(n + 1) * words + 1) * sizeof(Set), __func__, __FILE__, __LINE__);
    Set *out  = xalloc((words + 1) * sizeof(Set), __func__, __FILE__, __LINE__);
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = n - 1; i >= 0; i--) {
            memset(out, 0, words * sizeof(Set));
            for (int j = g.start[i]; j < g.start[i + 1]; j++)
                for (int w = 0; w < words; w++)
                    out[w] |= live[(size_t)g.succ[j] * words + w];
            const Wasm_Instr *in = g.code[i];
            if (writes(in))
                out[in->imm / 64] &= ~((uint64_t)1 << (in->imm % 64));
            if (in->op == WASM_LOCAL_GET)
                add(out, (int)in->imm);
            Set *l = &live[(size_t)i * words];
            if (memcmp(l, out, words * sizeof(Set)) != 0) {
                memcpy(l, out, words * sizeof(Set));
                changed = true;
            }
        }
    }

    // Interference, and the copies.
    Set *conflict = xalloc(((size_t)nv * words + 1) * sizeof(Set), __func__, __FILE__, __LINE__);
    bool *used    = xalloc((nv + 1) * sizeof(bool), __func__, __FILE__, __LINE__);
    int *copies   = xalloc((2 * n + 1) * sizeof(int), __func__, __FILE__, __LINE__);
    int ncopies   = 0;
    for (int i = 0; i < n; i++) {
        const Wasm_Instr *in = g.code[i];
        if (wasm_op_form(in->op) == WASM_FORM_LOCAL)
            used[in->imm] = true;
        if (!writes(in))
            continue;
        int x = (int)in->imm, copy = -1;
        const Wasm_Instr *p = i > 0 ? g.code[i - 1] : NULL;
        if (p && (p->op == WASM_LOCAL_GET || p->op == WASM_LOCAL_TEE) && p->imm != x) {
            copy              = (int)p->imm;
            copies[ncopies++] = x;
            copies[ncopies++] = copy;
        }
        // Live after i: the union over its successors.
        memset(out, 0, words * sizeof(Set));
        for (int j = g.start[i]; j < g.start[i + 1]; j++)
            for (int w = 0; w < words; w++)
                out[w] |= live[(size_t)g.succ[j] * words + w];
        for (int y = 0; y < nv; y++)
            if (y != x && y != copy && has(out, y)) {
                add(&conflict[(size_t)x * words], y);
                add(&conflict[(size_t)y * words], x);
            }
    }
    for (int p = 0; p < fn->nparams; p++)
        used[p] = true;
    for (int x = 0; x < nv; x++) {
        if (!(x < fn->nparams || (n && has(live, x))))
            continue;
        for (int y = 0; y < nv; y++)
            if (y != x && (y < fn->nparams || (n && has(live, y))))
                add(&conflict[(size_t)x * words], y);
    }

    // The locals of a copy that never interfere become one group, its conflicts
    // those of all its members; a group holds one parameter at most.
    int *rep            = xalloc((nv + 1) * sizeof(int), __func__, __FILE__, __LINE__);
    int *param          = xalloc((nv + 1) * sizeof(int), __func__, __FILE__, __LINE__);
    Wasm_ValType *types = xalloc((nv + 1) * sizeof(Wasm_ValType), __func__, __FILE__, __LINE__);
    Set *members = xalloc(((size_t)nv * words + 1) * sizeof(Set), __func__, __FILE__, __LINE__);
    for (int v = 0; v < nv; v++) {
        rep[v]   = v;
        param[v] = v < fn->nparams ? v : -1;
        types[v] = v < fn->nparams ? fn->params[v] : fn->locals[v - fn->nparams];
        add(&members[(size_t)v * words], v);
    }
    for (int k = 0; k < ncopies; k += 2) {
        int a = find(rep, copies[k]), b = find(rep, copies[k + 1]);
        if (a == b || types[a] != types[b] || (param[a] >= 0 && param[b] >= 0) ||
            meets(&conflict[(size_t)a * words], &members[(size_t)b * words], words))
            continue;
        if (param[b] >= 0) { // the parameter's group keeps its number
            int t = a;
            a     = b;
            b     = t;
        }
        rep[b] = a;
        for (int w = 0; w < words; w++) {
            conflict[(size_t)a * words + w] |= conflict[(size_t)b * words + w];
            members[(size_t)a * words + w] |= members[(size_t)b * words + w];
        }
    }

    // Colouring, group by group: a parameter's keeps it; another takes the first
    // local of its type that no group it interferes with holds.
    int *color          = xalloc((nv + 1) * sizeof(int), __func__, __FILE__, __LINE__);
    Wasm_ValType *ctype = xalloc((nv + 1) * sizeof(Wasm_ValType), __func__, __FILE__, __LINE__);
    Set *held   = xalloc(((size_t)nv * words + 1) * sizeof(Set), __func__, __FILE__, __LINE__);
    int ncolors = fn->nparams;
    for (int v = 0; v < nv; v++)
        color[v] = -1;
    for (int p = 0; p < fn->nparams; p++) {
        int r    = find(rep, p);
        color[r] = p;
        ctype[p] = fn->params[p];
        for (int w = 0; w < words; w++)
            held[(size_t)p * words + w] = members[(size_t)r * words + w];
    }
    for (int v = fn->nparams; v < nv; v++) {
        int r = find(rep, v);
        if (!used[v] || color[r] >= 0)
            continue;
        int pick = -1;
        for (int c = 0; c < ncolors && pick < 0; c++)
            if (ctype[c] == types[r] &&
                !meets(&conflict[(size_t)r * words], &held[(size_t)c * words], words))
                pick = c;
        if (pick < 0) {
            pick        = ncolors++;
            ctype[pick] = types[r];
        }
        color[r] = pick;
        for (int w = 0; w < words; w++)
            held[(size_t)pick * words + w] |= members[(size_t)r * words + w];
    }
    for (int v = 0; v < nv; v++)
        color[v] = color[find(rep, v)];

    // The new locals.
    for (int c = fn->nparams; c < ncolors; c++)
        fn->locals[c - fn->nparams] = ctype[c];
    fn->nlocals = ncolors - fn->nparams;
    for (Wasm_Instr *in = fn->first; in; in = in->next)
        if (wasm_op_form(in->op) == WASM_FORM_LOCAL)
            in->imm = color[in->imm];
    if (fn->frame >= 0)
        fn->frame = color[fn->frame];

    xfree(live);
    xfree(out);
    xfree(conflict);
    xfree(used);
    xfree(copies);
    xfree(rep);
    xfree(param);
    xfree(color);
    xfree(types);
    xfree(ctype);
    xfree(members);
    xfree(held);
    free_graph(&g);
}
