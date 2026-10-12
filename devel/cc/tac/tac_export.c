#include <stdlib.h>

#include "tac.h"
#include "tags.h"
#include "wio.h"

int export_tac_debug;

static void export_type(WFILE *out, const Tac_Type *t);
static void export_static_init(WFILE *out, const Tac_StaticInit *si);

static void export_const(WFILE *out, const Tac_Const *c)
{
    if (!c) {
        wputw(TAG_EOL, out);
        return;
    }
    wputw(TAG_TAC_CONST + c->kind, out);
    switch (c->kind) {
    case TAC_CONST_INT:
        wputw((size_t)(uint64_t)c->u.int_val, out);
        break;
    case TAC_CONST_LONG:
        wputw((size_t)c->u.long_val, out);
        break;
    case TAC_CONST_LONG_LONG:
        wputw((size_t)(unsigned long long)c->u.long_long_val, out);
        break;
    case TAC_CONST_UINT:
        wputw((size_t)c->u.uint_val, out);
        break;
    case TAC_CONST_ULONG:
        wputw((size_t)c->u.ulong_val, out);
        break;
    case TAC_CONST_ULONG_LONG:
        wputw((size_t)c->u.ulong_long_val, out);
        break;
    case TAC_CONST_FLOAT:
        wputd(c->u.float_val, out);
        break;
    case TAC_CONST_DOUBLE:
        wputd(c->u.double_val, out);
        break;
    case TAC_CONST_LONG_DOUBLE:
        wputf128(c->u.long_double_val, out);
        break;
    case TAC_CONST_SCHAR:
        wputw((unsigned)c->u.char_val, out);
        break;
    case TAC_CONST_UCHAR:
        wputw((unsigned)c->u.uchar_val, out);
        break;
    default:
        break;
    }
}

static void export_val(WFILE *out, const Tac_Val *v)
{
    if (!v) {
        wputw(TAG_EOL, out);
        return;
    }
    wputw(TAG_TAC_VAL + v->kind, out);
    if (v->kind == TAC_VAL_CONSTANT) {
        export_const(out, v->u.constant);
    } else {
        wputstr(v->u.var_name ? v->u.var_name : "", out);
    }
    export_val(out, v->next);
}

static void export_instr(WFILE *out, const Tac_Instruction *instr)
{
    if (!instr) {
        wputw(TAG_EOL, out);
        return;
    }
    {
        size_t tag = TAG_TAC_INSTR + instr->kind;
        if (instr->is_volatile)
            tag |= TAG_INSTR_VOLATILE;
        wputw(tag, out);
    }
    switch (instr->kind) {
    case TAC_INSTRUCTION_RETURN:
        export_val(out, instr->u.return_.src);
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
        export_val(out, instr->u.sign_extend.src);
        export_val(out, instr->u.sign_extend.dst);
        break;
    case TAC_INSTRUCTION_UNARY:
        wputw((size_t)instr->u.unary.op, out);
        export_val(out, instr->u.unary.src);
        export_val(out, instr->u.unary.dst);
        break;
    case TAC_INSTRUCTION_BINARY:
        wputw((size_t)instr->u.binary.op, out);
        export_val(out, instr->u.binary.src1);
        export_val(out, instr->u.binary.src2);
        export_val(out, instr->u.binary.dst);
        break;
    case TAC_INSTRUCTION_COPY:
        export_val(out, instr->u.copy.src);
        export_val(out, instr->u.copy.dst);
        break;
    case TAC_INSTRUCTION_GET_ADDRESS:
    case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
    case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
        export_val(out, instr->u.get_address.src);
        export_val(out, instr->u.get_address.dst);
        break;
    case TAC_INSTRUCTION_LOAD:
    case TAC_INSTRUCTION_LOAD_BYTE:
        export_val(out, instr->u.load.src_ptr);
        export_val(out, instr->u.load.dst);
        break;
    case TAC_INSTRUCTION_STORE:
    case TAC_INSTRUCTION_STORE_BYTE:
        export_val(out, instr->u.store.src);
        export_val(out, instr->u.store.dst_ptr);
        break;
    case TAC_INSTRUCTION_ADD_PTR:
        export_val(out, instr->u.add_ptr.ptr);
        export_val(out, instr->u.add_ptr.index);
        wputw((size_t)instr->u.add_ptr.scale, out);
        export_val(out, instr->u.add_ptr.dst);
        break;
    case TAC_INSTRUCTION_PTR_DIFF:
        export_val(out, instr->u.ptr_diff.ptr_a);
        export_val(out, instr->u.ptr_diff.ptr_b);
        export_val(out, instr->u.ptr_diff.dst);
        break;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET:
        export_val(out, instr->u.copy_to_offset.src);
        wputstr(instr->u.copy_to_offset.dst ? instr->u.copy_to_offset.dst : "", out);
        wputw((size_t)instr->u.copy_to_offset.offset, out);
        break;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
        wputstr(instr->u.copy_from_offset.src ? instr->u.copy_from_offset.src : "", out);
        wputw((size_t)instr->u.copy_from_offset.offset, out);
        export_val(out, instr->u.copy_from_offset.dst);
        break;
    case TAC_INSTRUCTION_JUMP:
        wputstr(instr->u.jump.target ? instr->u.jump.target : "", out);
        break;
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
        export_val(out, instr->u.jump_if_zero.condition);
        wputstr(instr->u.jump_if_zero.target ? instr->u.jump_if_zero.target : "", out);
        break;
    case TAC_INSTRUCTION_LABEL:
        wputstr(instr->u.label.name ? instr->u.label.name : "", out);
        break;
    case TAC_INSTRUCTION_JUMP_TABLE:
        export_val(out, instr->u.jump_table.index);
        wputw((size_t)instr->u.jump_table.count, out);
        for (int i = 0; i < instr->u.jump_table.count; i++)
            wputstr(instr->u.jump_table.targets[i], out);
        wputstr(instr->u.jump_table.default_target, out);
        break;
    case TAC_INSTRUCTION_FUN_CALL:
    case TAC_INSTRUCTION_FUN_CALL_NORETURN:
        wputstr(instr->u.fun_call.fun_name ? instr->u.fun_call.fun_name : "", out);
        wputw(instr->u.fun_call.indirect ? 1 : 0, out);
        export_val(out, instr->u.fun_call.args);
        export_val(out, instr->u.fun_call.dst);
        export_type(out, instr->u.fun_call.fun_type);
        break;
    case TAC_INSTRUCTION_ALLOCATE_LOCAL:
        wputstr(instr->u.allocate_local.name ? instr->u.allocate_local.name : "", out);
        wputw((size_t)instr->u.allocate_local.size, out);
        wputw((size_t)instr->u.allocate_local.alignment, out);
        break;
    default:
        break;
    }
    export_instr(out, instr->next);
}

