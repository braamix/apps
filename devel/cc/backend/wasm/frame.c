//
// Types, signatures, and where a function's names live: a scalar in a wasm local.
//
#include <string.h>

#include "flow.h"
#include "internal.h"
#include "xalloc.h"

int wasm_type_size(const Tac_Type *t)
{
    switch (t->kind) {
    case TAC_TYPE_SCHAR:
    case TAC_TYPE_UCHAR:
    case TAC_TYPE_VOID:
        return 1;
    case TAC_TYPE_SHORT:
    case TAC_TYPE_USHORT:
        return 2;
    case TAC_TYPE_INT:
    case TAC_TYPE_UINT:
    case TAC_TYPE_LONG:
    case TAC_TYPE_ULONG:
    case TAC_TYPE_FLOAT:
    case TAC_TYPE_POINTER:
    case TAC_TYPE_FUN_TYPE:
        return 4;
    case TAC_TYPE_LONG_LONG:
    case TAC_TYPE_ULONG_LONG:
    case TAC_TYPE_DOUBLE:
        return 8;
    case TAC_TYPE_LONG_DOUBLE:
        return 16;
    case TAC_TYPE_ARRAY:
        return t->u.array.size * wasm_type_size(t->u.array.elem_type);
    case TAC_TYPE_STRUCTURE:
        return t->u.structure.size;
    }
    return 4;
}

int wasm_type_align(const Tac_Type *t)
{
    switch (t->kind) {
    case TAC_TYPE_ARRAY:
        return wasm_type_align(t->u.array.elem_type);
    case TAC_TYPE_STRUCTURE:
        return t->u.structure.alignment > 0 ? t->u.structure.alignment : 1;
    default:
        return wasm_type_size(t);
    }
}

Wasm_ValType wasm_valtype(const Tac_Type *t)
{
    switch (t->kind) {
    case TAC_TYPE_LONG_LONG:
    case TAC_TYPE_ULONG_LONG:
        return WASM_I64;
    case TAC_TYPE_FLOAT:
        return WASM_F32;
    case TAC_TYPE_DOUBLE:
        return WASM_F64;
    case TAC_TYPE_VOID:
        return WASM_VOID;
    case TAC_TYPE_LONG_DOUBLE:
    case TAC_TYPE_ARRAY:
    case TAC_TYPE_STRUCTURE:
        internal_error("wasm: no value type for an aggregate or a long double");
    default:
        return WASM_I32;
    }
}

static void add_param(Wasm_Sig *sig, Wasm_ValType t)
{
    if (sig->nparams >= (int)(sizeof(sig->params) / sizeof(sig->params[0])))
        fatal_error("a function with more than %d parameters is not supported on target 'wasm32'",
                    (int)(sizeof(sig->params) / sizeof(sig->params[0])));
    sig->params[sig->nparams++] = t;
}

Wasm_Pass wasm_pass(const Tac_Type *t, const Tac_Type **scalar)
{
    if (t->kind == TAC_TYPE_STRUCTURE && !t->u.structure.members && t->u.structure.size > 0)
        internal_error("wasm: structure %s passed with no members known",
                       t->u.structure.tag ? t->u.structure.tag : "?");
    const Tac_Type *s = tac_wasm32_scalar(t);
    if (scalar)
        *scalar = s;
    if (!s)
        return tac_wasm32_empty(t) ? WASM_PASS_NONE : WASM_PASS_REF;
    return s->kind == TAC_TYPE_LONG_DOUBLE ? WASM_PASS_PAIR : WASM_PASS_VALUE;
}

bool wasm_sret(const Tac_Type *ret)
{
    Wasm_Pass pass = wasm_pass(ret, NULL);
    return pass == WASM_PASS_PAIR || pass == WASM_PASS_REF;
}

