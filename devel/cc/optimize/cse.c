// ============================================================================
// cse.c — common-subexpression elimination via available-expressions analysis.
//
// When `h = a + b` has been computed and neither a, b nor h has changed since,
// on every path, a later `d = a + b` can take the value from h instead:
// `d = h`. Copy propagation then forwards h into the uses of d, and dead-store
// elimination removes the copy. Should d be h itself, the recomputation is
// deleted outright. No new temporaries are introduced, so the function's typed
// locals stay exactly the names of its body.
//
// A forward dataflow analysis establishes which expressions are available:
//
//   - Lattice element: a set of facts "expression E is held in variable h",
//     keyed by E, holding on *every* path reaching the program point.
//   - Initial value at entry: empty.
//   - Meet at merge points: intersection — a fact survives only when every
//     predecessor has it with the same holder. A predecessor not yet visited is
//     left out of the meet (the optimistic start), so an expression computed
//     ahead of a loop stays available inside it.
//   - Transfer for one instruction:
//       Kill: defining a variable v removes every fact whose holder is v or
//             whose expression reads v.
//       Gen:  a candidate `h = E` adds (E → h), unless E reads h itself.
//
// Candidates are the pure value computations, which have no side effect and
// whose result depends on their operands alone: BINARY, UNARY, the conversions,
// ADD_PTR, PTR_DIFF and the three GET_ADDRESS kinds of a static object (an
// address never changes, so GET_ADDRESS reads no operand value; the address of
// a frame slot is cheaper to recompute than to hold). Removing a recomputation
// that an identical one dominates never introduces a trap, so division is
// included. So are the memory reads, LOAD through a pointer and COPY_FROM_OFFSET
// of a named aggregate's member; volatile ones excepted.
//
// Conservatism around aliasing (see alias.c): static-duration and address-taken
// variables may be changed behind our back. A FunCall may write any of them,
// and so may a Store through a pointer — a global's address may have been taken
// in another function — so both kill every fact that mentions one. A read
// through a pointer may see any memory: a store, a call, or a write to a static
// or address-taken variable kills it (no type-based aliasing — the code this
// compiler builds puns types freely). A holder is always private (a temporary,
// parameter or automatic local) and of the same type as the destination it
// replaces.
//
// See docs/TAC_Optimization.md §"Common-subexpression elimination".
// ============================================================================

#include <stdarg.h>
#include <string.h>

#include "alias.h"
#include "cfg.h"
#include "dataflow.h"
#include "optimize.h"
#include "string_map.h"
#include "tac.h"
#include "xalloc.h"

// ============================================================================
// Fact — one available expression. An expression set is a StringMap keyed by
// the expression's canonical spelling (see expr_key); the Fact is the value.
// ============================================================================

typedef struct {
    char *key;        // owned; the expression's spelling, == the map key
    char *holder;     // owned; the variable that holds the expression's value
    char *reads[2];   // owned; the variables the expression reads, or NULL
    bool load;        // a read through a pointer: any write to memory may change it
    Tac_Const *konst; // owned; when the holder is a constant (forwarded from a
                      // store), that constant; `holder` is then its spelling
} Fact;

// Iterations of the fixpoint after which an out-set may only shrink (see below).
enum { CSE_MONOTONE_AFTER = 64 };

static void fact_free(intptr_t value)
{
    Fact *f = (Fact *)value;
    if (!f)
        return;
    xfree(f->key);
    xfree(f->holder);
    xfree(f->reads[0]);
    xfree(f->reads[1]);
    if (f->konst)
        tac_free_const(f->konst);
    xfree(f);
}

static void expr_set_destroy(StringMap *es)
{
    map_destroy_free(es, fact_free);
}

static Fact *fact_dup(const Fact *f)
{
    Fact *nf     = xalloc(sizeof(Fact), __func__, __FILE__, __LINE__);
    nf->key      = xstrdup(f->key);
    nf->holder   = xstrdup(f->holder);
    nf->reads[0] = f->reads[0] ? xstrdup(f->reads[0]) : NULL;
    nf->reads[1] = f->reads[1] ? xstrdup(f->reads[1]) : NULL;
    nf->load     = f->load;
    nf->konst    = NULL;
    if (f->konst) {
        nf->konst  = tac_new_const(f->konst->kind);
        *nf->konst = *f->konst;
    }
    return nf;
}

