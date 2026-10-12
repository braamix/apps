#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "srcloc.h"
#include "tac.h"
#include "tags.h"
#include "wio.h"
#include "xalloc.h"

//
// The input is not TAC this compiler wrote: another kind of file, or one cut short.
//
static _Noreturn void bad_input(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    diag_print_prefix(stderr, diag_loc, "error");
    fprintf(stderr, "the input is not TAC from this compiler: ");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(1);
}

static void check_input(const WFILE *in, const char *ctx)
{
    if (weof(in)) {
        bad_input("it ends while reading the %s", ctx);
    }
    if (werror(in)) {
        bad_input("read error in the %s", ctx);
    }
}

static Tac_Const *import_const(WFILE *in)
{
    size_t tag = wgetw(in);
    check_input(in, "const tag");
    if (tag < TAG_TAC_CONST || tag > TAG_TAC_CONST + TAC_CONST_UCHAR)
        return NULL;
    Tac_Const *c = tac_new_const((Tac_ConstKind)(tag - TAG_TAC_CONST));
    switch (c->kind) {
    case TAC_CONST_INT:
        c->u.int_val = (int64_t)wgetw(in);
        check_input(in, "const int");
        break;
    case TAC_CONST_LONG:
        c->u.long_val = (long)wgetw(in);
        check_input(in, "const long");
        break;
    case TAC_CONST_LONG_LONG:
        c->u.long_long_val = (long long)wgetw(in);
        check_input(in, "const long long");
        break;
    case TAC_CONST_UINT:
        c->u.uint_val = (uint64_t)wgetw(in);
        check_input(in, "const uint");
        break;
    case TAC_CONST_ULONG:
        c->u.ulong_val = (unsigned long)wgetw(in);
        check_input(in, "const ulong");
        break;
    case TAC_CONST_ULONG_LONG:
        c->u.ulong_long_val = (unsigned long long)wgetw(in);
        check_input(in, "const ulong_long");
        break;
    case TAC_CONST_FLOAT:
        c->u.float_val = wgetd(in);
        check_input(in, "const float");
        break;
    case TAC_CONST_DOUBLE:
        c->u.double_val = wgetd(in);
        check_input(in, "const double");
        break;
    case TAC_CONST_LONG_DOUBLE:
        c->u.long_double_val = wgetf128(in);
        check_input(in, "const long double");
        break;
    case TAC_CONST_SCHAR:
        c->u.char_val = (int)wgetw(in);
        check_input(in, "const char");
        break;
    case TAC_CONST_UCHAR:
        c->u.uchar_val = (unsigned char)wgetw(in);
        check_input(in, "const uchar");
        break;
    default:
        break;
    }
    return c;
}

static Tac_Val *import_val(WFILE *in)
{
    size_t tag = wgetw(in);
    check_input(in, "val tag");
    if (tag < TAG_TAC_VAL || tag > TAG_TAC_VAL + TAC_VAL_VAR)
        return NULL;
    Tac_Val *v = tac_new_val((Tac_ValKind)(tag - TAG_TAC_VAL));
    if (v->kind == TAC_VAL_CONSTANT) {
        v->u.constant = import_const(in);
    } else {
        v->u.var_name = wgetstr(in);
        check_input(in, "val var_name");
    }
    v->next = import_val(in);
    return v;
}

static Tac_Type *import_type(WFILE *in);

static Tac_Type *import_type(WFILE *in)
{
    size_t tag = wgetw(in);
    check_input(in, "type tag");
    if (tag < TAG_TAC_TYPE || tag > TAG_TAC_TYPE + TAC_TYPE_STRUCTURE)
        return NULL;
    Tac_Type *t = tac_new_type((Tac_TypeKind)(tag - TAG_TAC_TYPE));
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
        t->u.fun_type.param_types = import_type(in);
        t->u.fun_type.ret_type    = import_type(in);
        t->u.fun_type.variadic    = (bool)wgetw(in);
        check_input(in, "fun_type variadic");
        break;
    case TAC_TYPE_POINTER:
        t->u.pointer.target_type = import_type(in);
        break;
    case TAC_TYPE_ARRAY:
        t->u.array.elem_type = import_type(in);
        t->u.array.size      = (int)wgetw(in);
        check_input(in, "array size");
        break;
    case TAC_TYPE_STRUCTURE:
        t->u.structure.tag = wgetstr(in);
        check_input(in, "structure tag");
        t->u.structure.size = (int)wgetw(in);
        check_input(in, "structure size");
        t->u.structure.alignment = (int)wgetw(in);
        check_input(in, "structure alignment");
        t->u.structure.is_union = (bool)wgetw(in);
        check_input(in, "structure is_union");
        for (Tac_Member **tail = &t->u.structure.members;;) {
            size_t mtag = wgetw(in);
            check_input(in, "member tag");
            if (mtag != TAG_TAC_MEMBER)
                break;
            Tac_Member *m = tac_new_member();
            m->name       = wgetstr(in);
            check_input(in, "member name");
            m->offset = (int)wgetw(in);
            check_input(in, "member offset");
            m->type = import_type(in);
            *tail   = m;
            tail    = &m->next;
        }
        break;
    default:
        break;
    }
    t->next = import_type(in);
    return t;
}

