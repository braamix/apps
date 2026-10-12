//
// The function's code, rewritten on the stack machine's own terms once it is complete.
//
// Selection gives every TAC name a local and moves every value through one: each
// instruction pushes its operands with local.get and pops its result with local.set.
// Stackify leaves a value on the operand stack instead, when its local is set once
// and read once, later in the same straight run of code: either nothing between
// disturbs the stack below it (the code between pushes and pops as much as it takes
// back), or its expression has no effect and nothing between writes what it reads,
// and it moves down to its use.  A local set and read at once becomes local.tee; one
// never read is dropped, and with it the expression when that has no effect.  The
// peephole rewrites then fold tests into comparisons, constant additions into the
// offsets of loads and stores, and remove code after a branch, a branch to the next
// instruction, blocks and loops no branch names, and the return at the very end.
// Coalescing (locals.c) then shares locals whose values are never live at once.
//
#include <stdlib.h>
#include <string.h>

#include "internal.h"
#include "xalloc.h"

bool wasm_peephole = true;
bool wasm_stackify = true;
bool wasm_coalesce = true;

static bool is_get(const Wasm_Instr *in, int local)
{
    return in && in->op == WASM_LOCAL_GET && in->imm == local;
}

// Whether instruction `in` changes nothing but the stack: removing, repeating or
// moving it alters no state.  A load may trap, but only where C leaves the behaviour
// undefined; a division may too, so it is left alone.
static bool is_pure(const Wasm_Instr *in)
{
    int pops, pushes;
    if (in->barrier || !wasm_stack_effect(in, &pops, &pushes))
        return false;
    switch (in->op) {
    case WASM_CALL:
    case WASM_CALL_INDIRECT:
    case WASM_DROP:
    case WASM_LOCAL_SET:
    case WASM_LOCAL_TEE:
    case WASM_GLOBAL_SET:
    case WASM_MEMORY_GROW:
    case WASM_MEMORY_COPY:
    case WASM_MEMORY_FILL:
    case WASM_I32_DIV_S:
    case WASM_I32_DIV_U:
    case WASM_I32_REM_S:
    case WASM_I32_REM_U:
    case WASM_I64_DIV_S:
    case WASM_I64_DIV_U:
    case WASM_I64_REM_S:
    case WASM_I64_REM_U:
        return false;
    default:
        return !(in->op >= WASM_I32_STORE && in->op <= WASM_I64_STORE32);
    }
}

static bool is_load(Wasm_Op op)
{
    return op >= WASM_I32_LOAD && op <= WASM_I64_LOAD32_U;
}

static bool is_store(Wasm_Op op)
{
    return op >= WASM_I32_STORE && op <= WASM_I64_STORE32;
}

// Whether instruction `in` may write memory, as a store, a call or a volatile access.
static bool writes_memory(const Wasm_Instr *in)
{
    return in->barrier || is_store(in->op) || in->op == WASM_CALL || in->op == WASM_CALL_INDIRECT ||
           in->op == WASM_MEMORY_COPY || in->op == WASM_MEMORY_FILL || in->op == WASM_MEMORY_GROW;
}

// The first instruction of the expression whose value `last` leaves on the stack, or
// NULL when it is not a plain run of code.
static Wasm_Instr *expr_start(Wasm_Instr *last)
{
    int need = 1;
    for (Wasm_Instr *in = last; in; in = in->prev) {
        int pops, pushes;
        if (!wasm_stack_effect(in, &pops, &pushes))
            return NULL;
        need -= pushes;
        if (need < 0)
            return NULL;
        need += pops;
        if (need == 0)
            return in;
    }
    return NULL;
}

// How many times each local is read and written.
typedef struct {
    int n;
    int *gets, *sets;
} Counts;

static void count_locals(const Wasm_Func *fn, Counts *c)
{
    c->n    = fn->nparams + fn->nlocals;
    c->gets = xalloc((c->n + 1) * sizeof(int), __func__, __FILE__, __LINE__);
    c->sets = xalloc((c->n + 1) * sizeof(int), __func__, __FILE__, __LINE__);
    for (const Wasm_Instr *in = fn->first; in; in = in->next)
        if (in->op == WASM_LOCAL_GET)
            c->gets[in->imm]++;
        else if (in->op == WASM_LOCAL_SET || in->op == WASM_LOCAL_TEE)
            c->sets[in->imm]++;
}

