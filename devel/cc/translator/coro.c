//
// Coroutines (docs/Coroutines_in_C.md; docs/Coroutines_Internals.md §5), in two stages.
//
// Stage 1 lowers a coroutine `coro(Y) T f(params)` as an ordinary function
// f$resume(fp, params) over a frame at fp, whose header is the runtime's
// (libc/wasm32/co.c): yield stores the value in the frame and calls __coro_suspend(fp),
// an opaque call to the optimizer; return stores the result and the state DONE.  The
// co_* operations become calls of the runtime.
//
// Stage 2, the split pass, runs after the optimizer: every name live across a
// suspension, every object whose address is taken, and the user's parameters move into
// the frame; each __coro_suspend becomes a return with the state set, and a label the
// dispatch at the top jumps to.  f$init stores the arguments, and f$co holds the frame's
// size and alignment for co_sizeof, co_alignof and co_alloca: clang's wasm assembler
// cannot make the absolute symbols f$size and f$align the first design meant to use.  A
// coroutine that takes (void) or (void *) has a coro_ptr, the address of its f$co, which
// then also holds f$init (or for (void), f$initp, which takes the void * and ignores it)
// and f$resume, so a frame can be set up and started through the pointer alone.
//
#include <string.h>

#include "cfg.h"
#include "liveness.h"
#include "target.h"
#include "translate.h"
#include "typecheck.h"
#include "xalloc.h"

// The header (libc/common/co.c): state and flags of the target's unsigned, then four
// pointers.  The state's two highest values are DONE and DESTROYED.
enum { CO_STATE = 0 };
#define CO_FLAGS           ((int)target_config->int_size)
#define CO_STATE_DONE      (co_state_max() - 1)
#define CO_STATE_DESTROYED co_state_max()
#define CO_SIGNAL_DESTROY  2

static int align_up(int n, int a)
{
    return (n + a - 1) / a * a;
}

static unsigned co_state_max(void)
{
    return target_config->int_size >= 4 ? 0xffffffffu : (1u << (8 * target_config->int_size)) - 1;
}

int co_header_size(void)
{
    return align_up(2 * (int)target_config->int_size, (int)target_config->pointer_align) +
           4 * (int)target_config->pointer_size;
}

static int co_header_align(void)
{
    int a = (int)target_config->int_align, p = (int)target_config->pointer_align;
    return a > p ? a : p;
}

void coro_layout(const Type *yield, const Type *result, int *value_off, int *result_off, int *end,
                 int *align)
{
    int off = co_header_size(), a = co_header_align();
    *value_off = *result_off = off;
    if (unalias(yield)->kind != TYPE_VOID) {
        int ya = (int)get_alignment(yield);
        off = *value_off = align_up(off, ya);
        off += (int)get_size(yield);
        a = ya > a ? ya : a;
    }
    if (unalias(result)->kind != TYPE_VOID) {
        int ra = (int)get_alignment(result);
        off = *result_off = align_up(off, ra);
        off += (int)get_size(result);
        a = ra > a ? ra : a;
    }
    *end   = off;
    *align = a > 16 ? 16 : a;
}

//
// Stage 1
//

static Tac_Type *tac_kind(Tac_TypeKind k)
{
    return tac_new_type(k);
}

static Tac_Type *void_ptr(void)
{
    return tac_type_ptr(tac_kind(TAC_TYPE_VOID));
}

static Tac_Type *char_ptr(void)
{
    return tac_type_ptr(tac_type_char());
}

static Tac_Type *size_type(void)
{
    Type t = { .kind = size_kind() };
    return ast_type_to_tac_type(&t);
}

// The type of f$resume: int (char *).
static Tac_Type *resume_type(void)
{
    Tac_Type *ft               = tac_kind(TAC_TYPE_FUN_TYPE);
    ft->u.fun_type.param_types = char_ptr();
    ft->u.fun_type.ret_type    = tac_kind(TAC_TYPE_INT);
    return ft;
}

// The type of f$co: the frame's size and alignment, then for a coroutine with a coro_ptr
// its init and resume functions, four words in all.
static Tac_Type *desc_type(bool with_ptr)
{
    Tac_Type *t          = tac_kind(TAC_TYPE_ARRAY);
    t->u.array.elem_type = size_type();
    t->u.array.size      = with_ptr ? 4 : 2;
    return t;
}

// The words of a descriptor.
enum { DESC_SIZE, DESC_ALIGN, DESC_INIT, DESC_RESUME };

// A word of a descriptor holding `n`: a size_t.
static Tac_StaticInit *size_init(unsigned n)
{
    Tac_StaticInit *w;
    switch (target_config->pointer_size) {
    case 2:
        w               = tac_new_static_init(TAC_STATIC_INIT_U16);
        w->u.ushort_val = (uint16_t)n;
        break;
    case 8:
        w              = tac_new_static_init(TAC_STATIC_INIT_U64);
        w->u.ulong_val = n;
        break;
    default:
        w             = tac_new_static_init(TAC_STATIC_INIT_U32);
        w->u.uint_val = n;
    }
    return w;
}

// The type every init function a descriptor holds has: void (char *, void *).
static Tac_Type *ptr_init_type(void)
{
    Tac_Type *ft                     = tac_kind(TAC_TYPE_FUN_TYPE);
    ft->u.fun_type.param_types       = char_ptr();
    ft->u.fun_type.param_types->next = void_ptr();
    ft->u.fun_type.ret_type          = tac_kind(TAC_TYPE_VOID);
    return ft;
}

// The type of f$init: void (char *, params...), for coroutine type `fn`.
static Tac_Type *init_type(const Type *fn)
{
    Tac_Type *ft  = tac_kind(TAC_TYPE_FUN_TYPE);
    Tac_Type **pt = &ft->u.fun_type.param_types;
    *pt           = char_ptr();
    pt            = &(*pt)->next;
    for (const Param *p = unalias(fn)->u.function.params; p; p = p->next) {
        *pt = ast_type_to_tac_type(p->type);
        pt  = &(*pt)->next;
    }
    ft->u.fun_type.ret_type = tac_kind(TAC_TYPE_VOID);
    return ft;
}

static char *suffixed(const char *name, const char *suffix)
{
    size_t n = strlen(name), m = strlen(suffix);
    char *s = xalloc(n + m + 1, __func__, __FILE__, __LINE__);
    memcpy(s, name, n);
    memcpy(s + n, suffix, m + 1);
    return s;
}

static void append(TacCtx *ctx, Tac_Instruction *in)
{
    tac_append(ctx, in);
}

// %d = ptr + off, a pointer to `pointee` (owned).
static Tac_Val *emit_offset(TacCtx *ctx, const char *ptr, int off, Tac_Type *pointee)
{
    Tac_Val *dst        = new_var_val(ctx, tac_type_ptr(pointee));
    Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_ADD_PTR);
    in->u.add_ptr.ptr   = val_var(ptr);
    in->u.add_ptr.index = val_int(off);
    in->u.add_ptr.scale = 1;
    in->u.add_ptr.dst   = dst;
    append(ctx, in);
    return val_var(dst->u.var_name);
}

static void emit_store(TacCtx *ctx, Tac_Val *src, Tac_Val *ptr)
{
    Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_STORE);
    in->u.store.src     = src;
    in->u.store.dst_ptr = ptr;
    append(ctx, in);
}

static Tac_Val *emit_load(TacCtx *ctx, Tac_Val *ptr, Tac_Type *type)
{
    Tac_Val *dst        = new_var_val(ctx, type);
    Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_LOAD);
    in->u.load.src_ptr  = ptr;
    in->u.load.dst      = dst;
    append(ctx, in);
    return val_var(dst->u.var_name);
}

static void emit_copy(TacCtx *ctx, Tac_Val *src, const char *dst)
{
    Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_COPY);
    in->u.copy.src      = src;
    in->u.copy.dst      = val_var(dst);
    append(ctx, in);
}

static Tac_Val *emit_address(TacCtx *ctx, const char *name, Tac_Type *pointee)
{
    Tac_Val *dst          = new_var_val(ctx, tac_type_ptr(pointee));
    Tac_Instruction *in   = tac_new_instruction(TAC_INSTRUCTION_GET_ADDRESS);
    in->u.get_address.src = val_var(name);
    in->u.get_address.dst = dst;
    append(ctx, in);
    return val_var(dst->u.var_name);
}

static void emit_return_int(TacCtx *ctx, int status)
{
    Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_RETURN);
    in->u.return_.src   = val_int(status);
    append(ctx, in);
}

