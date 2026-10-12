//
// Statement lowering: AST Stmt → TAC instructions.
//

#include <stdlib.h>
#include <string.h>

#include "c_escape.h"
#include "target.h"
#include "translate.h"
#include "typecheck.h"
#include "xalloc.h"

static void collect_cases(TacCtx *ctx, Stmt *stmt, CaseList *list)
{
    if (!stmt)
        return;
    switch (stmt->kind) {
    case STMT_CASE: {
        xfree(stmt->branch_target_label); // a copy of a deferred switch is labeled anew
        stmt->branch_target_label = new_temp(ctx);
        CaseEntry *e              = xalloc(sizeof *e, __func__, __FILE__, __LINE__);
        e->expr                   = stmt->u.case_stmt.expr;
        e->label                  = stmt->branch_target_label;
        e->next                   = NULL;
        *list->tail               = e;
        list->tail                = &e->next;
        collect_cases(ctx, stmt->u.case_stmt.stmt, list);
        break;
    }
    case STMT_DEFAULT:
        xfree(stmt->branch_target_label);
        stmt->branch_target_label = new_temp(ctx);
        list->default_label       = stmt->branch_target_label;
        collect_cases(ctx, stmt->u.default_stmt, list);
        break;
    case STMT_COMPOUND:
        for (DeclOrStmt *ds = stmt->u.compound; ds; ds = ds->next)
            if (ds->kind == DECL_OR_STMT_STMT)
                collect_cases(ctx, ds->u.stmt, list);
        break;
    case STMT_IF:
        collect_cases(ctx, stmt->u.if_stmt.then_stmt, list);
        collect_cases(ctx, stmt->u.if_stmt.else_stmt, list);
        break;
    case STMT_WHILE:
        collect_cases(ctx, stmt->u.while_stmt.body, list);
        break;
    case STMT_DO_WHILE:
        collect_cases(ctx, stmt->u.do_while.body, list);
        break;
    case STMT_FOR:
        collect_cases(ctx, stmt->u.for_stmt.body, list);
        break;
    case STMT_LABELED:
        collect_cases(ctx, stmt->u.labeled.stmt, list);
        break;
    default: // STMT_SWITCH and leaf statements: do not recurse
        break;
    }
}

// Lower `char arr[N] = "…"` to a run of byte stores into the frame slot: one
// COPY_BYTE_TO_OFFSET per source byte at successive byte offsets, then zero-fill up to
// the array's size (C string-init semantics — the terminating null and any trailing
// zeros).  Used for a 1-D char array and for each inner string row of a multi-dimensional
// char array (whose contiguous rows the caller has already offset).  Char data keeps its
// source (ASCII) encoding, like every scalar char value; only the static-data path repacks
// to KOI-7 (which differs solely for lowercase Latin — see backend/besm6/KOI7_Encoding.md).
// With skip_zero the object was bulk-zeroed, so zero bytes are not stored.
static void gen_char_array_string_init(TacCtx *ctx, const char *var_name, int base_offset,
                                       const Expr *str_expr, int array_bytes, bool skip_zero)
{
    size_t len;
    char *decoded = c_decode_string_literal(str_expr->u.literal->u.string_val, &len);
    for (int i = 0; i < array_bytes; i++) {
        int byte = (size_t)i < len ? (unsigned char)decoded[i] : 0;
        if (skip_zero && byte == 0)
            continue;
        Tac_Instruction *in         = tac_new_instruction(TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET);
        in->u.copy_to_offset.src    = val_int(byte);
        in->u.copy_to_offset.dst    = xstrdup(var_name);
        in->u.copy_to_offset.offset = base_offset + i;
        tac_append(ctx, in);
    }
    xfree(decoded);
}

// True for a string literal initializing a char array (`char a[N] = "…"`), whether a
// top-level 1-D array or an inner row of a multi-dimensional char array.
static bool is_char_array_string_init(const Type *type, const Initializer *init)
{
    return init->kind == INITIALIZER_SINGLE && type && unalias(type)->kind == TYPE_ARRAY &&
           init->u.expr->kind == EXPR_LITERAL && init->u.expr->u.literal->kind == LITERAL_STRING;
}

// Objects with at least this many zero stores (word or byte) are zeroed by a loop first.
#define ZERO_FILL_STORES 8

// Zero bytes a string stores into a char array of array_bytes.
static int string_zero_bytes(const Expr *str_expr, int array_bytes)
{
    size_t len;
    char *decoded = c_decode_string_literal(str_expr->u.literal->u.string_val, &len);
    int n         = 0;
    for (int i = 0; i < array_bytes; i++)
        n += (size_t)i >= len || decoded[i] == 0;
    xfree(decoded);
    return n;
}

// A leaf storing an integer, char or pointer zero, made redundant by bulk zeroing.  A
// floating zero is kept: an all-zero word is not +0.0 on every target.
static bool is_zero_leaf(const Initializer *init)
{
    return init->kind == INITIALIZER_SINGLE && init->type &&
           (is_integer(init->type) || is_pointer(init->type)) &&
           init->u.expr->kind == EXPR_LITERAL && is_zero_int(init->u.expr->u.literal);
}

// Zero stores an initializer makes: zero leaves, and the zero bytes of strings.
static int zero_stores(const Initializer *init)
{
    if (init->kind == INITIALIZER_SINGLE) {
        if (is_char_array_string_init(init->type, init))
            return string_zero_bytes(init->u.expr, (int)get_size(init->type));
        return is_zero_leaf(init);
    }
    int n = 0;
    for (const InitItem *item = init->u.items; item; item = item->next)
        n += zero_stores(item->init);
    return n;
}