static void free_counts(Counts *c)
{
    xfree(c->gets);
    xfree(c->sets);
}

// A local never read: its set becomes a drop, its tee goes; then a value dropped as
// soon as made goes, with its expression when that has no effect.
static bool dead_values(Wasm_Func *fn, Counts *c)
{
    bool changed = false;
    for (Wasm_Instr *in = fn->first, *next; in; in = next) {
        next = in->next;
        if ((in->op == WASM_LOCAL_SET || in->op == WASM_LOCAL_TEE) && !c->gets[in->imm]) {
            c->sets[in->imm]--;
            if (in->op == WASM_LOCAL_SET) {
                in->op  = WASM_DROP;
                in->imm = 0;
            } else {
                wasm_remove(fn, in);
            }
            changed = true;
        }
    }
    for (Wasm_Instr *in = fn->first, *next; in; in = next) {
        next = in->next;
        if (in->op != WASM_DROP || !in->prev || !is_pure(in->prev))
            continue;
        Wasm_Instr *x = in->prev;
        int pops, pushes;
        wasm_stack_effect(x, &pops, &pushes);
        if (pushes != 1)
            continue;
        if (x->op == WASM_LOCAL_GET)
            c->gets[x->imm]--;
        // x's operands are dropped in its place.
        if (pops == 0) {
            wasm_remove(fn, x);
            wasm_remove(fn, in);
        } else {
            wasm_remove(fn, x);
            for (int i = 1; i < pops; i++)
                wasm_insert(fn, in, WASM_DROP);
        }
        changed = true;
        next    = fn->first; // the operands may be dropped in turn
    }
    return changed;
}

// What a run of code does that an expression moved across it must not depend on.
typedef struct {
    bool writes_memory;
    bool writes_global;
    const Wasm_Instr *first, *last; // the run, or first NULL when empty
} Effects;

// Whether expression first..last can move down across run e.
static bool can_move(const Wasm_Instr *first, const Wasm_Instr *last, const Effects *e)
{
    for (const Wasm_Instr *x = first;; x = x->next) {
        if (!is_pure(x))
            return false;
        if (is_load(x->op) && e->writes_memory)
            return false;
        if (x->op == WASM_GLOBAL_GET && e->writes_global)
            return false;
        if (x->op == WASM_LOCAL_GET && e->first)
            for (const Wasm_Instr *y = e->first;; y = y->next) {
                if ((y->op == WASM_LOCAL_SET || y->op == WASM_LOCAL_TEE) && y->imm == x->imm)
                    return false;
                if (y == e->last)
                    break;
            }
        if (x == last)
            return true;
    }
}

// Keep on the stack a value set once and read once later in the same run of code.
static bool stackify(Wasm_Func *fn, Counts *c)
{
    bool changed = false;
    for (Wasm_Instr *set = fn->first, *next; set; set = next) {
        next = set->next;
        if (set->op != WASM_LOCAL_SET)
            continue;
        int local = (int)set->imm;
        if (c->sets[local] != 1 || c->gets[local] != 1)
            continue;

        // Find the read, with what the code between does to the stack.
        int height = 0, low = 0;
        Effects e = { 0 };
        Wasm_Instr *get;
        for (get = set->next; get && !is_get(get, local); get = get->next) {
            int pops, pushes;
            if (!wasm_stack_effect(get, &pops, &pushes))
                break;
            height -= pops;
            if (height < low)
                low = height;
            height += pushes;
            if (writes_memory(get) || get->op == WASM_CALL || get->op == WASM_CALL_INDIRECT)
                e.writes_memory = true;
            if (get->op == WASM_GLOBAL_SET || get->op == WASM_CALL || get->op == WASM_CALL_INDIRECT)
                e.writes_global = true;
            if (!e.first)
                e.first = get;
            e.last = get;
        }
        if (!is_get(get, local))
            continue;

        next = set->next == get ? get->next : set->next;
        if (height == 0 && low >= 0) {
            // The value waits on the stack below the code between.
        } else {
            Wasm_Instr *start = expr_start(set->prev);
            if (!start || !can_move(start, set->prev, &e))
                continue;
            wasm_move(fn, start, set->prev, get);
        }
        wasm_remove(fn, set);
        wasm_remove(fn, get);
        c->sets[local]--;
        c->gets[local]--;
        changed = true;
    }
    return changed;
}

