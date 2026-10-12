//
// Control-flow graph and liveness over TAC.  Blocks start at the entry, at labels
// and after jumps, returns and calls to _Noreturn functions; liveness is the usual
// backward fixpoint: out = ∪ in(succ), in = use ∪ (out − def).
//
#include "flow.h"

#include <ctype.h>
#include <string.h>

#include "srcloc.h"
#include "xalloc.h"

_Noreturn void fatal_error(const char *fmt, ...);

typedef void (*NameVisitor)(const Flow *f, const char *name, void *arg);

typedef struct {
    Flow_Visitor fn;
    void *arg;
} VisitArg;

static void visit_var(const Flow *f, const char *name, void *arg)
{
    int v = flow_var(f, name);
    if (v >= 0) {
        VisitArg *va = arg;
        va->fn(v, va->arg);
    }
}

static void visit_val(const Flow *f, const Tac_Val *v, NameVisitor fn, void *arg)
{
    if (v && v->kind == TAC_VAL_VAR && v->u.var_name)
        fn(f, v->u.var_name, arg);
}

// Visit the names `in` reads (`defs` false) or writes (`defs` true).
static void operands(const Flow *f, const Tac_Instruction *in, bool defs, NameVisitor fn, void *arg)
{
    switch (in->kind) {
    case TAC_INSTRUCTION_RETURN:
        if (!defs)
            visit_val(f, in->u.return_.src, fn, arg);
        break;
    case TAC_INSTRUCTION_GET_ADDRESS:
    case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
    case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
        if (defs)
            visit_val(f, in->u.get_address.dst, fn, arg);
        break; // the source's address, not its value
    case TAC_INSTRUCTION_UNARY:
        visit_val(f, defs ? in->u.unary.dst : in->u.unary.src, fn, arg);
        break;
    case TAC_INSTRUCTION_BINARY:
        if (defs) {
            visit_val(f, in->u.binary.dst, fn, arg);
        } else {
            visit_val(f, in->u.binary.src1, fn, arg);
            visit_val(f, in->u.binary.src2, fn, arg);
        }
        break;
    case TAC_INSTRUCTION_LOAD:
    case TAC_INSTRUCTION_LOAD_BYTE:
        visit_val(f, defs ? in->u.load.dst : in->u.load.src_ptr, fn, arg);
        break;
    case TAC_INSTRUCTION_STORE:
    case TAC_INSTRUCTION_STORE_BYTE:
        if (!defs) {
            visit_val(f, in->u.store.src, fn, arg);
            visit_val(f, in->u.store.dst_ptr, fn, arg);
        }
        break;
    case TAC_INSTRUCTION_ADD_PTR:
        if (defs) {
            visit_val(f, in->u.add_ptr.dst, fn, arg);
        } else {
            visit_val(f, in->u.add_ptr.ptr, fn, arg);
            visit_val(f, in->u.add_ptr.index, fn, arg);
        }
        break;
    case TAC_INSTRUCTION_PTR_DIFF:
        if (defs) {
            visit_val(f, in->u.ptr_diff.dst, fn, arg);
        } else {
            visit_val(f, in->u.ptr_diff.ptr_a, fn, arg);
            visit_val(f, in->u.ptr_diff.ptr_b, fn, arg);
        }
        break;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET:
        if (!defs) {
            visit_val(f, in->u.copy_to_offset.src, fn, arg);
            fn(f, in->u.copy_to_offset.dst, arg);
        }
        break;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
        if (defs)
            visit_val(f, in->u.copy_from_offset.dst, fn, arg);
        else
            fn(f, in->u.copy_from_offset.src, arg);
        break;
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
        if (!defs)
            visit_val(f, in->u.jump_if_zero.condition, fn, arg);
        break;
    case TAC_INSTRUCTION_JUMP_TABLE:
        if (!defs)
            visit_val(f, in->u.jump_table.index, fn, arg);
        break;
    case TAC_INSTRUCTION_FUN_CALL:
    case TAC_INSTRUCTION_FUN_CALL_NORETURN:
        if (defs) {
            visit_val(f, in->u.fun_call.dst, fn, arg);
        } else {
            if (in->u.fun_call.indirect)
                fn(f, in->u.fun_call.fun_name, arg);
            for (const Tac_Val *v = in->u.fun_call.args; v; v = v->next)
                visit_val(f, v, fn, arg);
        }
        break;
    case TAC_INSTRUCTION_ALLOCATE_LOCAL:
        // The declaration: the object's earlier value is gone.
        if (defs)
            fn(f, in->u.allocate_local.name, arg);
        break;
    case TAC_INSTRUCTION_JUMP:
    case TAC_INSTRUCTION_LABEL:
        break;
    default:
        // Every conversion and COPY has the {src, dst} layout.
        visit_val(f, defs ? in->u.copy.dst : in->u.copy.src, fn, arg);
        break;
    }
}

