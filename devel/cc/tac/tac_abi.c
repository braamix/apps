//
// AAPCS64 argument classification over TAC types.  One function serves the AArch64
// backend (where a call puts each argument) and the semantic pass (the value of
// __builtin_va_class, which va_arg hands to the runtime), so the two cannot disagree.
// AAPCS (32-bit) has the same homogeneous aggregates, with long double a double, and so
// has Apple's arm64 ABI.  The
// System V AMD64 classes serve the x86-64 backend the same way.
//
#include "tac.h"

// Size of `t` under AAPCS64 (LP64, binary128 long double); under AAPCS only an FP
// type, an array of one, and a structure are asked about.
static int size_of(const Tac_Type *t, bool aapcs32)
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
    case TAC_TYPE_FLOAT:
        return 4;
    case TAC_TYPE_LONG_DOUBLE:
        return aapcs32 ? 8 : 16;
    case TAC_TYPE_ARRAY:
        return t->u.array.size * size_of(t->u.array.elem_type, aapcs32);
    case TAC_TYPE_STRUCTURE:
        return t->u.structure.size;
    default:
        return 8;
    }
}

static bool is_fp(const Tac_Type *t)
{
    return t->kind == TAC_TYPE_FLOAT || t->kind == TAC_TYPE_DOUBLE ||
           t->kind == TAC_TYPE_LONG_DOUBLE;
}

// The FP kind `t` counts as in an aggregate: under AAPCS long double is double.
static Tac_TypeKind fp_kind(const Tac_Type *t, bool aapcs32)
{
    return aapcs32 && t->kind == TAC_TYPE_LONG_DOUBLE ? TAC_TYPE_DOUBLE : t->kind;
}

// Count the FP members of `t`, all of kind `*base` (set by the first one found); false
// when some member is not one.  A union counts as its largest member.
static bool hfa_members(const Tac_Type *t, bool aapcs32, Tac_TypeKind *base, int *count)
{
    switch (t->kind) {
    case TAC_TYPE_ARRAY: {
        int n = 0;
        if (t->u.array.size > 0 && !hfa_members(t->u.array.elem_type, aapcs32, base, &n))
            return false;
        *count += n * t->u.array.size;
        return true;
    }
    case TAC_TYPE_STRUCTURE: {
        int most = 0;
        for (const Tac_Member *m = t->u.structure.members; m; m = m->next) {
            int n = 0;
            if (!hfa_members(m->type, aapcs32, base, &n))
                return false;
            if (t->u.structure.is_union)
                most = n > most ? n : most;
            else
                *count += n;
        }
        *count += most;
        return true;
    }
    default:
        if (!is_fp(t) || (*base != TAC_TYPE_VOID && *base != fp_kind(t, aapcs32)))
            return false;
        *base = fp_kind(t, aapcs32);
        *count += 1;
        return true;
    }
}

static int hfa(const Tac_Type *t, bool aapcs32, int *esize)
{
    if (is_fp(t)) {
        *esize = size_of(t, aapcs32);
        return 1;
    }
    if (t->kind != TAC_TYPE_ARRAY && t->kind != TAC_TYPE_STRUCTURE)
        return 0;
    Tac_TypeKind base = TAC_TYPE_VOID;
    int count         = 0;
    if (!hfa_members(t, aapcs32, &base, &count) || count < 1 || count > 4)
        return 0;
    Tac_Type b = { .kind = base };
    *esize     = size_of(&b, aapcs32);
    // Padding would make it something else.
    return size_of(t, aapcs32) == count * *esize ? count : 0;
}

int tac_aapcs64_hfa(const Tac_Type *t, int *esize)
{
    return hfa(t, false, esize);
}

int tac_apple64_hfa(const Tac_Type *t, int *esize)
{
    return hfa(t, true, esize); // long double is double, as under AAPCS
}

int tac_apple64_class(const Tac_Type *t)
{
    int esize;
    if ((t->kind == TAC_TYPE_ARRAY || t->kind == TAC_TYPE_STRUCTURE) && size_of(t, true) > 16 &&
        !hfa(t, true, &esize))
        return TAC_AAPCS64_BY_REF;
    return TAC_AAPCS64_GENERAL;
}