static void export_param(WFILE *out, const Tac_Param *p)
{
    if (!p) {
        wputw(TAG_EOL, out);
        return;
    }
    wputw(TAG_TAC_PARAM, out);
    wputstr(p->name ? p->name : "", out);
    export_type(out, p->type);
    export_param(out, p->next);
}

static void export_static_local(WFILE *out, const Tac_StaticLocal *sl)
{
    if (!sl) {
        wputw(TAG_EOL, out);
        return;
    }
    wputw(TAG_TAC_STATIC_LOC, out);
    wputstr(sl->name ? sl->name : "", out);
    export_type(out, sl->type);
    export_static_init(out, sl->init_list);
    wputw((size_t)sl->alignment, out);
    export_static_local(out, sl->next);
}

void tac_export_toplevel(WFILE *out, const Tac_TopLevel *tl)
{
    if (!tl) {
        wputw(TAG_EOL, out);
        return;
    }
    wputw(TAG_TAC_TOPLEVEL + tl->kind, out);
    switch (tl->kind) {
    case TAC_TOPLEVEL_FUNCTION:
        wputstr(tl->u.function.name ? tl->u.function.name : "", out);
        wputw(tl->u.function.global ? 1 : 0, out);
        wputw(tl->u.function.variadic ? 1 : 0, out);
        wputw(tl->u.function.noret ? 1 : 0, out);
        export_type(out, tl->u.function.type);
        export_param(out, tl->u.function.params);
        export_param(out, tl->u.function.locals);
        export_static_local(out, tl->u.function.static_locals);
        export_instr(out, tl->u.function.body);
        break;
    case TAC_TOPLEVEL_STATIC_VARIABLE:
        wputstr(tl->u.static_variable.name ? tl->u.static_variable.name : "", out);
        wputw(tl->u.static_variable.global ? 1 : 0, out);
        export_type(out, tl->u.static_variable.type);
        export_static_init(out, tl->u.static_variable.init_list);
        wputw((size_t)tl->u.static_variable.alignment, out);
        break;
    case TAC_TOPLEVEL_STATIC_CONSTANT:
        wputstr(tl->u.static_constant.name ? tl->u.static_constant.name : "", out);
        export_type(out, tl->u.static_constant.type);
        export_static_init(out, tl->u.static_constant.init);
        break;
    case TAC_TOPLEVEL_EXTERN:
        wputstr(tl->u.extern_.name ? tl->u.extern_.name : "", out);
        export_type(out, tl->u.extern_.type);
        break;
    default:
        break;
    }
}

void tac_export_begin_stream(WFILE *out)
{
    wputw(TAG_TAC_MAGIC, out);
}

void tac_export_end_stream(WFILE *out)
{
    wputw(TAG_EOL, out);
    wflush(out);
}

