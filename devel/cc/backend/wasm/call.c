//
// Calls and returns, as clang's wasm32 makes them.  A scalar is a wasm value, a long
// double two i64 (the low half first); an aggregate holding one scalar travels as it,
// an empty one not at all, any other as the address of a copy the caller makes, which
// the callee uses in place.  A result that is no wasm value goes where a hidden first
// parameter points.  The variable arguments of a call fill a buffer, each in a slot of
// at least 4 bytes aligned to its type (an aggregate by reference takes a pointer's),
// whose address is the last parameter.
//
// The copies, the buffer and a result with nowhere to go live in the calls' area at
// the bottom of the frame, which the calls share: laid out for each call by `layout`,
// sized for the largest by call_area_size.
//
#include <string.h>

#include "flow.h"
#include "internal.h"
#include "xalloc.h"

static bool is_va_start(const Tac_Instruction *in)
{
    return !in->u.fun_call.indirect && strcmp(in->u.fun_call.fun_name, "__va_start") == 0;
}

// The shadow-stack builtins of co_alloca (translator/coro.c), expanded in place.
enum { STACK_SAVE = 1, STACK_RESTORE, STACK_ALLOCA };

int wasm_stack_builtin(const char *name)
{
    if (strcmp(name, "__builtin_stack_save") == 0)
        return STACK_SAVE;
    if (strcmp(name, "__builtin_stack_restore") == 0)
        return STACK_RESTORE;
    if (strcmp(name, "__builtin_alloca") == 0)
        return STACK_ALLOCA;
    return 0;
}

static int stack_builtin(const Tac_Instruction *in)
{
    return in->u.fun_call.indirect ? 0 : wasm_stack_builtin(in->u.fun_call.fun_name);
}

static void global_sp(Gen *g, Wasm_Op op)
{
    emit(g, op)->sym = xstrdup("__stack_pointer");
}

// save: dst = __stack_pointer; restore: __stack_pointer = arg; alloca: __stack_pointer
// = (__stack_pointer - arg) & -16, dst = __stack_pointer.  The frame pointer keeps the
// frame where it was (gen_init gives such a function one), and the epilogue resets
// __stack_pointer from it.
static void gen_stack_builtin(Gen *g, const Tac_Instruction *in, int kind)
{
    const Tac_Val *dst = in->u.fun_call.dst;
    if (kind == STACK_RESTORE || kind == STACK_ALLOCA) {
        if (!in->u.fun_call.args)
            internal_error("wasm: %s: %s takes one argument", g->fn->name, in->u.fun_call.fun_name);
        if (kind == STACK_ALLOCA) {
            global_sp(g, WASM_GLOBAL_GET);
            push_val(g, in->u.fun_call.args, WASM_I32);
            emit(g, WASM_I32_SUB);
            emit_imm(g, WASM_I32_CONST, -16);
            emit(g, WASM_I32_AND);
        } else {
            push_val(g, in->u.fun_call.args, WASM_I32);
        }
        global_sp(g, WASM_GLOBAL_SET);
    }
    if (dst && kind != STACK_RESTORE) {
        begin_dst(g, dst);
        global_sp(g, WASM_GLOBAL_GET);
        end_dst(g, dst);
    }
}

bool wasm_uses_alloca(const Tac_TopLevel *tl)
{
    for (const Tac_Instruction *in = tl->u.function.body; in; in = in->next)
        if (in->kind == TAC_INSTRUCTION_FUN_CALL && stack_builtin(in) == STACK_ALLOCA)
            return true;
    return false;
}

// Where a call's pieces go in the calls' area.
typedef struct {
    int nargs;
    int *copy;  // per argument: the offset of its copy, or -1
    int *slot;  // per argument: the offset of its slot in the buffer, or -1
    int nvar;   // the variable arguments: their buffer is at offset 0
    int result; // the offset of the result when it has nowhere to go, or -1
    int size;
} Layout;

static int align_to(int n, int align)
{
    return (n + align - 1) & -align;
}

static int clamp_align(int align)
{
    return align > 16 ? 16 : align < 1 ? 1 : align;
}

// Whether the result of a call can be made right in its destination `dst`: a slot no
// pointer can reach, so the callee cannot see it change.
static bool result_in_place(const Gen *g, const Tac_Val *dst)
{
    if (!dst || var_slot(g, dst->u.var_name) < 0)
        return false;
    int v = flow_var(g->flow, dst->u.var_name);
    return v < 0 || !flow_has(g->flow->in_memory, v);
}