// Zero the first `bytes` bytes of var_name: a word-store loop, then byte stores for a tail.
// A word-addressed target's slot is whole words, so there the loop covers the tail too.
static void gen_zero_fill(TacCtx *ctx, const char *var_name, int bytes)
{
    int w = target_word_bytes();
    if (target_word_addressed())
        bytes = (bytes + w - 1) / w * w;
    int nwords = bytes / w;

    char *ptr             = new_typed_temp(ctx, tac_type_ptr(tac_type_word()));
    Tac_Instruction *ga   = tac_new_instruction(TAC_INSTRUCTION_GET_ADDRESS);
    ga->u.get_address.src = val_var(var_name);
    ga->u.get_address.dst = val_var(ptr);
    tac_append(ctx, ga);

    char *count         = new_typed_temp(ctx, tac_new_type(TAC_TYPE_INT));
    Tac_Instruction *cp = tac_new_instruction(TAC_INSTRUCTION_COPY);
    cp->u.copy.src      = val_int(nwords);
    cp->u.copy.dst      = val_var(count);
    tac_append(ctx, cp);

    char *top = new_temp(ctx);
    emit_label(ctx, top);

    // A word-sized zero: on a byte-addressed target an int is narrower than a pointer.
    Tac_Instruction *st = tac_new_instruction(TAC_INSTRUCTION_STORE);
    st->u.store.src     = (int)target_config->int_size == w ? val_int(0) : val_long_long(0);
    st->u.store.dst_ptr = val_var(ptr);
    tac_append(ctx, st);

    Tac_Instruction *ap = tac_new_instruction(TAC_INSTRUCTION_ADD_PTR);
    ap->u.add_ptr.ptr   = val_var(ptr);
    ap->u.add_ptr.index = val_int(1);
    ap->u.add_ptr.scale = w;
    ap->u.add_ptr.dst   = val_var(ptr);
    tac_append(ctx, ap);

    Tac_Instruction *sub = tac_new_instruction(TAC_INSTRUCTION_BINARY);
    sub->u.binary.op     = TAC_BINARY_SUBTRACT;
    sub->u.binary.src1   = val_var(count);
    sub->u.binary.src2   = val_int(1);
    sub->u.binary.dst    = val_var(count);
    tac_append(ctx, sub);

    Tac_Instruction *jnz              = tac_new_instruction(TAC_INSTRUCTION_JUMP_IF_NOT_ZERO);
    jnz->u.jump_if_not_zero.condition = val_var(count);
    jnz->u.jump_if_not_zero.target    = top;
    tac_append(ctx, jnz);

    for (int i = nwords * w; i < bytes; i++) {
        Tac_Instruction *in         = tac_new_instruction(TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET);
        in->u.copy_to_offset.src    = val_int(0);
        in->u.copy_to_offset.dst    = xstrdup(var_name);
        in->u.copy_to_offset.offset = i;
        tac_append(ctx, in);
    }
    xfree(ptr);
    xfree(count);
}

static void gen_init(TacCtx *ctx, const char *var_name, int base_offset, const Initializer *init,
                     bool skip_zero);

// Initialize the struct or union member of `item`, in the aggregate at `base_offset` of
// var_name.  A bit-field merges its bits into its storage unit; with no initializer it is
// still stored, as zero, unless the aggregate was zero-filled.
static void gen_member_init(TacCtx *ctx, const char *var_name, int base_offset,
                            const InitItem *item, bool skip_zero)
{
    if (!item->bf.width) {
        gen_init(ctx, var_name, base_offset + item->offset, item->init, skip_zero);
        return;
    }
    if (skip_zero && is_zero_leaf(item->init))
        return;
    gen_bitfield_init(ctx, var_name, base_offset + item->offset, &item->bf, item->init->type,
                      gen_expr(ctx, item->init->u.expr));
}

static void gen_init(TacCtx *ctx, const char *var_name, int base_offset, const Initializer *init,
                     bool skip_zero)
{
    if (init->kind == INITIALIZER_SINGLE) {
        if (skip_zero && is_zero_leaf(init))
            return;
        // A string literal initializing a char array (e.g. an inner row of a multi-dim
        // char array): store its bytes individually rather than COPY a whole-word pointer.
        if (is_char_array_string_init(init->type, init)) {
            gen_char_array_string_init(ctx, var_name, base_offset, init->u.expr,
                                       (int)get_size(init->type), skip_zero);
            return;
        }
        Tac_Val *src    = gen_expr(ctx, init->u.expr);
        const Type *it0 = init->type ? unalias(init->type) : NULL;
        if (it0 && (it0->kind == TYPE_STRUCT || it0->kind == TYPE_UNION)) {
            // A whole struct/union value (not a scalar leaf) — copy it word by word.
            gen_struct_assign(ctx, var_name, base_offset, src->u.var_name, init->type);
            tac_free_val(src);
            return;
        }
        // A char/signed char/unsigned char leaf occupies a single packed byte;
        // its element offset is a byte offset, so it must use the byte-store kind
        // (the word-store kind rejects a sub-word offset).
        bool byte_leaf      = init->type && get_size(init->type) == 1;
        Tac_Instruction *in = tac_new_instruction(byte_leaf ? TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET
                                                            : TAC_INSTRUCTION_COPY_TO_OFFSET);
        in->u.copy_to_offset.src    = src;
        in->u.copy_to_offset.dst    = xstrdup(var_name);
        in->u.copy_to_offset.offset = base_offset;
        tac_append(ctx, in);
        return;
    }
    const Type *t = unalias(init->type);
    if (t->kind == TYPE_ARRAY) {
        int elem_size = (int)get_size(t->u.array.element);
        int i         = 0;
        for (const InitItem *item = init->u.items; item; item = item->next, i++)
            gen_init(ctx, var_name, base_offset + i * elem_size, item->init, skip_zero);
    } else if (t->kind == TYPE_STRUCT) {
        // Member byte offsets were resolved and cached on each InitItem by typecheck
        // (typecheck_init), while the struct tag was still live in structtab.  Consume
        // them here instead of re-querying structtab, which a block-local tag has left.
        for (const InitItem *item = init->u.items; item; item = item->next)
            gen_member_init(ctx, var_name, base_offset, item, skip_zero);
    } else if (t->kind == TYPE_UNION) {
        // typecheck_init reduced the union initializer to the chosen member, at offset
        // 0, then the zeros for the rest of the union's storage at their offset.  No
        // structtab lookup is needed, so this works for block-scope unions too.
        for (const InitItem *item = init->u.items; item; item = item->next)
            gen_member_init(ctx, var_name, base_offset, item, skip_zero);
    } else {
        internal_error("compound initializer for unsupported type %d in TAC lowering",
                       (int)t->kind);
    }
}

void gen_compound_init(TacCtx *ctx, const char *var_name, int base_offset, const Initializer *init)
{
    gen_init(ctx, var_name, base_offset, init, false);
}

// Initialize a whole aggregate object of `bytes` bytes.  With many zero leaves, zero it
// by a loop first and store only the rest.
void gen_aggregate_init(TacCtx *ctx, const char *var_name, const Initializer *init, int bytes)
{
    bool bulk = zero_stores(init) >= ZERO_FILL_STORES;
    if (bulk)
        gen_zero_fill(ctx, var_name, bytes);
    gen_init(ctx, var_name, 0, init, bulk);
}