// A call of routine `name` of return type `ret` (owned), with `n` arguments (owned) of
// types `params` (owned); declared by an EXTERN.  Its value, or NULL for void.
static Tac_Val *emit_call(TacCtx *ctx, const char *name, Tac_Type *ret, int n, Tac_Val **args,
                          Tac_Type **params)
{
    Tac_Type *ft        = tac_kind(TAC_TYPE_FUN_TYPE);
    Tac_Type **pt       = &ft->u.fun_type.param_types;
    Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_FUN_CALL);
    Tac_Val **at        = &in->u.fun_call.args;
    for (int i = 0; i < n; i++) {
        *pt = params[i];
        pt  = &(*pt)->next;
        *at = args[i];
        at  = &(*at)->next;
    }
    ft->u.fun_type.ret_type = ret;
    in->u.fun_call.fun_name = xstrdup(name);
    in->u.fun_call.fun_type = ft;
    if (ret->kind != TAC_TYPE_VOID)
        in->u.fun_call.dst = new_var_val(ctx, tac_clone_type(ret));
    tac_record_extern_tac(ctx, name, tac_clone_type(ft));
    append(ctx, in);
    return in->u.fun_call.dst ? val_var(in->u.fun_call.dst->u.var_name) : NULL;
}

static void emit_state(TacCtx *ctx, unsigned state)
{
    emit_store(ctx, val_uint(state),
               emit_offset(ctx, ctx->coro->fp, CO_STATE, tac_kind(TAC_TYPE_UINT)));
}

static bool is_aggregate(const Type *t)
{
    t = unalias(t);
    return t->kind == TYPE_STRUCT || t->kind == TYPE_UNION || t->kind == TYPE_ARRAY;
}

// Store value `v` (owned) of type `t` at `off` in the frame.
static void store_in_frame(TacCtx *ctx, Tac_Val *v, const Type *t, int off)
{
    Tac_Val *addr = emit_offset(ctx, ctx->coro->fp, off, ast_type_to_tac_type(t));
    if (is_aggregate(t)) {
        AggPlace dst = { NULL, 0, addr->u.var_name };
        AggPlace src = { v->u.var_name, 0, NULL };
        gen_aggregate_copy(ctx, &dst, &src, t);
        tac_free_val(v);
        tac_free_val(addr);
    } else {
        emit_store(ctx, v, addr);
    }
}

// The end of a coroutine, its exit actions run: the final state, and CO_DONE or the
// status of a destroy, which is 1 too.
void gen_finish_code(TacCtx *ctx, unsigned state)
{
    emit_state(ctx, state);
    emit_return_int(ctx, 1);
}

Tac_Val *gen_yield(TacCtx *ctx, Expr *e)
{
    const TacCoro *co = ctx->coro;
    if (e->u.yield_expr)
        store_in_frame(ctx, gen_expr(ctx, e->u.yield_expr), co->yield, co->value_off);
    Tac_Val *args[]    = { val_var(co->fp) };
    Tac_Type *params[] = { char_ptr() };
    Tac_Val *signal    = emit_call(ctx, "__coro_suspend", tac_kind(TAC_TYPE_INT), 1, args, params);

    // Resumed by co_destroy: leave every block, running its exit actions, and finish.
    Tac_Val *destroy     = new_var_val(ctx, tac_kind(TAC_TYPE_INT));
    Tac_Instruction *cmp = tac_new_instruction(TAC_INSTRUCTION_BINARY);
    cmp->u.binary.op     = TAC_BINARY_EQUAL;
    cmp->u.binary.src1   = val_var(signal->u.var_name);
    cmp->u.binary.src2   = val_int(CO_SIGNAL_DESTROY);
    cmp->u.binary.dst    = destroy;
    append(ctx, cmp);
    char *over                   = new_temp(ctx);
    Tac_Instruction *jz          = tac_new_instruction(TAC_INSTRUCTION_JUMP_IF_ZERO);
    jz->u.jump_if_zero.condition = val_var(destroy->u.var_name);
    jz->u.jump_if_zero.target    = over;
    append(ctx, jz);
    gen_finish(ctx, CO_STATE_DESTROYED);
    emit_label(ctx, over);
    return signal;
}

void gen_coro_return(TacCtx *ctx, Tac_Val *value, const Type *type)
{
    if (value)
        store_in_frame(ctx, value, type, ctx->coro->result_off);
    gen_finish(ctx, CO_STATE_DONE); // CO_DONE
}

// Does coroutine `g` have a coro_ptr, and so a descriptor of four words?
static bool has_coro_ptr(const char *g)
{
    const Symbol *sym = symtab_get_opt(g);
    return sym && sym->kind == SYM_FUNC && coroutine_has_coro_ptr(sym->type);
}

// The descriptor and f$resume of coroutine `g`, declared by EXTERNs.
static Tac_Val *coro_desc(TacCtx *ctx, const char *g)
{
    char *name = suffixed(g, "$co");
    tac_record_extern_tac(ctx, name, desc_type(has_coro_ptr(g)));
    Tac_Val *v = emit_address(ctx, name, size_type());
    xfree(name);
    return v;
}

static Tac_Val *coro_resume_fn(TacCtx *ctx, const char *g)
{
    char *name = suffixed(g, "$resume");
    tac_record_extern_tac(ctx, name, resume_type());
    Tac_Val *v = emit_address(ctx, name, resume_type());
    xfree(name);
    return v;
}

// A frame for coroutine `g` (the EXPR_VAR naming it) at `storage` of `bytes`: set up
// by the runtime, the root of a task of its own or, with a `parent` frame, of the
// parent's task; then the arguments `args` stored by g$init.
static void setup_frame(TacCtx *ctx, const Expr *g, Tac_Val *storage, Tac_Val *bytes, Tac_Val *desc,
                        Tac_Val *parent, Expr *args)
{
    Tac_Val *sargs[]    = { storage, bytes, desc, coro_resume_fn(ctx, g->u.var),
                            parent ? parent : val_int(0) };
    Tac_Type *sparams[] = { void_ptr(), size_type(), tac_type_ptr(size_type()),
                            tac_type_ptr(resume_type()), void_ptr() };
    tac_free_val(emit_call(ctx, "__coro_setup", void_ptr(), 5, sargs, sparams));

    char *init              = suffixed(g->u.var, "$init");
    Tac_Type *it            = init_type(g->type);
    Tac_Instruction *in     = tac_new_instruction(TAC_INSTRUCTION_FUN_CALL);
    in->u.fun_call.fun_name = xstrdup(init);
    in->u.fun_call.fun_type = tac_clone_type(it);
    Tac_Val **at            = &in->u.fun_call.args;
    *at                     = dup_val(storage);
    at                      = &(*at)->next;
    for (Expr *a = args; a; a = a->next) {
        *at = gen_expr(ctx, a);
        at  = &(*at)->next;
    }
    append(ctx, in);
    tac_record_extern_tac(ctx, init, it);
    xfree(init);
}

// A coroutine's name used as a value: its coro_ptr, the address of its descriptor.
Tac_Val *gen_coro_ptr(TacCtx *ctx, const char *g, const Type *type)
{
    char *name = suffixed(g, "$co");
    tac_record_extern_tac(ctx, name, desc_type(true));
    Tac_Val *dst          = new_var_val(ctx, ast_type_to_tac_type(type));
    Tac_Instruction *in   = tac_new_instruction(TAC_INSTRUCTION_GET_ADDRESS);
    in->u.get_address.src = val_var(name);
    in->u.get_address.dst = dst;
    append(ctx, in);
    xfree(name);
    return val_var(dst->u.var_name);
}

// Word `k` of the descriptor at variable `desc`, of type `type` (owned).
static Tac_Val *desc_word(TacCtx *ctx, const char *desc, int k, Tac_Type *type)
{
    Tac_Val *at =
        emit_offset(ctx, desc, k * (int)target_config->pointer_size, tac_clone_type(type));
    return emit_load(ctx, at, type);
}

// Is `e`, a co_* operation's coroutine, a coro_ptr rather than a coroutine's name?
static bool is_coro_ptr(const Expr *e)
{
    return coro_desc_target(e->type) != NULL;
}

// Like setup_frame, for the coroutine a coro_ptr's descriptor `desc` (a variable)
// describes: its resume and init functions read from the descriptor, init called
// through the pointer with the one argument `arg` (NULL: a null pointer).
static void setup_frame_by_ptr(TacCtx *ctx, const char *desc, Tac_Val *storage, Tac_Val *bytes,
                               Tac_Val *parent, Expr *arg)
{
    Tac_Val *resume     = desc_word(ctx, desc, DESC_RESUME, tac_type_ptr(resume_type()));
    Tac_Val *sargs[]    = { storage, bytes, val_var(desc), resume, parent ? parent : val_int(0) };
    Tac_Type *sparams[] = { void_ptr(), size_type(), tac_type_ptr(size_type()),
                            tac_type_ptr(resume_type()), void_ptr() };
    tac_free_val(emit_call(ctx, "__coro_setup", void_ptr(), 5, sargs, sparams));

    Tac_Val *init             = desc_word(ctx, desc, DESC_INIT, tac_type_ptr(ptr_init_type()));
    Tac_Instruction *in       = tac_new_instruction(TAC_INSTRUCTION_FUN_CALL);
    in->u.fun_call.fun_name   = xstrdup(init->u.var_name);
    in->u.fun_call.indirect   = true;
    in->u.fun_call.fun_type   = ptr_init_type();
    in->u.fun_call.args       = dup_val(storage);
    in->u.fun_call.args->next = arg ? gen_expr(ctx, arg) : val_int(0);
    append(ctx, in);
    tac_free_val(init);
}