static void copy_cb(const char *key, intptr_t value, const void *arg)
{
    (void)key;
    const Fact *nf = fact_dup((const Fact *)value);
    map_insert_free((StringMap *)arg, nf->key, (intptr_t)nf, 0, fact_free);
}

// cppcheck-suppress constParameterPointer ; dst is written, through the map_iterate context
static void expr_set_copy(StringMap *dst, const StringMap *src)
{
    map_iterate((StringMap *)src, copy_cb, dst);
}

static bool fact_mentions(const Fact *f, const char *name)
{
    return strcmp(f->holder, name) == 0 || (f->reads[0] && strcmp(f->reads[0], name) == 0) ||
           (f->reads[1] && strcmp(f->reads[1], name) == 0);
}

// ============================================================================
// Kill rules: by one name (a definition), and by a whole class of names (the
// aliased variables, at a call or a store through a pointer).
// ============================================================================

typedef struct {
    KeyBuf *kb;
    const char *name;
} KillNameCtx;

static void kill_name_cb(const char *key, intptr_t value, const void *arg)
{
    (void)key;
    const KillNameCtx *ctx = (const KillNameCtx *)arg;
    Fact *f                = (Fact *)value;
    if (fact_mentions(f, ctx->name))
        keybuf_push(ctx->kb, f->key);
}

static void kill_name(StringMap *es, const char *name)
{
    KeyBuf kb       = { 0 };
    KillNameCtx ctx = { &kb, name };
    map_iterate(es, kill_name_cb, &ctx);
    keybuf_flush(&kb, es, fact_free);
}

typedef struct {
    KeyBuf *kb;
    const StringMap *alias;
} KillAliasCtx;

static void kill_alias_cb(const char *key, intptr_t value, const void *arg)
{
    (void)key;
    const KillAliasCtx *ctx = (const KillAliasCtx *)arg;
    Fact *f                 = (Fact *)value;
    const StringMap *alias  = ctx->alias;
    if (map_get(alias, f->holder, NULL) || (f->reads[0] && map_get(alias, f->reads[0], NULL)) ||
        (f->reads[1] && map_get(alias, f->reads[1], NULL)))
        keybuf_push(ctx->kb, f->key);
}

static void kill_alias_set(StringMap *es, const StringMap *alias)
{
    KeyBuf kb        = { 0 };
    KillAliasCtx ctx = { &kb, alias };
    map_iterate(es, kill_alias_cb, &ctx);
    keybuf_flush(&kb, es, fact_free);
}

// A write that may reach memory through a pointer: every read through a pointer
// is then stale. (A member read of a named aggregate needs no such rule: it reads
// the aggregate's name, so the alias and name kills above cover it.)
static void kill_loads_cb(const char *key, intptr_t value, const void *arg)
{
    (void)key;
    Fact *f = (Fact *)value;
    if (f->load)
        keybuf_push((KeyBuf *)arg, f->key);
}

static void kill_loads(StringMap *es)
{
    KeyBuf kb = { 0 };
    map_iterate(es, kill_loads_cb, &kb);
    keybuf_flush(&kb, es, fact_free);
}

// ============================================================================
// Meet and fixed-point test.
// ============================================================================

typedef struct {
    KeyBuf *kb;
    const StringMap *other;
    bool *equal;
} CompareCtx;

// Does `other` hold the same fact (same key, same holder)?
static bool has_fact(const StringMap *other, const Fact *f)
{
    intptr_t oval = 0;
    if (!map_get((StringMap *)other, f->key, &oval))
        return false;
    return strcmp(((const Fact *)oval)->holder, f->holder) == 0;
}

static void intersect_cb(const char *key, intptr_t value, const void *arg)
{
    (void)key;
    const CompareCtx *ctx = (const CompareCtx *)arg;
    Fact *f               = (Fact *)value;
    if (!has_fact(ctx->other, f))
        keybuf_push(ctx->kb, f->key);
}

static void expr_set_intersect(StringMap *result, const StringMap *other)
{
    KeyBuf kb      = { 0 };
    CompareCtx ctx = { &kb, other, NULL };
    map_iterate(result, intersect_cb, &ctx);
    keybuf_flush(&kb, result, fact_free);
}

