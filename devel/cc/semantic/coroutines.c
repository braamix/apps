//
// Type-checking for coroutines (docs/Coroutines_in_C.md; docs/Coroutines_Internals.md §3.2, §6).
//
// A coroutine is a function symbol marked `coro` with its yield type Y; its C type
// stays `T f(params)`, so calls through co_init, co_alloca and an arena await check
// their arguments as any call does.  A frame type _Coro_frame(Y, T) is a struct tagged
// __co_frame that is never defined (the parser builds it); two are the same type when
// their Y and T are.  _Coro_ptr(Y, T) is a pointer to another such struct, tagged
// __co_desc: the descriptor f$co of a coroutine that takes (void) or (void *), which is
// what the coroutine's name converts to when it is used as a value.  Every compile-time rule of
// §2.4 is here, but for the jumps past a co_alloca, which semantic/defer.c checks with those past a
// defer.
//
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "semantic.h"
#include "symtab.h"
#include "target.h"
#include "typecheck.h"
#include "xalloc.h"

// The lint for frames in automatic storage, at the end of this file.
static void lint_init(const Expr *e, const Expr *storage, const char *coro);
static void lint_settled(const Expr *p);

// The function being checked when it is a coroutine: its yield type, else NULL.
static Type *coro_yield;
static bool in_coro;

// Depth of deferred statements and of loop heads around the expression being checked.
int coro_defer_depth;
int coro_loop_head_depth;

// The call an arena await is checking: the one call of a coroutine allowed.
static const Expr *arena_call;

static void require_target(void)
{
    if (target_config->no_coroutines)
        fatal_error("coroutines are not supported on target '%s'", target_config->name);
}

// A short name for the messages: co_init for __co_init.
static const char *op_name(CoOp op)
{
    return co_op_name[op] + 2;
}

bool is_coro_struct(const Type *t)
{
    t = unalias(t);
    return t && t->kind == TYPE_STRUCT && t->u.struct_t.frame_yield;
}

bool is_frame_type(const Type *t)
{
    return is_coro_struct(t) && strcmp(unalias(t)->u.struct_t.name, "__co_frame") == 0;
}

// The descriptor type a coro_ptr(Y, T) points to, or NULL for any other type.
const Type *coro_desc_target(const Type *t)
{
    t = unalias(t);
    if (!t || t->kind != TYPE_POINTER)
        return NULL;
    const Type *d = unalias(t->u.pointer.target);
    return is_coro_struct(d) && strcmp(d->u.struct_t.name, "__co_desc") == 0 ? d : NULL;
}

// The frame type a co_frame(Y, T) * points to, or NULL for any other type.
static const Type *frame_target(const Type *t)
{
    t = unalias(t);
    if (!t || t->kind != TYPE_POINTER)
        return NULL;
    const Type *f = unalias(t->u.pointer.target);
    return is_frame_type(f) ? f : NULL;
}

bool same_frame_type(const Type *a, const Type *b)
{
    return strcmp(a->u.struct_t.name, b->u.struct_t.name) == 0 &&
           compatible_type(a->u.struct_t.frame_yield, b->u.struct_t.frame_yield) &&
           compatible_type(a->u.struct_t.frame_result, b->u.struct_t.frame_result);
}

// A new co_frame(Y, T) *, or with `tag` __co_desc a coro_ptr(Y, T).
static Type *new_coro_pointer(const char *tag, const Type *y, const Type *t)
{
    Type *frame                    = new_type(TYPE_STRUCT, __func__, __FILE__, __LINE__);
    frame->u.struct_t.name         = xstrdup(tag);
    frame->u.struct_t.frame_yield  = clone_type(y, __func__, __FILE__, __LINE__);
    frame->u.struct_t.frame_result = clone_type(t, __func__, __FILE__, __LINE__);
    Type *ptr                      = new_type(TYPE_POINTER, __func__, __FILE__, __LINE__);
    ptr->u.pointer.target          = frame;
    return ptr;
}

// Y or T of a coroutine or a frame: void, or an object type that is not an array.
static void check_value_type(const Type *t, const char *what)
{
    const Type *u = unalias(t);
    if (u->kind == TYPE_ARRAY)
        fatal_error("%s cannot be an array", what);
    if (u->kind == TYPE_FUNCTION)
        fatal_error("%s cannot be a function", what);
}