int flow_var(const Flow *f, const char *name)
{
    intptr_t v;
    return name && map_get(&f->index, name, &v) ? (int)v : -1;
}

void flow_uses(const Flow *f, const Tac_Instruction *in, Flow_Visitor fn, void *arg)
{
    VisitArg va = { fn, arg };
    operands(f, in, false, visit_var, &va);
}

void flow_defs(const Flow *f, const Tac_Instruction *in, Flow_Visitor fn, void *arg)
{
    VisitArg va = { fn, arg };
    operands(f, in, true, visit_var, &va);
}

static void set_remove(int v, void *s)
{
    flow_remove(s, v);
}

static void set_add(int v, void *s)
{
    flow_add(s, v);
}

void flow_step(const Flow *f, const Tac_Instruction *in, Flow_Set *live)
{
    flow_defs(f, in, set_remove, live);
    flow_uses(f, in, set_add, live);
}

Flow_Set *flow_set_new(const Flow *f)
{
    Flow_Set *s =
        xalloc((f->words ? f->words : 1) * sizeof(Flow_Set), __func__, __FILE__, __LINE__);
    memset(s, 0, (f->words ? f->words : 1) * sizeof(Flow_Set));
    return s;
}

static void add_var(Flow *f, const Tac_Param *p, int *n)
{
    for (; p; p = p->next) {
        if (flow_var(f, p->name) >= 0)
            continue;
        f->names[*n] = p->name;
        f->types[*n] = p->type;
        map_insert(&f->index, p->name, *n, 0);
        (*n)++;
    }
}

static void mark_in_memory(const Flow *f, const char *name)
{
    int v = flow_var(f, name);
    if (v >= 0)
        flow_add(f->in_memory, v);
}

// In memory for a reason other than its declaration.
static void mark_addressed(const Flow *f, const char *name)
{
    int v = flow_var(f, name);
    if (v >= 0) {
        flow_add(f->in_memory, v);
        flow_add(f->addressed, v);
    }
}

// Whether `v` is a temporary (`%` and a digit), not a named variable.
static bool is_temporary(const Tac_Val *v)
{
    return v->u.var_name[0] == '%' && isdigit((unsigned char)v->u.var_name[1]);
}

// The volatile object of a volatile COPY stays in memory, so every access really
// reaches it (and it keeps its value across longjmp).  The translator reads a volatile
// variable only into a temporary, so a copy to a named variable writes it, and a copy
// to a temporary reads its source.
static void mark_volatile_copy(const Flow *f, const Tac_Instruction *in)
{
    const Tac_Val *obj = is_temporary(in->u.copy.dst) ? in->u.copy.src : in->u.copy.dst;
    if (obj->kind == TAC_VAL_VAR && !is_temporary(obj))
        mark_addressed(f, obj->u.var_name);
}

