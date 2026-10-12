//
// AST types to TAC types: the form the translator emits and the backends read.  Here
// rather than in the translator so the semantic pass can apply a target's ABI rule
// (tac_aapcs64_class) to a type, for __builtin_va_class.
//
#include "semantic.h"
#include "structtab.h"
#include "target.h"
#include "xalloc.h"

// The definition bound to a struct/union type node: the one typecheck cached on the
// node (a block-scope tag is purged from structtab by now, and a sibling scope may
// reuse the tag), else the tag's current one.  NULL when incomplete or unknown.
static const StructDef *struct_def_of(const Type *t)
{
    const StructDef *d = t->u.struct_t.cached_def;
    if (!d)
        d = structtab_find_opt(t->u.struct_t.name);
    return d && d->complete ? d : NULL;
}

// `deep`: list struct members.  Below a pointer a struct is shallow (tag, size and
// alignment only), which also ends the recursion of a self-referential struct.
static Tac_Type *convert_type(const Type *t, bool deep);

// Add member `name` (NULL for a bit-field storage unit) of `type` at byte `offset` to
// structure `ts`, keeping the list in offset order: storage units of bit-fields can come
// out of order, as each is the smallest that holds its field.
static void add_member(Tac_Type *ts, const char *name, int offset, Tac_Type *type)
{
    Tac_Member *m  = tac_new_member();
    m->name        = name ? xstrdup(name) : NULL;
    m->offset      = offset;
    m->type        = type;
    Tac_Member **p = &ts->u.structure.members;
    while (*p && (*p)->offset <= offset)
        p = &(*p)->next;
    m->next = *p;
    *p      = m;
}

// A bit-field's storage unit is a member of its own, an unsigned integer, once for each
// place and size: the unit is what the code loads and stores, and a backend sizes a
// constant stored into a structure by the member it finds there.  It also makes the ABI
// classifiers see bit-fields as integers.  With `per_field` it is listed once per
// bit-field it holds, for a classifier that counts them (Target.bitfield_unit_per_field).
static void add_unit_member(Tac_Type *ts, int offset, int size, bool per_field)
{
    Tac_Type *type;
    TypeKind k = unsigned_kind_of_size(size);
    if (k != TYPE_VOID) {
        Type ut = { .kind = k };
        type    = convert_type(&ut, true);
    } else {
        // An access unit of an odd size (clang's i24, i40...): its bytes.
        type                    = tac_new_type(TAC_TYPE_ARRAY);
        type->u.array.elem_type = tac_new_type(TAC_TYPE_UCHAR);
        type->u.array.size      = size;
    }
    for (const Tac_Member *m = ts->u.structure.members; m && !per_field; m = m->next) {
        if (!m->name && m->offset == offset && tac_compare_type(m->type, type)) {
            tac_free_type(type);
            return;
        }
    }
    add_member(ts, NULL, offset, type);
}

// An unnamed bit-field, for an ABI that sees it: its part of clang's access unit
// (Target.bitfield_access_bits), else its own storage unit, and for a `:0` a marker, an
// unnamed array of no bytes (Target.bitfield_unit_per_field).
static void add_unnamed(Tac_Type *ts, const FieldDef *u, bool access)
{
    if (access) {
        if (u->bf.width)
            add_unit_member(ts, u->access_offset, u->access_size, false);
    } else if (u->bf.width) {
        if (!u->bf.bytewise)
            add_unit_member(ts, u->offset, u->bf.unit_size, true);
    } else {
        Tac_Type *marker          = tac_new_type(TAC_TYPE_ARRAY);
        marker->u.array.elem_type = tac_new_type(TAC_TYPE_UCHAR);
        marker->u.array.size      = 0;
        add_member(ts, NULL, u->offset, marker);
    }
}