static void layout(const Gen *g, const Tac_Instruction *in, Layout *L)
{
    const Tac_Type *ft = in->u.fun_call.fun_type;
    memset(L, 0, sizeof(*L));
    for (const Tac_Val *a = in->u.fun_call.args; a; a = a->next)
        L->nargs++;
    L->copy = xalloc((L->nargs + 1) * sizeof(int), __func__, __FILE__, __LINE__);
    L->slot = xalloc((L->nargs + 1) * sizeof(int), __func__, __FILE__, __LINE__);

    // The buffer of variable arguments first, at the area's start.
    const Tac_Type *p = ft->u.fun_type.param_types;
    int i = 0, size = 0;
    for (const Tac_Val *a = in->u.fun_call.args; a; a = a->next, i++) {
        L->copy[i] = L->slot[i] = -1;
        if (p) {
            p = p->next;
            continue;
        }
        if (!ft->u.fun_type.variadic)
            continue;
        const Tac_Type *t = any_type(g, a);
        L->nvar++;
        switch (wasm_pass(t, NULL)) {
        case WASM_PASS_NONE:
            break;
        case WASM_PASS_REF:
            size       = align_to(size, 4);
            L->slot[i] = size;
            size += 4;
            break;
        default: {
            int align  = clamp_align(wasm_type_align(t) > 4 ? wasm_type_align(t) : 4);
            size       = align_to(size, align);
            L->slot[i] = size;
            size += align_to(wasm_type_size(t) > 4 ? wasm_type_size(t) : 4, 4);
            break;
        }
        }
    }

    // Then the copies of the arguments passed by reference.
    p = ft->u.fun_type.param_types;
    i = 0;
    for (const Tac_Val *a = in->u.fun_call.args; a; a = a->next, i++) {
        const Tac_Type *t = any_type(g, a);
        if (wasm_pass(p ? p : t, NULL) == WASM_PASS_REF) {
            size       = align_to(size, clamp_align(wasm_type_align(t)));
            L->copy[i] = size;
            size += wasm_type_size(t);
        }
        if (p)
            p = p->next;
    }

    // Then the result, when it goes through memory and its destination cannot take it.
    L->result = -1;
    if (wasm_sret(ft->u.fun_type.ret_type) && !result_in_place(g, in->u.fun_call.dst)) {
        const Tac_Type *ret = ft->u.fun_type.ret_type;
        size                = align_to(size, clamp_align(wasm_type_align(ret)));
        L->result           = size;
        size += wasm_type_size(ret);
    }
    L->size = align_to(size, 16);
}

static void free_layout(Layout *L)
{
    xfree(L->copy);
    xfree(L->slot);
}

int call_area_size(const Gen *g, const Tac_Instruction *in)
{
    if (is_va_start(in) || stack_builtin(in) || !in->u.fun_call.fun_type)
        return 0;
    Layout L;
    layout(g, in, &L);
    int size = L.size;
    free_layout(&L);
    return size;
}

// Push the address of offset `off` of the calls' area.
static void push_area(Gen *g, int off)
{
    Place p = place_local(g->fp, g->call_area + off);
    place_addr(g, &p);
}

// Push argument a, of type t (or its own, when the call has no prototype for it).
static void push_arg(Gen *g, const Tac_Val *a, const Tac_Type *t, int copy)
{
    const Tac_Type *s;
    switch (wasm_pass(t, &s)) {
    case WASM_PASS_NONE:
        return;
    case WASM_PASS_REF:
        push_area(g, copy);
        return;
    case WASM_PASS_PAIR:
        if (a->kind == TAC_VAL_CONSTANT) {
            Float128 bits = a->u.constant->u.long_double_val;
            emit_imm(g, WASM_I64_CONST, (int64_t)bits.lo);
            emit_imm(g, WASM_I64_CONST, (int64_t)bits.hi);
        } else {
            for (int half = 0; half < 2; half++) {
                Place p = place_named(a->u.var_name, 8 * half);
                load_value(g, &p, &(Tac_Type){ .kind = TAC_TYPE_ULONG_LONG });
            }
        }
        return;
    case WASM_PASS_VALUE:
        if (is_memory_type(t)) {
            Place p = place_named(a->u.var_name, 0);
            load_value(g, &p, s);
        } else {
            push_val_as(g, a, t);
        }
        return;
    }
}

