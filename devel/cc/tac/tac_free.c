#include <stdlib.h>

#include "tac.h"
#include "xalloc.h"

void tac_free_program(Tac_Program *program)
{
    if (program == NULL)
        return;
    tac_free_toplevel(program->decls);
    xfree(program);
}

// Free a Tac_Const and its contents
void tac_free_const(Tac_Const *constant)
{
    if (!constant)
        return;
    xfree(constant);
}

// Free a Tac_Val and its contents recursively
void tac_free_val(Tac_Val *val)
{
    if (!val)
        return;
    if (val->kind == TAC_VAL_CONSTANT) {
        tac_free_const(val->u.constant);
    } else if (val->u.var_name) {
        xfree(val->u.var_name);
    }
    tac_free_val(val->next);
    xfree(val);
}

// Free a Tac_Instruction and its contents recursively
void tac_free_instruction(Tac_Instruction *instr)
{
    if (!instr)
        return;
    switch (instr->kind) {
    case TAC_INSTRUCTION_RETURN:
        tac_free_val(instr->u.return_.src);
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
        tac_free_val(instr->u.sign_extend.src);
        tac_free_val(instr->u.sign_extend.dst);
        break;
    case TAC_INSTRUCTION_UNARY:
        tac_free_val(instr->u.unary.src);
        tac_free_val(instr->u.unary.dst);
        break;
    case TAC_INSTRUCTION_BINARY:
        tac_free_val(instr->u.binary.src1);
        tac_free_val(instr->u.binary.src2);
        tac_free_val(instr->u.binary.dst);
        break;
    case TAC_INSTRUCTION_COPY:
    case TAC_INSTRUCTION_GET_ADDRESS:
    case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
    case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
        tac_free_val(instr->u.copy.src);
        tac_free_val(instr->u.copy.dst);
        break;
    case TAC_INSTRUCTION_LOAD:
    case TAC_INSTRUCTION_LOAD_BYTE:
        tac_free_val(instr->u.load.src_ptr);
        tac_free_val(instr->u.load.dst);
        break;
    case TAC_INSTRUCTION_STORE:
    case TAC_INSTRUCTION_STORE_BYTE:
        tac_free_val(instr->u.store.src);
        tac_free_val(instr->u.store.dst_ptr);
        break;
    case TAC_INSTRUCTION_ADD_PTR:
        tac_free_val(instr->u.add_ptr.ptr);
        tac_free_val(instr->u.add_ptr.index);
        tac_free_val(instr->u.add_ptr.dst);
        break;
    case TAC_INSTRUCTION_PTR_DIFF:
        tac_free_val(instr->u.ptr_diff.ptr_a);
        tac_free_val(instr->u.ptr_diff.ptr_b);
        tac_free_val(instr->u.ptr_diff.dst);
        break;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET:
        tac_free_val(instr->u.copy_to_offset.src);
        if (instr->u.copy_to_offset.dst) {
            xfree(instr->u.copy_to_offset.dst);
        }
        break;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
        if (instr->u.copy_from_offset.src) {
            xfree(instr->u.copy_from_offset.src);
        }
        tac_free_val(instr->u.copy_from_offset.dst);
        break;
    case TAC_INSTRUCTION_JUMP:
        if (instr->u.jump.target) {
            xfree(instr->u.jump.target);
        }
        break;
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
        tac_free_val(instr->u.jump_if_zero.condition);
        if (instr->u.jump_if_zero.target) {
            xfree(instr->u.jump_if_zero.target);
        }
        break;
    case TAC_INSTRUCTION_JUMP_TABLE:
        tac_free_val(instr->u.jump_table.index);
        for (int i = 0; i < instr->u.jump_table.count; i++)
            xfree(instr->u.jump_table.targets[i]);
        xfree(instr->u.jump_table.targets);
        xfree(instr->u.jump_table.default_target);
        break;
    case TAC_INSTRUCTION_LABEL:
        if (instr->u.label.name) {
            xfree(instr->u.label.name);
        }
        break;
    case TAC_INSTRUCTION_FUN_CALL:
    case TAC_INSTRUCTION_FUN_CALL_NORETURN:
        if (instr->u.fun_call.fun_name) {
            xfree(instr->u.fun_call.fun_name);
        }
        tac_free_val(instr->u.fun_call.args);
        tac_free_val(instr->u.fun_call.dst);
        tac_free_type(instr->u.fun_call.fun_type);
        break;
    case TAC_INSTRUCTION_ALLOCATE_LOCAL:
        if (instr->u.allocate_local.name) {
            xfree(instr->u.allocate_local.name);
        }
        break;
    }
    tac_free_instruction(instr->next);
    xfree(instr);
}