void check_frame_type(const Type *t)
{
    const char *what = strcmp(t->u.struct_t.name, "__co_desc") == 0 ? "coro_ptr" : "co_frame";
    char msg[64];
    validate_type(t->u.struct_t.frame_yield);
    validate_type(t->u.struct_t.frame_result);
    snprintf(msg, sizeof msg, "The yield type of a %s", what);
    check_value_type(t->u.struct_t.frame_yield, msg);
    snprintf(msg, sizeof msg, "The result type of a %s", what);
    check_value_type(t->u.struct_t.frame_result, msg);
}

bool coroutine_has_coro_ptr(const Type *fn_type)
{
    const Param *p = unalias(fn_type)->u.function.params;
    if (!p)
        return true; // (void), its sentinel stripped: a coroutine always has a prototype
    const Type *t = unalias(p->type);
    if (t->kind == TYPE_VOID && !p->name && !p->next)
        return true; // (void)
    return !p->next && t->kind == TYPE_POINTER && unalias(t->u.pointer.target)->kind == TYPE_VOID;
}

static FunctionSpec *coro_spec(const DeclSpec *spec)
{
    FunctionSpec *found = NULL;
    for (FunctionSpec *fs = spec ? spec->func_specs : NULL; fs; fs = fs->next) {
        if (fs->kind == FUNC_SPEC_CORO) {
            if (found)
                fatal_error("more than one '_Coro' in a declaration");
            found = fs;
        }
    }
    return found;
}

void reject_coro_spec(const DeclSpec *spec, const char *name)
{
    if (coro_spec(spec))
        fatal_error("'_Coro' on '%s', which is not a function", name);
}

// On Braam (docs/Braam.md §7) the runtime awaits main: it is a coroutine yielding
// the runtime's requests, and takes argc and argv.
static void check_braam_main(const Type *yield, const Type *fn_type)
{
    static const char shape[] = "on Braam, main is coro(braam_call *) int main(int, char **)";
    const Type *y             = unalias(yield);
    const Type *t             = y->kind == TYPE_POINTER ? unalias(y->u.pointer.target) : NULL;
    if (!t || t->kind != TYPE_STRUCT || !t->u.struct_t.name ||
        strcmp(t->u.struct_t.name, "braam_call") != 0)
        fatal_error("%s: the yield type is not braam_call *", shape);
    fn_type = unalias(fn_type);
    if (unalias(fn_type->u.function.return_type)->kind != TYPE_INT)
        fatal_error("%s: it does not return int", shape);
    const Param *argc = fn_type->u.function.params;
    const Param *argv = argc ? argc->next : NULL;
    const Type *pp    = argv ? unalias(argv->type) : NULL;
    if (!argv || argv->next || unalias(argc->type)->kind != TYPE_INT ||
        (pp->kind != TYPE_POINTER && pp->kind != TYPE_ARRAY))
        fatal_error("%s: the parameters are not (int, char **)", shape);
}

const Type *check_coroutine_decl(const char *name, const DeclSpec *spec, const Type *fn_type)
{
    FunctionSpec *cs = coro_spec(spec);
    bool main        = strcmp(name, "main") == 0;
    if (!cs) {
        if (main && target_config->braam)
            fatal_error("on Braam, main is coro(braam_call *) int main(int, char **)");
        return NULL;
    }
    require_target();
    if (main && !target_config->braam)
        fatal_error("main cannot be a coroutine");
    for (const FunctionSpec *fs = spec->func_specs; fs; fs = fs->next) {
        if (fs->kind == FUNC_SPEC_INLINE)
            fatal_error("coroutine '%s' cannot be 'inline'", name);
        if (fs->kind == FUNC_SPEC_NORETURN)
            fatal_error("coroutine '%s' cannot be '_Noreturn'", name);
    }
    fn_type = unalias(fn_type);
    if (fn_type->u.function.variadic)
        fatal_error("coroutine '%s' cannot be variadic", name);
    if (!fn_type->u.function.params)
        fatal_error("coroutine '%s' needs a prototype: write (void) for no parameters", name);
    cs->yield_type = resolve_typedef_names(cs->yield_type);
    validate_type(cs->yield_type);
    check_value_type(cs->yield_type, "The yield type of a coroutine");
    if (main)
        check_braam_main(cs->yield_type, fn_type);
    return cs->yield_type;
}