static Tac_Type *convert_type(const Type *t, bool deep)
{
    t = unalias(t); // global typedef names survive into the translator as references
    switch (t->kind) {
    case TYPE_VOID:
        return tac_new_type(TAC_TYPE_VOID);
    case TYPE_CHAR:
        // Plain `char` has target-defined signedness: it lowers to the signed-char TAC
        // kind on a signed-`char` target and the unsigned-char kind otherwise.  After
        // this boundary the TAC layer carries no ambiguous plain `char`.
        return tac_new_type((target_config && target_config->char_signed) ? TAC_TYPE_SCHAR
                                                                          : TAC_TYPE_UCHAR);
    case TYPE_SCHAR:
        return tac_new_type(TAC_TYPE_SCHAR);
    case TYPE_UCHAR:
        return tac_new_type(TAC_TYPE_UCHAR);
    case TYPE_SHORT:
        return tac_new_type(TAC_TYPE_SHORT);
    case TYPE_USHORT:
        return tac_new_type(TAC_TYPE_USHORT);
    case TYPE_BOOL:
        // There is no TAC _Bool kind: _Bool borrows the carrier of the integer kind of
        // its own width, exactly as TYPE_ENUM below borrows int's.  On a word-addressed
        // target that must not be a *char* kind — those mean byte-packed storage and fat
        // byte pointers in the BESM-6 backend (codegen_sizeof/is_char_array), which is
        // wrong for a type get_size gives a whole word.  The 0/1 invariant is maintained
        // by emit_cast, not by the carrier, so the carrier's signedness never shows; int
        // is the one that keeps `double d = b;` on the inline INT_TO_DOUBLE path.
        return tac_new_type(get_size(t) == 1 ? TAC_TYPE_UCHAR : TAC_TYPE_INT);
    case TYPE_INT:
        return tac_new_type(TAC_TYPE_INT);
    case TYPE_UINT:
        return tac_new_type(TAC_TYPE_UINT);
    case TYPE_LONG:
        return tac_new_type(TAC_TYPE_LONG);
    case TYPE_ULONG:
        return tac_new_type(TAC_TYPE_ULONG);
    case TYPE_LONG_LONG:
        return tac_new_type(TAC_TYPE_LONG_LONG);
    case TYPE_ULONG_LONG:
        return tac_new_type(TAC_TYPE_ULONG_LONG);
    case TYPE_FLOAT:
        return tac_new_type(TAC_TYPE_FLOAT);
    case TYPE_DOUBLE:
        return tac_new_type(TAC_TYPE_DOUBLE);
    case TYPE_LONG_DOUBLE:
        return tac_new_type(TAC_TYPE_LONG_DOUBLE);
    case TYPE_ENUM:
        return tac_new_type(TAC_TYPE_INT);
    case TYPE_POINTER: {
        Tac_Type *tp              = tac_new_type(TAC_TYPE_POINTER);
        tp->u.pointer.target_type = convert_type(t->u.pointer.target, false);
        return tp;
    }
    case TYPE_ARRAY: {
        Tac_Type *ta          = tac_new_type(TAC_TYPE_ARRAY);
        ta->u.array.elem_type = convert_type(t->u.array.element, deep);
        if (t->u.array.size) {
            ta->u.array.size = (int)(get_size(t) / get_size(t->u.array.element));
        } else {
            ta->u.array.size = 0; // incomplete array type (e.g. extern int arr[])
        }
        return ta;
    }
    case TYPE_FUNCTION: {
        Tac_Type *tf          = tac_new_type(TAC_TYPE_FUN_TYPE);
        Tac_Type **param_tail = &tf->u.fun_type.param_types;
        for (const Param *p = t->u.function.params; p; p = p->next) {
            if (!p->name && unalias(p->type)->kind == TYPE_VOID)
                continue; // skip void sentinel
            Tac_Type *pt = convert_type(p->type, deep);
            *param_tail  = pt;
            param_tail   = &pt->next;
        }
        tf->u.fun_type.ret_type = convert_type(t->u.function.return_type, deep);
        tf->u.fun_type.variadic = t->u.function.variadic;
        return tf;
    }
    case TYPE_STRUCT:
    case TYPE_UNION: {
        // An incomplete type (an extern of an undefined tag) has size 0.
        const StructDef *d        = struct_def_of(t);
        Tac_Type *ts              = tac_new_type(TAC_TYPE_STRUCTURE);
        ts->u.structure.tag       = t->u.struct_t.name ? xstrdup(t->u.struct_t.name) : NULL;
        ts->u.structure.size      = d ? d->size : t->u.struct_t.cached_size;
        ts->u.structure.alignment = d ? d->alignment : t->u.struct_t.cached_align;
        ts->u.structure.is_union  = t->kind == TYPE_UNION;
        if (deep && d) {
            bool access       = target_config && target_config->bitfield_access_bits;
            bool per_field    = target_config && target_config->bitfield_unit_per_field;
            const FieldDef *u = d->unnamed; // merged in declaration order
            for (const FieldDef *f = d->members;; f = f->next) {
                for (; u && (!f || u->index < f->index); u = u->next)
                    add_unnamed(ts, u, access);
                if (!f)
                    break;
                if (!f->bf.width)
                    add_member(ts, f->name, f->offset, convert_type(f->type, true));
                else if (access)
                    add_unit_member(ts, f->access_offset, f->access_size, false);
                else if (!f->bf.bytewise)
                    add_unit_member(ts, f->offset, f->bf.unit_size, per_field);
            }
        }
        return ts;
    }
    default:
        internal_error("ast_type_to_tac_type: unsupported type kind %d", (int)t->kind);
    }
}

Tac_Type *ast_type_to_tac_type(const Type *t)
{
    return convert_type(t, true);
}

int va_class_of(const Type *t)
{
    if (!target_config->va_class)
        fatal_error("'__builtin_va_class' is not supported on target '%s'", target_config->name);
    Tac_Type *tt = ast_type_to_tac_type(t);
    int c        = target_config->va_class(tt);
    tac_free_type(tt);
    return c;
}
