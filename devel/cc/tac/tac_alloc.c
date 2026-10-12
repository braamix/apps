#include <stdlib.h>

#include "tac.h"
#include "xalloc.h"

// Allocate a new Tac_Val with the specified kind
Tac_Val *tac_new_val(Tac_ValKind kind)
{
    Tac_Val *val = (Tac_Val *)xalloc(sizeof(Tac_Val), __func__, __FILE__, __LINE__);
    val->kind    = kind;
    return val;
}

// Allocate a new Tac_Instruction with the specified kind
Tac_Instruction *tac_new_instruction(Tac_InstructionKind kind)
{
    Tac_Instruction *instr =
        (Tac_Instruction *)xalloc(sizeof(Tac_Instruction), __func__, __FILE__, __LINE__);
    instr->kind = kind;
    // Integer-width and floating→integer conversions default to "no destination kind supplied"
    // (-1); the calloc above would otherwise leave dst_kind == 0 (TAC_CONST_INT, a valid kind),
    // which would change folding for conversions built without an explicit dst_kind
    // (e.g. unit-test fixtures and imported TAC). emit_cast overrides it with the real
    // destination kind.
    switch (kind) {
    case TAC_INSTRUCTION_SIGN_EXTEND:
    case TAC_INSTRUCTION_TRUNCATE:
    case TAC_INSTRUCTION_ZERO_EXTEND:
    case TAC_INSTRUCTION_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_DOUBLE_TO_UINT:
    case TAC_INSTRUCTION_FLOAT_TO_INT:
    case TAC_INSTRUCTION_FLOAT_TO_UINT:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_UINT:
        instr->u.sign_extend.dst_kind = TAC_DST_KIND_UNKNOWN;
        break;
    default:
        break;
    }
    return instr;
}

// Allocate a new Tac_Type with the specified kind
Tac_Type *tac_new_type(Tac_TypeKind kind)
{
    Tac_Type *type = (Tac_Type *)xalloc(sizeof(Tac_Type), __func__, __FILE__, __LINE__);
    type->kind     = kind;
    return type;
}

Tac_Type *tac_clone_type(const Tac_Type *type)
{
    if (!type)
        return NULL;
    Tac_Type *t = tac_new_type(type->kind);
    switch (type->kind) {
    case TAC_TYPE_FUN_TYPE:
        t->u.fun_type.param_types = tac_clone_type(type->u.fun_type.param_types);
        t->u.fun_type.ret_type    = tac_clone_type(type->u.fun_type.ret_type);
        t->u.fun_type.variadic    = type->u.fun_type.variadic;
        break;
    case TAC_TYPE_POINTER:
        t->u.pointer.target_type = tac_clone_type(type->u.pointer.target_type);
        break;
    case TAC_TYPE_ARRAY:
        t->u.array.elem_type = tac_clone_type(type->u.array.elem_type);
        t->u.array.size      = type->u.array.size;
        break;
    case TAC_TYPE_STRUCTURE: {
        t->u.structure.tag       = type->u.structure.tag ? xstrdup(type->u.structure.tag) : NULL;
        t->u.structure.size      = type->u.structure.size;
        t->u.structure.alignment = type->u.structure.alignment;
        t->u.structure.is_union  = type->u.structure.is_union;
        Tac_Member **tail        = &t->u.structure.members;
        for (const Tac_Member *m = type->u.structure.members; m; m = m->next) {
            Tac_Member *nm = tac_new_member();
            nm->name       = m->name ? xstrdup(m->name) : NULL;
            nm->offset     = m->offset;
            nm->type       = tac_clone_type(m->type);
            *tail          = nm;
            tail           = &nm->next;
        }
        break;
    }
    default:
        break;
    }
    t->next = tac_clone_type(type->next);
    return t;
}

// Allocate a new Tac_Const with the specified kind
Tac_Const *tac_new_const(Tac_ConstKind kind)
{
    Tac_Const *constant = (Tac_Const *)xalloc(sizeof(Tac_Const), __func__, __FILE__, __LINE__);
    constant->kind      = kind;
    return constant;
}

// Allocate a new Tac_Param
Tac_Param *tac_new_param(void)
{
    Tac_Param *param = (Tac_Param *)xalloc(sizeof(Tac_Param), __func__, __FILE__, __LINE__);
    return param;
}

// Allocate a new Tac_Member
Tac_Member *tac_new_member(void)
{
    return (Tac_Member *)xalloc(sizeof(Tac_Member), __func__, __FILE__, __LINE__);
}

// Allocate a new Tac_StaticLocal
Tac_StaticLocal *tac_new_static_local(void)
{
    return (Tac_StaticLocal *)xalloc(sizeof(Tac_StaticLocal), __func__, __FILE__, __LINE__);
}

// Allocate a new Tac_TopLevel with the specified kind
Tac_TopLevel *tac_new_toplevel(Tac_TopLevelKind kind)
{
    Tac_TopLevel *toplevel =
        (Tac_TopLevel *)xalloc(sizeof(Tac_TopLevel), __func__, __FILE__, __LINE__);
    toplevel->kind = kind;
    return toplevel;
}

// Allocate a new Tac_StaticInit with the specified kind
Tac_StaticInit *tac_new_static_init(Tac_StaticInitKind kind)
{
    Tac_StaticInit *init =
        (Tac_StaticInit *)xalloc(sizeof(Tac_StaticInit), __func__, __FILE__, __LINE__);
    init->kind = kind;
    return init;
}

Tac_Program *tac_new_program()
{
    Tac_Program *p = xalloc(sizeof(Tac_Program), __func__, __FILE__, __LINE__);
    return p;
}
