//
// WebAssembly IR: allocation, release and the opcode tables.
//
#include "wasm_ir.h"

#include <string.h>

#include "xalloc.h"

static const char *const mnemonic[WASM_NUM_OPS] = {
#define WASM_MNEM(op, name, form) [WASM_##op] = name,
    WASM_OPS(WASM_MNEM)
#undef WASM_MNEM
};

static const Wasm_Form form[WASM_NUM_OPS] = {
#define WASM_FORM(op, name, form) [WASM_##op] = WASM_FORM_##form,
    WASM_OPS(WASM_FORM)
#undef WASM_FORM
};

const char *wasm_op_name(Wasm_Op op)
{
    return mnemonic[op];
}

Wasm_Form wasm_op_form(Wasm_Op op)
{
    return form[op];
}

const char *wasm_valtype_name(Wasm_ValType t)
{
    switch (t) {
    case WASM_I32:
        return "i32";
    case WASM_I64:
        return "i64";
    case WASM_F32:
        return "f32";
    case WASM_F64:
        return "f64";
    case WASM_VOID:
        break;
    }
    return "";
}

Wasm_Func *wasm_new_func(const char *name, bool global)
{
    Wasm_Func *fn = xalloc(sizeof(Wasm_Func), __func__, __FILE__, __LINE__);
    fn->name      = xstrdup(name);
    fn->global    = global;
    fn->frame     = -1;
    return fn;
}

static void free_instr(Wasm_Instr *in)
{
    xfree(in->sym);
    xfree(in->table);
    xfree(in);
}

void wasm_free_func(Wasm_Func *fn)
{
    Wasm_Instr *in = fn->first;
    while (in) {
        Wasm_Instr *next = in->next;
        free_instr(in);
        in = next;
    }
    xfree(fn->params);
    xfree(fn->locals);
    xfree(fn->name);
    xfree(fn);
}

Wasm_Instr *wasm_append(Wasm_Func *fn, Wasm_Op op)
{
    Wasm_Instr *in = xalloc(sizeof(Wasm_Instr), __func__, __FILE__, __LINE__);
    in->op         = op;
    in->prev       = fn->last;
    if (fn->last)
        fn->last->next = in;
    else
        fn->first = in;
    fn->last = in;
    return in;
}

void wasm_remove(Wasm_Func *fn, Wasm_Instr *in)
{
    if (in->prev)
        in->prev->next = in->next;
    else
        fn->first = in->next;
    if (in->next)
        in->next->prev = in->prev;
    else
        fn->last = in->prev;
    free_instr(in);
}

Wasm_Instr *wasm_insert(Wasm_Func *fn, Wasm_Instr *at, Wasm_Op op)
{
    if (!at)
        return wasm_append(fn, op);
    Wasm_Instr *in = xalloc(sizeof(Wasm_Instr), __func__, __FILE__, __LINE__);
    in->op         = op;
    in->next       = at;
    in->prev       = at->prev;
    if (at->prev)
        at->prev->next = in;
    else
        fn->first = in;
    at->prev = in;
    return in;
}

void wasm_move(Wasm_Func *fn, Wasm_Instr *first, Wasm_Instr *last, Wasm_Instr *at)
{
    // Unlink the run.
    if (first->prev)
        first->prev->next = last->next;
    else
        fn->first = last->next;
    if (last->next)
        last->next->prev = first->prev;
    else
        fn->last = first->prev;
    // Link it in before `at`.
    first->prev = at ? at->prev : fn->last;
    last->next  = at;
    if (first->prev)
        first->prev->next = first;
    else
        fn->first = first;
    if (at)
        at->prev = last;
    else
        fn->last = last;
}

bool wasm_stack_effect(const Wasm_Instr *in, int *pops, int *pushes)
{
    Wasm_Op op = in->op;
    *pops = *pushes = 0;
    switch (op) {
    case WASM_UNREACHABLE:
    case WASM_BLOCK:
    case WASM_LOOP:
    case WASM_IF:
    case WASM_ELSE:
    case WASM_END_BLOCK:
    case WASM_END_LOOP:
    case WASM_END_IF:
    case WASM_BR:
    case WASM_BR_IF:
    case WASM_BR_TABLE:
    case WASM_RETURN:
        return false;
    case WASM_NOP:
        return true;
    case WASM_CALL:
    case WASM_CALL_INDIRECT:
        *pops   = in->pops;
        *pushes = in->pushes;
        return true;
    case WASM_DROP:
    case WASM_LOCAL_SET:
    case WASM_GLOBAL_SET:
        *pops = 1;
        return true;
    case WASM_SELECT:
        *pops   = 3;
        *pushes = 1;
        return true;
    case WASM_LOCAL_GET:
    case WASM_GLOBAL_GET:
    case WASM_MEMORY_SIZE:
    case WASM_I32_CONST:
    case WASM_I64_CONST:
    case WASM_F32_CONST:
    case WASM_F64_CONST:
        *pushes = 1;
        return true;
    case WASM_MEMORY_COPY:
    case WASM_MEMORY_FILL:
        *pops = 3;
        return true;
    default:
        break;
    }
    if (op >= WASM_I32_STORE && op <= WASM_I64_STORE32) {
        *pops = 2;
        return true;
    }
    // A comparison or a binary operation takes two; the rest (a test, a load, a unary
    // operation or a conversion) one; each leaves one.
    bool binary =
        (op >= WASM_I32_EQ && op <= WASM_I32_GE_U) || (op >= WASM_I64_EQ && op <= WASM_I64_GE_U) ||
        (op >= WASM_F32_EQ && op <= WASM_F64_GE) || (op >= WASM_I32_ADD && op <= WASM_I64_SHR_U) ||
        (op >= WASM_F32_ADD && op <= WASM_F32_DIV) || (op >= WASM_F64_ADD && op <= WASM_F64_DIV);
    *pops   = binary ? 2 : 1;
    *pushes = 1;
    return true;
}

int wasm_add_local(Wasm_Func *fn, Wasm_ValType t)
{
    Wasm_ValType *locals =
        xalloc((fn->nlocals + 1) * sizeof(Wasm_ValType), __func__, __FILE__, __LINE__);
    if (fn->nlocals)
        memcpy(locals, fn->locals, fn->nlocals * sizeof(Wasm_ValType));
    locals[fn->nlocals] = t;
    xfree(fn->locals);
    fn->locals = locals;
    return fn->nparams + fn->nlocals++;
}