static bool ends_block(const Tac_Instruction *in)
{
    switch (in->kind) {
    case TAC_INSTRUCTION_JUMP:
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
    case TAC_INSTRUCTION_JUMP_TABLE:
    case TAC_INSTRUCTION_RETURN:
    case TAC_INSTRUCTION_FUN_CALL_NORETURN:
        return true;
    default:
        return false;
    }
}

static int count_list(const Tac_Param *p)
{
    int n = 0;
    for (; p; p = p->next)
        n++;
    return n;
}

static void build_vars(Flow *f)
{
    const Tac_TopLevel *fn = f->fn;
    int max                = count_list(fn->u.function.params) + count_list(fn->u.function.locals);
    f->names               = xalloc((max ? max : 1) * sizeof(char *), __func__, __FILE__, __LINE__);
    f->types = xalloc((max ? max : 1) * sizeof(Tac_Type *), __func__, __FILE__, __LINE__);
    map_init(&f->index);
    int n = 0;
    add_var(f, fn->u.function.params, &n);
    add_var(f, fn->u.function.locals, &n);
    f->nvars     = n;
    f->words     = (n + 63) / 64;
    f->in_memory = flow_set_new(f);
    f->addressed = flow_set_new(f);
    for (int i = 0; i < f->ninstrs; i++) {
        const Tac_Instruction *in = f->instrs[i];
        switch (in->kind) {
        case TAC_INSTRUCTION_GET_ADDRESS:
        case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
        case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
            if (in->u.get_address.src->kind == TAC_VAL_VAR)
                mark_addressed(f, in->u.get_address.src->u.var_name);
            break;
        case TAC_INSTRUCTION_ALLOCATE_LOCAL:
            mark_in_memory(f, in->u.allocate_local.name);
            break;
        case TAC_INSTRUCTION_COPY:
            if (in->is_volatile)
                mark_volatile_copy(f, in);
            break;
        case TAC_INSTRUCTION_COPY_TO_OFFSET:
        case TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET:
            if (in->is_volatile)
                mark_addressed(f, in->u.copy_to_offset.dst);
            break;
        case TAC_INSTRUCTION_COPY_FROM_OFFSET:
        case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
            if (in->is_volatile)
                mark_addressed(f, in->u.copy_from_offset.src);
            break;
        default:
            break;
        }
    }
}

static void build_blocks(Flow *f)
{
    int n = 0;
    for (int i = 0; i < f->ninstrs; i++)
        if (i == 0 || f->instrs[i]->kind == TAC_INSTRUCTION_LABEL || ends_block(f->instrs[i - 1]))
            n++;
    f->nblocks = n;
    f->blocks  = xalloc((n ? n : 1) * sizeof(Flow_Block), __func__, __FILE__, __LINE__);
    memset(f->blocks, 0, (n ? n : 1) * sizeof(Flow_Block));

    StringMap labels; // label → block
    map_init(&labels);
    int b = -1;
    for (int i = 0; i < f->ninstrs; i++) {
        const Tac_Instruction *in = f->instrs[i];
        if (i == 0 || in->kind == TAC_INSTRUCTION_LABEL || ends_block(f->instrs[i - 1]))
            f->blocks[++b].first = i;
        f->blocks[b].last = i;
        if (in->kind == TAC_INSTRUCTION_LABEL)
            map_insert(&labels, in->u.label.name, b, 0);
    }

    for (b = 0; b < n; b++) {
        Flow_Block *blk           = &f->blocks[b];
        const Tac_Instruction *in = f->instrs[blk->last];
        const char *target        = NULL;
        bool falls                = true;
        blk->succ                 = xalloc(2 * sizeof(int), __func__, __FILE__, __LINE__);
        switch (in->kind) {
        case TAC_INSTRUCTION_JUMP_TABLE: {
            // An edge to each distinct target, the default among them.
            int k = in->u.jump_table.count + 1;
            xfree(blk->succ);
            blk->succ = xalloc(k * sizeof(int), __func__, __FILE__, __LINE__);
            for (int j = 0; j < k; j++) {
                const char *l =
                    j < k - 1 ? in->u.jump_table.targets[j] : in->u.jump_table.default_target;
                intptr_t t;
                if (!map_get(&labels, l, &t))
                    internal_error("flow: %s: no label %s", f->fn->u.function.name, l);
                bool seen = false;
                for (int m = 0; m < blk->nsucc; m++)
                    seen |= blk->succ[m] == (int)t;
                if (!seen)
                    blk->succ[blk->nsucc++] = (int)t;
            }
            falls = false;
            break;
        }
        case TAC_INSTRUCTION_JUMP:
            target = in->u.jump.target;
            falls  = false;
            break;
        case TAC_INSTRUCTION_JUMP_IF_ZERO:
        case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
            target = in->u.jump_if_zero.target;
            break;
        case TAC_INSTRUCTION_RETURN:
        case TAC_INSTRUCTION_FUN_CALL_NORETURN:
            falls = false;
            break;
        default:
            break;
        }
        intptr_t t;
        if (target) {
            if (!map_get(&labels, target, &t))
                internal_error("flow: %s: no label %s", f->fn->u.function.name, target);
            blk->succ[blk->nsucc++] = (int)t;
        }
        if (falls && b + 1 < n && !(blk->nsucc && blk->succ[0] == b + 1))
            blk->succ[blk->nsucc++] = b + 1;
    }
    map_destroy(&labels);
}