// local.set x; local.get x → local.tee x.
static bool tees(Wasm_Func *fn, Counts *c)
{
    bool changed = false;
    for (Wasm_Instr *in = fn->first; in; in = in->next)
        if (in->op == WASM_LOCAL_SET && is_get(in->next, (int)in->imm)) {
            wasm_remove(fn, in->next);
            c->gets[in->imm]--;
            in->op  = WASM_LOCAL_TEE;
            changed = true;
        }
    return changed;
}

static bool is_end(Wasm_Op op)
{
    return op == WASM_END_BLOCK || op == WASM_END_LOOP || op == WASM_END_IF;
}

static bool opens(Wasm_Op op)
{
    return op == WASM_BLOCK || op == WASM_LOOP || op == WASM_IF;
}

// Whether control never runs on from `in` to the next instruction.
static bool is_transfer(Wasm_Op op)
{
    return op == WASM_BR || op == WASM_BR_TABLE || op == WASM_RETURN || op == WASM_UNREACHABLE;
}

// Code after a branch, up to the end of its construct (or else), never runs.
static bool dead_code(Wasm_Func *fn)
{
    bool changed = false;
    for (Wasm_Instr *in = fn->first; in; in = in->next) {
        if (!is_transfer(in->op))
            continue;
        int depth = 0;
        while (in->next) {
            Wasm_Instr *x = in->next;
            if (depth == 0 && (is_end(x->op) || x->op == WASM_ELSE))
                break;
            if (opens(x->op))
                depth++;
            else if (is_end(x->op))
                depth--;
            wasm_remove(fn, x);
            changed = true;
        }
    }
    return changed;
}

// Whether a branch of depth d at nesting `nest` within a construct names it.
static void branch_depths(Wasm_Instr *in, int nest, bool *named, bool adjust)
{
    if (in->op == WASM_BR || in->op == WASM_BR_IF) {
        if (in->imm == nest)
            *named = true;
        else if (adjust && in->imm > nest)
            in->imm--;
    } else if (in->op == WASM_BR_TABLE) {
        for (int i = 0; i < in->ntable; i++)
            if (in->table[i] == nest)
                *named = true;
            else if (adjust && in->table[i] > nest)
                in->table[i]--;
    }
}

// The end of the construct `open` begins.
static Wasm_Instr *end_of(Wasm_Instr *open)
{
    int depth = 0;
    for (Wasm_Instr *in = open->next; in; in = in->next)
        if (opens(in->op))
            depth++;
        else if (is_end(in->op) && depth-- == 0)
            return in;
    return NULL;
}

// A block or loop no branch names is just its code.
static bool unnamed_constructs(Wasm_Func *fn)
{
    bool changed = false;
    for (Wasm_Instr *in = fn->first, *next; in; in = next) {
        next = in->next;
        if (in->op != WASM_BLOCK && in->op != WASM_LOOP)
            continue;
        Wasm_Instr *end = end_of(in);
        bool named      = false;
        int nest        = 0;
        for (Wasm_Instr *x = in->next; x != end; x = x->next) {
            if (opens(x->op))
                nest++;
            else if (is_end(x->op))
                nest--;
            else
                branch_depths(x, nest, &named, false);
        }
        if (named)
            continue;
        nest = 0;
        for (Wasm_Instr *x = in->next; x != end; x = x->next) {
            if (opens(x->op))
                nest++;
            else if (is_end(x->op))
                nest--;
            else
                branch_depths(x, nest, &named, true);
        }
        wasm_remove(fn, end);
        wasm_remove(fn, in);
        changed = true;
    }
    return changed;
}