static void subset_cb(const char *key, intptr_t value, const void *arg)
{
    (void)key;
    const CompareCtx *ctx = (const CompareCtx *)arg;
    if (*ctx->equal && !has_fact(ctx->other, (const Fact *)value))
        *ctx->equal = false;
}

static bool expr_set_equal(const StringMap *a, const StringMap *b)
{
    bool eq        = true;
    CompareCtx ctx = { NULL, b, &eq };
    map_iterate((StringMap *)a, subset_cb, &ctx);
    if (eq) {
        ctx.other = a;
        map_iterate((StringMap *)b, subset_cb, &ctx);
    }
    return eq;
}

// ============================================================================
// Expression keys. An expression is spelled as its instruction kind, operator
// and immediate fields, then its operands: a variable by name, a constant by its
// kind and exact bits (so -0.0 and 0.0, or an int 1 and a long 1, stay apart).
// The destination's type is not part of the key: a fact is only used for a
// destination of the same type as its holder.
// ============================================================================

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} StrBuf;

static void sb_printf(StrBuf *sb, const char *fmt, ...)
{
    for (;;) {
        va_list ap;
        va_start(ap, fmt);
        size_t room = sb->cap - sb->len;
        int n       = vsnprintf(sb->buf ? sb->buf + sb->len : NULL, room, fmt, ap);
        va_end(ap);
        if ((size_t)n < room) {
            sb->len += (size_t)n;
            return;
        }
        size_t new_cap = (sb->cap + (size_t)n + 1) * 2;
        char *nb       = xalloc(new_cap, __func__, __FILE__, __LINE__);
        if (sb->buf)
            memcpy(nb, sb->buf, sb->len + 1);
        xfree(sb->buf);
        sb->buf = nb;
        sb->cap = new_cap;
    }
}

static void spell_val(StrBuf *sb, const Tac_Val *v)
{
    if (v->kind == TAC_VAL_VAR) {
        sb_printf(sb, "|v:%s", v->u.var_name);
        return;
    }
    const Tac_Const *c = v->u.constant;
    uint64_t bits      = 0;
    switch (c->kind) {
    case TAC_CONST_INT:
        bits = (uint64_t)c->u.int_val;
        break;
    case TAC_CONST_LONG:
        bits = (uint64_t)c->u.long_val;
        break;
    case TAC_CONST_LONG_LONG:
        bits = (uint64_t)c->u.long_long_val;
        break;
    case TAC_CONST_UINT:
        bits = c->u.uint_val;
        break;
    case TAC_CONST_ULONG:
        bits = c->u.ulong_val;
        break;
    case TAC_CONST_ULONG_LONG:
        bits = c->u.ulong_long_val;
        break;
    case TAC_CONST_FLOAT:
        memcpy(&bits, &c->u.float_val, sizeof bits);
        break;
    case TAC_CONST_DOUBLE:
        memcpy(&bits, &c->u.double_val, sizeof bits);
        break;
    case TAC_CONST_LONG_DOUBLE:
        sb_printf(sb, "|c%d:%llx:%llx", (int)c->kind, (unsigned long long)c->u.long_double_val.hi,
                  (unsigned long long)c->u.long_double_val.lo);
        return;
    case TAC_CONST_SCHAR:
        bits = (uint64_t)(int64_t)c->u.char_val;
        break;
    case TAC_CONST_UCHAR:
        bits = c->u.uchar_val;
        break;
    }
    sb_printf(sb, "|c%d:%llx", (int)c->kind, (unsigned long long)bits);
}

static bool is_commutative(Tac_BinaryOperator op)
{
    switch (op) {
    case TAC_BINARY_ADD:
    case TAC_BINARY_MULTIPLY:
    case TAC_BINARY_EQUAL:
    case TAC_BINARY_NOT_EQUAL:
    case TAC_BINARY_BITWISE_AND:
    case TAC_BINARY_BITWISE_OR:
    case TAC_BINARY_BITWISE_XOR:
    case TAC_BINARY_ADD_UNSIGNED:
    case TAC_BINARY_MULTIPLY_UNSIGNED:
    case TAC_BINARY_ADD_DOUBLE:
    case TAC_BINARY_MULTIPLY_DOUBLE:
        return true;
    default:
        return false;
    }
}