// The frame of the coroutine being lowered.
static Tac_Val *own_frame(const TacCtx *ctx)
{
    return val_var(ctx->coro->fp);
}

// A copy of `v` (owned) in a variable of its own, of type `type` (owned).
static Tac_Val *in_variable(TacCtx *ctx, Tac_Val *v, Tac_Type *type)
{
    Tac_Val *p = new_var_val(ctx, type);
    emit_copy(ctx, v, p->u.var_name);
    Tac_Val *r = val_var(p->u.var_name);
    tac_free_val(p);
    return r;
}

static Tac_Val *emit_binary(TacCtx *ctx, Tac_BinaryOperator op, Tac_Val *a, Tac_Val *b,
                            Tac_Type *type)
{
    Tac_Val *dst        = new_var_val(ctx, type);
    Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_BINARY);
    in->u.binary.op     = op;
    in->u.binary.src1   = a;
    in->u.binary.src2   = b;
    in->u.binary.dst    = dst;
    append(ctx, in);
    return val_var(dst->u.var_name);
}

static void emit_jump_if(TacCtx *ctx, bool nonzero, Tac_Val *cond, const char *target)
{
    Tac_Instruction *j = tac_new_instruction(nonzero ? TAC_INSTRUCTION_JUMP_IF_NOT_ZERO
                                                     : TAC_INSTRUCTION_JUMP_IF_ZERO);
    if (nonzero) {
        j->u.jump_if_not_zero.condition = cond;
        j->u.jump_if_not_zero.target    = xstrdup(target);
    } else {
        j->u.jump_if_zero.condition = cond;
        j->u.jump_if_zero.target    = xstrdup(target);
    }
    append(ctx, j);
}

// co_resume, co_cancel, co_destroy: __coro_resume(p, signal), the status.
static Tac_Val *emit_resume(TacCtx *ctx, Tac_Val *p, Tac_Val *signal)
{
    Tac_Val *args[]    = { p, signal };
    Tac_Type *params[] = { void_ptr(), tac_kind(TAC_TYPE_INT) };
    return emit_call(ctx, "__coro_resume", tac_kind(TAC_TYPE_INT), 2, args, params);
}

// An arena await's resume of its own sub-frame `sub`, a frame of coroutine `g`: what
// __coro_resume does, without its checks or its indirect call.  They cannot fail here:
// the frame is private to the await, which never resumes it once it is done or while it
// runs, and destroys it only after it has suspended.
static Tac_Val *emit_direct_resume(TacCtx *ctx, const char *g, Tac_Val *sub, Tac_Val *signal)
{
    Tac_Val *flags =
        emit_binary(ctx, TAC_BINARY_LEFT_SHIFT, signal, val_int(1), tac_kind(TAC_TYPE_INT));
    flags = emit_binary(ctx, TAC_BINARY_BITWISE_OR, flags, val_int(1), tac_kind(TAC_TYPE_INT));
    emit_store(ctx, flags, emit_offset(ctx, sub->u.var_name, CO_FLAGS, tac_kind(TAC_TYPE_INT)));
    char *name         = suffixed(g, "$resume");
    Tac_Val *args[]    = { dup_val(sub) };
    Tac_Type *params[] = { char_ptr() };
    Tac_Val *status    = emit_call(ctx, name, tac_kind(TAC_TYPE_INT), 1, args, params);
    xfree(name);
    emit_store(ctx, val_int(0),
               emit_offset(ctx, sub->u.var_name, CO_FLAGS, tac_kind(TAC_TYPE_INT)));
    tac_free_val(sub);
    return status;
}

// Memory off the arena of the task the coroutine being lowered belongs to, for a frame
// of coroutine `g`, which a trap names when it does not fit.
static Tac_Val *emit_push(TacCtx *ctx, Tac_Val *bytes, Tac_Val *align, const char *g)
{
    Tac_Val *args[]    = { own_frame(ctx), bytes, align, gen_string_constant(ctx, g, strlen(g)) };
    Tac_Type *params[] = { void_ptr(), size_type(), size_type(), char_ptr() };
    return emit_call(ctx, "__coro_push", void_ptr(), 4, args, params);
}

static void emit_pop(TacCtx *ctx, Tac_Val *p)
{
    Tac_Val *args[]    = { own_frame(ctx), p };
    Tac_Type *params[] = { void_ptr(), void_ptr() };
    emit_call(ctx, "__coro_pop", tac_kind(TAC_TYPE_VOID), 2, args, params);
}

// co_alloca's and alloca's memory in a function: the stack, through the builtins every
// backend expands in place.
static const char *stack_builtin(const char *what)
{
    if (strcmp(what, "stack_save") == 0)
        return "__builtin_stack_save";
    if (strcmp(what, "alloca") == 0)
        return "__builtin_alloca";
    return "__builtin_stack_restore";
}

// bytes = (bytes + 15) & -16, in the variable `bytes`: a multiple of 16, as a
// coroutine's frame and the task's arena take it.
static void emit_round16(TacCtx *ctx, const Tac_Val *bytes)
{
    Tac_Instruction *rnd = tac_new_instruction(TAC_INSTRUCTION_BINARY);
    rnd->u.binary.op     = TAC_BINARY_ADD_UNSIGNED;
    rnd->u.binary.src1   = dup_val(bytes);
    rnd->u.binary.src2   = val_size(15);
    rnd->u.binary.dst    = dup_val(bytes);
    append(ctx, rnd);
    Tac_Instruction *msk = tac_new_instruction(TAC_INSTRUCTION_BINARY);
    msk->u.binary.op     = TAC_BINARY_BITWISE_AND;
    msk->u.binary.src1   = dup_val(bytes);
    msk->u.binary.src2   = val_size(target_config->pointer_size == 4 ? 0xfffffff0u : ~(uint64_t)15);
    msk->u.binary.dst    = dup_val(bytes);
    append(ctx, msk);
}

// alloca(n) (<alloca.h>): memory until the function returns.  The backend rounds n,
// and its epilogue gives the memory back.
Tac_Val *gen_alloca(TacCtx *ctx, Expr *e)
{
    Tac_Val *args[]    = { gen_expr(ctx, e->u.call.args) };
    Tac_Type *params[] = { size_type() };
    return emit_call(ctx, stack_builtin("alloca"), void_ptr(), 1, args, params);
}