// Initialize a char array of `bytes` bytes from a string, bulk-zeroing a long zero tail.
void gen_string_array_init(TacCtx *ctx, const char *var_name, const Expr *str_expr, int bytes)
{
    bool bulk = string_zero_bytes(str_expr, bytes) >= ZERO_FILL_STORES;
    if (bulk)
        gen_zero_fill(ctx, var_name, bytes);
    gen_char_array_string_init(ctx, var_name, 0, str_expr, bytes, bulk);
}

static void gen_local_decl(TacCtx *ctx, const Declaration *decl)
{
    if (decl->kind != DECL_VAR)
        return;
    // Variables with automatic storage are private to this function; record
    // their names so the optimizer does not mistake them for observable globals.
    // static/extern/typedef/thread-local declarators are not automatics: a
    // static or extern name denotes observable storage and must be left out.
    StorageClass storage =
        decl->u.var.specifiers ? decl->u.var.specifiers->storage : STORAGE_CLASS_NONE;
    bool is_automatic = storage == STORAGE_CLASS_NONE || storage == STORAGE_CLASS_AUTO ||
                        storage == STORAGE_CLASS_REGISTER;
    for (const InitDeclarator *id = decl->u.var.declarators; id; id = id->next) {
        if (!id->name)
            continue;
        const Type *idt = id->type ? unalias(id->type) : NULL;
        // A block-scope function declaration (int f(void);) is not an automatic:
        // it names an external-linkage function with no frame slot, so it must
        // not be percent-prefixed like a local.
        if (idt && idt->kind == TYPE_FUNCTION) {
            tac_record_extern(ctx, id->name, id->type);
            continue;
        }
        if (storage == STORAGE_CLASS_EXTERN)
            tac_record_extern(ctx, id->name, id->type);
        if (!is_automatic) {
            // A static/extern local array has no frame slot — its storage is the
            // module-local static datum, addressed by its label like a global — but it
            // still decays to its address when used as a value, so record it for the decay.
            if (idt && idt->kind == TYPE_ARRAY)
                tac_record_array_local(ctx, id->name);
            continue;
        }
        tac_record_local(ctx, id->name, id->type);
        // Aggregate locals (arrays, structs, unions) occupy contiguous frame slots;
        // emit an AllocateLocal so the backend reserves the full size instead of a
        // single slot. Scalars keep their implicit one-slot allocation. Size and
        // alignment are in target bytes (like every other offset in the TAC stream);
        // each backend converts to its own allocation unit (the besm6 backend divides
        // by the 6-byte machine word).
        // A local array decays to a pointer when used as a value; record it so gen_expr
        // can emit the array-decay GET_ADDRESS (symtab locals are gone by then).
        if (idt && idt->kind == TYPE_ARRAY)
            tac_record_array_local(ctx, id->name);
        // An over-aligned (_Alignas) scalar is allocated the same way, to carry its
        // alignment.
        int align   = (int)get_alignment(id->type);
        int alignas = alignas_bytes(decl->u.var.specifiers);
        if ((idt &&
             (idt->kind == TYPE_ARRAY || idt->kind == TYPE_STRUCT || idt->kind == TYPE_UNION)) ||
            alignas > align) {
            int bytes = (int)get_size(id->type);
            if (bytes > 0) {
                Tac_Instruction *in       = tac_new_instruction(TAC_INSTRUCTION_ALLOCATE_LOCAL);
                in->u.allocate_local.name = xstrdup(id->name);
                in->u.allocate_local.size = bytes;
                in->u.allocate_local.alignment = alignas > align ? alignas : align;
                tac_append(ctx, in);
            }
        }
    }
    for (InitDeclarator *id = decl->u.var.declarators; id; id = id->next) {
        if (id->init && id->init->kind == INITIALIZER_SINGLE) {
            // `char a[N] = "…"`: copy the string's bytes into the frame slot (a real
            // per-byte copy, not an alias of the string-constant pointer).
            if (is_char_array_string_init(id->type, id->init)) {
                gen_string_array_init(ctx, id->name, id->init->u.expr, (int)get_size(id->type));
                continue;
            }
            const Type *idt2 = id->type ? unalias(id->type) : NULL;
            if (idt2 && (idt2->kind == TYPE_STRUCT || idt2->kind == TYPE_UNION)) {
                // Whole-aggregate initialization (e.g. struct r = other; struct r = f();
                // union u = *ptr;): copy every word from the source into the new local.
                // The source may be a named aggregate, a call/compound rvalue, or — the
                // case gen_struct_assign mishandled — an lvalue reached through a pointer.
                gen_aggregate_init_from_expr(ctx, id->name, 0, id->init->u.expr, id->type);
            } else {
                Tac_Val *src        = gen_expr(ctx, id->init->u.expr);
                Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_COPY);
                in->is_volatile     = type_is_volatile(id->type); // never dropped as dead
                in->u.copy.src      = src;
                in->u.copy.dst      = val_var(id->name);
                tac_append(ctx, in);
            }
        } else if (id->init && id->init->kind == INITIALIZER_COMPOUND) {
            gen_aggregate_init(ctx, id->name, id->init, (int)get_size(id->type));
        }
    }
}

// The test of a loop: jump to `target` when `cond` is true (`if_true`) or false. A
// rotated loop lowers its condition twice, once for each copy of the test.
static void emit_loop_test(TacCtx *ctx, Expr *cond, bool if_true, const char *target)
{
    gen_cond_jump(ctx, cond, if_true, target);
}

// The entry of a rotated loop: a copy of the test as a guard, or, for a condition that
// is not simple (`&&`, `||`, a call, a side effect), a jump to the test at the bottom,
// labelled `test`, smaller than the copy.  BESM-6 keeps the guard.
static bool jump_to_test(const Expr *cond)
{
    return cond_jumps() && !is_simple_cond(cond);
}

// Whether `s` does nothing: `;` or `{}`.  A loop of such a body jumping to its test
// would jump to the next instruction, so it falls in instead.
static bool is_empty_stmt(const Stmt *s)
{
    return !s || (s->kind == STMT_EXPR && !s->u.expr) ||
           (s->kind == STMT_COMPOUND && !s->u.compound);
}