// Whether br `in` goes where control would run on to from it: through the ends of the
// constructs that follow, and from an if's then-arm to its end, to the end of the
// block or if it names.
static bool falls_to_target(const Wasm_Instr *in)
{
    int k = 0;
    for (const Wasm_Instr *x = in->next; x; x = x->next) {
        if (x->op == WASM_ELSE) { // the then-arm runs on to the end of the if
            int depth = 0;
            for (x = x->next; x && !(depth == 0 && x->op == WASM_END_IF); x = x->next)
                if (opens(x->op))
                    depth++;
                else if (is_end(x->op))
                    depth--;
            if (!x)
                return false;
        }
        if (!is_end(x->op))
            return false;
        if (k == in->imm)
            return x->op != WASM_END_LOOP;
        k++;
    }
    return false;
}

static bool is_int_compare(Wasm_Op op)
{
    return (op >= WASM_I32_EQ && op <= WASM_I32_GE_U) || (op >= WASM_I64_EQ && op <= WASM_I64_GE_U);
}

// The comparison true where integer comparison op is false.
static Wasm_Op invert(Wasm_Op op)
{
    switch (op) {
    case WASM_I32_EQ:
        return WASM_I32_NE;
    case WASM_I32_NE:
        return WASM_I32_EQ;
    case WASM_I32_LT_S:
        return WASM_I32_GE_S;
    case WASM_I32_LT_U:
        return WASM_I32_GE_U;
    case WASM_I32_GT_S:
        return WASM_I32_LE_S;
    case WASM_I32_GT_U:
        return WASM_I32_LE_U;
    case WASM_I32_LE_S:
        return WASM_I32_GT_S;
    case WASM_I32_LE_U:
        return WASM_I32_GT_U;
    case WASM_I32_GE_S:
        return WASM_I32_LT_S;
    case WASM_I32_GE_U:
        return WASM_I32_LT_U;
    case WASM_I64_EQ:
        return WASM_I64_NE;
    case WASM_I64_NE:
        return WASM_I64_EQ;
    case WASM_I64_LT_S:
        return WASM_I64_GE_S;
    case WASM_I64_LT_U:
        return WASM_I64_GE_U;
    case WASM_I64_GT_S:
        return WASM_I64_LE_S;
    case WASM_I64_GT_U:
        return WASM_I64_LE_U;
    case WASM_I64_LE_S:
        return WASM_I64_GT_S;
    case WASM_I64_LE_U:
        return WASM_I64_GT_U;
    case WASM_I64_GE_S:
        return WASM_I64_LT_S;
    default:
        return WASM_I64_LT_U;
    }
}

// Whether instruction `in` takes its operand only as true or false.
static bool tests(const Wasm_Instr *in)
{
    return in && (in->op == WASM_BR_IF || in->op == WASM_IF || in->op == WASM_I32_EQZ);
}

static bool is_const(const Wasm_Instr *in, Wasm_Op op, int64_t v)
{
    return in && in->op == op && !in->sym && in->imm == v && !in->barrier;
}

// Fold `i32.const k; i32.add` ahead of the address of access `at` into its offset.
static bool fold_offset(Wasm_Func *fn, Wasm_Instr *at, Wasm_Instr *add)
{
    if (!add || add->op != WASM_I32_ADD || add->barrier)
        return false;
    Wasm_Instr *k = add->prev;
    if (!k || k->op != WASM_I32_CONST || k->sym || k->imm < 0 || k->barrier)
        return false;
    int64_t off = at->imm + k->imm;
    if (off > 0x7fffffff)
        return false;
    at->imm = off;
    wasm_remove(fn, k);
    wasm_remove(fn, add);
    return true;
}

// Whether op leaves 0 or 1.
static bool is_boolean(Wasm_Op op)
{
    return is_int_compare(op) || (op >= WASM_F32_EQ && op <= WASM_F64_GE) || op == WASM_I32_EQZ ||
           op == WASM_I64_EQZ;
}

// The width of a store of an i32 that merge_stores widens, or 0.
static int narrow_store_width(Wasm_Op op)
{
    switch (op) {
    case WASM_I32_STORE8:
        return 1;
    case WASM_I32_STORE16:
        return 2;
    case WASM_I32_STORE:
        return 4;
    default:
        return 0;
    }
}