static Tac_Val *start_frame(TacCtx *ctx, Expr *e)
{
    Expr *a = e->u.co_op.args;
    if (e->u.co_op.op == CO_OP_INIT) {
        Tac_Val *storage = gen_expr(ctx, a);
        Tac_Val *bytes   = gen_expr(ctx, a->next);
        Expr *g          = a->next->next;
        // Into a variable: the setup and g$init both take it.
        Tac_Val *p = in_variable(ctx, storage, ast_type_to_tac_type(e->type));
        if (is_coro_ptr(g)) {
            Tac_Val *desc = in_variable(ctx, gen_expr(ctx, g), ast_type_to_tac_type(g->type));
            setup_frame_by_ptr(ctx, desc->u.var_name, dup_val(p), bytes, NULL, g->next);
            tac_free_val(desc);
        } else {
            setup_frame(ctx, g, dup_val(p), bytes, coro_desc(ctx, g->u.var), NULL, g->next);
        }
        return p;
    }

    // co_alloca: memory until the end of the block, off the shadow stack in a function
    // and off the task's arena in a coroutine, whose shadow stack goes at each suspension.
    Expr *g     = a;
    bool by_ptr = is_coro_ptr(g);
    Tac_Val *sp =
        ctx->coro ? NULL : emit_call(ctx, stack_builtin("stack_save"), void_ptr(), 0, NULL, NULL);
    Tac_Val *desc  = by_ptr ? in_variable(ctx, gen_expr(ctx, g), tac_type_ptr(size_type()))
                            : coro_desc(ctx, g->u.var);
    Tac_Val *size  = emit_load(ctx, dup_val(desc), size_type());
    Tac_Val *extra = gen_expr(ctx, a->next);

    // bytes = (size + extra + 15) & -16
    Tac_Val *bytes       = new_var_val(ctx, size_type());
    Tac_Instruction *add = tac_new_instruction(TAC_INSTRUCTION_BINARY);
    add->u.binary.op     = TAC_BINARY_ADD_UNSIGNED;
    add->u.binary.src1   = size;
    add->u.binary.src2   = extra;
    add->u.binary.dst    = dup_val(bytes);
    append(ctx, add);
    emit_round16(ctx, bytes);

    Tac_Val *mem;
    if (ctx->coro) {
        mem = emit_push(ctx, dup_val(bytes), val_size(16), by_ptr ? "(coro_ptr)" : g->u.var);
    } else {
        Tac_Val *aargs[]    = { dup_val(bytes) };
        Tac_Type *aparams[] = { size_type() };
        mem = emit_call(ctx, stack_builtin("alloca"), void_ptr(), 1, aargs, aparams);
    }
    Tac_Val *p = in_variable(ctx, mem, ast_type_to_tac_type(e->type));
    if (by_ptr) {
        setup_frame_by_ptr(ctx, desc->u.var_name, dup_val(p), bytes, NULL, g->next->next);
        tac_free_val(desc);
    } else {
        setup_frame(ctx, g, dup_val(p), bytes, desc, NULL, g->next->next);
    }

    // The frame is null until the co_alloca runs, so a release that finds it null does
    // nothing: one that ran on another path, or the block entered again.
    Tac_Instruction *zero = tac_new_instruction(TAC_INSTRUCTION_COPY);
    zero->u.copy.src      = val_int(0);
    zero->u.copy.dst      = val_var(p->u.var_name);
    tac_scope_entry(ctx, zero);
    tac_scope_add(ctx, (ExitAction){ EXIT_CO_RELEASE, NULL, xstrdup(p->u.var_name),
                                     sp ? xstrdup(sp->u.var_name) : NULL });
    tac_free_val(sp);
    return p;
}

// At the end of a co_alloca's block: destroy the coroutine if it has not finished
// (its defers run), and give the memory back.
void gen_co_release(TacCtx *ctx, const ExitAction *a)
{
    char *skip = new_temp(ctx);
    emit_jump_if(ctx, false, val_var(a->frame), skip);
    Tac_Val *dargs[]    = { val_var(a->frame) };
    Tac_Type *dparams[] = { void_ptr() };
    Tac_Val *done       = emit_call(ctx, "__coro_done", tac_kind(TAC_TYPE_INT), 1, dargs, dparams);
    char *kept          = new_temp(ctx);
    emit_jump_if(ctx, true, done, kept);
    tac_free_val(emit_resume(ctx, val_var(a->frame), val_int(CO_SIGNAL_DESTROY)));
    emit_label(ctx, kept);
    if (a->sp) {
        Tac_Val *sargs[]    = { val_var(a->sp) };
        Tac_Type *sparams[] = { void_ptr() };
        emit_call(ctx, stack_builtin("stack_restore"), tac_kind(TAC_TYPE_VOID), 1, sargs, sparams);
    } else {
        emit_pop(ctx, val_var(a->frame));
    }
    emit_copy(ctx, val_int(0), a->frame);
    emit_label(ctx, skip);
    xfree(skip);
    xfree(kept);
}

// A value of type `t` at `addr` (a pointer variable): loaded, or an aggregate copied
// into a slot of its own.
static Tac_Val *read_at(TacCtx *ctx, const char *addr, const Type *t)
{
    if (!is_aggregate(t))
        return emit_load(ctx, val_var(addr), ast_type_to_tac_type(t));
    char *slot                     = new_typed_temp(ctx, ast_type_to_tac_type(t));
    Tac_Instruction *al            = tac_new_instruction(TAC_INSTRUCTION_ALLOCATE_LOCAL);
    al->u.allocate_local.name      = xstrdup(slot);
    al->u.allocate_local.size      = (int)get_size(t);
    al->u.allocate_local.alignment = (int)get_alignment(t);
    append(ctx, al);
    AggPlace dst = { slot, 0, NULL };
    AggPlace src = { NULL, 0, addr };
    gen_aggregate_copy(ctx, &dst, &src, t);
    Tac_Val *v = val_var(slot);
    xfree(slot);
    return v;
}

// Is `e` a call of a coroutine, an arena await's operand?
static bool is_coroutine_call(const Expr *e)
{
    if (e->kind != EXPR_CALL || e->u.call.func->kind != EXPR_VAR)
        return false;
    const Symbol *sym = symtab_get_opt(e->u.call.func->u.var);
    return sym && sym->kind == SYM_FUNC && sym->u.func.coro;
}

// Is `e` a call of a coro_ptr, the arena await of the coroutine it points to?
static bool is_coro_ptr_call(const Expr *e)
{
    return e->kind == EXPR_CALL && is_coro_ptr(e->u.call.func);
}

// await (docs/Coroutines_in_C.md, section 5): resume the sub-coroutine; while it
// suspends, suspend too, its value forwarded to our resumer and the signal we are
// resumed with forwarded into it; its result is the value.  The arena form takes the
// sub-coroutine's frame off the task's arena and gives it back at the end.
Tac_Val *gen_await(TacCtx *ctx, Expr *e)
{
    const TacCoro *co = ctx->coro;
    Expr *op          = e->u.await_expr;
    bool by_ptr       = is_coro_ptr_call(op);
    bool arena        = by_ptr || is_coroutine_call(op);
    Tac_Val *sub;
    if (by_ptr) {
        Tac_Val *desc = in_variable(ctx, gen_expr(ctx, op->u.call.func), tac_type_ptr(size_type()));
        Tac_Val *size = desc_word(ctx, desc->u.var_name, DESC_SIZE, size_type());
        Tac_Val *align = desc_word(ctx, desc->u.var_name, DESC_ALIGN, size_type());
        sub = in_variable(ctx, emit_push(ctx, dup_val(size), align, "(coro_ptr)"), void_ptr());
        setup_frame_by_ptr(ctx, desc->u.var_name, dup_val(sub), size, own_frame(ctx),
                           op->u.call.args);
        tac_free_val(desc);
    } else if (arena) {
        const Expr *g = op->u.call.func;
        Tac_Val *desc = coro_desc(ctx, g->u.var);
        Tac_Val *size = emit_load(ctx, dup_val(desc), size_type());
        Tac_Val *alp =
            emit_offset(ctx, desc->u.var_name, (int)target_config->pointer_size, size_type());
        Tac_Val *align = emit_load(ctx, alp, size_type());
        sub = in_variable(ctx, emit_push(ctx, dup_val(size), align, g->u.var), void_ptr());
        setup_frame(ctx, g, dup_val(sub), size, desc, own_frame(ctx), op->u.call.args);
    } else {
        sub = in_variable(ctx, gen_expr(ctx, op), void_ptr());
    }
    int value_off, result_off, end, align;
    coro_layout(co->yield, e->type, &value_off, &result_off, &end, &align);

    Tac_Val *sig = in_variable(ctx, val_int(0), tac_kind(TAC_TYPE_INT));
    char *loop   = new_temp(ctx);
    char *done   = new_temp(ctx);
    emit_label(ctx, loop);
    const char *g = arena && !by_ptr ? op->u.call.func->u.var : NULL;
    emit_jump_if(ctx, true,
                 g ? emit_direct_resume(ctx, g, dup_val(sub), dup_val(sig))
                   : emit_resume(ctx, dup_val(sub), dup_val(sig)),
                 done);

    // Suspended: its value is ours, and so is the suspension.
    if (unalias(co->yield)->kind != TYPE_VOID) {
        Tac_Type *yt  = ast_type_to_tac_type(co->yield);
        Tac_Val *from = emit_offset(ctx, sub->u.var_name, value_off, tac_clone_type(yt));
        Tac_Val *to   = emit_offset(ctx, co->fp, value_off, yt);
        if (is_aggregate(co->yield)) {
            AggPlace dst = { NULL, 0, to->u.var_name };
            AggPlace src = { NULL, 0, from->u.var_name };
            gen_aggregate_copy(ctx, &dst, &src, co->yield);
            tac_free_val(to);
        } else {
            emit_store(ctx, emit_load(ctx, dup_val(from), ast_type_to_tac_type(co->yield)), to);
        }
        tac_free_val(from);
    }
    Tac_Val *fargs[]    = { own_frame(ctx) };
    Tac_Type *fparams[] = { char_ptr() };
    Tac_Val *signal = emit_call(ctx, "__coro_suspend", tac_kind(TAC_TYPE_INT), 1, fargs, fparams);

    // Destroyed: the sub-coroutine first, then every block of ours.
    char *over = new_temp(ctx);
    emit_jump_if(ctx, false,
                 emit_binary(ctx, TAC_BINARY_EQUAL, dup_val(signal), val_int(CO_SIGNAL_DESTROY),
                             tac_kind(TAC_TYPE_INT)),
                 over);
    tac_free_val(g ? emit_direct_resume(ctx, g, dup_val(sub), val_int(CO_SIGNAL_DESTROY))
                   : emit_resume(ctx, dup_val(sub), val_int(CO_SIGNAL_DESTROY)));
    if (arena)
        emit_pop(ctx, dup_val(sub));
    gen_finish(ctx, CO_STATE_DESTROYED);
    emit_label(ctx, over);
    emit_copy(ctx, signal, sig->u.var_name);
    emit_jump(ctx, loop);

    // Done: the result, read before the frame is given back.
    emit_label(ctx, done);
    Tac_Val *result = val_int(0);
    if (unalias(e->type)->kind != TYPE_VOID) {
        tac_free_val(result);
        Tac_Val *addr =
            emit_offset(ctx, sub->u.var_name, result_off, ast_type_to_tac_type(e->type));
        result = read_at(ctx, addr->u.var_name, e->type);
        tac_free_val(addr);
    }
    if (arena)
        emit_pop(ctx, dup_val(sub));
    tac_free_val(sub);
    tac_free_val(sig);
    xfree(loop);
    xfree(done);
    xfree(over);
    return result;
}