//
// defer (docs/Coroutines_in_C.md, section 1).  Each block being lowered is a TacScope
// holding the exit actions registered in it so far; every edge that leaves blocks
// lowers their actions on the spot, innermost block first and each block's last
// action first.  A deferred statement is thus lowered once per exit, as fresh code:
// its loops and labels are renamed each time, and its own blocks and loops are a
// stack of their own, since nothing leaves it (semantic/defer.c checks that).
//
// An exit whose cleanup is large (a coroutine's destroy paths, one per suspension
// point, from a smaller size) shares it instead: it sets the "where next" variable and jumps
// into the chain of the innermost block it leaves that has actions, the actions lowered
// once more at that block's end.  The chain runs them from the exit's point on, then
// tests the variable for the exits that end there and otherwise goes on into the chain
// of the next block out.  This works because an exit from inside a block always goes
// on into the same place outside it: the actions of an enclosing block cannot change
// while the inner block is open.  The fall-through keeps its own copy, so the common
// path pays nothing.  BESM-6, whose code must not change, keeps the copies.
//

static void scope_push(TacCtx *ctx, const Stmt *key)
{
    if (ctx->nscopes == ctx->scopes_cap) {
        ctx->scopes_cap = ctx->scopes_cap ? 2 * ctx->scopes_cap : 8;
        TacScope *s     = xalloc(ctx->scopes_cap * sizeof *s, __func__, __FILE__, __LINE__);
        for (int i = 0; i < ctx->nscopes; i++)
            s[i] = ctx->scopes[i];
        xfree(ctx->scopes);
        ctx->scopes = s;
    }
    ctx->scopes[ctx->nscopes++] = (TacScope){ .key = key, .entry = ctx->tail };
}

static void scope_pop(TacCtx *ctx)
{
    TacScope *sc = &ctx->scopes[--ctx->nscopes];
    for (int i = 0; i < sc->count; i++) {
        xfree(sc->actions[i].frame);
        xfree(sc->actions[i].sp);
    }
    xfree(sc->actions);
    for (int i = 0; i < sc->nentries; i++)
        xfree(sc->entries[i]);
    xfree(sc->entries);
    for (int i = 0; i < sc->ndests; i++)
        xfree(sc->dests[i].label);
    xfree(sc->dests);
}

void tac_scope_entry(TacCtx *ctx, Tac_Instruction *in)
{
    Tac_Instruction *after = ctx->scopes[ctx->nscopes - 1].entry;
    if (!after) {
        in->next  = ctx->head;
        ctx->head = in;
        if (!ctx->tail)
            ctx->tail = in;
        return;
    }
    in->next    = after->next;
    after->next = in;
    if (ctx->tail == after)
        ctx->tail = in;
}

void tac_scope_add(TacCtx *ctx, ExitAction action)
{
    TacScope *sc = &ctx->scopes[ctx->nscopes - 1];
    if (sc->count == sc->cap) {
        sc->cap       = sc->cap ? 2 * sc->cap : 4;
        ExitAction *a = xalloc(sc->cap * sizeof *a, __func__, __FILE__, __LINE__);
        for (int i = 0; i < sc->count; i++)
            a[i] = sc->actions[i];
        xfree(sc->actions);
        sc->actions = a;
    }
    sc->actions[sc->count++] = action;
}

static void breaks_push(TacCtx *ctx, const char *break_label, const char *cont_label)
{
    if (ctx->nbreaks == ctx->breaks_cap) {
        ctx->breaks_cap = ctx->breaks_cap ? 2 * ctx->breaks_cap : 8;
        TacBreak *b     = xalloc(ctx->breaks_cap * sizeof *b, __func__, __FILE__, __LINE__);
        for (int i = 0; i < ctx->nbreaks; i++)
            b[i] = ctx->breaks[i];
        xfree(ctx->breaks);
        ctx->breaks = b;
    }
    ctx->breaks[ctx->nbreaks++] = (TacBreak){ break_label, cont_label, ctx->nscopes };
}

static bool have_exits(const TacCtx *ctx)
{
    for (int i = 0; i < ctx->nscopes; i++)
        if (ctx->scopes[i].count > 0)
            return true;
    return false;
}

static void gen_sub(TacCtx *ctx, Stmt *stmt);

// Lower a deferred statement afresh, with blocks, loops and labels of its own.
static void gen_deferred(TacCtx *ctx, Stmt *body)
{
    TacScope *scopes = ctx->scopes;
    int nscopes      = ctx->nscopes;
    int scopes_cap   = ctx->scopes_cap;
    TacBreak *breaks = ctx->breaks;
    int nbreaks      = ctx->nbreaks;
    int breaks_cap   = ctx->breaks_cap;
    StringMap labels = ctx->user_labels;
    char *where      = ctx->where;
    ctx->scopes      = NULL;
    ctx->nscopes = ctx->scopes_cap = 0;
    ctx->breaks                    = NULL;
    ctx->nbreaks = ctx->breaks_cap = 0;
    ctx->where                     = NULL; // its own, should an exit inside it share a cleanup
    map_init(&ctx->user_labels);

    label_loops_stmt(body, &ctx->temp_id);
    ctx->defer_depth++;
    gen_sub(ctx, body);
    ctx->defer_depth--;

    xfree(ctx->scopes);
    xfree(ctx->breaks);
    free_user_labels(&ctx->user_labels);
    ctx->scopes      = scopes;
    ctx->nscopes     = nscopes;
    ctx->scopes_cap  = scopes_cap;
    ctx->breaks      = breaks;
    ctx->nbreaks     = nbreaks;
    ctx->breaks_cap  = breaks_cap;
    ctx->user_labels = labels;
    xfree(ctx->where);
    ctx->where = where;
}

static void run_action(TacCtx *ctx, const ExitAction *a)
{
    switch (a->kind) {
    case EXIT_DEFER:
        gen_deferred(ctx, a->stmt);
        break;
    case EXIT_CO_RELEASE:
        gen_co_release(ctx, a);
        break;
    }
}

// The actions of block `i` from the `from`-th registered down to the first.
static void run_scope(TacCtx *ctx, int i, int from)
{
    for (int j = from - 1; j >= 0; j--) {
        // The array may move while a deferred statement is lowered: index it anew.
        ExitAction a = ctx->scopes[i].actions[j];
        run_action(ctx, &a);
    }
}

// Leaving every block from the innermost one out to block `depth`, that one included.
static void run_exits(TacCtx *ctx, int depth)
{
    for (int i = ctx->nscopes - 1; i >= depth; i--)
        run_scope(ctx, i, ctx->scopes[i].count);
}

void gen_exits_all(TacCtx *ctx)
{
    run_exits(ctx, 0);
}

// The cleanup an exit shares from this size on: a deferred statement weighs three per
// statement in it, the release of a co_alloca as much as four.
// A coroutine's destroy paths, one per suspension point, share from a smaller size.
enum { CHAIN_MIN = 12, CHAIN_MIN_DESTROY = 6, RELEASE_SIZE = 12 };

