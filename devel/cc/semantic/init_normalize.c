//
// Initializer normalization: designators, brace elision and braced scalars
// (C11 §6.7.9p11, p17–p21).
//
// normalize_init turns a raw parser initializer for a known type into canonical form,
// which build_static_init and typecheck_init then consume positionally:
//  - scalar: an INITIALIZER_SINGLE (a braced `{ e }` is unwrapped);
//  - char array from a string literal, bare or braced: that INITIALIZER_SINGLE;
//  - array of N: a COMPOUND with exactly N items, in index order;
//  - struct: one item per member, in declaration order;
//  - union: exactly one item; unless it is for the first member, the item keeps a
//    single DESIGNATOR_FIELD naming its member (the only designator left);
//  - an item whose init is NULL is not explicitly initialized, i.e. zero.
// An unsized top-level array gets its length here.
//
// In INIT_AUTOMATIC mode this is the only place leaf expressions are typechecked.  A
// typechecked leaf is marked by a non-NULL Initializer.type (the parser leaves it NULL),
// so an item visited again under brace elision is not typechecked twice.
//
#include <stdio.h>
#include <string.h>

#include "semantic.h"
#include "structtab.h"
#include "typecheck.h"
#include "xalloc.h"

static Initializer *normalize_compound(const Type *t, Initializer *init, InitMode mode);

static bool is_aggregate(const Type *t)
{
    return t->kind == TYPE_ARRAY || t->kind == TYPE_STRUCT || t->kind == TYPE_UNION;
}

// A string literal takes a whole array of integers (the consumer rejects a non-char
// element), as GCC does; for any other array it starts brace elision.
static bool takes_string(const Type *t)
{
    return t->kind == TYPE_ARRAY && is_integer(t->u.array.element);
}

static bool is_string_init(const Initializer *init)
{
    return init->kind == INITIALIZER_SINGLE && init->u.expr->kind == EXPR_LITERAL &&
           init->u.expr->u.literal->kind == LITERAL_STRING;
}

static const char *aggregate_name(const Type *t)
{
    return t->kind == TYPE_ARRAY ? "array" : t->kind == TYPE_STRUCT ? "struct" : "union";
}

// Remove the item at *cur, keeping its init (now owned by the caller).
static Initializer *take_item(InitItem **cur)
{
    InitItem *item    = *cur;
    Initializer *init = item->init;
    if (item->designators)
        fatal_error("designator in a scalar initializer");
    *cur = item->next;
    xfree(item);
    return init;
}

// Typecheck a leaf expression once (automatic mode only).
static Initializer *check_leaf(Initializer *init, InitMode mode)
{
    if (mode == INIT_AUTOMATIC && !init->type) {
        init->u.expr = typecheck_and_decay(init->u.expr);
        init->type   = clone_type(init->u.expr->type, __func__, __FILE__, __LINE__);
    }
    return init;
}

// Empty canonical node for aggregate t: one NULL item per element or member, a single
// item for a union, none for an unsized array (it grows as it is filled).
static Initializer *new_canonical(const Type *t)
{
    Initializer *node = new_initializer(INITIALIZER_COMPOUND);
    size_t n          = 1;
    if (t->kind == TYPE_ARRAY) {
        n = t->u.array.size ? get_array_size(t) : 0;
    } else if (t->kind == TYPE_STRUCT) {
        n = 0;
        for (const FieldDef *f = structtab_find(t->u.struct_t.name)->members; f; f = f->next)
            n++;
    }
    InitItem **tail = &node->u.items;
    for (size_t i = 0; i < n; i++) {
        *tail = new_init_item(NULL, NULL);
        tail  = &(*tail)->next;
    }
    return node;
}

// Replace the initializer in *slot, freeing the one it overrides (§6.7.9p19).
static void set_slot(Initializer **slot, Initializer *init)
{
    free_initializer(*slot);
    *slot = init;
}

static void place(const Type *t, Initializer **slot, InitItem **cur, InitMode mode);