// local.get b; const c; store o — the store of a constant at a local plus an offset.
static bool const_store(const Wasm_Instr *st, Wasm_Op op)
{
    const Wasm_Instr *c = st ? st->prev : NULL;
    return st && st->op == op && !st->sym && !st->barrier && c && c->op == WASM_I32_CONST &&
           !c->sym && !c->barrier && c->prev && c->prev->op == WASM_LOCAL_GET;
}

// Constants stored next to each other through one local → one store of both; wasm
// takes an access at any address, its alignment only a hint.
static bool merge_stores(Wasm_Func *fn, Wasm_Instr *s2)
{
    int w = narrow_store_width(s2->op);
    if (!w || !const_store(s2, s2->op))
        return false;
    Wasm_Instr *c2 = s2->prev, *b2 = c2->prev, *s1 = b2->prev;
    if (!const_store(s1, s2->op) || s1->prev->prev->imm != b2->imm)
        return false;
    Wasm_Instr *c1 = s1->prev;
    int64_t lo, hi, off;
    if (s2->imm == s1->imm + w) {
        lo = c1->imm, hi = c2->imm, off = s1->imm;
    } else if (s1->imm == s2->imm + w) {
        lo = c2->imm, hi = c1->imm, off = s2->imm;
    } else {
        return false;
    }
    uint64_t mask  = w == 4 ? 0xffffffffu : ((uint64_t)1 << (8 * w)) - 1;
    uint64_t value = ((uint64_t)lo & mask) | (((uint64_t)hi & mask) << (8 * w));
    int align      = s1->align && s1->align < w ? s1->align : 0;
    if (s2->align && s2->align < w && (!align || s2->align < align))
        align = s2->align;
    if (!align && (off % (2 * w) != 0 || b2->imm != fn->frame))
        align = w; // the frame alone is known to be aligned
    switch (w) {
    case 1:
        s1->op  = WASM_I32_STORE16;
        c1->imm = (int16_t)value;
        break;
    case 2:
        s1->op  = WASM_I32_STORE;
        c1->imm = (int32_t)value;
        break;
    default:
        s1->op  = WASM_I64_STORE;
        c1->op  = WASM_I64_CONST;
        c1->imm = (int64_t)value;
        break;
    }
    s1->imm   = off;
    s1->align = align;
    wasm_remove(fn, b2);
    wasm_remove(fn, c2);
    wasm_remove(fn, s2);
    return true;
}