// ============================================================================
// The pass context: the alias classes, and the private names with their types.
// ============================================================================

typedef struct {
    StringMap observable;
    StringMap address_taken;
    StringMap private_types; // param/local name → its Tac_Type* (may be NULL)
    bool have_fn;            // false: no function context, every name private
} CseCtx;

static bool is_private(const CseCtx *ctx, const char *name)
{
    return !ctx->have_fn || map_get((StringMap *)&ctx->private_types, name, NULL);
}

// May `holder` stand in for `dst`? Both private, and of the same type.
static bool same_type(const CseCtx *ctx, const char *holder, const char *dst)
{
    if (!ctx->have_fn)
        return true;
    intptr_t th = 0, td = 0;
    if (!map_get((StringMap *)&ctx->private_types, holder, &th) ||
        !map_get((StringMap *)&ctx->private_types, dst, &td))
        return false;
    if (!th || !td)
        return !th && !td; // untyped hand-built TAC: only names typed alike
    return tac_compare_type((const Tac_Type *)th, (const Tac_Type *)td);
}

// May the constant `k` stand in for `dst`? Its kind must be dst's own type.
static bool const_fits(const CseCtx *ctx, const Tac_Const *k, const char *dst)
{
    intptr_t td = 0;
    if (!ctx->have_fn)
        return true;
    if (!map_get((StringMap *)&ctx->private_types, dst, &td) || !td)
        return false;
    Tac_TypeKind t = ((const Tac_Type *)td)->kind;
    switch (k->kind) {
    case TAC_CONST_INT:
        return t == TAC_TYPE_INT;
    case TAC_CONST_LONG:
        return t == TAC_TYPE_LONG;
    case TAC_CONST_LONG_LONG:
        return t == TAC_TYPE_LONG_LONG;
    case TAC_CONST_UINT:
        return t == TAC_TYPE_UINT;
    case TAC_CONST_ULONG:
        return t == TAC_TYPE_ULONG;
    case TAC_CONST_ULONG_LONG:
        return t == TAC_TYPE_ULONG_LONG;
    case TAC_CONST_FLOAT:
        return t == TAC_TYPE_FLOAT;
    case TAC_CONST_DOUBLE:
        return t == TAC_TYPE_DOUBLE;
    case TAC_CONST_LONG_DOUBLE:
        return t == TAC_TYPE_LONG_DOUBLE;
    case TAC_CONST_SCHAR:
        return t == TAC_TYPE_SCHAR;
    case TAC_CONST_UCHAR:
        return t == TAC_TYPE_UCHAR;
    }
    return false;
}

// ============================================================================
// Candidates: an instruction taken apart into its destination, its operands
// and the immediate fields that complete its spelling.
// ============================================================================

typedef struct {
    Tac_Val **dst;          // the dst field, so a rewrite can take it over
    const Tac_Val *opnd[2]; // the operands read (NULL when fewer)
    bool reads_operands;    // false for GET_ADDRESS: an address reads no value
    int op;                 // operator, scale, dst_kind or member offset; 0 when none
    const char *agg;        // COPY_FROM_OFFSET: the aggregate it reads
    bool load;              // LOAD: a read through a pointer
} Candidate;