void wasm_signature(const Tac_Type *fun_type, Wasm_Sig *sig)
{
    memset(sig, 0, sizeof(*sig));
    const Tac_Type *ret = fun_type->u.fun_type.ret_type, *s;
    sig->result         = WASM_VOID;
    if (wasm_sret(ret))
        add_param(sig, WASM_I32); // where the result goes
    else if (wasm_pass(ret, &s) == WASM_PASS_VALUE)
        sig->result = wasm_valtype(s);
    for (const Tac_Type *p = fun_type->u.fun_type.param_types; p; p = p->next) {
        switch (wasm_pass(p, &s)) {
        case WASM_PASS_VALUE:
            add_param(sig, wasm_valtype(s));
            break;
        case WASM_PASS_PAIR:
            add_param(sig, WASM_I64);
            add_param(sig, WASM_I64);
            break;
        case WASM_PASS_REF:
            add_param(sig, WASM_I32);
            break;
        case WASM_PASS_NONE:
            break;
        }
    }
    // The variable arguments come in a buffer, by its address.
    if (fun_type->u.fun_type.variadic)
        add_param(sig, WASM_I32);
}

// Whether a value of type t can only live in memory: an aggregate or a long double.
static bool memory_type(const Tac_Type *t)
{
    return t->kind == TAC_TYPE_STRUCTURE || t->kind == TAC_TYPE_ARRAY ||
           t->kind == TAC_TYPE_LONG_DOUBLE;
}

// Whether frame-resident `name` lives in a frame slot: its address is taken, it is an
// ALLOCATE_LOCAL object or volatile (Flow.in_memory), or its type is an aggregate.
static bool in_slot(const Gen *g, const char *name, const Tac_Type *t)
{
    int v = flow_var(g->flow, name);
    return memory_type(t) || (v >= 0 && flow_has(g->flow->in_memory, v));
}

// A slot for `name` of `size` bytes aligned to `align` (at most 16, the stack's).
static void add_slot(Gen *g, const char *name, int size, int align)
{
    for (const Tac_Instruction *in = g->tl->u.function.body; in; in = in->next)
        if (in->kind == TAC_INSTRUCTION_ALLOCATE_LOCAL &&
            strcmp(in->u.allocate_local.name, name) == 0) {
            if (in->u.allocate_local.size > size)
                size = in->u.allocate_local.size;
            if (in->u.allocate_local.alignment > align)
                align = in->u.allocate_local.alignment;
        }
    if (align > 16)
        align = 16;
    if (align < 1)
        align = 1;
    int off = (g->frame_size + align - 1) & -align;
    map_insert(&g->slots, name, off + 1, 0);
    g->frame_size = off + (size > 0 ? size : 1);
}