void agree_coroutine(const Symbol *existing, const Type *yield, const char *name)
{
    if (!existing || existing->kind != SYM_FUNC)
        return;
    if (existing->u.func.coro != (yield != NULL) ||
        (yield && !compatible_type(existing->u.func.yield_type, yield)))
        fatal_error("conflicting types for '%s'", name);
}

void coro_begin_body(const Type *yield)
{
    in_coro    = yield != NULL;
    coro_yield = yield ? clone_type(yield, __func__, __FILE__, __LINE__) : NULL;
}

void coro_end_body(void)
{
    free_type(coro_yield);
    coro_yield = NULL;
    in_coro    = false;
}

// The coroutine a co_* operation or an await names, which must be a bare identifier.
static const Symbol *named_coroutine(Expr *e, const char *what)
{
    if (e->kind != EXPR_VAR)
        fatal_error("'%s' needs the name of a coroutine", what);
    const Symbol *sym = symtab_get(e->u.var);
    if (sym->kind != SYM_FUNC || !sym->u.func.coro)
        fatal_error("'%s': '%s' is not a coroutine", what, e->u.var);
    // Typed as the function, so the translator can tell the callee (as for a call).
    free_type(e->type);
    e->type = clone_type(sym->type, __func__, __FILE__, __LINE__);
    return sym;
}

static const Type *result_of(const Symbol *sym)
{
    return unalias(sym->type)->u.function.return_type;
}

// alloca (<alloca.h>): memory on the stack of an ordinary function, which a
// coroutine's frame outlives.
void check_alloca_call(void)
{
    if (in_coro)
        fatal_error("'alloca' in a coroutine: its frame outlives the stack");
}

bool coroutine_call_allowed(const Expr *call)
{
    return call == arena_call;
}

void check_coroutine_name(const Symbol *sym)
{
    if (sym->kind == SYM_FUNC && sym->u.func.coro)
        fatal_error(
            "coroutine '%s' may only be named in co_init, co_alloca, co_sizeof, "
            "co_alignof or await, or used as a coro_ptr when it takes (void) or "
            "(void *)",
            sym->name);
}

bool coroutine_value(Expr *e, const Symbol *sym)
{
    if (sym->kind != SYM_FUNC || !sym->u.func.coro)
        return false;
    if (!coroutine_has_coro_ptr(sym->type))
        check_coroutine_name(sym);
    free_type(e->type);
    e->type = new_coro_pointer("__co_desc", sym->u.func.yield_type,
                               unalias(sym->type)->u.function.return_type);
    return true;
}

// The argument a coro_ptr's coroutine is started with: none, or one converted to void *.
static Expr *coro_ptr_args(Expr *args, const char *what)
{
    if (!args)
        return NULL;
    if (args->next)
        fatal_error("'%s': a coro_ptr takes at most one argument, a void *", what);
    static const Type void_type = { .kind = TYPE_VOID };
    Type *void_ptr              = new_type(TYPE_POINTER, __func__, __FILE__, __LINE__);
    void_ptr->u.pointer.target  = clone_type(&void_type, __func__, __FILE__, __LINE__);
    Expr *a = coerce_for_assignment(typecheck_and_decay(args), void_ptr, "passing the argument");
    free_type(void_ptr);
    return a;
}

const Type *typecheck_coro_ptr_call(Expr *call, const Type *desc)
{
    if (!coroutine_call_allowed(call))
        fatal_error("a coro_ptr can only be called by 'await'");
    call->u.call.args = coro_ptr_args(call->u.call.args, "await");
    return desc->u.struct_t.frame_result;
}

// The coroutine a co_* operation names, or the coro_ptr it is given: an expression
// typechecked in place, whose descriptor type is returned (NULL for a name).
static const Type *named_or_pointer(Expr **e, const char *what)
{
    if ((*e)->kind == EXPR_VAR) { // a name: a coroutine's, unless a coro_ptr object's
        const Symbol *sym = symtab_get_opt((*e)->u.var);
        if (!sym || sym->kind == SYM_FUNC || !coro_desc_target(sym->type)) {
            named_coroutine(*e, what);
            return NULL;
        }
    }
    *e               = typecheck_and_decay(*e);
    const Type *desc = coro_desc_target((*e)->type);
    if (!desc)
        fatal_error("'%s' needs the name of a coroutine or a coro_ptr", what);
    return desc;
}

static void check_suspension(const char *what)
{
    require_target();
    if (!in_coro)
        fatal_error("'%s' outside a coroutine", what);
    if (coro_defer_depth > 0)
        fatal_error("'%s' inside a deferred statement", what);
}