static bool rewrite(Wasm_Func *fn)
{
    bool changed = false;
    for (Wasm_Instr *in = fn->first, *next; in; in = next) {
        next          = in->next;
        Wasm_Instr *p = in->prev;
        // x == 0 → eqz x
        if ((in->op == WASM_I32_EQ && is_const(p, WASM_I32_CONST, 0)) ||
            (in->op == WASM_I64_EQ && is_const(p, WASM_I64_CONST, 0))) {
            in->op = in->op == WASM_I32_EQ ? WASM_I32_EQZ : WASM_I64_EQZ;
            wasm_remove(fn, p);
            changed = true;
            continue;
        }
        // A truth value != 0 → the truth value
        if (in->op == WASM_I32_NE && is_const(p, WASM_I32_CONST, 0) && p->prev &&
            is_boolean(p->prev->op)) {
            wasm_remove(fn, p);
            wasm_remove(fn, in);
            changed = true;
            continue;
        }
        // Two constants stored side by side → one twice as wide.
        if (merge_stores(fn, in)) {
            changed = true;
            continue;
        }
        // x != 0, tested → x
        if (in->op == WASM_I32_NE && is_const(p, WASM_I32_CONST, 0) && tests(in->next)) {
            wasm_remove(fn, p);
            wasm_remove(fn, in);
            changed = true;
            continue;
        }
        // !!x, tested → x
        if (in->op == WASM_I32_EQZ && p && p->op == WASM_I32_EQZ && !p->barrier &&
            tests(in->next)) {
            wasm_remove(fn, p);
            wasm_remove(fn, in);
            changed = true;
            continue;
        }
        // !(a < b) → a >= b
        if (in->op == WASM_I32_EQZ && p && is_int_compare(p->op) && !p->barrier) {
            p->op = invert(p->op);
            wasm_remove(fn, in);
            changed = true;
            continue;
        }
        // An address plus a constant → the offset of the access.
        if (is_load(in->op) && fold_offset(fn, in, p)) {
            changed = true;
            continue;
        }
        if (is_store(in->op)) {
            Wasm_Instr *value = expr_start(p);
            if (value && fold_offset(fn, in, value->prev)) {
                changed = true;
                continue;
            }
        }
        // A branch to where control runs on to anyway.
        if (in->op == WASM_BR && falls_to_target(in)) {
            wasm_remove(fn, in);
            changed = true;
            continue;
        }
        // A multiplication by a power of two → a shift.
        if ((in->op == WASM_I32_MUL || in->op == WASM_I64_MUL) && p && !p->sym && !p->barrier &&
            p->op == (in->op == WASM_I32_MUL ? WASM_I32_CONST : WASM_I64_CONST) && p->imm > 0 &&
            (p->imm & (p->imm - 1)) == 0) {
            int k = 0;
            while (((int64_t)1 << k) != p->imm)
                k++;
            p->imm  = k;
            in->op  = in->op == WASM_I32_MUL ? WASM_I32_SHL : WASM_I64_SHL;
            changed = true;
            continue;
        }
        // An empty else.
        if (in->op == WASM_ELSE && in->next && in->next->op == WASM_END_IF) {
            wasm_remove(fn, in);
            changed = true;
            continue;
        }
        // A local copied to itself, after coalescing.
        if (in->op == WASM_LOCAL_SET && is_get(p, (int)in->imm)) {
            wasm_remove(fn, p);
            wasm_remove(fn, in);
            changed = true;
            continue;
        }
        if (in->op == WASM_LOCAL_SET && p && p->op == WASM_LOCAL_TEE && p->imm == in->imm) {
            wasm_remove(fn, p);
            changed = true;
            continue;
        }
        if (in->op == WASM_LOCAL_TEE && is_get(p, (int)in->imm)) {
            wasm_remove(fn, in);
            changed = true;
            continue;
        }
    }
    // The function's end returns what is on the stack.
    if (fn->last && fn->last->op == WASM_RETURN) {
        wasm_remove(fn, fn->last);
        changed = true;
    }
    return changed;
}

// Renumber the locals in use, dropping the others.
static void compact_locals(Wasm_Func *fn)
{
    int n    = fn->nparams + fn->nlocals;
    int *map = xalloc((n + 1) * sizeof(int), __func__, __FILE__, __LINE__);
    for (int i = 0; i < n; i++)
        map[i] = i < fn->nparams ? i : -1;
    for (const Wasm_Instr *in = fn->first; in; in = in->next)
        if (wasm_op_form(in->op) == WASM_FORM_LOCAL && in->imm >= fn->nparams)
            map[in->imm] = 0;
    int k = 0;
    for (int i = fn->nparams; i < n; i++)
        if (map[i] == 0) {
            fn->locals[k] = fn->locals[i - fn->nparams];
            map[i]        = fn->nparams + k++;
        }
    fn->nlocals = k;
    for (Wasm_Instr *in = fn->first; in; in = in->next)
        if (wasm_op_form(in->op) == WASM_FORM_LOCAL)
            in->imm = map[in->imm];
    if (fn->frame >= 0)
        fn->frame = map[fn->frame];
    xfree(map);
}

static bool round_of(Wasm_Func *fn)
{
    bool changed = false;
    if (wasm_peephole) {
        changed |= dead_code(fn);
        changed |= unnamed_constructs(fn);
    }
    if (wasm_stackify) {
        Counts c;
        count_locals(fn, &c);
        changed |= dead_values(fn, &c);
        changed |= stackify(fn, &c);
        changed |= tees(fn, &c);
        free_counts(&c);
    }
    if (wasm_peephole)
        changed |= rewrite(fn);
    return changed;
}

void wasm_optimize(Wasm_Func *fn)
{
    if (!wasm_peephole && !wasm_stackify && !wasm_coalesce)
        return;
    while (round_of(fn))
        ;
    if (wasm_coalesce) {
        wasm_coalesce_locals(fn);
        while (round_of(fn))
            ;
    }
    compact_locals(fn);
}