bool translate_shared_cleanup = true;

// May an exit share its cleanup here?  Never on BESM-6, whose code must not change.
static bool may_share(void)
{
    return translate_shared_cleanup && !target_config->no_loop_opt;
}

static int stmt_size(const Stmt *s)
{
    if (!s)
        return 0;
    switch (s->kind) {
    case STMT_COMPOUND: {
        int n = 0;
        for (const DeclOrStmt *ds = s->u.compound; ds; ds = ds->next)
            n += ds->kind == DECL_OR_STMT_STMT ? stmt_size(ds->u.stmt) : 3;
        return n;
    }
    case STMT_IF:
        return 3 + stmt_size(s->u.if_stmt.then_stmt) + stmt_size(s->u.if_stmt.else_stmt);
    case STMT_SWITCH:
        return 3 + stmt_size(s->u.switch_stmt.body);
    case STMT_WHILE:
        return 3 + stmt_size(s->u.while_stmt.body);
    case STMT_DO_WHILE:
        return 3 + stmt_size(s->u.do_while.body);
    case STMT_FOR:
        return 6 + stmt_size(s->u.for_stmt.body);
    case STMT_LABELED:
        return stmt_size(s->u.labeled.stmt);
    case STMT_CASE:
        return stmt_size(s->u.case_stmt.stmt);
    case STMT_DEFAULT:
        return stmt_size(s->u.default_stmt);
    case STMT_DEFER:
        return stmt_size(s->u.defer_stmt);
    default:
        return 3;
    }
}

// The size of the cleanup of an exit out to block `depth`.
static int exit_size(const TacCtx *ctx, int depth)
{
    int n = 0;
    for (int i = depth; i < ctx->nscopes; i++)
        for (int j = 0; j < ctx->scopes[i].count; j++)
            n += ctx->scopes[i].actions[j].kind == EXIT_CO_RELEASE
                     ? RELEASE_SIZE
                     : stmt_size(ctx->scopes[i].actions[j].stmt);
    return n;
}

// Where an exit goes once its cleanup has run.
static void emit_dest(TacCtx *ctx, const ExitDest *d)
{
    switch (d->kind) {
    case DEST_LABEL:
        emit_jump(ctx, d->label);
        break;
    case DEST_RETURN: {
        Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_RETURN);
        in->u.return_.src   = d->value ? val_var(ctx->ret_var) : NULL;
        tac_append(ctx, in);
        break;
    }
    case DEST_FINISH:
        gen_finish_code(ctx, d->state);
        break;
    }
}

// The label in front of action j in the chain of block m, made when first asked for.
static const char *entry_label(TacCtx *ctx, int m, int j)
{
    TacScope *sc = &ctx->scopes[m];
    sc->chained  = true;
    if (j >= sc->nentries) {
        char **e = xalloc((j + 1) * sizeof(char *), __func__, __FILE__, __LINE__);
        for (int i = 0; i < sc->nentries; i++)
            e[i] = sc->entries[i];
        xfree(sc->entries);
        sc->entries  = e;
        sc->nentries = j + 1;
    }
    if (!sc->entries[j])
        sc->entries[j] = new_temp(ctx);
    return sc->entries[j];
}

static bool same_dest(const ExitDest *a, const ExitDest *b)
{
    return a->kind == b->kind && a->value == b->value && a->state == b->state &&
           (a->kind != DEST_LABEL || strcmp(a->label, b->label) == 0);
}

// Leave the blocks from the innermost out to block `depth`, that one included, and go
// to `d` (its label owned): the cleanup lowered here, or shared through the chains when
// it is at least `min` large.  Shared, a return's value is in ret_var already.
static void leave(TacCtx *ctx, int depth, ExitDest d, int min)
{
    int top = -1, low = -1;
    for (int m = ctx->nscopes - 1; m >= depth; m--)
        if (ctx->scopes[m].count > 0) {
            if (top < 0)
                top = m;
            low = m;
        }
    bool share = top >= 0 && may_share() && exit_size(ctx, depth) >= min;
    if (!share) {
        run_exits(ctx, depth);
        emit_dest(ctx, &d);
        xfree(d.label);
        return;
    }
    TacScope *sc = &ctx->scopes[low];
    int k        = 0;
    while (k < sc->ndests && !same_dest(&sc->dests[k], &d))
        k++;
    if (k == sc->ndests) {
        if (sc->ndests == sc->dests_cap) {
            sc->dests_cap = sc->dests_cap ? 2 * sc->dests_cap : 4;
            ExitDest *n   = xalloc(sc->dests_cap * sizeof *n, __func__, __FILE__, __LINE__);
            for (int i = 0; i < sc->ndests; i++)
                n[i] = sc->dests[i];
            xfree(sc->dests);
            sc->dests = n;
        }
        d.id                    = ctx->ndest_ids++;
        sc->dests[sc->ndests++] = d;
    } else {
        xfree(d.label);
    }
    if (!ctx->where)
        ctx->where = new_typed_temp(ctx, tac_new_type(TAC_TYPE_INT));
    Tac_Instruction *set = tac_new_instruction(TAC_INSTRUCTION_COPY);
    set->u.copy.src      = val_int(sc->dests[k].id);
    set->u.copy.dst      = val_var(ctx->where);
    tac_append(ctx, set);
    for (int m = low + 1; m <= top; m++)
        if (ctx->scopes[m].count > 0)
            ctx->scopes[m].continues = true;
    emit_jump(ctx, entry_label(ctx, top, ctx->scopes[top].count - 1));
}

void gen_finish(TacCtx *ctx, unsigned state)
{
    leave(ctx, 0, (ExitDest){ .kind = DEST_FINISH, .state = state }, CHAIN_MIN_DESTROY);
}