static void export_type(WFILE *out, const Tac_Type *t)
{
    if (!t) {
        wputw(TAG_EOL, out);
        return;
    }
    wputw(TAG_TAC_TYPE + t->kind, out);
    switch (t->kind) {
    case TAC_TYPE_SCHAR:
    case TAC_TYPE_UCHAR:
    case TAC_TYPE_SHORT:
    case TAC_TYPE_INT:
    case TAC_TYPE_LONG:
    case TAC_TYPE_LONG_LONG:
    case TAC_TYPE_USHORT:
    case TAC_TYPE_UINT:
    case TAC_TYPE_ULONG:
    case TAC_TYPE_ULONG_LONG:
    case TAC_TYPE_FLOAT:
    case TAC_TYPE_DOUBLE:
    case TAC_TYPE_LONG_DOUBLE:
    case TAC_TYPE_VOID:
        break;
    case TAC_TYPE_FUN_TYPE:
        export_type(out, t->u.fun_type.param_types);
        export_type(out, t->u.fun_type.ret_type);
        wputw(t->u.fun_type.variadic ? 1 : 0, out);
        break;
    case TAC_TYPE_POINTER:
        export_type(out, t->u.pointer.target_type);
        break;
    case TAC_TYPE_ARRAY:
        export_type(out, t->u.array.elem_type);
        wputw((size_t)t->u.array.size, out);
        break;
    case TAC_TYPE_STRUCTURE:
        wputstr(t->u.structure.tag ? t->u.structure.tag : "", out);
        wputw((size_t)t->u.structure.size, out);
        wputw((size_t)t->u.structure.alignment, out);
        wputw(t->u.structure.is_union ? 1 : 0, out);
        for (const Tac_Member *m = t->u.structure.members; m; m = m->next) {
            wputw(TAG_TAC_MEMBER, out);
            wputstr(m->name ? m->name : "", out);
            wputw((size_t)m->offset, out);
            export_type(out, m->type);
        }
        wputw(TAG_EOL, out);
        break;
    default:
        break;
    }
    export_type(out, t->next);
}

static void export_static_init(WFILE *out, const Tac_StaticInit *si)
{
    if (!si) {
        wputw(TAG_EOL, out);
        return;
    }
    wputw(TAG_TAC_STATIC_INIT + si->kind, out);
    switch (si->kind) {
    case TAC_STATIC_INIT_I8:
        wputw((size_t)(unsigned char)si->u.char_val, out);
        break;
    case TAC_STATIC_INIT_I16:
        wputw((size_t)(uint16_t)si->u.short_val, out);
        break;
    case TAC_STATIC_INIT_I32:
        wputw((size_t)(unsigned int)si->u.int_val, out);
        break;
    case TAC_STATIC_INIT_I64:
        wputw((size_t)(unsigned long long)si->u.long_val, out);
        break;
    case TAC_STATIC_INIT_U8:
        wputw((size_t)si->u.uchar_val, out);
        break;
    case TAC_STATIC_INIT_U16:
        wputw((size_t)si->u.ushort_val, out);
        break;
    case TAC_STATIC_INIT_U32:
        wputw((size_t)si->u.uint_val, out);
        break;
    case TAC_STATIC_INIT_U64:
        wputw((size_t)si->u.ulong_val, out);
        break;
    case TAC_STATIC_INIT_FLOAT:
        wputd(si->u.float_val, out);
        break;
    case TAC_STATIC_INIT_DOUBLE:
        wputd(si->u.double_val, out);
        break;
    case TAC_STATIC_INIT_LONG_DOUBLE:
        wputf128(si->u.long_double_val, out);
        break;
    case TAC_STATIC_INIT_ZERO:
        wputw((size_t)si->u.zero_bytes, out);
        break;
    case TAC_STATIC_INIT_STRING:
        // A length-prefixed blob, not wputstr: the decoded bytes may include NULs.
        wputdata(si->u.string.val ? si->u.string.val : "", si->u.string.val ? si->u.string.len : 0,
                 out);
        wputw(si->u.string.null_terminated ? 1 : 0, out);
        break;
    case TAC_STATIC_INIT_POINTER:
        wputstr(si->u.pointer.name ? si->u.pointer.name : "", out);
        wputw(si->u.pointer.byte_offset, out);
        break;
    case TAC_STATIC_INIT_FAT_POINTER:
        wputstr(si->u.pointer.name ? si->u.pointer.name : "", out);
        wputw(si->u.pointer.byte_offset, out);
        break;
    default:
        break;
    }
    export_static_init(out, si->next);
}

void tac_export_program(WFILE *out, const Tac_Program *prog)
{
    tac_export_begin_stream(out);
    if (prog) {
        for (const Tac_TopLevel *tl = prog->decls; tl; tl = tl->next)
            tac_export_toplevel(out, tl);
    }
    tac_export_end_stream(out);
}