static void solve(Flow *f)
{
    for (int b = 0; b < f->nblocks; b++) {
        Flow_Block *blk = &f->blocks[b];
        blk->use        = flow_set_new(f);
        blk->def        = flow_set_new(f);
        blk->live_in    = flow_set_new(f);
        blk->live_out   = flow_set_new(f);
        for (int i = blk->last; i >= blk->first; i--) {
            flow_defs(f, f->instrs[i], set_add, blk->def);
            flow_defs(f, f->instrs[i], set_remove, blk->use);
            flow_uses(f, f->instrs[i], set_add, blk->use);
        }
    }
    bool changed = true;
    while (changed) {
        changed = false;
        for (int b = f->nblocks - 1; b >= 0; b--) {
            Flow_Block *blk = &f->blocks[b];
            for (int w = 0; w < f->words; w++) {
                uint64_t out = 0;
                for (int s = 0; s < blk->nsucc; s++)
                    out |= f->blocks[blk->succ[s]].live_in[w];
                uint64_t in = blk->use[w] | (out & ~blk->def[w]);
                if (out != blk->live_out[w] || in != blk->live_in[w])
                    changed = true;
                blk->live_out[w] = out;
                blk->live_in[w]  = in;
            }
        }
    }
}

Flow *flow_build(const Tac_TopLevel *fn)
{
    Flow *f = xalloc(sizeof(Flow), __func__, __FILE__, __LINE__);
    memset(f, 0, sizeof(*f));
    f->fn = fn;
    for (const Tac_Instruction *in = fn->u.function.body; in; in = in->next)
        f->ninstrs++;
    f->instrs = xalloc((f->ninstrs ? f->ninstrs : 1) * sizeof(Tac_Instruction *), __func__,
                       __FILE__, __LINE__);
    int i     = 0;
    for (const Tac_Instruction *in = fn->u.function.body; in; in = in->next)
        f->instrs[i++] = in;
    build_vars(f);
    build_blocks(f);
    solve(f);
    return f;
}

void flow_free(Flow *f)
{
    for (int b = 0; b < f->nblocks; b++) {
        xfree(f->blocks[b].succ);
        xfree(f->blocks[b].use);
        xfree(f->blocks[b].def);
        xfree(f->blocks[b].live_in);
        xfree(f->blocks[b].live_out);
    }
    xfree(f->blocks);
    xfree(f->in_memory);
    xfree(f->addressed);
    map_destroy(&f->index);
    xfree(f->names);
    xfree(f->types);
    xfree(f->instrs);
    xfree(f);
}