// Free a Tac_Type and its contents recursively
void tac_free_type(Tac_Type *type)
{
    if (!type)
        return;
    switch (type->kind) {
    case TAC_TYPE_FUN_TYPE:
        tac_free_type(type->u.fun_type.param_types);
        tac_free_type(type->u.fun_type.ret_type);
        break;
    case TAC_TYPE_POINTER:
        tac_free_type(type->u.pointer.target_type);
        break;
    case TAC_TYPE_ARRAY:
        tac_free_type(type->u.array.elem_type);
        break;
    case TAC_TYPE_STRUCTURE:
        if (type->u.structure.tag) {
            xfree(type->u.structure.tag);
        }
        tac_free_member(type->u.structure.members);
        break;
    default:
        break;
    }
    tac_free_type(type->next);
    xfree(type);
}

// Free a Tac_Param and its contents recursively
void tac_free_param(Tac_Param *param)
{
    if (!param)
        return;
    if (param->name) {
        xfree(param->name);
    }
    tac_free_type(param->type);
    tac_free_param(param->next);
    xfree(param);
}

// Free a Tac_Member list and its contents
void tac_free_member(Tac_Member *member)
{
    while (member) {
        Tac_Member *next = member->next;
        if (member->name)
            xfree(member->name);
        tac_free_type(member->type);
        xfree(member);
        member = next;
    }
}

// Free a Tac_StaticLocal list and its contents recursively
void tac_free_static_local(Tac_StaticLocal *sl)
{
    if (!sl)
        return;
    if (sl->name) {
        xfree(sl->name);
    }
    tac_free_type(sl->type);
    tac_free_static_init(sl->init_list);
    tac_free_static_local(sl->next);
    xfree(sl);
}

// Free a Tac_StaticInit and its contents recursively
void tac_free_static_init(Tac_StaticInit *init)
{
    if (!init)
        return;
    if (init->kind == TAC_STATIC_INIT_STRING && init->u.string.val) {
        xfree(init->u.string.val);
    }
    if ((init->kind == TAC_STATIC_INIT_POINTER || init->kind == TAC_STATIC_INIT_FAT_POINTER) &&
        init->u.pointer.name) {
        xfree(init->u.pointer.name);
    }
    tac_free_static_init(init->next);
    xfree(init);
}

// Free a Tac_TopLevel and its contents recursively
void tac_free_toplevel(Tac_TopLevel *toplevel)
{
    if (!toplevel)
        return;
    switch (toplevel->kind) {
    case TAC_TOPLEVEL_FUNCTION:
        if (toplevel->u.function.name) {
            xfree(toplevel->u.function.name);
        }
        tac_free_type(toplevel->u.function.type);
        tac_free_param(toplevel->u.function.params);
        tac_free_param(toplevel->u.function.locals);
        tac_free_static_local(toplevel->u.function.static_locals);
        tac_free_instruction(toplevel->u.function.body);
        break;
    case TAC_TOPLEVEL_STATIC_VARIABLE:
        if (toplevel->u.static_variable.name) {
            xfree(toplevel->u.static_variable.name);
        }
        tac_free_type(toplevel->u.static_variable.type);
        tac_free_static_init(toplevel->u.static_variable.init_list);
        break;
    case TAC_TOPLEVEL_STATIC_CONSTANT:
        if (toplevel->u.static_constant.name) {
            xfree(toplevel->u.static_constant.name);
        }
        tac_free_type(toplevel->u.static_constant.type);
        tac_free_static_init(toplevel->u.static_constant.init);
        break;
    case TAC_TOPLEVEL_EXTERN:
        if (toplevel->u.extern_.name) {
            xfree(toplevel->u.extern_.name);
        }
        tac_free_type(toplevel->u.extern_.type);
        break;
    }
    tac_free_toplevel(toplevel->next);
    xfree(toplevel);
}