// co_value and co_result: the address the runtime checks and returns, then the value.
static Tac_Val *read_frame(TacCtx *ctx, Expr *e)
{
    Tac_Val *p        = gen_expr(ctx, e->u.co_op.args);
    const Type *frame = unalias(unalias(e->u.co_op.args->type)->u.pointer.target);
    int value_off, result_off, end, align;
    coro_layout(frame->u.struct_t.frame_yield, frame->u.struct_t.frame_result, &value_off,
                &result_off, &end, &align);
    bool value         = e->u.co_op.op == CO_OP_VALUE;
    Tac_Val *args[]    = { p, val_uint(value ? value_off : result_off) };
    Tac_Type *params[] = { void_ptr(), tac_kind(TAC_TYPE_UINT) };
    Tac_Val *raw =
        emit_call(ctx, value ? "__coro_value" : "__coro_result", void_ptr(), 2, args, params);
    Tac_Val *addr = in_variable(ctx, raw, tac_type_ptr_to(e->type));
    Tac_Val *v    = read_at(ctx, addr->u.var_name, e->type);
    tac_free_val(addr);
    return v;
}

Tac_Val *gen_co_op(TacCtx *ctx, Expr *e)
{
    switch (e->u.co_op.op) {
    case CO_OP_INIT:
    case CO_OP_ALLOCA:
        return start_frame(ctx, e);
    case CO_OP_RESUME:
    case CO_OP_CANCEL:
    case CO_OP_DESTROY: {
        int signal = e->u.co_op.op - CO_OP_RESUME;
        return emit_resume(ctx, gen_expr(ctx, e->u.co_op.args), val_int(signal));
    }
    case CO_OP_DONE: {
        Tac_Val *args[]    = { gen_expr(ctx, e->u.co_op.args) };
        Tac_Type *params[] = { void_ptr() };
        return emit_call(ctx, "__coro_done", tac_kind(TAC_TYPE_INT), 1, args, params);
    }
    case CO_OP_VALUE:
    case CO_OP_RESULT:
        return read_frame(ctx, e);
    case CO_OP_SIZEOF:
    case CO_OP_ALIGNOF: {
        const Expr *g = e->u.co_op.args;
        Tac_Val *desc = is_coro_ptr(g)
                            ? in_variable(ctx, gen_expr(ctx, (Expr *)g), tac_type_ptr(size_type()))
                            : coro_desc(ctx, g->u.var);
        Tac_Val *v =
            desc_word(ctx, desc->u.var_name,
                      e->u.co_op.op == CO_OP_ALIGNOF ? DESC_ALIGN : DESC_SIZE, size_type());
        tac_free_val(desc);
        return v;
    }
    }
    internal_error("coroutines: unknown operation %d", (int)e->u.co_op.op);
}

//
// Stage 2: the split pass
//

typedef struct {
    char *name;
    const Tac_Type *type; // borrowed from the function's params or locals
    int off;
    int size, align;
    bool memory;  // an aggregate or long double: copied through a shadow, not loaded
    char *shadow; // memory: its copy in the shadow-stack frame, for whole-value uses
} Home;

typedef struct {
    Tac_TopLevel *fn;
    const char *fp;
    Home *homes;
    int nhomes, cap;
    StringMap index; // name -> home index + 1
    int ntemps;
    Tac_Instruction *head, *tail; // the rewritten body
} Split;

// The label of the dispatch's entry `k` (0: the start), with the coroutine's name (f of
// f$resume): the labels of a unit share one namespace in most assemblers, and `$` is
// a separator in some (avr-as).
static char *entry_label(const Split *s, int k)
{
    const char *f = s->fn->u.function.name;
    int n         = (int)strcspn(f, "$");
    char *l       = xalloc(strlen(f) + 32, __func__, __FILE__, __LINE__);
    if (k == 0)
        sprintf(l, "%%co.start.%.*s", n, f);
    else
        sprintf(l, "%%co.resume%d.%.*s", k, n, f);
    return l;
}

static int type_size(const Tac_Type *t)
{
    Tac_Layout layout;
    tac_layout_of_target(&layout);
    switch (t->kind) {
    case TAC_TYPE_POINTER:
    case TAC_TYPE_FUN_TYPE:
        return layout.pointer;
    case TAC_TYPE_ARRAY:
        return t->u.array.size * type_size(t->u.array.elem_type);
    case TAC_TYPE_STRUCTURE:
        return t->u.structure.size;
    case TAC_TYPE_VOID:
        return 1;
    default:
        return layout.scalar[t->kind];
    }
}

static int type_align(const Tac_Type *t)
{
    switch (t->kind) {
    case TAC_TYPE_ARRAY:
        return type_align(t->u.array.elem_type);
    case TAC_TYPE_STRUCTURE:
        return t->u.structure.alignment > 0 ? t->u.structure.alignment : 1;
    case TAC_TYPE_SHORT:
    case TAC_TYPE_USHORT:
        return (int)target_config->short_align;
    case TAC_TYPE_INT:
    case TAC_TYPE_UINT:
        return (int)target_config->int_align;
    case TAC_TYPE_LONG:
    case TAC_TYPE_ULONG:
        return (int)target_config->long_align;
    case TAC_TYPE_LONG_LONG:
    case TAC_TYPE_ULONG_LONG:
        return (int)target_config->llong_align;
    case TAC_TYPE_FLOAT:
        return (int)target_config->float_align;
    case TAC_TYPE_DOUBLE:
        return (int)target_config->double_align;
    case TAC_TYPE_LONG_DOUBLE:
        return (int)target_config->ldouble_align;
    case TAC_TYPE_POINTER:
    case TAC_TYPE_FUN_TYPE:
        return (int)target_config->pointer_align;
    default:
        return 1; // the chars
    }
}

static bool memory_type(const Tac_Type *t)
{
    return t->kind == TAC_TYPE_STRUCTURE || t->kind == TAC_TYPE_ARRAY ||
           t->kind == TAC_TYPE_LONG_DOUBLE;
}

static const Tac_Type *frame_name_type(const Tac_TopLevel *fn, const char *name)
{
    for (const Tac_Param *p = fn->u.function.params; p; p = p->next)
        if (strcmp(p->name, name) == 0)
            return p->type;
    for (const Tac_Param *p = fn->u.function.locals; p; p = p->next)
        if (strcmp(p->name, name) == 0)
            return p->type;
    return NULL;
}

static Home *home_of(const Split *s, const char *name)
{
    intptr_t v;
    return name && map_get((StringMap *)&s->index, name, &v) ? &s->homes[v - 1] : NULL;
}

// Give frame-resident `name` a home in the frame (once).
static void add_home(Split *s, const char *name, int size, int align)
{
    if (strcmp(name, s->fp) == 0)
        return;
    Home *h = home_of(s, name);
    if (h) {
        if (size > h->size)
            h->size = size;
        if (align > h->align)
            h->align = align;
        return;
    }
    const Tac_Type *t = frame_name_type(s->fn, name);
    if (!t)
        return; // a global
    if (s->nhomes == s->cap) {
        s->cap  = s->cap ? 2 * s->cap : 16;
        Home *n = xalloc(s->cap * sizeof *n, __func__, __FILE__, __LINE__);
        for (int i = 0; i < s->nhomes; i++)
            n[i] = s->homes[i];
        xfree(s->homes);
        s->homes = n;
    }
    h  = &s->homes[s->nhomes++];
    *h = (Home){ xstrdup(name), t, 0, type_size(t), type_align(t), memory_type(t), NULL };
    if (size > h->size)
        h->size = size;
    if (align > h->align)
        h->align = align;
    map_insert(&s->index, name, s->nhomes, 0);
}