// va_start(ap), a call of __va_start(&ap): ap = the address of the variable arguments.
static void gen_va_start(Gen *g, const Tac_Instruction *in)
{
    if (!g->tl->u.function.variadic)
        internal_error("wasm: %s: va_start in a function without ...", g->fn->name);
    if (!in->u.fun_call.args || in->u.fun_call.args->next)
        internal_error("wasm: %s: __va_start takes one argument", g->fn->name);
    push_val(g, in->u.fun_call.args, WASM_I32);
    emit_imm(g, WASM_LOCAL_GET, g->va_param);
    emit(g, WASM_I32_STORE);
}

// A call; with `keep` and no destination, its result stays on the stack.
static void call_with(Gen *g, const Tac_Instruction *in, bool noreturn, bool keep)
{
    if (is_va_start(in)) {
        gen_va_start(g, in);
        return;
    }
    if (stack_builtin(in)) {
        gen_stack_builtin(g, in, stack_builtin(in));
        return;
    }
    const Tac_Val *dst = in->u.fun_call.dst;
    const Tac_Type *ft = in->u.fun_call.fun_type;
    if (!ft)
        internal_error("wasm: %s: call of %s with no type", g->fn->name, in->u.fun_call.fun_name);
    const Tac_Type *ret = ft->u.fun_type.ret_type, *rs;
    bool sret           = wasm_sret(ret);
    Wasm_Pass rpass     = wasm_pass(ret, &rs);
    Layout L;
    layout(g, in, &L);

    // The copies of the arguments passed by reference, and the variable arguments.
    int i = 0;
    for (const Tac_Val *a = in->u.fun_call.args; a; a = a->next, i++) {
        const Tac_Type *t = any_type(g, a);
        if (L.copy[i] >= 0) {
            Place p = place_local(g->fp, g->call_area + L.copy[i]);
            store_value(g, &p, a, t);
        }
        if (L.slot[i] < 0)
            continue;
        Place p = place_local(g->fp, g->call_area + L.slot[i]);
        if (L.copy[i] >= 0) {
            emit_imm(g, WASM_LOCAL_GET, g->fp);
            push_area(g, L.copy[i]);
            emit_access(g, WASM_I32_STORE, NULL, g->call_area + L.slot[i]);
        } else if (!is_memory_type(t) && wasm_type_size(t) < 4) {
            // A narrow value, held extended: all of its i32.
            store_value(g, &p, a, &(Tac_Type){ .kind = TAC_TYPE_INT });
        } else {
            store_value(g, &p, a, t);
        }
    }

    // The destination's address under the arguments, when in memory.
    const char *dsym = NULL;
    int64_t doff     = 0;
    if (dst && !sret && rpass == WASM_PASS_VALUE) {
        if (is_memory_type(type_of(g, dst->u.var_name)))
            push_base(g, dst->u.var_name, &dsym, &doff);
        else
            begin_dst(g, dst);
    }

    // The arguments.
    if (sret) {
        if (L.result >= 0)
            push_area(g, L.result);
        else
            push_addr(g, dst->u.var_name);
    }
    const Tac_Type *p = ft->u.fun_type.param_types;
    i                 = 0;
    for (const Tac_Val *a = in->u.fun_call.args; a; a = a->next, i++) {
        if (p) {
            push_arg(g, a, p, L.copy[i]);
            p = p->next;
        } else if (!ft->u.fun_type.variadic) {
            push_arg(g, a, any_type(g, a), L.copy[i]); // no prototype: its own type
        }
    }
    if (ft->u.fun_type.variadic) {
        if (L.nvar)
            push_area(g, 0);
        else
            emit_imm(g, WASM_I32_CONST, 0);
    }

    // The call.
    Wasm_Sig sig;
    wasm_signature(ft, &sig);
    Wasm_Instr *call;
    if (in->u.fun_call.indirect) {
        Tac_Val callee = { .kind = TAC_VAL_VAR, .u.var_name = in->u.fun_call.fun_name };
        push_val(g, &callee, WASM_I32);
        call       = emit(g, WASM_CALL_INDIRECT);
        call->sym  = wasm_sig_string(&sig);
        call->pops = 1;
    } else {
        call      = emit(g, WASM_CALL);
        call->sym = xstrdup(wasm_symbol(g->program, in->u.fun_call.fun_name));
    }
    call->pops += sig.nparams;
    call->pushes = sig.result != WASM_VOID;

    // The result.
    if (noreturn) {
        emit(g, WASM_UNREACHABLE);
    } else if (sret) {
        if (dst && L.result >= 0) {
            push_addr(g, dst->u.var_name);
            push_area(g, L.result);
            emit_imm(g, WASM_I32_CONST, wasm_type_size(ret));
            emit(g, WASM_MEMORY_COPY);
        }
    } else if (rpass == WASM_PASS_VALUE && dst) {
        const Tac_Type *dt = type_of(g, dst->u.var_name);
        if (is_memory_type(dt)) {
            emit_access(g, wasm_store_op(rs), dsym, doff);
        } else {
            if (wasm_type_size(ret) < 4 && dt->kind != ret->kind)
                narrow(g, dt);
            end_dst(g, dst);
        }
    } else if (rpass == WASM_PASS_VALUE && ret->kind != TAC_TYPE_VOID && !keep) {
        emit(g, WASM_DROP);
    }
    free_layout(&L);
}