static Tac_StaticInit *import_static_init(WFILE *in)
{
    size_t tag = wgetw(in);
    check_input(in, "static_init tag");
    if (tag < TAG_TAC_STATIC_INIT || tag > TAG_TAC_STATIC_INIT + TAC_STATIC_INIT_FAT_POINTER)
        return NULL;
    Tac_StaticInit *si = tac_new_static_init((Tac_StaticInitKind)(tag - TAG_TAC_STATIC_INIT));
    switch (si->kind) {
    case TAC_STATIC_INIT_I8:
        si->u.char_val = (int8_t)wgetw(in);
        check_input(in, "static_init i8");
        break;
    case TAC_STATIC_INIT_I16:
        si->u.short_val = (int16_t)wgetw(in);
        check_input(in, "static_init i16");
        break;
    case TAC_STATIC_INIT_I32:
        si->u.int_val = (int32_t)wgetw(in);
        check_input(in, "static_init i32");
        break;
    case TAC_STATIC_INIT_I64:
        si->u.long_val = (int64_t)wgetw(in);
        check_input(in, "static_init i64");
        break;
    case TAC_STATIC_INIT_U8:
        si->u.uchar_val = (uint8_t)wgetw(in);
        check_input(in, "static_init u8");
        break;
    case TAC_STATIC_INIT_U16:
        si->u.ushort_val = (uint16_t)wgetw(in);
        check_input(in, "static_init u16");
        break;
    case TAC_STATIC_INIT_U32:
        si->u.uint_val = (uint32_t)wgetw(in);
        check_input(in, "static_init u32");
        break;
    case TAC_STATIC_INIT_U64:
        si->u.ulong_val = (uint64_t)wgetw(in);
        check_input(in, "static_init u64");
        break;
    case TAC_STATIC_INIT_FLOAT:
        si->u.float_val = wgetd(in);
        check_input(in, "static_init float");
        break;
    case TAC_STATIC_INIT_DOUBLE:
        si->u.double_val = wgetd(in);
        check_input(in, "static_init double");
        break;
    case TAC_STATIC_INIT_LONG_DOUBLE:
        si->u.long_double_val = wgetf128(in);
        check_input(in, "static_init long double");
        break;
    case TAC_STATIC_INIT_ZERO:
        si->u.zero_bytes = (int)wgetw(in);
        check_input(in, "static_init zero");
        break;
    case TAC_STATIC_INIT_STRING:
        si->u.string.val = wgetdata(&si->u.string.len, in);
        check_input(in, "static_init string val");
        si->u.string.null_terminated = (bool)wgetw(in);
        check_input(in, "static_init string null_terminated");
        break;
    case TAC_STATIC_INIT_POINTER:
        si->u.pointer.name = wgetstr(in);
        check_input(in, "static_init pointer name");
        si->u.pointer.byte_offset = (int)wgetw(in);
        check_input(in, "static_init pointer offset");
        break;
    case TAC_STATIC_INIT_FAT_POINTER:
        si->u.pointer.name = wgetstr(in);
        check_input(in, "static_init fat_pointer name");
        si->u.pointer.byte_offset = (int)wgetw(in);
        check_input(in, "static_init fat_pointer offset");
        break;
    default:
        break;
    }
    si->next = import_static_init(in);
    return si;
}

static Tac_Param *import_param(WFILE *in)
{
    size_t tag = wgetw(in);
    check_input(in, "param tag");
    if (tag != TAG_TAC_PARAM)
        return NULL;
    Tac_Param *p = tac_new_param();
    p->name      = wgetstr(in);
    check_input(in, "param name");
    p->type = import_type(in);
    p->next = import_param(in);
    return p;
}