// The yield types of an await and its awaiter must be the same: the suspensions are
// forwarded unchanged.
static void check_same_yield(const Type *y)
{
    if (unalias(y)->kind != unalias(coro_yield)->kind || !compatible_type(y, coro_yield))
        fatal_error("'await' of a coroutine with another yield type");
}

Expr *typecheck_yield(Expr *e)
{
    check_suspension("yield");
    bool is_void = unalias(coro_yield)->kind == TYPE_VOID;
    if (e->u.yield_expr) {
        if (is_void)
            fatal_error("'yield' with a value in a coroutine that yields void");
        e->u.yield_expr =
            coerce_for_assignment(typecheck_and_decay(e->u.yield_expr), coro_yield, "yielding");
    } else if (!is_void) {
        fatal_error("'yield' without a value in a coroutine that yields a value");
    }
    free_type(e->type);
    e->type = new_type(TYPE_INT, __func__, __FILE__, __LINE__); // co_signal
    return e;
}

Expr *typecheck_await(Expr *e)
{
    check_suspension("await");
    Expr *op = e->u.await_expr;
    const Type *result;
    if (op->kind == EXPR_CALL && op->u.call.func->kind == EXPR_VAR) {
        const Symbol *sym = symtab_get_opt(op->u.call.func->u.var);
        if (sym && sym->kind == SYM_FUNC && sym->u.func.coro) {
            // The arena form: a call of the coroutine, checked as any call.
            check_same_yield(sym->u.func.yield_type);
            const Expr *outer = arena_call;
            arena_call        = op;
            op                = typecheck_and_decay(op);
            arena_call        = outer;
            e->u.await_expr   = op;
            free_type(e->type);
            e->type = clone_type(op->type, __func__, __FILE__, __LINE__);
            return e;
        }
    }
    // The explicit form: a frame the program made; or the arena form of a coro_ptr's
    // coroutine, a call of the coro_ptr.
    const Expr *outer = arena_call;
    if (op->kind == EXPR_CALL)
        arena_call = op;
    op              = typecheck_and_decay(op);
    arena_call      = outer;
    e->u.await_expr = op;
    if (op->kind == EXPR_CALL) {
        const Type *desc = coro_desc_target(op->u.call.func->type);
        if (desc) {
            check_same_yield(desc->u.struct_t.frame_yield);
            free_type(e->type);
            e->type = clone_type(op->type, __func__, __FILE__, __LINE__);
            return e;
        }
    }
    const Type *frame = frame_target(op->type);
    if (!frame)
        fatal_error("'await' needs a call of a coroutine or a co_frame pointer");
    lint_settled(op); // an await runs it to its end
    check_same_yield(frame->u.struct_t.frame_yield);
    result = frame->u.struct_t.frame_result;
    free_type(e->type);
    e->type = clone_type(result, __func__, __FILE__, __LINE__);
    return e;
}

// A size in bytes, converted to size_t.
static Expr *size_argument(Expr *e, const char *what)
{
    e = typecheck_and_decay(e);
    if (!is_integer(e->type))
        fatal_error("%s must be an integer", what);
    return convert_to_kind(e, size_kind());
}