// The slot of array element d names, growing an unsized array up to it.
static InitItem **designate_index(const Type *t, Initializer *node, Designator *d)
{
    d->u.expr = typecheck_and_decay(d->u.expr);
    long index;
    if (!is_integer(d->u.expr->type) || !try_eval_const_int(d->u.expr, &index))
        fatal_error("array designator index is not an integer constant expression");
    if (index < 0)
        fatal_error("array designator index %ld is negative", index);
    bool unsized = !t->u.array.size;
    if (!unsized && (size_t)index >= get_array_size(t))
        fatal_error("array designator index %ld exceeds the bounds of '%s'", index, type_to_c(t));

    InitItem **s = &node->u.items;
    for (long i = 0;; i++, s = &(*s)->next) {
        if (!*s)
            *s = new_init_item(NULL, NULL); // only an unsized array runs short
        if (i == index)
            return s;
    }
}

// Resolve the first designator of item (§6.7.9p17): point *slot and *field at the
// element or member it names, and drop it from the item.  For a union, the canonical
// item records the member (§3 of the plan), unless it is the first.
static void designate(const Type *t, Initializer *node, InitItem *item, InitItem ***slot,
                      const FieldDef **field)
{
    Designator *d = item->designators;
    if (d->kind == DESIGNATOR_ARRAY && t->kind != TYPE_ARRAY)
        fatal_error("array designator in an initializer of '%s'", type_to_c(t));
    if (d->kind == DESIGNATOR_FIELD && t->kind == TYPE_ARRAY)
        fatal_error("field designator '.%s' in an initializer of '%s'", d->u.name, type_to_c(t));

    InitItem **s;
    if (t->kind == TYPE_ARRAY) {
        s = designate_index(t, node, d);
    } else {
        const FieldDef *members = structtab_find(t->u.struct_t.name)->members;
        const FieldDef *f       = members;
        s                       = &node->u.items;
        for (; f && strcmp(f->name, d->u.name) != 0; f = f->next) {
            if (t->kind == TYPE_STRUCT)
                s = &(*s)->next;
        }
        if (!f)
            fatal_error("no member named '%s' in '%s'", d->u.name, type_to_c(t));
        if (t->kind == TYPE_UNION) {
            // Another member's value does not survive a switch of member.
            const char *old = (*s)->designators ? (*s)->designators->u.name : members->name;
            if (strcmp(old, f->name) != 0)
                set_slot(&(*s)->init, NULL);
            free_designator((*s)->designators);
            (*s)->designators = NULL;
            if (f != members) {
                (*s)->designators         = new_designator(DESIGNATOR_FIELD);
                (*s)->designators->u.name = xstrdup(f->name);
            }
        }
        *field = f;
    }
    // The last designator's initializer replaces the subobject's earlier one outright;
    // one further up a chain refines it in place.
    if (!d->next)
        set_slot(&(*s)->init, NULL);
    item->designators = d->next;
    d->next           = NULL;
    free_designator(d);
    *slot = s;
}

// Fill the canonical node of aggregate t from the items at *cur.  When braced, the items
// are t's own brace list and a leftover is an excess element; otherwise (brace elision,
// or the rest of a designator chain) filling stops once t is full, or at a designator,
// and the enclosing level takes the rest.  When designated, the first item carries the
// rest of a designator chain for t.
static void fill(const Type *t, Initializer *node, InitItem **cur, bool braced, bool designated,
                 InitMode mode)
{
    InitItem **slot = &node->u.items;
    // The member of the slot at hand; NULL for an array.  A struct's canonical node has
    // one slot per member, so the member never runs out before the slots do.
    const FieldDef *field =
        t->kind == TYPE_ARRAY ? NULL : structtab_find(t->u.struct_t.name)->members;
    bool unsized = t->kind == TYPE_ARRAY && !t->u.array.size;

    while (*cur) {
        if ((*cur)->designators) {
            // A designator belongs to the innermost brace level.
            if (!braced && !designated)
                return;
            designated = false;
            designate(t, node, *cur, &slot, &field);
            if ((*cur)->designators) {
                // The chain goes on into the designated subobject, where initialization
                // then continues in order (§6.7.9p17).
                const Type *sub = unalias(t->kind == TYPE_ARRAY ? t->u.array.element : field->type);
                Initializer **sub_init = &(*slot)->init;
                if (!is_aggregate(sub))
                    fatal_error("designator in a scalar initializer");
                if (*sub_init && (*sub_init)->kind != INITIALIZER_COMPOUND)
                    fatal_error(
                        "a designator into a subobject initialized by an expression is "
                        "not supported");
                if (!*sub_init)
                    *sub_init = new_canonical(sub);
                fill(sub, *sub_init, cur, false, true, mode);
                slot = &(*slot)->next;
                if (t->kind != TYPE_ARRAY)
                    field = field->next;
                continue;
            }
        } else if (!*slot) {
            if (!unsized) {
                if (braced)
                    fatal_error("excess elements in %s initializer", aggregate_name(t));
                return;
            }
            *slot = new_init_item(NULL, NULL);
        } else if ((*slot)->designators) {
            // A positional union initializer is for the first member; drop another's.
            free_designator((*slot)->designators);
            (*slot)->designators = NULL;
            set_slot(&(*slot)->init, NULL);
        }
        const Type *sub = t->kind == TYPE_ARRAY ? t->u.array.element : field->type;
        place(sub, &(*slot)->init, cur, mode);
        slot = &(*slot)->next;
        if (t->kind != TYPE_ARRAY)
            field = field->next;
    }
}