void gen_init(Gen *g, const Tac_TopLevel *program, const Tac_TopLevel *tl)
{
    memset(g, 0, sizeof(*g));
    g->program = program;
    g->tl      = tl;
    g->sret    = -1;
    map_init(&g->locals);
    map_init(&g->types);
    map_init(&g->slots);
    map_init(&g->refs);
    map_init(&g->pindex);
    g->fn         = wasm_new_func(wasm_symbol(program, tl->u.function.name), tl->u.function.global);
    g->flow       = flow_build(tl);
    Wasm_Func *fn = g->fn;
    if (!tl->u.function.type)
        internal_error("wasm: %s has no type", fn->name);

    // The parameters are the first locals, as the signature has them: the result's
    // address first when it goes through one.  A parameter that lives in a slot is
    // stored there by the prologue; one passed by reference is used where it is.
    Wasm_Sig sig;
    wasm_signature(tl->u.function.type, &sig);
    fn->params = xalloc((sig.nparams + 1) * sizeof(Wasm_ValType), __func__, __FILE__, __LINE__);
    memcpy(fn->params, sig.params, sig.nparams * sizeof(Wasm_ValType));
    fn->nparams = sig.nparams;
    fn->result  = sig.result;
    int index   = 0;
    if (wasm_sret(tl->u.function.type->u.fun_type.ret_type))
        g->sret = index++;
    for (const Tac_Param *p = tl->u.function.params; p; p = p->next) {
        if (!p->type)
            internal_error("wasm: %s: parameter %s has no type", fn->name, p->name);
        map_insert(&g->types, p->name, (intptr_t)p->type, 0);
        map_insert(&g->pindex, p->name, index + 1, 0);
        switch (wasm_pass(p->type, NULL)) {
        case WASM_PASS_VALUE:
            if (in_slot(g, p->name, p->type))
                add_slot(g, p->name, wasm_type_size(p->type), wasm_type_align(p->type));
            else
                map_insert(&g->locals, p->name, index + 1, 0);
            index++;
            break;
        case WASM_PASS_PAIR:
            add_slot(g, p->name, wasm_type_size(p->type), wasm_type_align(p->type));
            index += 2;
            break;
        case WASM_PASS_REF:
            map_insert(&g->refs, p->name, index + 1, 0);
            index++;
            break;
        case WASM_PASS_NONE:
            add_slot(g, p->name, wasm_type_size(p->type), wasm_type_align(p->type));
            break;
        }
    }
    if (tl->u.function.variadic)
        g->va_param = index;

    // Every other frame-resident name: an automatic local or a temporary.
    for (const Tac_Param *p = tl->u.function.locals; p; p = p->next) {
        if (!p->type)
            internal_error("wasm: %s: local %s has no type", fn->name, p->name);
        map_insert(&g->types, p->name, (intptr_t)p->type, 0);
        if (in_slot(g, p->name, p->type))
            add_slot(g, p->name, wasm_type_size(p->type), wasm_type_align(p->type));
        else
            map_insert(&g->locals, p->name, wasm_add_local(fn, wasm_valtype(p->type)) + 1, 0);
    }

    // Below them, the calls' area: copies of arguments, variable arguments, results.
    int area = 0;
    for (const Tac_Instruction *in = tl->u.function.body; in; in = in->next)
        if (in->kind == TAC_INSTRUCTION_FUN_CALL || in->kind == TAC_INSTRUCTION_FUN_CALL_NORETURN) {
            int size = call_area_size(g, in);
            if (size > area)
                area = size;
        } else if (is_ld_op(g, in) && area < 16) {
            area = 16; // a long double result
        }
    if (area) {
        g->frame_size = (g->frame_size + 15) & -16;
        g->call_area  = g->frame_size;
        g->frame_size += area;
    }
    g->frame_size = (g->frame_size + 15) & -16;
    // co_alloca moves __stack_pointer: the epilogue puts it back from the frame pointer.
    if (!g->frame_size && wasm_uses_alloca(tl))
        g->frame_size = 16;
    if (g->frame_size)
        fn->frame = g->fp = wasm_add_local(fn, WASM_I32);
}

void gen_prologue(Gen *g)
{
    if (!g->frame_size)
        return;
    Wasm_Func *fn                         = g->fn;
    wasm_append(fn, WASM_GLOBAL_GET)->sym = xstrdup("__stack_pointer");
    wasm_append(fn, WASM_I32_CONST)->imm  = g->frame_size;
    wasm_append(fn, WASM_I32_SUB);
    wasm_append(fn, WASM_LOCAL_TEE)->imm  = g->fp;
    wasm_append(fn, WASM_GLOBAL_SET)->sym = xstrdup("__stack_pointer");
    for (const Tac_Param *p = g->tl->u.function.params; p; p = p->next) {
        int off = var_slot(g, p->name);
        if (off < 0)
            continue;
        intptr_t index;
        map_get(&g->pindex, p->name, &index);
        index--;
        const Tac_Type *s;
        switch (wasm_pass(p->type, &s)) {
        case WASM_PASS_VALUE:
            wasm_append(fn, WASM_LOCAL_GET)->imm   = g->fp;
            wasm_append(fn, WASM_LOCAL_GET)->imm   = index;
            wasm_append(fn, wasm_store_op(s))->imm = off;
            break;
        case WASM_PASS_PAIR:
            for (int half = 0; half < 2; half++) {
                wasm_append(fn, WASM_LOCAL_GET)->imm = g->fp;
                wasm_append(fn, WASM_LOCAL_GET)->imm = index + half;
                wasm_append(fn, WASM_I64_STORE)->imm = off + 8 * half;
            }
            break;
        default:
            break;
        }
    }
}

void gen_epilogue(Gen *g)
{
    if (!g->frame_size)
        return;
    wasm_append(g->fn, WASM_LOCAL_GET)->imm = g->fp;
    wasm_append(g->fn, WASM_I32_CONST)->imm = g->frame_size;
    wasm_append(g->fn, WASM_I32_ADD);
    wasm_append(g->fn, WASM_GLOBAL_SET)->sym = xstrdup("__stack_pointer");
}