int tac_aapcs64_class(const Tac_Type *t)
{
    int esize;
    int count = tac_aapcs64_hfa(t, &esize);
    if (count)
        return esize * 8 + count;
    if ((t->kind == TAC_TYPE_ARRAY || t->kind == TAC_TYPE_STRUCTURE) && size_of(t, false) > 16)
        return TAC_AAPCS64_BY_REF;
    return TAC_AAPCS64_GENERAL;
}

int tac_msp430_class(const Tac_Type *t)
{
    return t->kind == TAC_TYPE_ARRAY || t->kind == TAC_TYPE_STRUCTURE ? TAC_MSP430_BY_REF
                                                                      : TAC_MSP430_VALUE;
}

int tac_aapcs32_class(const Tac_Type *t)
{
    int esize;
    int count = hfa(t, true, &esize);
    return count ? esize * 8 + count : TAC_AAPCS32_CORE;
}

//
// System V AMD64 (psABI §3.2.3): each eightbyte of a type is classed by the merge of
// the classes of the scalars it overlaps.
//
enum { NO_CLASS = -1, X87UP = 4 };

static int merge(int a, int b)
{
    if (a == b || b == NO_CLASS)
        return a;
    if (a == NO_CLASS)
        return b;
    if (a == TAC_SYSV64_MEMORY || b == TAC_SYSV64_MEMORY)
        return TAC_SYSV64_MEMORY;
    if (a == TAC_SYSV64_INTEGER || b == TAC_SYSV64_INTEGER)
        return TAC_SYSV64_INTEGER;
    if (a == TAC_SYSV64_X87 || b == TAC_SYSV64_X87 || a == X87UP || b == X87UP)
        return TAC_SYSV64_MEMORY;
    return TAC_SYSV64_SSE;
}

// Merge the scalars of `t`, at byte `offset` of the whole, into eb[0..1].
static void sysv_leaves(const Tac_Type *t, int offset, int eb[2])
{
    switch (t->kind) {
    case TAC_TYPE_ARRAY: {
        int esize = size_of(t->u.array.elem_type, false);
        for (int i = 0; i < t->u.array.size; i++)
            sysv_leaves(t->u.array.elem_type, offset + i * esize, eb);
        return;
    }
    case TAC_TYPE_STRUCTURE:
        for (const Tac_Member *m = t->u.structure.members; m; m = m->next)
            sysv_leaves(m->type, offset + m->offset, eb);
        return;
    case TAC_TYPE_LONG_DOUBLE:
        eb[offset / 8]     = merge(eb[offset / 8], TAC_SYSV64_X87);
        eb[offset / 8 + 1] = merge(eb[offset / 8 + 1], X87UP);
        return;
    case TAC_TYPE_FLOAT:
    case TAC_TYPE_DOUBLE:
        eb[offset / 8] = merge(eb[offset / 8], TAC_SYSV64_SSE);
        return;
    default:
        eb[offset / 8] = merge(eb[offset / 8], TAC_SYSV64_INTEGER);
        return;
    }
}

int tac_sysv64_class(const Tac_Type *t)
{
    int size = size_of(t, false);
    if (size > 16 || size == 0)
        return TAC_SYSV64_MEMORY;
    int eb[2] = { NO_CLASS, NO_CLASS };
    sysv_leaves(t, 0, eb);
    if (eb[0] == TAC_SYSV64_X87 && eb[1] == X87UP)
        return TAC_SYSV64_X87;
    // An eightbyte of padding alone takes no register.
    int n = size > 8 && eb[1] != NO_CLASS ? 2 : 1;
    for (int i = 0; i < n; i++) {
        if (eb[i] == NO_CLASS)
            eb[i] = TAC_SYSV64_SSE;
        if (eb[i] == TAC_SYSV64_MEMORY || eb[i] == TAC_SYSV64_X87 || eb[i] == X87UP)
            return TAC_SYSV64_MEMORY;
    }
    return n == 2 ? eb[0] | eb[1] << 2 : eb[0];
}