static bool is_suspend(const Tac_Instruction *in)
{
    return in->kind == TAC_INSTRUCTION_FUN_CALL && !in->u.fun_call.indirect &&
           strcmp(in->u.fun_call.fun_name, "__coro_suspend") == 0;
}

typedef struct {
    Split *s;
    const char *except;
} LiveAdd;

static void add_live_cb(const char *key, intptr_t value, const void *arg)
{
    (void)value;
    const LiveAdd *la = arg;
    if (!la->except || strcmp(key, la->except) != 0)
        add_home(la->s, key, 0, 1);
}

// The names live across a suspension: live just after one, but for its own result.
static Tac_Instruction *find_live_across(Split *s, Tac_Instruction *body)
{
    OptCfg *cfg = cfg_build(body);
    int n       = cfg->nblocks;
    StringMap none;
    map_init(&none);
    StringMap *in_sets  = xalloc((n + 1) * sizeof(StringMap), __func__, __FILE__, __LINE__);
    StringMap *out_sets = xalloc((n + 1) * sizeof(StringMap), __func__, __FILE__, __LINE__);
    opt_live_solve(cfg, &none, &none, false, in_sets, out_sets);
    for (int i = 0; i < n; i++) {
        int cnt = 0;
        for (const Tac_Instruction *in = cfg->blocks[i]->first; in; in = in->next)
            cnt++;
        if (!cnt)
            continue;
        Tac_Instruction **ins = xalloc(cnt * sizeof *ins, __func__, __FILE__, __LINE__);
        int k                 = 0;
        for (Tac_Instruction *in = cfg->blocks[i]->first; in; in = in->next)
            ins[k++] = in;
        StringMap live;
        map_init(&live);
        opt_live_copy(&live, &out_sets[i]);
        for (int j = cnt - 1; j >= 0; j--) {
            const Tac_Instruction *in = ins[j];
            const Tac_Val *dst        = NULL;
            if (in->kind == TAC_INSTRUCTION_FUN_CALL)
                dst = in->u.fun_call.dst;
            if (is_suspend(in)) {
                LiveAdd la = { s, dst ? dst->u.var_name : NULL };
                map_iterate(&live, add_live_cb, &la);
            }
            if (dst && dst->kind == TAC_VAL_VAR)
                opt_live_remove(&live, dst->u.var_name); // a call defines its result
            opt_live_transfer(&live, in, &none, &none);
        }
        map_destroy(&live);
        xfree(ins);
    }
    opt_live_free(n, in_sets, out_sets);
    xfree(in_sets);
    xfree(out_sets);
    map_destroy(&none);
    body = cfg_flatten(cfg);
    cfg_free(cfg);
    return body;
}

// A fresh temporary of `type` (owned), recorded among the function's locals.
static char *split_temp(Split *s, Tac_Type *type)
{
    char buf[32];
    snprintf(buf, sizeof buf, "%%co.%d", s->ntemps++);
    Tac_Param *p             = tac_new_param();
    p->name                  = xstrdup(buf);
    p->type                  = type;
    p->next                  = s->fn->u.function.locals;
    s->fn->u.function.locals = p;
    return xstrdup(buf);
}

static void put(Split *s, Tac_Instruction *in)
{
    in->next = NULL;
    if (s->tail)
        s->tail->next = in;
    else
        s->head = in;
    s->tail = in;
}

// %a = fp + off, a pointer to a copy of `pointee`.
static char *put_offset(Split *s, int off, const Tac_Type *pointee)
{
    char *a             = split_temp(s, tac_type_ptr(tac_clone_type(pointee)));
    Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_ADD_PTR);
    in->u.add_ptr.ptr   = val_var(s->fp);
    in->u.add_ptr.index = val_int(off);
    in->u.add_ptr.scale = 1;
    in->u.add_ptr.dst   = val_var(a);
    put(s, in);
    return a;
}

static void put_load(Split *s, const char *ptr, const char *dst)
{
    Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_LOAD);
    in->u.load.src_ptr  = val_var(ptr);
    in->u.load.dst      = val_var(dst);
    put(s, in);
}

static void put_store(Split *s, Tac_Val *src, const char *ptr)
{
    Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_STORE);
    in->u.store.src     = src;
    in->u.store.dst_ptr = val_var(ptr);
    put(s, in);
}

// The unsigned integer type of a chunk of `n` bytes.
static Tac_Type *chunk_type(int n)
{
    return tac_new_type(n >= 8   ? TAC_TYPE_ULONG_LONG
                        : n == 4 ? TAC_TYPE_UINT
                        : n == 2 ? TAC_TYPE_USHORT
                                 : TAC_TYPE_UCHAR);
}

static int chunk_of(const Home *h)
{
    int c = h->align > 8 ? 8 : h->align;
    while (h->size % c)
        c /= 2;
    return c > 0 ? c : 1;
}

// Copy a home in memory between the frame and its shadow: in, before a use; out,
// after a definition.
static void put_shadow_copy(Split *s, const Home *h, bool in)
{
    int c = chunk_of(h);
    for (int k = 0; k < h->size; k += c) {
        Tac_Type *ct = chunk_type(c);
        char *a      = put_offset(s, h->off + k, ct);
        char *v      = split_temp(s, ct);
        if (in) {
            put_load(s, a, v);
            Tac_Instruction *to         = tac_new_instruction(TAC_INSTRUCTION_COPY_TO_OFFSET);
            to->u.copy_to_offset.src    = val_var(v);
            to->u.copy_to_offset.dst    = xstrdup(h->shadow);
            to->u.copy_to_offset.offset = k;
            put(s, to);
        } else {
            Tac_Instruction *from           = tac_new_instruction(TAC_INSTRUCTION_COPY_FROM_OFFSET);
            from->u.copy_from_offset.src    = xstrdup(h->shadow);
            from->u.copy_from_offset.offset = k;
            from->u.copy_from_offset.dst    = val_var(v);
            put(s, from);
            put_store(s, val_var(v), a);
        }
        xfree(a);
        xfree(v);
    }
}

// The shadow of a home in memory, made on first use.
static const char *shadow_of(Split *s, Home *h)
{
    if (!h->shadow)
        h->shadow = split_temp(s, tac_clone_type(h->type));
    return h->shadow;
}

// A use of a homed name in an operand: load it (or copy it into its shadow) and
// rename the operand.
static void use_val(Split *s, Tac_Val *v)
{
    for (; v; v = v->next) {
        Home *h = v->kind == TAC_VAL_VAR ? home_of(s, v->u.var_name) : NULL;
        if (!h)
            continue;
        xfree(v->u.var_name);
        if (h->memory) {
            v->u.var_name = xstrdup(shadow_of(s, h));
            put_shadow_copy(s, h, true);
        } else {
            char *a       = put_offset(s, h->off, h->type);
            v->u.var_name = split_temp(s, tac_clone_type(h->type));
            put_load(s, a, v->u.var_name);
            xfree(a);
        }
    }
}

// A definition of a homed name: rename it, and say how to write it back.
static Home *def_val(Split *s, Tac_Val *v, char **renamed)
{
    Home *h = v && v->kind == TAC_VAL_VAR ? home_of(s, v->u.var_name) : NULL;
    if (!h)
        return NULL;
    xfree(v->u.var_name);
    v->u.var_name = h->memory ? xstrdup(shadow_of(s, h)) : split_temp(s, tac_clone_type(h->type));
    *renamed      = v->u.var_name;
    return h;
}

static void write_back(Split *s, const Home *h, const char *renamed)
{
    if (!h)
        return;
    if (h->memory) {
        put_shadow_copy(s, h, false);
        return;
    }
    char *a = put_offset(s, h->off, h->type);
    put_store(s, val_var(renamed), a);
    xfree(a);
}

// The scalar member of `t` that starts at `off`, or NULL.
static const Tac_Type *member_at(const Tac_Type *t, int off)
{
    if (!t)
        return NULL;
    switch (t->kind) {
    case TAC_TYPE_STRUCTURE:
        for (const Tac_Member *m = t->u.structure.members; m; m = m->next)
            if (off >= m->offset && off < m->offset + type_size(m->type))
                return member_at(m->type, off - m->offset);
        return NULL;
    case TAC_TYPE_ARRAY: {
        int es = type_size(t->u.array.elem_type);
        return es ? member_at(t->u.array.elem_type, off % es) : NULL;
    }
    default:
        return off == 0 ? t : NULL;
    }
}