// co_init(storage, bytes, f, args...) and co_alloca(f, extra, args...): the arguments
// before f's own, then f's checked against its parameters, or a coro_ptr's one void *.
// Sets f's yield and result types.
static void typecheck_start(Expr *e, const Type **yield, const Type **result)
{
    CoOp op    = e->u.co_op.op;
    Expr *args = e->u.co_op.args;
    Expr *name;
    Expr *rest;
    if (op == CO_OP_INIT) {
        Expr *storage = args, *bytes = args->next;
        name          = bytes->next;
        rest          = name->next;
        storage->next = bytes->next = name->next = NULL;

        static const Type void_type = { .kind = TYPE_VOID };
        Type *void_ptr              = new_type(TYPE_POINTER, __func__, __FILE__, __LINE__);
        void_ptr->u.pointer.target  = clone_type(&void_type, __func__, __FILE__, __LINE__);
        storage =
            coerce_for_assignment(typecheck_and_decay(storage), void_ptr, "passing the storage");
        free_type(void_ptr);
        bytes         = size_argument(bytes, "co_init: the size");
        storage->next = bytes;
        bytes->next   = name;
        args          = storage;
    } else {
        if (coro_loop_head_depth > 0)
            fatal_error("'co_alloca' in the head of a loop");
        name       = args;
        Expr *size = args->next;
        rest       = size->next;
        name->next = size->next = NULL;
        name->next              = size_argument(size, "co_alloca: the extra size");
    }
    Expr *next       = name->next;
    name->next       = NULL;
    Expr *orig       = name;
    const Type *desc = named_or_pointer(&name, op_name(op));
    name->next       = next;
    if (args == orig)
        args = name;
    else
        args->next->next = name; // co_init: storage, bytes, then the coroutine
    Expr *tail = name;
    while (tail->next)
        tail = tail->next;
    if (op == CO_OP_INIT)
        lint_init(e, args, desc ? "(coro_ptr)" : name->u.var);
    if (desc) {
        tail->next = coro_ptr_args(rest, op_name(op));
        *yield     = desc->u.struct_t.frame_yield;
        *result    = desc->u.struct_t.frame_result;
    } else {
        const Symbol *sym = symtab_get(name->u.var);
        tail->next        = typecheck_call_args(unalias(sym->type), rest, name->u.var);
        *yield            = sym->u.func.yield_type;
        *result           = result_of(sym);
    }
    e->u.co_op.args = args;
}

Expr *typecheck_co_op(Expr *e)
{
    require_target();
    CoOp op = e->u.co_op.op;
    Type *type;
    switch (op) {
    case CO_OP_INIT:
    case CO_OP_ALLOCA: {
        const Type *y, *t;
        typecheck_start(e, &y, &t);
        type = new_coro_pointer("__co_frame", y, t);
        break;
    }
    case CO_OP_SIZEOF:
    case CO_OP_ALIGNOF:
        named_or_pointer(&e->u.co_op.args, op_name(op));
        type = new_type(size_kind(), __func__, __FILE__, __LINE__);
        break;
    default: {
        Expr *p           = typecheck_and_decay(e->u.co_op.args);
        e->u.co_op.args   = p;
        const Type *frame = frame_target(p->type);
        if (!frame)
            fatal_error("'%s' needs a co_frame pointer", op_name(op));
        if (op == CO_OP_DESTROY || op == CO_OP_DONE || op == CO_OP_RESULT)
            lint_settled(p);
        if (op == CO_OP_VALUE) {
            if (unalias(frame->u.struct_t.frame_yield)->kind == TYPE_VOID)
                fatal_error("'co_value' of a coroutine that yields void");
            type = clone_type(frame->u.struct_t.frame_yield, __func__, __FILE__, __LINE__);
        } else if (op == CO_OP_RESULT) {
            if (unalias(frame->u.struct_t.frame_result)->kind == TYPE_VOID)
                fatal_error("'co_result' of a coroutine that returns void");
            type = clone_type(frame->u.struct_t.frame_result, __func__, __FILE__, __LINE__);
        } else {
            type = new_type(TYPE_INT, __func__, __FILE__, __LINE__); // co_status, or a truth
        }
        break;
    }
    }
    free_type(e->type);
    e->type = type;
    return e;
}

//
// The lint for frames in automatic storage.  The language cannot catch
// storage given to co_init going out of scope while its frame is suspended
// (docs/Coroutines_in_C.md, section 7), so these warnings flag the two plain cases:
// a frame whose storage is an automatic object stored where it outlives the object, or
// returned; and such a frame resumed by a statement that drops the status, when nothing
// in the block destroys it, asks co_done, reads its result or awaits it.  A warning
// does not stop the compilation.
//
typedef struct {
    char *storage; // the automatic object given to co_init
    int level;     // its block's
    char *frame;   // the variable holding the frame, or NULL
    char *coro;    // the coroutine, for the message
    bool resumed;  // by a statement that drops the status
    bool settled;  // destroyed, asked co_done, its result read or awaited
} AutoFrame;

static AutoFrame *auto_frames;
static int nauto, cap_auto;
static const Expr *last_auto_init; // the co_init of auto_frames[nauto - 1], just checked
static char *lint_fn;