static Tac_StaticLocal *import_static_local(WFILE *in)
{
    size_t tag = wgetw(in);
    check_input(in, "static local tag");
    if (tag != TAG_TAC_STATIC_LOC)
        return NULL;
    Tac_StaticLocal *sl = tac_new_static_local();
    sl->name            = wgetstr(in);
    check_input(in, "static local name");
    sl->type      = import_type(in);
    sl->init_list = import_static_init(in);
    sl->alignment = (int)wgetw(in);
    check_input(in, "static local alignment");
    sl->next = import_static_local(in);
    return sl;
}

static Tac_Instruction *import_instr(WFILE *in)
{
    size_t tag = wgetw(in);
    check_input(in, "instr tag");
    bool is_volatile = (tag & TAG_INSTR_VOLATILE) != 0;
    tag &= ~TAG_INSTR_VOLATILE;
    if (tag < TAG_TAC_INSTR || tag > TAG_TAC_INSTR + TAC_INSTRUCTION_JUMP_TABLE)
        return NULL;
    Tac_Instruction *instr = tac_new_instruction((Tac_InstructionKind)(tag - TAG_TAC_INSTR));
    instr->is_volatile     = is_volatile;
    switch (instr->kind) {
    case TAC_INSTRUCTION_RETURN:
        instr->u.return_.src = import_val(in);
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
        instr->u.sign_extend.src = import_val(in);
        instr->u.sign_extend.dst = import_val(in);
        break;
    case TAC_INSTRUCTION_UNARY:
        instr->u.unary.op = (Tac_UnaryOperator)wgetw(in);
        check_input(in, "unary op");
        instr->u.unary.src = import_val(in);
        instr->u.unary.dst = import_val(in);
        break;
    case TAC_INSTRUCTION_BINARY:
        instr->u.binary.op = (Tac_BinaryOperator)wgetw(in);
        check_input(in, "binary op");
        instr->u.binary.src1 = import_val(in);
        instr->u.binary.src2 = import_val(in);
        instr->u.binary.dst  = import_val(in);
        break;
    case TAC_INSTRUCTION_COPY:
        instr->u.copy.src = import_val(in);
        instr->u.copy.dst = import_val(in);
        break;
    case TAC_INSTRUCTION_GET_ADDRESS:
    case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
    case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
        instr->u.get_address.src = import_val(in);
        instr->u.get_address.dst = import_val(in);
        break;
    case TAC_INSTRUCTION_LOAD:
    case TAC_INSTRUCTION_LOAD_BYTE:
        instr->u.load.src_ptr = import_val(in);
        instr->u.load.dst     = import_val(in);
        break;
    case TAC_INSTRUCTION_STORE:
    case TAC_INSTRUCTION_STORE_BYTE:
        instr->u.store.src     = import_val(in);
        instr->u.store.dst_ptr = import_val(in);
        break;
    case TAC_INSTRUCTION_ADD_PTR:
        instr->u.add_ptr.ptr   = import_val(in);
        instr->u.add_ptr.index = import_val(in);
        instr->u.add_ptr.scale = (int)wgetw(in);
        check_input(in, "add_ptr scale");
        instr->u.add_ptr.dst = import_val(in);
        break;
    case TAC_INSTRUCTION_PTR_DIFF:
        instr->u.ptr_diff.ptr_a = import_val(in);
        instr->u.ptr_diff.ptr_b = import_val(in);
        instr->u.ptr_diff.dst   = import_val(in);
        break;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET:
        instr->u.copy_to_offset.src = import_val(in);
        instr->u.copy_to_offset.dst = wgetstr(in);
        check_input(in, "copy_to_offset dst");
        instr->u.copy_to_offset.offset = (int)wgetw(in);
        check_input(in, "copy_to_offset offset");
        break;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
        instr->u.copy_from_offset.src = wgetstr(in);
        check_input(in, "copy_from_offset src");
        instr->u.copy_from_offset.offset = (int)wgetw(in);
        check_input(in, "copy_from_offset offset");
        instr->u.copy_from_offset.dst = import_val(in);
        break;
    case TAC_INSTRUCTION_JUMP:
        instr->u.jump.target = wgetstr(in);
        check_input(in, "jump target");
        break;
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
        instr->u.jump_if_zero.condition = import_val(in);
        instr->u.jump_if_zero.target    = wgetstr(in);
        check_input(in, "jump_if_zero target");
        break;
    case TAC_INSTRUCTION_LABEL:
        instr->u.label.name = wgetstr(in);
        check_input(in, "label name");
        break;
    case TAC_INSTRUCTION_JUMP_TABLE:
        instr->u.jump_table.index = import_val(in);
        instr->u.jump_table.count = (int)wgetw(in);
        check_input(in, "jump_table count");
        instr->u.jump_table.targets =
            xalloc((instr->u.jump_table.count + 1) * sizeof(char *), __func__, __FILE__, __LINE__);
        for (int i = 0; i < instr->u.jump_table.count; i++)
            instr->u.jump_table.targets[i] = wgetstr(in);
        instr->u.jump_table.default_target = wgetstr(in);
        check_input(in, "jump_table targets");
        break;
    case TAC_INSTRUCTION_FUN_CALL:
    case TAC_INSTRUCTION_FUN_CALL_NORETURN:
        instr->u.fun_call.fun_name = wgetstr(in);
        check_input(in, "fun_call fun_name");
        instr->u.fun_call.indirect = (bool)wgetw(in);
        check_input(in, "fun_call indirect");
        instr->u.fun_call.args     = import_val(in);
        instr->u.fun_call.dst      = import_val(in);
        instr->u.fun_call.fun_type = import_type(in);
        break;
    case TAC_INSTRUCTION_ALLOCATE_LOCAL:
        instr->u.allocate_local.name = wgetstr(in);
        check_input(in, "allocate_local name");
        instr->u.allocate_local.size = (int)wgetw(in);
        check_input(in, "allocate_local size");
        instr->u.allocate_local.alignment = (int)wgetw(in);
        check_input(in, "allocate_local alignment");
        break;
    default:
        break;
    }
    instr->next = import_instr(in);
    return instr;
}