// Rewrite one instruction of the body (owned), the homed names into the frame.
static void rewrite(Split *s, Tac_Instruction *in)
{
    Home *h;
    switch (in->kind) {
    case TAC_INSTRUCTION_ALLOCATE_LOCAL:
        if (home_of(s, in->u.allocate_local.name)) {
            tac_free_instruction(in);
            return;
        }
        break;
    case TAC_INSTRUCTION_GET_ADDRESS:
    case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
    case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
        h = in->u.get_address.src->kind == TAC_VAL_VAR
                ? home_of(s, in->u.get_address.src->u.var_name)
                : NULL;
        if (h) {
            Tac_Instruction *ap   = tac_new_instruction(TAC_INSTRUCTION_ADD_PTR);
            ap->u.add_ptr.ptr     = val_var(s->fp);
            ap->u.add_ptr.index   = val_int(h->off);
            ap->u.add_ptr.scale   = 1;
            ap->u.add_ptr.dst     = in->u.get_address.dst;
            in->u.get_address.dst = NULL;
            tac_free_instruction(in);
            rewrite(s, ap);
            return;
        }
        break;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET:
        h = home_of(s, in->u.copy_to_offset.dst);
        if (h) {
            Tac_Val *src = in->u.copy_to_offset.src;
            const Tac_Type *t =
                src->kind == TAC_VAL_VAR ? frame_name_type(s->fn, src->u.var_name) : NULL;
            if (!t)
                t = member_at(h->type, in->u.copy_to_offset.offset);
            if (!t)
                internal_error("coroutines: a constant of unknown width stored in %s", h->name);
            use_val(s, src);
            char *a                  = put_offset(s, h->off + in->u.copy_to_offset.offset, t);
            in->u.copy_to_offset.src = NULL;
            put_store(s, src, a);
            xfree(a);
            tac_free_instruction(in);
            return;
        }
        break;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
        h = home_of(s, in->u.copy_from_offset.src);
        if (h) {
            Tac_Val *dst      = in->u.copy_from_offset.dst;
            const Tac_Type *t = frame_name_type(s->fn, dst->u.var_name);
            if (!t)
                internal_error("coroutines: %s has no type", dst->u.var_name);
            char *a                    = put_offset(s, h->off + in->u.copy_from_offset.offset, t);
            Tac_Instruction *ld        = tac_new_instruction(TAC_INSTRUCTION_LOAD);
            ld->u.load.src_ptr         = val_var(a);
            ld->u.load.dst             = dst;
            in->u.copy_from_offset.dst = NULL;
            tac_free_instruction(in);
            xfree(a);
            rewrite(s, ld);
            return;
        }
        break;
    default:
        break;
    }

    // Operands: the uses first, then the definition.
    Tac_Val *def = NULL;
    switch (in->kind) {
    case TAC_INSTRUCTION_RETURN:
        use_val(s, in->u.return_.src);
        break;
    case TAC_INSTRUCTION_UNARY:
        use_val(s, in->u.unary.src);
        def = in->u.unary.dst;
        break;
    case TAC_INSTRUCTION_BINARY:
        use_val(s, in->u.binary.src1);
        use_val(s, in->u.binary.src2);
        def = in->u.binary.dst;
        break;
    case TAC_INSTRUCTION_LOAD:
    case TAC_INSTRUCTION_LOAD_BYTE:
        use_val(s, in->u.load.src_ptr);
        def = in->u.load.dst;
        break;
    case TAC_INSTRUCTION_STORE:
    case TAC_INSTRUCTION_STORE_BYTE:
        use_val(s, in->u.store.src);
        use_val(s, in->u.store.dst_ptr);
        break;
    case TAC_INSTRUCTION_ADD_PTR:
        use_val(s, in->u.add_ptr.ptr);
        use_val(s, in->u.add_ptr.index);
        def = in->u.add_ptr.dst;
        break;
    case TAC_INSTRUCTION_PTR_DIFF:
        use_val(s, in->u.ptr_diff.ptr_a);
        use_val(s, in->u.ptr_diff.ptr_b);
        def = in->u.ptr_diff.dst;
        break;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET:
        use_val(s, in->u.copy_to_offset.src);
        break;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
        def = in->u.copy_from_offset.dst;
        break;
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
        use_val(s, in->u.jump_if_zero.condition);
        break;
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
        use_val(s, in->u.jump_if_not_zero.condition);
        break;
    case TAC_INSTRUCTION_FUN_CALL:
    case TAC_INSTRUCTION_FUN_CALL_NORETURN:
        use_val(s, in->u.fun_call.args);
        if (in->u.fun_call.indirect && home_of(s, in->u.fun_call.fun_name)) {
            Tac_Val v = { .kind = TAC_VAL_VAR, .u.var_name = in->u.fun_call.fun_name };
            use_val(s, &v);
            in->u.fun_call.fun_name = v.u.var_name;
        }
        def = in->u.fun_call.dst;
        break;
    case TAC_INSTRUCTION_JUMP:
    case TAC_INSTRUCTION_LABEL:
    case TAC_INSTRUCTION_ALLOCATE_LOCAL:
        break;
    default: // COPY, the conversions and GET_ADDRESS share the {src, dst} layout
        if (in->kind != TAC_INSTRUCTION_GET_ADDRESS &&
            in->kind != TAC_INSTRUCTION_GET_ADDRESS_BYTE &&
            in->kind != TAC_INSTRUCTION_GET_ADDRESS_DECAY)
            use_val(s, in->u.copy.src);
        def = in->u.copy.dst;
        break;
    }
    char *renamed  = NULL;
    const Home *dh = def_val(s, def, &renamed);
    put(s, in);
    write_back(s, dh, renamed);
}

// The k-th suspension: set the state and return CO_SUSPENDED; resumed, at the label,
// the signal is read out of the flags.
static void put_suspension(Split *s, Tac_Instruction *call, int k)
{
    char *a = put_offset(s, CO_STATE, &(Tac_Type){ .kind = TAC_TYPE_UINT });
    put_store(s, val_uint((unsigned)k), a);
    xfree(a);
    Tac_Instruction *ret = tac_new_instruction(TAC_INSTRUCTION_RETURN);
    ret->u.return_.src   = val_int(0);
    put(s, ret);
    Tac_Instruction *lab = tac_new_instruction(TAC_INSTRUCTION_LABEL);
    lab->u.label.name    = entry_label(s, k);
    put(s, lab);
    if (call->u.fun_call.dst) {
        char *f  = put_offset(s, CO_FLAGS, &(Tac_Type){ .kind = TAC_TYPE_INT });
        char *fl = split_temp(s, tac_new_type(TAC_TYPE_INT));
        put_load(s, f, fl);
        Tac_Instruction *sh  = tac_new_instruction(TAC_INSTRUCTION_BINARY);
        sh->u.binary.op      = TAC_BINARY_RIGHT_SHIFT;
        sh->u.binary.src1    = val_var(fl);
        sh->u.binary.src2    = val_int(1);
        sh->u.binary.dst     = call->u.fun_call.dst;
        call->u.fun_call.dst = NULL;
        rewrite(s, sh);
        xfree(f);
        xfree(fl);
    }
    tac_free_instruction(call);
}

static void free_split(Split *s)
{
    for (int i = 0; i < s->nhomes; i++) {
        xfree(s->homes[i].name);
        xfree(s->homes[i].shadow);
    }
    xfree(s->homes);
    map_destroy(&s->index);
}

// The suspension points from which the dispatch is a jump table rather than a chain of
// compares.
int coro_table_min = 3;