static void lint_warning(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    diag_print_prefix(stderr, diag_loc, "warning");
    fprintf(stderr, "%s: ", lint_fn ? lint_fn : "?");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

void coro_lint_function(const char *name)
{
    xfree(lint_fn);
    lint_fn = name ? xstrdup(name) : NULL;
}

void coro_recover(void)
{
    for (int i = 0; i < nauto; i++) {
        xfree(auto_frames[i].storage);
        xfree(auto_frames[i].frame);
        xfree(auto_frames[i].coro);
    }
    xfree(auto_frames);
    auto_frames          = NULL;
    nauto                = 0;
    cap_auto             = 0;
    last_auto_init       = NULL;
    arena_call           = NULL;
    coro_defer_depth     = 0;
    coro_loop_head_depth = 0;
    coro_end_body();
    coro_lint_function(NULL);
}

static const Expr *strip_casts(const Expr *e)
{
    while (e && e->kind == EXPR_CAST)
        e = e->u.cast.expr;
    return e;
}

// The automatic object `e` is the address of, or NULL: a local array, decayed, or &x of
// a local.  (A local pointer's value is memory from somewhere else.)
static const char *auto_object(const Expr *e)
{
    e         = strip_casts(e);
    bool addr = e && e->kind == EXPR_UNARY_OP && e->u.unary_op.op == UNARY_ADDRESS;
    if (addr)
        e = e->u.unary_op.expr;
    if (!e || e->kind != EXPR_VAR)
        return NULL;
    const Symbol *sym = symtab_get_opt(e->u.var);
    if (!sym || sym->kind != SYM_LOCAL || sym->has_linkage || symtab_level(e->u.var) < 0)
        return NULL;
    return addr || unalias(sym->type)->kind == TYPE_ARRAY ? e->u.var : NULL;
}

// A co_init on `storage` of coroutine `coro`, `e` the operation.
static void lint_init(const Expr *e, const Expr *storage, const char *coro)
{
    const char *obj = auto_object(storage);
    last_auto_init  = NULL;
    if (!obj)
        return;
    if (nauto == cap_auto) {
        cap_auto     = cap_auto ? 2 * cap_auto : 8;
        AutoFrame *n = xalloc(cap_auto * sizeof *n, __func__, __FILE__, __LINE__);
        for (int i = 0; i < nauto; i++)
            n[i] = auto_frames[i];
        xfree(auto_frames);
        auto_frames = n;
    }
    auto_frames[nauto++] = (AutoFrame){ xstrdup(obj), symtab_level(obj), NULL, xstrdup(coro) };
    last_auto_init       = e;
}

// The frame `e` names: the co_init just checked, or a variable holding a frame.
static AutoFrame *frame_of(const Expr *e)
{
    e = strip_casts(e);
    if (!e)
        return NULL;
    if (e == last_auto_init)
        return &auto_frames[nauto - 1];
    if (e->kind != EXPR_VAR)
        return NULL;
    for (int i = nauto - 1; i >= 0; i--)
        if (auto_frames[i].frame && strcmp(auto_frames[i].frame, e->u.var) == 0)
            return &auto_frames[i];
    return NULL;
}

void coro_lint_bind(const char *var, int level, const Expr *value)
{
    AutoFrame *f = frame_of(value);
    if (!f)
        return;
    if (!var || level < f->level) {
        lint_warning(
            "the frame of '%s' outlives its storage '%s', an automatic object: "
            "use static or allocated storage",
            f->coro, f->storage);
        return;
    }
    xfree(f->frame);
    f->frame = xstrdup(var);
}

static void lint_settled(const Expr *p)
{
    AutoFrame *f = frame_of(p);
    if (f)
        f->settled = true;
}

void coro_lint_statement(const Expr *e)
{
    e = strip_casts(e);
    if (e && e->kind == EXPR_CO_OP &&
        (e->u.co_op.op == CO_OP_RESUME || e->u.co_op.op == CO_OP_CANCEL)) {
        AutoFrame *f = frame_of(e->u.co_op.args);
        if (f)
            f->resumed = true;
    }
}

void coro_lint_scope_exit(int level)
{
    while (nauto > 0 && auto_frames[nauto - 1].level >= level) {
        AutoFrame *f = &auto_frames[--nauto];
        if (f->resumed && !f->settled)
            lint_warning(
                "the frame of '%s' in '%s' may be left suspended at the end of the "
                "block, its defers never run: co_destroy it, or use co_alloca",
                f->coro, f->storage);
        xfree(f->storage);
        xfree(f->frame);
        xfree(f->coro);
    }
    if (nauto == 0) {
        xfree(auto_frames);
        auto_frames = NULL;
        cap_auto    = 0;
    }
    last_auto_init = NULL;
}