// The chain of the innermost block, about to close (see above): after a jump, so the
// fall-through does not run it.
static void emit_chain(TacCtx *ctx)
{
    int i = ctx->nscopes - 1;
    if (!ctx->scopes[i].chained)
        return;
    char *after    = new_temp(ctx);
    bool reachable = !ctx->tail || (ctx->tail->kind != TAC_INSTRUCTION_RETURN &&
                                    ctx->tail->kind != TAC_INSTRUCTION_JUMP);
    if (reachable)
        emit_jump(ctx, after);
    for (int j = ctx->scopes[i].count - 1; j >= 0; j--) {
        if (j < ctx->scopes[i].nentries && ctx->scopes[i].entries[j])
            emit_label(ctx, ctx->scopes[i].entries[j]);
        // The array may move while a deferred statement is lowered: index it anew.
        ExitAction a = ctx->scopes[i].actions[j];
        run_action(ctx, &a);
    }
    const TacScope *sc = &ctx->scopes[i];
    int nd             = sc->ndests;
    bool cont          = sc->continues;
    char **handlers    = xalloc((nd + 1) * sizeof(char *), __func__, __FILE__, __LINE__);
    for (int k = 0; k < nd; k++) {
        const ExitDest *d = &ctx->scopes[i].dests[k];
        handlers[k]       = d->kind == DEST_LABEL ? NULL : new_temp(ctx);
        const char *to    = handlers[k] ? handlers[k] : d->label;
        if (k == nd - 1 && !cont) {
            emit_jump(ctx, to);
            break;
        }
        Tac_Val *c          = new_var_val(ctx, tac_new_type(TAC_TYPE_INT));
        Tac_Instruction *eq = tac_new_instruction(TAC_INSTRUCTION_BINARY);
        eq->u.binary.op     = TAC_BINARY_EQUAL;
        eq->u.binary.src1   = val_var(ctx->where);
        eq->u.binary.src2   = val_int(d->id);
        eq->u.binary.dst    = c;
        tac_append(ctx, eq);
        Tac_Instruction *j              = tac_new_instruction(TAC_INSTRUCTION_JUMP_IF_NOT_ZERO);
        j->u.jump_if_not_zero.condition = val_var(c->u.var_name);
        j->u.jump_if_not_zero.target    = xstrdup(to);
        tac_append(ctx, j);
    }
    if (cont) {
        int m = i - 1;
        while (ctx->scopes[m].count == 0)
            m--;
        emit_jump(ctx, entry_label(ctx, m, ctx->scopes[m].count - 1));
    }
    for (int k = 0; k < nd; k++) {
        if (!handlers[k])
            continue;
        emit_label(ctx, handlers[k]);
        emit_dest(ctx, &ctx->scopes[i].dests[k]);
        xfree(handlers[k]);
    }
    xfree(handlers);
    emit_label(ctx, after);
    xfree(after);
}

// The end of the innermost block, reached by falling through: its own actions.
static void end_scope(TacCtx *ctx)
{
    const TacScope *sc = &ctx->scopes[ctx->nscopes - 1];
    if (sc->count > 0) {
        // Not after a jump or return: what follows one is unreachable.
        bool reachable = !ctx->tail || (ctx->tail->kind != TAC_INSTRUCTION_RETURN &&
                                        ctx->tail->kind != TAC_INSTRUCTION_JUMP);
        if (reachable)
            run_scope(ctx, ctx->nscopes - 1, sc->count);
    }
    emit_chain(ctx);
    scope_pop(ctx);
}

// A substatement of a selection, iteration or defer statement: a block of its own.
static void gen_sub(TacCtx *ctx, Stmt *stmt)
{
    if (!stmt || stmt->kind == STMT_COMPOUND) {
        gen_stmt(ctx, stmt);
        return;
    }
    scope_push(ctx, stmt);
    gen_stmt(ctx, stmt);
    end_scope(ctx);
}

// break or continue to `target`: leave the blocks inside its loop or switch.
static void emit_break(TacCtx *ctx, const char *target, bool is_continue)
{
    for (int i = ctx->nbreaks - 1; i >= 0; i--) {
        const char *l = is_continue ? ctx->breaks[i].cont_label : ctx->breaks[i].break_label;
        if (l && strcmp(l, target) == 0) {
            leave(ctx, ctx->breaks[i].depth,
                  (ExitDest){ .kind = DEST_LABEL, .label = xstrdup(target) }, CHAIN_MIN);
            return;
        }
    }
    emit_jump(ctx, target);
}

// goto: leave the blocks the label is not in, and the defers of the innermost common
// block registered after the label (a jump back over them).
static void emit_goto(TacCtx *ctx, const char *label)
{
    if (have_exits(ctx)) {
        if (!ctx->label_pos_ready) {
            map_init(&ctx->label_pos);
            defer_collect_labels(ctx->body, &ctx->label_pos);
            ctx->label_pos_ready = true;
        }
        intptr_t v;
        if (!map_get(&ctx->label_pos, label, &v))
            internal_error("goto: no position for label %s", label);
        const DeferPos *to = (const DeferPos *)v;
        int k              = 0;
        while (k < ctx->nscopes && k < to->depth && ctx->scopes[k].key == to->scopes[k].key)
            k++;
        if (k == 0 || ctx->scopes[k - 1].count <= to->scopes[k - 1].count) {
            leave(ctx, k,
                  (ExitDest){ .kind = DEST_LABEL, .label = xstrdup(user_label_name(ctx, label)) },
                  CHAIN_MIN);
            return;
        }
        run_exits(ctx, k); // and back over defers of the common block: lowered here
        if (k > 0) {
            int have = ctx->scopes[k - 1].count;
            int keep = to->scopes[k - 1].count;
            for (int j = have - 1; j >= keep; j--) {
                ExitAction a = ctx->scopes[k - 1].actions[j];
                run_action(ctx, &a);
            }
        }
    }
    emit_jump(ctx, user_label_name(ctx, label));
}

