//
// WebAssembly argument classification over TAC types.  One function serves the wasm
// backend (how a call passes each argument) and the semantic pass (the value of
// __builtin_va_class, which va_arg hands to the runtime), so the two cannot disagree.
//
#include "tac.h"

// Size of a char or short.
static int size_of(const Tac_Type *t)
{
    switch (t->kind) {
    case TAC_TYPE_SCHAR:
    case TAC_TYPE_UCHAR:
    case TAC_TYPE_VOID:
        return 1;
    case TAC_TYPE_SHORT:
    case TAC_TYPE_USHORT:
        return 2;
    default:
        return 8;
    }
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
        return size_of(t); // char, short
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