static bool as_candidate(Tac_Instruction *ins, const CseCtx *ctx, Candidate *c)
{
    memset(c, 0, sizeof *c);
    c->reads_operands = true;
    switch (ins->kind) {
    case TAC_INSTRUCTION_BINARY:
        c->dst     = &ins->u.binary.dst;
        c->opnd[0] = ins->u.binary.src1;
        c->opnd[1] = ins->u.binary.src2;
        c->op      = ins->u.binary.op;
        break;
    case TAC_INSTRUCTION_UNARY:
        c->dst     = &ins->u.unary.dst;
        c->opnd[0] = ins->u.unary.src;
        c->op      = ins->u.unary.op;
        break;
    case TAC_INSTRUCTION_SIGN_EXTEND:
    case TAC_INSTRUCTION_TRUNCATE:
    case TAC_INSTRUCTION_ZERO_EXTEND:
    case TAC_INSTRUCTION_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_DOUBLE_TO_UINT:
        // The five kinds that carry the destination's constant kind.
        c->op = ins->u.sign_extend.dst_kind;
        // fall through
    case TAC_INSTRUCTION_INT_TO_DOUBLE:
    case TAC_INSTRUCTION_UINT_TO_DOUBLE:
    case TAC_INSTRUCTION_FLOAT_TO_DOUBLE:
    case TAC_INSTRUCTION_DOUBLE_TO_FLOAT:
    case TAC_INSTRUCTION_INT_TO_FLOAT:
    case TAC_INSTRUCTION_UINT_TO_FLOAT:
    case TAC_INSTRUCTION_FLOAT_TO_INT:
    case TAC_INSTRUCTION_FLOAT_TO_UINT:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_UINT:
    case TAC_INSTRUCTION_INT_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_UINT_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_DOUBLE:
    case TAC_INSTRUCTION_DOUBLE_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_FLOAT:
    case TAC_INSTRUCTION_FLOAT_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_PTR_TO_CHAR_PTR:
    case TAC_INSTRUCTION_CHAR_PTR_TO_PTR:
        c->dst     = &ins->u.sign_extend.dst;
        c->opnd[0] = ins->u.sign_extend.src;
        break;
    case TAC_INSTRUCTION_ADD_PTR:
        c->dst     = &ins->u.add_ptr.dst;
        c->opnd[0] = ins->u.add_ptr.ptr;
        c->opnd[1] = ins->u.add_ptr.index;
        c->op      = ins->u.add_ptr.scale;
        break;
    case TAC_INSTRUCTION_PTR_DIFF:
        c->dst     = &ins->u.ptr_diff.dst;
        c->opnd[0] = ins->u.ptr_diff.ptr_a;
        c->opnd[1] = ins->u.ptr_diff.ptr_b;
        break;
    case TAC_INSTRUCTION_GET_ADDRESS:
    case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
    case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
        // The address of a frame slot is one instruction off the stack or frame
        // pointer: holding it in a register instead only adds to the pressure.
        if (ins->u.get_address.src->kind == TAC_VAL_VAR &&
            is_private(ctx, ins->u.get_address.src->u.var_name))
            return false;
        c->dst            = &ins->u.get_address.dst;
        c->opnd[0]        = ins->u.get_address.src;
        c->reads_operands = false;
        break;
    case TAC_INSTRUCTION_LOAD:
    case TAC_INSTRUCTION_LOAD_BYTE:
        c->dst     = &ins->u.load.dst;
        c->opnd[0] = ins->u.load.src_ptr;
        c->load    = true;
        break;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
        c->dst = &ins->u.copy_from_offset.dst;
        c->agg = ins->u.copy_from_offset.src;
        c->op  = ins->u.copy_from_offset.offset;
        break;
    default:
        return false;
    }
    return !ins->is_volatile && *c->dst && (*c->dst)->kind == TAC_VAL_VAR;
}

// Spell the candidate's expression; the caller frees the result.
static char *expr_key(const Tac_Instruction *ins, const Candidate *c)
{
    const Tac_Val *a = c->opnd[0], *b = c->opnd[1];
    if (b && ins->kind == TAC_INSTRUCTION_BINARY && is_commutative(ins->u.binary.op)) {
        // Put the operands in a canonical order, so a+b and b+a meet.
        StrBuf sa = { 0 }, sb = { 0 };
        spell_val(&sa, a);
        spell_val(&sb, b);
        if (strcmp(sa.buf, sb.buf) > 0) {
            const Tac_Val *t = a;
            a                = b;
            b                = t;
        }
        xfree(sa.buf);
        xfree(sb.buf);
    }
    StrBuf key = { 0 };
    sb_printf(&key, "%d:%d", (int)ins->kind, c->op);
    if (c->agg)
        sb_printf(&key, "|v:%s", c->agg);
    if (a)
        spell_val(&key, a);
    if (b)
        spell_val(&key, b);
    return key.buf;
}

// A write to the variable `name`: facts that mention it die, and, if a pointer
// may reach it (a static or address-taken variable), every read through one.
static void kill_written(StringMap *es, const CseCtx *ctx, const char *name)
{
    kill_name(es, name);
    if (map_get((StringMap *)&ctx->observable, name, NULL) ||
        map_get((StringMap *)&ctx->address_taken, name, NULL))
        kill_loads(es);
}