// return: the value is computed before the defers run, which may change what it names.
static void emit_return(TacCtx *ctx, Stmt *stmt)
{
    if (ctx->coro) {
        // The result goes into the frame before the defers run.
        Tac_Val *v = stmt->u.expr ? gen_expr(ctx, stmt->u.expr) : NULL;
        gen_coro_return(ctx, v, stmt->u.expr ? stmt->u.expr->type : NULL);
        return;
    }
    if (ctx->sret_name && stmt->u.expr) {
        // A struct return through the hidden pointer (sret): copy the result into the
        // caller's slot through the hidden return pointer, then return the pointer
        // itself.
        Tac_Val *src  = gen_expr(ctx, stmt->u.expr); // VAR naming the source aggregate
        AggPlace dst  = { NULL, 0, ctx->sret_name };
        AggPlace from = { src->u.var_name, 0, NULL };
        gen_aggregate_copy(ctx, &dst, &from, stmt->u.expr->type);
        tac_free_val(src);
        run_exits(ctx, 0);
        Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_RETURN);
        in->u.return_.src   = val_var(ctx->sret_name);
        tac_append(ctx, in);
        return;
    }
    Tac_Val *v = stmt->u.expr ? gen_expr(ctx, stmt->u.expr) : NULL;
    if (v && v->kind == TAC_VAL_VAR && v->u.var_name[0] != '%' && have_exits(ctx)) {
        // A named object a deferred statement could change: return a copy of it.
        const Type *t = unalias(stmt->u.expr->type);
        if (t->kind == TYPE_STRUCT || t->kind == TYPE_UNION) {
            char *slot                     = new_typed_temp(ctx, ast_type_to_tac_type(t));
            Tac_Instruction *al            = tac_new_instruction(TAC_INSTRUCTION_ALLOCATE_LOCAL);
            al->u.allocate_local.name      = xstrdup(slot);
            al->u.allocate_local.size      = (int)get_size(t);
            al->u.allocate_local.alignment = (int)get_alignment(t);
            tac_append(ctx, al);
            AggPlace dst  = { slot, 0, NULL };
            AggPlace from = { v->u.var_name, 0, NULL };
            gen_aggregate_copy(ctx, &dst, &from, t);
            tac_free_val(v);
            v = val_var(slot);
            xfree(slot);
        } else {
            Tac_Val *copy       = new_var_val(ctx, ast_type_to_tac_type(t));
            Tac_Instruction *cp = tac_new_instruction(TAC_INSTRUCTION_COPY);
            cp->u.copy.src      = v;
            cp->u.copy.dst      = copy;
            tac_append(ctx, cp);
            v = val_var(copy->u.var_name);
        }
    }
    // Through a shared cleanup, a scalar goes into the one return variable first.
    const Type *rt = stmt->u.expr ? unalias(stmt->u.expr->type) : NULL;
    bool scalar    = !rt || (rt->kind != TYPE_STRUCT && rt->kind != TYPE_UNION &&
                             rt->kind != TYPE_ARRAY && rt->kind != TYPE_LONG_DOUBLE);
    if (scalar && have_exits(ctx) && may_share() && exit_size(ctx, 0) >= CHAIN_MIN) {
        if (v) {
            if (!ctx->ret_var)
                ctx->ret_var = new_typed_temp(ctx, ast_type_to_tac_type(rt));
            Tac_Instruction *cp = tac_new_instruction(TAC_INSTRUCTION_COPY);
            cp->u.copy.src      = v;
            cp->u.copy.dst      = val_var(ctx->ret_var);
            tac_append(ctx, cp);
        }
        leave(ctx, 0, (ExitDest){ .kind = DEST_RETURN, .value = v != NULL }, CHAIN_MIN);
        return;
    }
    run_exits(ctx, 0);
    Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_RETURN);
    in->u.return_.src   = v;
    tac_append(ctx, in);
}

void gen_stmt(TacCtx *ctx, Stmt *stmt);