Tac_TopLevel *coro_split(Tac_TopLevel *fn, const CoroSplit *info)
{
    Split s = { 0 };
    s.fn    = fn;
    s.fp    = fn->u.function.params->name;
    map_init(&s.index);

    // The user's parameters, in order: f$init stores them.
    for (const Tac_Param *p = fn->u.function.params->next; p; p = p->next)
        add_home(&s, p->name, 0, 1);
    // Every object in memory or with its address taken.
    for (const Tac_Instruction *in = fn->u.function.body; in; in = in->next) {
        if (in->kind == TAC_INSTRUCTION_ALLOCATE_LOCAL)
            add_home(&s, in->u.allocate_local.name, in->u.allocate_local.size,
                     in->u.allocate_local.alignment);
        if ((in->kind == TAC_INSTRUCTION_GET_ADDRESS ||
             in->kind == TAC_INSTRUCTION_GET_ADDRESS_BYTE ||
             in->kind == TAC_INSTRUCTION_GET_ADDRESS_DECAY) &&
            in->u.get_address.src->kind == TAC_VAL_VAR)
            add_home(&s, in->u.get_address.src->u.var_name, 0, 1);
    }
    // Every name live across a suspension.
    fn->u.function.body = find_live_across(&s, fn->u.function.body);

    // Parameters in parameter order: their homes were added first, in that order.
    int align = info->frame_align;
    int end   = 0;
    {
        int off = info->frame_start;
        int i   = 0;
        for (const Tac_Param *p = fn->u.function.params->next; p; p = p->next, i++) {
            Home *h = &s.homes[i];
            int ha  = h->align > 16 ? 16 : h->align;
            off     = align_up(off, ha);
            h->off  = off;
            off += h->size;
            if (ha > align)
                align = ha;
        }
        for (int a = 16; a >= 1; a /= 2)
            for (int j = i; j < s.nhomes; j++) {
                Home *h = &s.homes[j];
                int ha  = h->align > 16 ? 16 : h->align;
                if (ha != a)
                    continue;
                off    = align_up(off, ha);
                h->off = off;
                off += h->size;
                if (ha > align)
                    align = ha;
            }
        end = align_up(off, align);
    }

    // The dispatch, then the body with the homed names in the frame.
    Tac_Instruction *body = fn->u.function.body;
    fn->u.function.body   = NULL;
    int k                 = 0;
    for (const Tac_Instruction *in = body; in; in = in->next)
        if (is_suspend(in))
            k++;
    if (target_config->jump_tables && k >= coro_table_min) {
        // A jump table on the state: 0 and anything else into the body's start.
        char *a  = put_offset(&s, CO_STATE, &(Tac_Type){ .kind = TAC_TYPE_UINT });
        char *st = split_temp(&s, tac_new_type(TAC_TYPE_UINT));
        put_load(&s, a, st);
        xfree(a);
        Tac_Instruction *jt      = tac_new_instruction(TAC_INSTRUCTION_JUMP_TABLE);
        jt->u.jump_table.index   = val_var(st);
        jt->u.jump_table.count   = k + 1;
        jt->u.jump_table.targets = xalloc((k + 1) * sizeof(char *), __func__, __FILE__, __LINE__);
        jt->u.jump_table.targets[0] = entry_label(&s, 0);
        for (int i = 1; i <= k; i++)
            jt->u.jump_table.targets[i] = entry_label(&s, i);
        jt->u.jump_table.default_target = entry_label(&s, 0);
        put(&s, jt);
        Tac_Instruction *start = tac_new_instruction(TAC_INSTRUCTION_LABEL);
        start->u.label.name    = entry_label(&s, 0);
        put(&s, start);
        xfree(st);
    } else if (k > 0) {
        char *a  = put_offset(&s, CO_STATE, &(Tac_Type){ .kind = TAC_TYPE_UINT });
        char *st = split_temp(&s, tac_new_type(TAC_TYPE_UINT));
        put_load(&s, a, st);
        xfree(a);
        for (int i = 1; i <= k; i++) {
            char *c             = split_temp(&s, tac_new_type(TAC_TYPE_INT));
            Tac_Instruction *eq = tac_new_instruction(TAC_INSTRUCTION_BINARY);
            eq->u.binary.op     = TAC_BINARY_EQUAL;
            eq->u.binary.src1   = val_var(st);
            eq->u.binary.src2   = val_uint((unsigned)i);
            eq->u.binary.dst    = val_var(c);
            put(&s, eq);
            Tac_Instruction *j              = tac_new_instruction(TAC_INSTRUCTION_JUMP_IF_NOT_ZERO);
            j->u.jump_if_not_zero.condition = val_var(c);
            j->u.jump_if_not_zero.target    = entry_label(&s, i);
            put(&s, j);
            xfree(c);
        }
        xfree(st);
    }
    k = 0;
    while (body) {
        Tac_Instruction *in = body;
        body                = body->next;
        in->next            = NULL;
        if (is_suspend(in))
            put_suspension(&s, in, ++k);
        else
            rewrite(&s, in);
    }
    // The shadows of the homes in memory, at the top of the shadow-stack frame.
    for (int i = s.nhomes - 1; i >= 0; i--) {
        if (!s.homes[i].shadow)
            continue;
        Tac_Instruction *al            = tac_new_instruction(TAC_INSTRUCTION_ALLOCATE_LOCAL);
        al->u.allocate_local.name      = xstrdup(s.homes[i].shadow);
        al->u.allocate_local.size      = s.homes[i].size;
        al->u.allocate_local.alignment = s.homes[i].align > 16 ? 16 : s.homes[i].align;
        al->next                       = s.head;
        s.head                         = al;
    }
    fn->u.function.body = s.head;

    // f$init(fp, params...): store each parameter in its home.
    Tac_TopLevel *init      = tac_new_toplevel(TAC_TOPLEVEL_FUNCTION);
    init->u.function.name   = suffixed(info->name, "$init");
    init->u.function.global = info->global;
    init->u.function.params = fn->u.function.params;
    init->u.function.type   = tac_new_type(TAC_TYPE_FUN_TYPE);
    Tac_Type **pt           = &init->u.function.type->u.fun_type.param_types;
    for (const Tac_Param *p = init->u.function.params; p; p = p->next) {
        *pt = tac_clone_type(p->type);
        pt  = &(*pt)->next;
    }
    init->u.function.type->u.fun_type.ret_type = tac_new_type(TAC_TYPE_VOID);
    {
        Split is  = { 0 };
        is.fn     = init;
        is.fp     = s.fp;
        is.ntemps = 0;
        map_init(&is.index);
        int i = 0;
        for (const Tac_Param *p = init->u.function.params->next; p; p = p->next, i++) {
            const Home *h = &s.homes[i];
            if (h->memory) {
                int c = chunk_of(h);
                for (int off = 0; off < h->size; off += c) {
                    Tac_Type *ct          = chunk_type(c);
                    char *v               = split_temp(&is, ct);
                    Tac_Instruction *from = tac_new_instruction(TAC_INSTRUCTION_COPY_FROM_OFFSET);
                    from->u.copy_from_offset.src    = xstrdup(p->name);
                    from->u.copy_from_offset.offset = off;
                    from->u.copy_from_offset.dst    = val_var(v);
                    put(&is, from);
                    char *a = put_offset(&is, h->off + off, ct);
                    put_store(&is, val_var(v), a);
                    xfree(a);
                    xfree(v);
                }
            } else {
                char *a = put_offset(&is, h->off, p->type);
                put_store(&is, val_var(p->name), a);
                xfree(a);
            }
        }
        Tac_Instruction *ret = tac_new_instruction(TAC_INSTRUCTION_RETURN);
        put(&is, ret);
        init->u.function.body = is.head;
        map_destroy(&is.index);
    }

    // f$resume keeps only the frame pointer.
    Tac_Param *fp         = tac_new_param();
    fp->name              = xstrdup(s.fp);
    fp->type              = tac_clone_type(init->u.function.params->type);
    fn->u.function.params = fp;
    tac_free_type(fn->u.function.type);
    fn->u.function.type = resume_type();

    // f$co: the frame's size and alignment, and for a coroutine with a coro_ptr its init
    // and resume functions.
    Tac_TopLevel *desc                = tac_new_toplevel(TAC_TOPLEVEL_STATIC_VARIABLE);
    desc->u.static_variable.name      = suffixed(info->name, "$co");
    desc->u.static_variable.global    = info->global;
    desc->u.static_variable.type      = desc_type(info->with_ptr);
    Tac_StaticInit *size              = size_init((unsigned)end);
    Tac_StaticInit *al                = size_init((unsigned)align);
    size->next                        = al;
    desc->u.static_variable.init_list = size;
    init->next                        = desc;

    if (info->with_ptr) {
        // (void): an init that takes the void * as every descriptor's does, and stores
        // nothing; (void *): f$init itself has that type.
        const char *init_name = init->u.function.name;
        if (!init->u.function.params->next) {
            Tac_TopLevel *initp      = tac_new_toplevel(TAC_TOPLEVEL_FUNCTION);
            initp->u.function.name   = suffixed(info->name, "$initp");
            initp->u.function.global = info->global;
            Tac_Param *pf            = tac_new_param();
            pf->name                 = xstrdup(s.fp);
            pf->type                 = char_ptr();
            pf->next                 = tac_new_param();
            pf->next->name           = xstrdup("%.arg");
            pf->next->type           = void_ptr();
            initp->u.function.params = pf;
            initp->u.function.type   = ptr_init_type();
            initp->u.function.body   = tac_new_instruction(TAC_INSTRUCTION_RETURN);
            desc->next               = initp;
            init_name                = initp->u.function.name;
        }
        Tac_StaticInit *ip = tac_new_static_init(TAC_STATIC_INIT_POINTER);
        ip->u.pointer.name = xstrdup(init_name);
        Tac_StaticInit *rp = tac_new_static_init(TAC_STATIC_INIT_POINTER);
        rp->u.pointer.name = xstrdup(fn->u.function.name);
        al->next           = ip;
        ip->next           = rp;
    }

    free_split(&s);
    return init;
}