// Store-to-load forwarding: after `*p = v`, a read `*p` is v until something is
// written. v must be a private variable or a constant; a byte store truncates,
// so only a word store gives the fact. It is spelled as a LOAD through p.
static void gen_store(StringMap *es, const Tac_Instruction *ins, const CseCtx *ctx)
{
    const Tac_Val *p = ins->u.store.dst_ptr, *v = ins->u.store.src;
    if (ins->kind != TAC_INSTRUCTION_STORE || ins->is_volatile || p->kind != TAC_VAL_VAR)
        return;
    if (v->kind == TAC_VAL_VAR && !is_private(ctx, v->u.var_name))
        return;

    Tac_Instruction probe;
    memset(&probe, 0, sizeof probe);
    probe.kind = TAC_INSTRUCTION_LOAD;
    Candidate c;
    memset(&c, 0, sizeof c);
    c.opnd[0]        = p;
    c.reads_operands = true;
    c.load           = true;

    Fact *f     = xalloc(sizeof(Fact), __func__, __FILE__, __LINE__);
    f->key      = expr_key(&probe, &c);
    f->reads[0] = xstrdup(p->u.var_name);
    f->load     = true;
    if (v->kind == TAC_VAL_VAR) {
        f->holder = xstrdup(v->u.var_name);
    } else {
        StrBuf sb = { 0 };
        spell_val(&sb, v);
        f->holder = sb.buf;
        f->konst  = tac_new_const(v->u.constant->kind);
        *f->konst = *v->u.constant;
    }
    map_insert_free(es, f->key, (intptr_t)f, 0, fact_free);
}

// ============================================================================
// apply_transfer: the Kill and Gen of one instruction, updating `es` in place.
// ============================================================================

static void apply_transfer(StringMap *es, Tac_Instruction *ins, const CseCtx *ctx)
{
    switch (ins->kind) {
    case TAC_INSTRUCTION_FUN_CALL:
    case TAC_INSTRUCTION_FUN_CALL_NORETURN:
        // The callee may write any static or address-taken variable.
        kill_alias_set(es, &ctx->observable);
        kill_alias_set(es, &ctx->address_taken);
        kill_loads(es);
        if (ins->u.fun_call.dst && ins->u.fun_call.dst->kind == TAC_VAL_VAR)
            kill_name(es, ins->u.fun_call.dst->u.var_name);
        return;
    case TAC_INSTRUCTION_STORE:
    case TAC_INSTRUCTION_STORE_BYTE:
        // The pointer may point at any static or address-taken variable.
        kill_alias_set(es, &ctx->observable);
        kill_alias_set(es, &ctx->address_taken);
        kill_loads(es);
        gen_store(es, ins, ctx);
        return;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET:
        kill_written(es, ctx, ins->u.copy_to_offset.dst);
        return;
    case TAC_INSTRUCTION_COPY:
        if (ins->u.copy.dst->kind == TAC_VAL_VAR)
            kill_written(es, ctx, ins->u.copy.dst->u.var_name);
        return;
    default:
        break;
    }

    const Tac_Val *dst = opt_defining_dst(ins);
    if (!dst || dst->kind != TAC_VAL_VAR)
        return;
    const char *name = dst->u.var_name;
    kill_written(es, ctx, name);

    // Gen: a candidate whose holder is private and not among its own operands.
    Candidate c;
    if (!as_candidate(ins, ctx, &c) || !is_private(ctx, name))
        return;
    Fact *f = xalloc(sizeof(Fact), __func__, __FILE__, __LINE__);
    for (int i = 0; i < 2; i++) {
        const Tac_Val *v = c.opnd[i];
        if (!c.reads_operands || !v || v->kind != TAC_VAL_VAR)
            continue;
        if (strcmp(v->u.var_name, name) == 0) {
            fact_free((intptr_t)f);
            return;
        }
        f->reads[i] = xstrdup(v->u.var_name);
    }
    f->key = expr_key(ins, &c);
    if (map_get(es, f->key, NULL)) {
        // Already held elsewhere: the rewrite turns this instruction into a
        // copy of that holder, which keeps it. Keeping the older holder is also
        // what lets a fact agree around a loop's back edge.
        fact_free((intptr_t)f);
        return;
    }
    if (c.agg) {
        if (strcmp(c.agg, name) == 0) {
            fact_free((intptr_t)f);
            return;
        }
        f->reads[0] = xstrdup(c.agg);
    }
    f->load   = c.load;
    f->holder = xstrdup(name);
    map_insert_free(es, f->key, (intptr_t)f, 0, fact_free);
}