void gen_call(Gen *g, const Tac_Instruction *in, bool noreturn)
{
    call_with(g, in, noreturn, false);
}

// The signature of a runtime routine, declared ahead of the function that calls it.
static void declare_helper(Gen *g, const char *name, const Tac_Type *ft)
{
    for (int i = 0; i < g->nhelpers; i++)
        if (strcmp(g->helpers[i].name, name) == 0)
            return;
    if (g->nhelpers >= (int)(sizeof(g->helpers) / sizeof(g->helpers[0])))
        fatal_error("function '%s' needs more than %d runtime routines, the wasm32 limit",
                    g->fn->name, (int)(sizeof(g->helpers) / sizeof(g->helpers[0])));
    Wasm_Sig sig;
    wasm_signature(ft, &sig);
    g->helpers[g->nhelpers].name  = name;
    g->helpers[g->nhelpers++].sig = wasm_sig_string(&sig);
}

void gen_runtime(Gen *g, const char *name, const Tac_Type *ret, const Tac_Val *const *args,
                 const Tac_Type *const *types, int n, const Tac_Val *dst)
{
    Tac_Val vals[4];
    Tac_Type params[4];
    Tac_Type rt            = *ret;
    rt.next                = NULL;
    Tac_Type ft            = { .kind = TAC_TYPE_FUN_TYPE };
    ft.u.fun_type.ret_type = &rt;
    for (int i = n - 1; i >= 0; i--) {
        vals[i]        = *args[i];
        vals[i].next   = i + 1 < n ? &vals[i + 1] : NULL;
        params[i]      = *types[i];
        params[i].next = i + 1 < n ? &params[i + 1] : NULL;
    }
    ft.u.fun_type.param_types = n ? &params[0] : NULL;
    Tac_Instruction in        = { .kind = TAC_INSTRUCTION_FUN_CALL };
    in.u.fun_call.fun_name    = (char *)name;
    in.u.fun_call.args        = n ? &vals[0] : NULL;
    in.u.fun_call.dst         = (Tac_Val *)dst;
    in.u.fun_call.fun_type    = &ft;
    declare_helper(g, name, &ft);
    call_with(g, &in, false, dst == NULL);
}

void gen_return(Gen *g, const Tac_Val *src)
{
    const Tac_Type *ret = g->tl->u.function.type->u.fun_type.ret_type, *s;
    if (src && g->sret >= 0) {
        Place p = place_local(g->sret, 0);
        store_value(g, &p, src, ret);
    } else if (src && wasm_pass(ret, &s) == WASM_PASS_VALUE) {
        if (is_memory_type(ret)) {
            Place p = place_named(src->u.var_name, 0);
            load_value(g, &p, s);
        } else {
            push_val_as(g, src, ret);
        }
    } else if (!src && g->fn->result != WASM_VOID) {
        // `return;` in a function with a result
        Tac_Const zero = { .kind = TAC_CONST_INT };
        Tac_Val v      = { .kind = TAC_VAL_CONSTANT, .u.constant = &zero };
        push_val(g, &v, g->fn->result);
    }
    gen_epilogue(g);
    emit(g, WASM_RETURN);
}