Tac_TopLevel *tac_import_toplevel(WFILE *in)
{
    size_t tag = wgetw(in);
    check_input(in, "toplevel tag");
    if (tag == TAG_EOL) {
        return NULL;
    }
    if (tag < TAG_TAC_TOPLEVEL || tag > TAG_TAC_TOPLEVEL + TAC_TOPLEVEL_EXTERN) {
        bad_input("tag 0x%zx where a top-level declaration belongs", tag);
    }
    Tac_TopLevel *tl = tac_new_toplevel((Tac_TopLevelKind)(tag - TAG_TAC_TOPLEVEL));
    switch (tl->kind) {
    case TAC_TOPLEVEL_FUNCTION:
        tl->u.function.name = wgetstr(in);
        check_input(in, "function name");
        tl->u.function.global = (bool)wgetw(in);
        check_input(in, "function global");
        tl->u.function.variadic = (bool)wgetw(in);
        check_input(in, "function variadic");
        tl->u.function.noret = (bool)wgetw(in);
        check_input(in, "function noret");
        tl->u.function.type          = import_type(in);
        tl->u.function.params        = import_param(in);
        tl->u.function.locals        = import_param(in);
        tl->u.function.static_locals = import_static_local(in);
        tl->u.function.body          = import_instr(in);
        break;
    case TAC_TOPLEVEL_STATIC_VARIABLE:
        tl->u.static_variable.name = wgetstr(in);
        check_input(in, "static_variable name");
        tl->u.static_variable.global = (bool)wgetw(in);
        check_input(in, "static_variable global");
        tl->u.static_variable.type      = import_type(in);
        tl->u.static_variable.init_list = import_static_init(in);
        tl->u.static_variable.alignment = (int)wgetw(in);
        check_input(in, "static_variable alignment");
        break;
    case TAC_TOPLEVEL_STATIC_CONSTANT:
        tl->u.static_constant.name = wgetstr(in);
        check_input(in, "static_constant name");
        tl->u.static_constant.type = import_type(in);
        tl->u.static_constant.init = import_static_init(in);
        break;
    case TAC_TOPLEVEL_EXTERN:
        tl->u.extern_.name = wgetstr(in);
        check_input(in, "extern name");
        tl->u.extern_.type = import_type(in);
        break;
    default:
        break;
    }
    return tl;
}

bool tac_import_begin_stream(WFILE *in)
{
    size_t magic = wgetw(in);
    return !weof(in) && !werror(in) && magic == TAG_TAC_MAGIC;
}

Tac_Program *tac_import_program(WFILE *in)
{
    Tac_Program *prog = tac_new_program();
    if (!tac_import_begin_stream(in)) {
        bad_input("no 'TAC5' magic word at the start");
    }
    for (Tac_TopLevel **p = &prog->decls;; p = &(*p)->next) {
        *p = tac_import_toplevel(in);
        if (*p == NULL)
            break;
    }
    return prog;
}