// ============================================================================
// Rewrite: `d = E` with (E → h) available becomes `d = h`, or is deleted when d
// is h. Returns the instruction now in its place (NULL when deleted).
// ============================================================================

static Tac_Instruction *rewrite(Tac_Instruction *ins, const StringMap *es, const CseCtx *ctx,
                                bool *deleted)
{
    *deleted = false;
    Candidate c;
    if (!as_candidate(ins, ctx, &c))
        return ins;
    char *key     = expr_key(ins, &c);
    intptr_t fval = 0;
    bool found    = map_get((StringMap *)es, key, &fval);
    xfree(key);
    if (!found)
        return ins;

    const Fact *f    = (const Fact *)fval;
    const char *name = (*c.dst)->u.var_name;
    if (f->konst) {
        // A value forwarded from a store: a constant of the destination's type.
        if (!is_private(ctx, name) || !const_fits(ctx, f->konst, name))
            return ins;
        opt_trace_instr("[cse] forward before:", ins);
        Tac_Instruction *cp = tac_new_instruction(TAC_INSTRUCTION_COPY);
        Tac_Val *src        = tac_new_val(TAC_VAL_CONSTANT);
        src->next           = NULL;
        src->u.constant     = tac_new_const(f->konst->kind);
        *src->u.constant    = *f->konst;
        cp->u.copy.src      = src;
        cp->u.copy.dst      = *c.dst;
        *c.dst              = NULL;
        cp->next            = ins->next;
        opt_trace_instr("[cse] forward after: ", cp);
        return cp;
    }
    if (strcmp(f->holder, name) == 0) {
        opt_trace_instr("[cse] delete recomputation:", ins);
        *deleted = true;
        return NULL;
    }
    if (!is_private(ctx, name) || !same_type(ctx, f->holder, name))
        return ins;

    opt_trace_instr("[cse] rewrite before:", ins);
    Tac_Instruction *cp = tac_new_instruction(TAC_INSTRUCTION_COPY);
    Tac_Val *src        = tac_new_val(TAC_VAL_VAR);
    src->next           = NULL;
    src->u.var_name     = xstrdup(f->holder);
    cp->u.copy.src      = src;
    cp->u.copy.dst      = *c.dst; // take over the destination
    *c.dst              = NULL;
    cp->next            = ins->next;
    opt_trace_instr("[cse] rewrite after: ", cp);
    return cp;
}

// ============================================================================
// eliminate_common_subexpressions: entry point. Alias pre-analysis, the forward
// available-expressions fixpoint, then the rewrite.
// ============================================================================