//
// WebAssembly (clang's wasm32): an aggregate holding exactly one scalar, through
// structures and one-element arrays, with no padding, travels as that scalar; an empty
// one not at all; any other by reference.  Fields that are empty (a `:0`, an array of
// no elements, an empty structure) do not count.  Each bit-field is a field of its own
// (Target.bitfield_unit_per_field), of its declared type, which its storage unit does
// not tell: the one a lone bit-field can have is the integer of the structure's size.
//
static bool wasm32_empty(const Tac_Type *t)
{
    switch (t->kind) {
    case TAC_TYPE_ARRAY:
        return t->u.array.size == 0 || wasm32_empty(t->u.array.elem_type);
    case TAC_TYPE_STRUCTURE:
        for (const Tac_Member *m = t->u.structure.members; m; m = m->next)
            if (!wasm32_empty(m->type))
                return false;
        return true;
    default:
        return false;
    }
}

static const Tac_Type *wasm32_unsigned(int size)
{
    static const Tac_Type types[] = {
        { .kind = TAC_TYPE_UCHAR },
        { .kind = TAC_TYPE_USHORT },
        { .kind = TAC_TYPE_UINT },
        { .kind = TAC_TYPE_ULONG_LONG },
    };
    switch (size) {
    case 1:
        return &types[0];
    case 2:
        return &types[1];
    case 4:
        return &types[2];
    case 8:
        return &types[3];
    default:
        return NULL;
    }
}

// The size of a scalar under ILP32 with a binary128 long double.
static int wasm32_scalar_size(const Tac_Type *t)
{
    switch (t->kind) {
    case TAC_TYPE_LONG_LONG:
    case TAC_TYPE_ULONG_LONG:
    case TAC_TYPE_DOUBLE:
        return 8;
    case TAC_TYPE_LONG_DOUBLE:
        return 16;
    case TAC_TYPE_INT:
    case TAC_TYPE_UINT:
    case TAC_TYPE_LONG:
    case TAC_TYPE_ULONG:
    case TAC_TYPE_FLOAT:
    case TAC_TYPE_POINTER:
        return 4;
    default:
        return size_of(t, false); // char, short
    }
}

// The one scalar of structure `t`, or NULL.
static const Tac_Type *wasm32_element(const Tac_Type *t)
{
    const Tac_Type *found = NULL;
    for (const Tac_Member *m = t->u.structure.members; m; m = m->next) {
        if (wasm32_empty(m->type))
            continue;
        if (found)
            return NULL;
        const Tac_Type *ft = m->type;
        while (ft->kind == TAC_TYPE_ARRAY && ft->u.array.size == 1)
            ft = ft->u.array.elem_type;
        if (ft->kind == TAC_TYPE_ARRAY)
            return NULL;
        if (ft->kind == TAC_TYPE_STRUCTURE) {
            found = wasm32_element(ft);
            if (!found)
                return NULL;
        } else if (!m->name) {
            found = wasm32_unsigned(t->u.structure.size); // a bit-field
            if (!found)
                return NULL;
        } else {
            found = ft;
        }
    }
    if (!found || wasm32_scalar_size(found) != t->u.structure.size)
        return NULL;
    return found;
}

const Tac_Type *tac_wasm32_scalar(const Tac_Type *t)
{
    switch (t->kind) {
    case TAC_TYPE_ARRAY:
        return NULL;
    case TAC_TYPE_STRUCTURE:
        return wasm32_empty(t) ? NULL : wasm32_element(t);
    default:
        return t;
    }
}

bool tac_wasm32_empty(const Tac_Type *t)
{
    return (t->kind == TAC_TYPE_STRUCTURE || t->kind == TAC_TYPE_ARRAY) && wasm32_empty(t);
}

int tac_wasm32_class(const Tac_Type *t)
{
    return tac_wasm32_scalar(t) || tac_wasm32_empty(t) ? TAC_WASM32_VALUE : TAC_WASM32_BY_REF;
}