void gen_done(Gen *g)
{
    map_destroy(&g->locals);
    map_destroy(&g->types);
    map_destroy(&g->slots);
    map_destroy(&g->refs);
    map_destroy(&g->pindex);
    for (int i = 0; i < g->nhelpers; i++)
        xfree(g->helpers[i].sig);
    flow_free(g->flow);
    wasm_free_func(g->fn);
    g->fn   = NULL;
    g->flow = NULL;
}

int var_ref(const Gen *g, const char *name)
{
    intptr_t v;
    return map_get(&g->refs, name, &v) ? (int)v - 1 : -1;
}

int var_slot(const Gen *g, const char *name)
{
    intptr_t v;
    return map_get(&g->slots, name, &v) ? (int)v - 1 : -1;
}

const Tac_Type *var_type(const Gen *g, const char *name)
{
    intptr_t v;
    return map_get(&g->types, name, &v) ? (const Tac_Type *)v : NULL;
}

int find_local(const Gen *g, const char *name)
{
    intptr_t v;
    return map_get(&g->locals, name, &v) ? (int)v - 1 : -1;
}

int var_local(const Gen *g, const char *name)
{
    int local = find_local(g, name);
    if (local < 0)
        internal_error("wasm: %s: %s is not a local", g->fn->name, name);
    return local;
}

Wasm_Op wasm_load_op(const Tac_Type *t)
{
    switch (t->kind) {
    case TAC_TYPE_SCHAR:
        return WASM_I32_LOAD8_S;
    case TAC_TYPE_UCHAR:
        return WASM_I32_LOAD8_U;
    case TAC_TYPE_SHORT:
        return WASM_I32_LOAD16_S;
    case TAC_TYPE_USHORT:
        return WASM_I32_LOAD16_U;
    case TAC_TYPE_LONG_LONG:
    case TAC_TYPE_ULONG_LONG:
        return WASM_I64_LOAD;
    case TAC_TYPE_FLOAT:
        return WASM_F32_LOAD;
    case TAC_TYPE_DOUBLE:
        return WASM_F64_LOAD;
    case TAC_TYPE_LONG_DOUBLE:
    case TAC_TYPE_ARRAY:
    case TAC_TYPE_STRUCTURE:
        internal_error("wasm: no scalar load of an aggregate or a long double");
    default:
        return WASM_I32_LOAD;
    }
}

Wasm_Op wasm_store_op(const Tac_Type *t)
{
    switch (t->kind) {
    case TAC_TYPE_SCHAR:
    case TAC_TYPE_UCHAR:
        return WASM_I32_STORE8;
    case TAC_TYPE_SHORT:
    case TAC_TYPE_USHORT:
        return WASM_I32_STORE16;
    case TAC_TYPE_LONG_LONG:
    case TAC_TYPE_ULONG_LONG:
        return WASM_I64_STORE;
    case TAC_TYPE_FLOAT:
        return WASM_F32_STORE;
    case TAC_TYPE_DOUBLE:
        return WASM_F64_STORE;
    case TAC_TYPE_LONG_DOUBLE:
    case TAC_TYPE_ARRAY:
    case TAC_TYPE_STRUCTURE:
        internal_error("wasm: no scalar store of an aggregate or a long double");
    default:
        return WASM_I32_STORE;
    }
}

const Tac_Type *global_type(const Gen *g, const char *name)
{
    for (const Tac_StaticLocal *s = g->tl->u.function.static_locals; s; s = s->next)
        if (strcmp(s->name, name) == 0)
            return s->type;
    for (const Tac_TopLevel *t = g->program; t; t = t->next) {
        switch (t->kind) {
        case TAC_TOPLEVEL_STATIC_VARIABLE:
            if (strcmp(t->u.static_variable.name, name) == 0)
                return t->u.static_variable.type;
            break;
        case TAC_TOPLEVEL_STATIC_CONSTANT:
            if (strcmp(t->u.static_constant.name, name) == 0)
                return t->u.static_constant.type;
            break;
        case TAC_TOPLEVEL_EXTERN:
            if (strcmp(t->u.extern_.name, name) == 0)
                return t->u.extern_.type;
            break;
        case TAC_TOPLEVEL_FUNCTION:
            if (strcmp(t->u.function.name, name) == 0)
                return t->u.function.type;
            break;
        }
    }
    return NULL;
}