void eliminate_common_subexpressions(OptCfg *cfg, const Tac_TopLevel *fn)
{
    if (cfg->nblocks == 0)
        return;

    CseCtx ctx;
    collect_alias_sets(cfg, fn, &ctx.observable, &ctx.address_taken);
    map_init(&ctx.private_types);
    ctx.have_fn = fn && fn->kind == TAC_TOPLEVEL_FUNCTION;
    if (ctx.have_fn) {
        for (const Tac_Param *p = fn->u.function.params; p; p = p->next)
            if (p->name)
                map_insert(&ctx.private_types, p->name, (intptr_t)p->type, 0);
        for (const Tac_Param *p = fn->u.function.locals; p; p = p->next)
            if (p->name)
                map_insert(&ctx.private_types, p->name, (intptr_t)p->type, 0);
    }

    int n = cfg->nblocks;
    OptPreds pr;
    opt_preds_build(&pr, cfg);

    StringMap *in_sets  = xalloc(n * sizeof(StringMap), __func__, __FILE__, __LINE__);
    StringMap *out_sets = xalloc(n * sizeof(StringMap), __func__, __FILE__, __LINE__);
    bool *visited       = xalloc(n * sizeof(bool), __func__, __FILE__, __LINE__);
    for (int i = 0; i < n; i++) {
        map_init(&in_sets[i]);
        map_init(&out_sets[i]);
        visited[i] = false;
    }

    // Fixpoint iteration. A block's in-set is the intersection of its visited
    // predecessors' out-sets; the sets only shrink once every reachable block
    // has been visited, so the loop terminates. An unreachable predecessor is
    // never visited, and rightly left out of the meet.
    bool changed = true;
    int iter     = 0;
    while (changed) {
        changed = false;
        iter++;
        OPT_TRACE("[cse] fixpoint iteration %d\n", iter);
        for (int i = 0; i < n; i++) {
            OptBlock *b = cfg->blocks[i];
            // An empty block still takes part: it passes its in-set through, and
            // left unvisited it would be ignored by the meet of its successors.
            if (!b->reachable)
                continue;

            // The entry block's in-set is the boundary value (empty): control
            // may enter the function there without passing any predecessor.
            StringMap new_in;
            map_init(&new_in);
            if (i != 0) {
                bool seeded = false;
                for (int k = 0; k < pr.npreds[i]; k++) {
                    int p = pr.preds[i][k];
                    if (!visited[p])
                        continue;
                    if (!seeded)
                        expr_set_copy(&new_in, &out_sets[p]);
                    else
                        expr_set_intersect(&new_in, &out_sets[p]);
                    seeded = true;
                }
            }

            StringMap new_out;
            map_init(&new_out);
            expr_set_copy(&new_out, &new_in);
            for (Tac_Instruction *ins = b->first; ins; ins = ins->next)
                apply_transfer(&new_out, ins, &ctx);

            // The transfer is not monotone: an expression already available keeps
            // its older holder, one that is not gets this instruction's, so a
            // smaller in-set may give an out-set the larger one's does not contain,
            // and around an irreducible graph the sets can cycle.  Past a bound no
            // converging function reaches, an out-set only shrinks; the facts kept
            // are still made by the transfer from the in-set, so they hold.
            if (visited[i] && iter > CSE_MONOTONE_AFTER)
                expr_set_intersect(&new_out, &out_sets[i]);

            if (!visited[i] || !expr_set_equal(&new_out, &out_sets[i])) {
                OPT_TRACE("[cse] block %d out-set changed\n", i);
                changed = true;
            }
            visited[i] = true;

            expr_set_destroy(&in_sets[i]);
            expr_set_destroy(&out_sets[i]);
            in_sets[i]  = new_in;
            out_sets[i] = new_out;
        }
    }
    OPT_TRACE("[cse] fixpoint converged after %d iteration(s)\n", iter);

    // Rewrite: replay each block from its in-set, replacing every candidate
    // whose expression is available.
    for (int i = 0; i < n; i++) {
        OptBlock *b = cfg->blocks[i];
        if (!b->reachable || !b->first)
            continue;

        StringMap current;
        map_init(&current);
        expr_set_copy(&current, &in_sets[i]);

        Tac_Instruction *prev = NULL;
        Tac_Instruction *ins  = b->first;
        while (ins) {
            Tac_Instruction *next = ins->next;
            bool deleted;
            Tac_Instruction *repl = rewrite(ins, &current, &ctx, &deleted);
            if (repl != ins) {
                if (deleted)
                    repl = NULL;
                Tac_Instruction *link = repl ? repl : next;
                if (prev)
                    prev->next = link;
                else
                    b->first = link;
                if (b->last == ins)
                    b->last = repl ? repl : prev;
                ins->next = NULL;
                tac_free_instruction(ins);
                if (!repl) {
                    ins = next;
                    continue;
                }
                ins = repl;
            }
            apply_transfer(&current, ins, &ctx);
            prev = ins;
            ins  = next;
        }
        expr_set_destroy(&current);
    }

    for (int i = 0; i < n; i++) {
        expr_set_destroy(&in_sets[i]);
        expr_set_destroy(&out_sets[i]);
    }
    xfree(in_sets);
    xfree(out_sets);
    xfree(visited);
    opt_preds_free(&pr);
    map_destroy(&ctx.observable);
    map_destroy(&ctx.address_taken);
    map_destroy(&ctx.private_types);
}