// Initialize the subobject of type t, whose canonical slot is *slot, from the item at *cur.
static void place(const Type *t, Initializer **slot, InitItem **cur, InitMode mode)
{
    const Type *ut    = unalias(t);
    Initializer *init = (*cur)->init;

    if (init->kind == INITIALIZER_COMPOUND) {
        set_slot(slot, normalize_compound(t, take_item(cur), mode));
        return;
    }
    if (!is_aggregate(ut)) {
        set_slot(slot, check_leaf(take_item(cur), mode));
        return;
    }
    if (is_string_init(init)) {
        if (takes_string(ut)) {
            set_slot(slot, take_item(cur));
            return;
        }
    } else if (mode == INIT_AUTOMATIC && ut->kind != TYPE_ARRAY) {
        // A struct/union-valued expression initializes the whole subobject.
        check_leaf(init, mode);
        if (compatible_type(ut, init->u.expr->type)) {
            set_slot(slot, take_item(cur));
            return;
        }
    }
    // Brace elision: the item starts the subobject's own initializer list, continuing
    // an earlier brace-list initializer of it if there is one.
    if (*slot && (*slot)->kind != INITIALIZER_COMPOUND)
        set_slot(slot, NULL);
    if (!*slot)
        *slot = new_canonical(ut);
    fill(ut, *slot, cur, false, false, mode);
}

// Normalize a brace-enclosed initializer for type t; consumes init.
static Initializer *normalize_compound(const Type *t, Initializer *init, InitMode mode);

static Initializer *normalize_compound_at(const Type *t, Initializer *init, InitMode mode)
{
    const Type *ut  = unalias(t);
    InitItem *items = init->u.items;
    init->u.items   = NULL;
    free_initializer(init);

    if (!is_aggregate(ut)) {
        // A braced scalar (§6.7.9p11).
        if (!items)
            fatal_error("scalar initializer cannot be empty");
        if (items->next)
            fatal_error("excess elements in scalar initializer");
        Initializer *inner = take_item(&items);
        return inner->kind == INITIALIZER_COMPOUND ? normalize_compound(t, inner, mode)
                                                   : check_leaf(inner, mode);
    }
    if (takes_string(ut) && items && !items->next && !items->designators &&
        is_string_init(items->init)) {
        // A braced string for a char array (§6.7.9p14).
        return take_item(&items);
    }
    Initializer *node = new_canonical(ut);
    fill(ut, node, &items, true, false, mode);
    return node;
}

// normalize_compound with diag_loc at the node, for the errors found in it.
static Initializer *normalize_compound(const Type *t, Initializer *init, InitMode mode)
{
    SrcLoc saved        = diag_enter(init ? init->loc : diag_loc);
    Initializer *result = normalize_compound_at(t, init, mode);
    diag_loc            = saved;
    return result;
}

Initializer *normalize_init(Type *type, Initializer *init, InitMode mode)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    const Type *t = unalias(type);

    if (init->kind == INITIALIZER_SINGLE) {
        // A string for an array is checked by the consumer; it is never decayed here.
        if (t->kind == TYPE_ARRAY && is_string_init(init))
            return init;
        return check_leaf(init, mode);
    }
    // An unsized array gets its size from the initializer, in place: in type itself, never
    // in the typedef it may name, which other declarations share.
    bool unsized = type->kind == TYPE_ARRAY && !type->u.array.size;
    init         = normalize_compound(t, init, mode);
    if (unsized && init->kind == INITIALIZER_COMPOUND) {
        size_t n = 0;
        for (const InitItem *item = init->u.items; item; item = item->next)
            n++;
        set_array_size(type, n);
    }
    return init;
}