static void gen_stmt_at(TacCtx *ctx, Stmt *stmt)
{
    if (!stmt) {
        return;
    }
    switch (stmt->kind) {
    case STMT_COMPOUND: {
        scope_push(ctx, stmt);
        for (DeclOrStmt *ds = stmt->u.compound; ds; ds = ds->next) {
            if (ds->kind == DECL_OR_STMT_DECL) {
                gen_local_decl(ctx, ds->u.decl);
            } else {
                gen_stmt(ctx, ds->u.stmt);
            }
        }
        end_scope(ctx);
        break;
    }
    case STMT_DEFER:
        tac_scope_add(ctx, (ExitAction){ EXIT_DEFER, stmt->u.defer_stmt, NULL, NULL });
        break;
    case STMT_EXPR:
        if (stmt->u.expr) {
            tac_free_val(gen_expr(ctx, stmt->u.expr));
        }
        break;
    case STMT_RETURN:
        emit_return(ctx, stmt);
        break;
    case STMT_IF: {
        char *else_l, *end_l;
        if (cond_jumps() && is_logical(stmt->u.if_stmt.condition)) {
            else_l = new_temp(ctx);
            end_l  = new_temp(ctx);
            gen_cond_jump(ctx, stmt->u.if_stmt.condition, false, else_l);
        } else {
            // The labels after the condition's temporaries, as they always were.
            Tac_Val *cond                = gen_cond_val(ctx, stmt->u.if_stmt.condition);
            else_l                       = new_temp(ctx);
            end_l                        = new_temp(ctx);
            Tac_Instruction *jz          = tac_new_instruction(TAC_INSTRUCTION_JUMP_IF_ZERO);
            jz->u.jump_if_zero.condition = cond;
            jz->u.jump_if_zero.target    = xstrdup(else_l);
            tac_append(ctx, jz);
        }
        gen_sub(ctx, stmt->u.if_stmt.then_stmt);
        emit_jump(ctx, end_l);
        emit_label(ctx, else_l);
        xfree(else_l);
        if (stmt->u.if_stmt.else_stmt) {
            gen_sub(ctx, stmt->u.if_stmt.else_stmt);
        }
        emit_label(ctx, end_l);
        xfree(end_l); // emit_jump and emit_label each xstrdup; free the original
        break;
    }
    case STMT_WHILE: {
        const char *cl = stmt->loop_continue_label;
        const char *bl = stmt->loop_end_label;
        if (!cl || !bl) {
            internal_error("while: missing loop labels (label_loops not run?)");
        }
        if (translate_rotate_loops) {
            // Rotated: the test at the bottom, and a copy of it at the top as a guard
            // (or a jump to it).
            if (jump_to_test(stmt->u.while_stmt.condition)) {
                if (!is_empty_stmt(stmt->u.while_stmt.body))
                    emit_jump(ctx, cl);
            } else
                emit_loop_test(ctx, stmt->u.while_stmt.condition, false, bl);
            char *top = new_temp(ctx);
            emit_label(ctx, top);
            breaks_push(ctx, bl, cl);
            gen_sub(ctx, stmt->u.while_stmt.body);
            ctx->nbreaks--;
            emit_label(ctx, cl);
            emit_loop_test(ctx, stmt->u.while_stmt.condition, true, top);
            xfree(top);
            emit_label(ctx, bl);
            break;
        }
        emit_label(ctx, cl);
        emit_loop_test(ctx, stmt->u.while_stmt.condition, false, bl);
        breaks_push(ctx, bl, cl);
        gen_sub(ctx, stmt->u.while_stmt.body);
        ctx->nbreaks--;
        emit_jump(ctx, cl);
        emit_label(ctx, bl);
        break;
    }
    case STMT_DO_WHILE: {
        const char *cl = stmt->loop_continue_label;
        const char *bl = stmt->loop_end_label;
        if (!cl || !bl) {
            internal_error("do-while: missing loop labels");
        }
        char *loop_top = new_temp(ctx);
        emit_label(ctx, loop_top);
        breaks_push(ctx, bl, cl);
        gen_sub(ctx, stmt->u.do_while.body);
        ctx->nbreaks--;
        emit_label(ctx, cl);
        gen_cond_jump(ctx, stmt->u.do_while.condition, true, loop_top);
        xfree(loop_top);
        emit_label(ctx, bl);
        break;
    }
    case STMT_FOR: {
        const char *cl = stmt->loop_continue_label;
        const char *bl = stmt->loop_end_label;
        if (!cl || !bl) {
            internal_error("for: missing loop labels");
        }
        if (stmt->u.for_stmt.init) {
            if (stmt->u.for_stmt.init->kind == FOR_INIT_EXPR) {
                if (stmt->u.for_stmt.init->u.expr) {
                    tac_free_val(gen_expr(ctx, stmt->u.for_stmt.init->u.expr));
                }
            } else {
                gen_local_decl(ctx, stmt->u.for_stmt.init->u.decl);
            }
        }
        Expr *cond  = stmt->u.for_stmt.condition;
        bool rotate = translate_rotate_loops && cond;
        char *test  = rotate && jump_to_test(cond) ? new_temp(ctx) : NULL;
        if (test && !(is_empty_stmt(stmt->u.for_stmt.body) && !stmt->u.for_stmt.update))
            emit_jump(ctx, test); // to the test, in place of a guard
        else if (rotate)
            emit_loop_test(ctx, cond, false, bl); // the guard
        char *top = new_temp(ctx);
        emit_label(ctx, top);
        if (cond && !rotate)
            emit_loop_test(ctx, cond, false, bl);
        breaks_push(ctx, bl, cl);
        gen_sub(ctx, stmt->u.for_stmt.body);
        ctx->nbreaks--;
        emit_label(ctx, cl);
        if (stmt->u.for_stmt.update) {
            tac_free_val(gen_expr(ctx, stmt->u.for_stmt.update));
        }
        if (test) {
            emit_label(ctx, test);
            xfree(test);
        }
        if (rotate)
            emit_loop_test(ctx, cond, true, top);
        else
            emit_jump(ctx, top);
        xfree(top); // emit_label and emit_jump each xstrdup; free the original
        emit_label(ctx, bl);
        break;
    }
    case STMT_SWITCH: {
        if (!stmt->loop_end_label)
            internal_error("switch: missing end label (label_loops not run?)");

        CaseList cases = { NULL, NULL, NULL };
        cases.tail     = &cases.head;
        collect_cases(ctx, stmt->u.switch_stmt.body, &cases);

        Tac_Val *ctrl_raw = gen_expr(ctx, stmt->u.switch_stmt.expr);
        Tac_Val *ctrl_dst = new_var_val(ctx, ast_type_to_tac_type(stmt->u.switch_stmt.expr->type));
        const char *ctrl_name = ctrl_dst->u.var_name; // save before ownership transfer
        Tac_Instruction *cp   = tac_new_instruction(TAC_INSTRUCTION_COPY);
        cp->u.copy.src        = ctrl_raw;
        cp->u.copy.dst        = ctrl_dst;
        tac_append(ctx, cp);

        const Type *ctrl_type = stmt->u.switch_stmt.expr->type;
        for (CaseEntry *e = cases.head; e; e = e->next) {
            // C11 §6.8.4.2p5: a case constant is converted to the promoted type of the
            // controlling expression.  Only a width change can alter the comparison.
            Tac_Val *cval = gen_expr(ctx, e->expr);
            if (get_size(e->expr->type) != get_size(ctrl_type))
                cval = emit_cast(ctx, cval, e->expr->type, ctrl_type);
            Tac_Val *cmp_dst     = new_var_val(ctx, tac_new_type(TAC_TYPE_INT));
            const char *cmp_name = cmp_dst->u.var_name;
            Tac_Instruction *bin = tac_new_instruction(TAC_INSTRUCTION_BINARY);
            bin->u.binary.op     = TAC_BINARY_EQUAL;
            bin->u.binary.src1   = val_var(ctrl_name);
            bin->u.binary.src2   = cval;
            bin->u.binary.dst    = cmp_dst;
            tac_append(ctx, bin);
            Tac_Instruction *jnz = tac_new_instruction(TAC_INSTRUCTION_JUMP_IF_NOT_ZERO);
            jnz->u.jump_if_not_zero.condition = val_var(cmp_name);
            jnz->u.jump_if_not_zero.target    = xstrdup(e->label);
            tac_append(ctx, jnz);
        }

        emit_jump(ctx, cases.default_label ? cases.default_label : stmt->loop_end_label);
        breaks_push(ctx, stmt->loop_end_label, NULL);
        gen_sub(ctx, stmt->u.switch_stmt.body);
        ctx->nbreaks--;
        emit_label(ctx, stmt->loop_end_label);

        for (CaseEntry *e = cases.head; e;) {
            CaseEntry *nx = e->next;
            xfree(e);
            e = nx;
        }
        break;
    }
    case STMT_BREAK: {
        if (!stmt->branch_target_label) {
            internal_error("break without target label");
        }
        emit_break(ctx, stmt->branch_target_label, false);
        break;
    }
    case STMT_CONTINUE: {
        if (!stmt->branch_target_label) {
            internal_error("continue without target label");
        }
        emit_break(ctx, stmt->branch_target_label, true);
        break;
    }
    case STMT_GOTO:
        emit_goto(ctx, stmt->u.goto_label);
        break;
    case STMT_LABELED:
        emit_label(ctx, user_label_name(ctx, stmt->u.labeled.label));
        gen_stmt(ctx, stmt->u.labeled.stmt);
        break;
    case STMT_CASE:
        if (!stmt->branch_target_label)
            internal_error("case: missing label (collect_cases not run?)");
        emit_label(ctx, stmt->branch_target_label);
        gen_stmt(ctx, stmt->u.case_stmt.stmt);
        break;
    case STMT_DEFAULT:
        if (!stmt->branch_target_label)
            internal_error("default: missing label (collect_cases not run?)");
        emit_label(ctx, stmt->branch_target_label);
        gen_stmt(ctx, stmt->u.default_stmt);
        break;
    default:
        internal_error("unsupported statement kind %d in TAC lowering", (int)stmt->kind);
    }
}

// gen_stmt with diag_loc at the node, for the errors found in it.
void gen_stmt(TacCtx *ctx, Stmt *stmt)
{
    SrcLoc saved = diag_enter(stmt ? stmt->loc : diag_loc);
    gen_stmt_at(ctx, stmt);
    diag_loc = saved;
}
