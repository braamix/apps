#include <string.h>

#include "tac.h"

static void visit_vals(const Tac_Val *v, Tac_NameVisitor fn, void *arg)
{
    for (; v; v = v->next)
        if (v->kind == TAC_VAL_VAR && v->u.var_name)
            fn(v->u.var_name, arg);
}

static void visit_name(const char *name, Tac_NameVisitor fn, void *arg)
{
    if (name)
        fn(name, arg);
}

void tac_visit_names(const Tac_Instruction *in, Tac_NameVisitor fn, void *arg)
{
    switch (in->kind) {
    case TAC_INSTRUCTION_RETURN:
        visit_vals(in->u.return_.src, fn, arg);
        break;
    case TAC_INSTRUCTION_SIGN_EXTEND:
    case TAC_INSTRUCTION_TRUNCATE:
    case TAC_INSTRUCTION_ZERO_EXTEND:
    case TAC_INSTRUCTION_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_DOUBLE_TO_UINT:
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
    case TAC_INSTRUCTION_COPY:
    case TAC_INSTRUCTION_GET_ADDRESS:
    case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
    case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
        visit_vals(in->u.copy.src, fn, arg);
        visit_vals(in->u.copy.dst, fn, arg);
        break;
    case TAC_INSTRUCTION_UNARY:
        visit_vals(in->u.unary.src, fn, arg);
        visit_vals(in->u.unary.dst, fn, arg);
        break;
    case TAC_INSTRUCTION_BINARY:
        visit_vals(in->u.binary.src1, fn, arg);
        visit_vals(in->u.binary.src2, fn, arg);
        visit_vals(in->u.binary.dst, fn, arg);
        break;
    case TAC_INSTRUCTION_LOAD:
    case TAC_INSTRUCTION_LOAD_BYTE:
        visit_vals(in->u.load.src_ptr, fn, arg);
        visit_vals(in->u.load.dst, fn, arg);
        break;
    case TAC_INSTRUCTION_STORE:
    case TAC_INSTRUCTION_STORE_BYTE:
        visit_vals(in->u.store.src, fn, arg);
        visit_vals(in->u.store.dst_ptr, fn, arg);
        break;
    case TAC_INSTRUCTION_ADD_PTR:
        visit_vals(in->u.add_ptr.ptr, fn, arg);
        visit_vals(in->u.add_ptr.index, fn, arg);
        visit_vals(in->u.add_ptr.dst, fn, arg);
        break;
    case TAC_INSTRUCTION_PTR_DIFF:
        visit_vals(in->u.ptr_diff.ptr_a, fn, arg);
        visit_vals(in->u.ptr_diff.ptr_b, fn, arg);
        visit_vals(in->u.ptr_diff.dst, fn, arg);
        break;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET:
        visit_vals(in->u.copy_to_offset.src, fn, arg);
        visit_name(in->u.copy_to_offset.dst, fn, arg);
        break;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
        visit_name(in->u.copy_from_offset.src, fn, arg);
        visit_vals(in->u.copy_from_offset.dst, fn, arg);
        break;
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
        visit_vals(in->u.jump_if_zero.condition, fn, arg);
        break;
    case TAC_INSTRUCTION_JUMP_TABLE:
        visit_vals(in->u.jump_table.index, fn, arg);
        break;
    case TAC_INSTRUCTION_FUN_CALL:
    case TAC_INSTRUCTION_FUN_CALL_NORETURN:
        visit_name(in->u.fun_call.fun_name, fn, arg);
        visit_vals(in->u.fun_call.args, fn, arg);
        visit_vals(in->u.fun_call.dst, fn, arg);
        break;
    case TAC_INSTRUCTION_ALLOCATE_LOCAL:
        visit_name(in->u.allocate_local.name, fn, arg);
        break;
    case TAC_INSTRUCTION_JUMP:
    case TAC_INSTRUCTION_LABEL:
        break;
    }
}

// A tentative (no-init) top-level static variable is redundant when the same name has another
// top-level static variable that is a real definition (carries an init_list), or an earlier
// tentative of the same name (collapse repeated tentatives to the first).  The streaming
// frontend typechecks and translates one declaration at a time, so a tentative "static int
// foo;" and a later "static int foo = 4;" arrive as two separate toplevels; only one storage
// definition may reach the assembler (two strong labels of one name are a duplicate-symbol
// error).  Typecheck guarantees at most one initialized definition per
// name, so the winner is unambiguous.
bool tac_static_superseded(const Tac_TopLevel *program, const Tac_TopLevel *tl)
{
    if (tl->u.static_variable.init_list != NULL)
        return false; // a real definition is always emitted
    const char *name = tl->u.static_variable.name;
    bool seen_self   = false;
    for (const Tac_TopLevel *o = program; o; o = o->next) {
        if (o == tl) {
            seen_self = true;
            continue;
        }
        if (o->kind != TAC_TOPLEVEL_STATIC_VARIABLE)
            continue;
        if (strcmp(o->u.static_variable.name, name) != 0)
            continue;
        if (o->u.static_variable.init_list != NULL)
            return true; // a real definition wins over this tentative
        if (!seen_self)
            return true; // an earlier tentative of the same name wins
    }
    return false;
}
