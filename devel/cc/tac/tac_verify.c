//
// TAC verifier: every name has a type, and operand types agree with the operator.
//
#include <stdarg.h>
#include <string.h>

#include "tac.h"

typedef enum { C_INT, C_PTR, C_FLOAT, C_DOUBLE, C_LDOUBLE, C_AGG, C_VOID, C_FUN } Class;

typedef struct {
    const Tac_TopLevel *fn;
    const Tac_Layout *layout;
    Tac_GlobalType global;
    void *arg;
    FILE *err;
    int errors;
    int index; // of the instruction being checked
    const Tac_Instruction *in;
} Verifier;

static const char *const instr_names[] = {
    "return",
    "sign_extend",
    "truncate",
    "zero_extend",
    "double_to_int",
    "double_to_uint",
    "int_to_double",
    "uint_to_double",
    "float_to_double",
    "double_to_float",
    "int_to_float",
    "uint_to_float",
    "float_to_int",
    "float_to_uint",
    "long_double_to_int",
    "long_double_to_uint",
    "int_to_long_double",
    "uint_to_long_double",
    "long_double_to_double",
    "double_to_long_double",
    "long_double_to_float",
    "float_to_long_double",
    "ptr_to_char_ptr",
    "char_ptr_to_ptr",
    "unary",
    "binary",
    "copy",
    "get_address",
    "get_address_byte",
    "get_address_decay",
    "load",
    "load_byte",
    "store",
    "store_byte",
    "add_ptr",
    "ptr_diff",
    "copy_to_offset",
    "copy_byte_to_offset",
    "copy_from_offset",
    "copy_byte_from_offset",
    "jump",
    "jump_if_zero",
    "jump_if_not_zero",
    "label",
    "fun_call",
    "fun_call_noreturn",
    "allocate_local",
};

const char *tac_instruction_name(Tac_InstructionKind kind)
{
    return instr_names[kind];
}

static void problem(Verifier *v, const char *fmt, ...)
{
    v->errors++;
    if (!v->err)
        return;
    fprintf(v->err, "verify: %s: #%d %s: ", v->fn->u.function.name, v->index,
            v->in ? tac_instruction_name(v->in->kind) : "symbols");
    va_list ap;
    va_start(ap, fmt);
    vfprintf(v->err, fmt, ap);
    va_end(ap);
    fputc('\n', v->err);
}

static Class class_of(const Tac_Type *t)
{
    switch (t->kind) {
    case TAC_TYPE_FLOAT:
        return C_FLOAT;
    case TAC_TYPE_DOUBLE:
        return C_DOUBLE;
    case TAC_TYPE_LONG_DOUBLE:
        return C_LDOUBLE;
    case TAC_TYPE_POINTER:
        return C_PTR;
    case TAC_TYPE_ARRAY:
    case TAC_TYPE_STRUCTURE:
        return C_AGG;
    case TAC_TYPE_VOID:
        return C_VOID;
    case TAC_TYPE_FUN_TYPE:
        return C_FUN;
    default:
        return C_INT;
    }
}

static int size_of(const Verifier *v, const Tac_Type *t)
{
    switch (t->kind) {
    case TAC_TYPE_POINTER:
        return v->layout->pointer;
    case TAC_TYPE_ARRAY:
        return t->u.array.size * size_of(v, t->u.array.elem_type);
    case TAC_TYPE_STRUCTURE:
        return t->u.structure.size;
    case TAC_TYPE_VOID:
    case TAC_TYPE_FUN_TYPE:
        return 0;
    default:
        return v->layout->scalar[t->kind];
    }
}

static bool is_integer_class(Class c)
{
    return c == C_INT || c == C_PTR;
}

static const Tac_Type *param_type(const Tac_Param *p, const char *name)
{
    for (; p; p = p->next)
        if (p->name && strcmp(p->name, name) == 0)
            return p->type;
    return NULL;
}

// The type of a name, or NULL (and a problem reported) when it has none.
static const Tac_Type *name_type(Verifier *v, const char *name)
{
    const Tac_Type *t = NULL;
    if (name[0] == '%') {
        t = param_type(v->fn->u.function.params, name);
        if (!t)
            t = param_type(v->fn->u.function.locals, name);
    } else {
        for (const Tac_StaticLocal *sl = v->fn->u.function.static_locals; sl && !t; sl = sl->next)
            if (strcmp(sl->name, name) == 0)
                t = sl->type;
        if (!t && v->global)
            t = v->global(name, v->arg);
    }
    if (!t)
        problem(v, "%s has no type", name);
    return t;
}

// The type of a value: a constant's own kind, or the variable's type.
typedef struct {
    Tac_Type type; // storage for a constant's type
    const Tac_Type *t;
    bool constant;
} ValType;

static bool val_type(Verifier *v, const Tac_Val *val, ValType *out)
{
    static const Tac_TypeKind const_type[] = {
        [TAC_CONST_INT]         = TAC_TYPE_INT,
        [TAC_CONST_LONG]        = TAC_TYPE_LONG,
        [TAC_CONST_LONG_LONG]   = TAC_TYPE_LONG_LONG,
        [TAC_CONST_UINT]        = TAC_TYPE_UINT,
        [TAC_CONST_ULONG]       = TAC_TYPE_ULONG,
        [TAC_CONST_ULONG_LONG]  = TAC_TYPE_ULONG_LONG,
        [TAC_CONST_FLOAT]       = TAC_TYPE_FLOAT,
        [TAC_CONST_DOUBLE]      = TAC_TYPE_DOUBLE,
        [TAC_CONST_LONG_DOUBLE] = TAC_TYPE_LONG_DOUBLE,
        [TAC_CONST_SCHAR]       = TAC_TYPE_SCHAR,
        [TAC_CONST_UCHAR]       = TAC_TYPE_UCHAR,
    };
    if (!val) {
        problem(v, "missing operand");
        return false;
    }
    if (val->kind == TAC_VAL_CONSTANT) {
        memset(&out->type, 0, sizeof out->type);
        out->type.kind = const_type[val->u.constant->kind];
        out->t         = &out->type;
        out->constant  = true;
        return true;
    }
    out->t        = name_type(v, val->u.var_name);
    out->constant = false;
    return out->t != NULL;
}

// Type both values, reporting each untyped one; true when both have a type.
static bool both(Verifier *v, const Tac_Val *x, ValType *xt, const Tac_Val *y, ValType *yt)
{
    bool ok = val_type(v, x, xt);
    return val_type(v, y, yt) && ok;
}

static const char *class_name(Class c)
{
    static const char *const names[] = { "integer",     "pointer",   "float", "double",
                                         "long double", "aggregate", "void",  "function" };
    return names[c];
}

// `what` must be of one of the classes in `mask` (bit per Class).
static void expect_class(Verifier *v, const char *what, const ValType *vt, unsigned mask)
{
    Class c = class_of(vt->t);
    // An integer constant may stand for a pointer: a null or an absolute address.
    if (vt->constant && c == C_INT && (mask & (1u << C_PTR)))
        return;
    if (!(mask & (1u << c)))
        problem(v, "%s is %s", what, class_name(c));
}

#define M(c)     (1u << (c))
#define M_INT    (M(C_INT) | M(C_PTR))
#define M_FP     (M(C_FLOAT) | M(C_DOUBLE) | M(C_LDOUBLE))
#define M_SCALAR (M_INT | M_FP)

// Two operands the operator treats as one type: same class family and, unless one is a
// constant (whose value carries over), the same size.
static void expect_same(Verifier *v, const char *a_name, const ValType *a, const char *b_name,
                        const ValType *b)
{
    Class ca = class_of(a->t), cb = class_of(b->t);
    bool compatible = ca == cb || (is_integer_class(ca) && is_integer_class(cb));
    if (!compatible) {
        problem(v, "%s is %s but %s is %s", a_name, class_name(ca), b_name, class_name(cb));
        return;
    }
    if (a->constant || b->constant)
        return;
    int sa = size_of(v, a->t), sb = size_of(v, b->t);
    if (sa != sb)
        problem(v, "%s has %d bytes but %s has %d", a_name, sa, b_name, sb);
}

static void check_conversion(Verifier *v, const Tac_Instruction *in)
{
    ValType src, dst;
    bool ok = val_type(v, in->u.sign_extend.src, &src);
    ok      = val_type(v, in->u.sign_extend.dst, &dst) && ok;
    if (!ok)
        return;
    unsigned from = 0, to = 0;
    switch (in->kind) {
    case TAC_INSTRUCTION_SIGN_EXTEND:
    case TAC_INSTRUCTION_ZERO_EXTEND:
    case TAC_INSTRUCTION_TRUNCATE:
        from = to = M_INT;
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_DOUBLE_TO_UINT:
        from = M(C_DOUBLE), to = M(C_INT);
        break;
    case TAC_INSTRUCTION_FLOAT_TO_INT:
    case TAC_INSTRUCTION_FLOAT_TO_UINT:
        from = M(C_FLOAT), to = M(C_INT);
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_INT:
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_UINT:
        from = M(C_LDOUBLE), to = M(C_INT);
        break;
    case TAC_INSTRUCTION_INT_TO_DOUBLE:
    case TAC_INSTRUCTION_UINT_TO_DOUBLE:
        from = M(C_INT), to = M(C_DOUBLE);
        break;
    case TAC_INSTRUCTION_INT_TO_FLOAT:
    case TAC_INSTRUCTION_UINT_TO_FLOAT:
        from = M(C_INT), to = M(C_FLOAT);
        break;
    case TAC_INSTRUCTION_INT_TO_LONG_DOUBLE:
    case TAC_INSTRUCTION_UINT_TO_LONG_DOUBLE:
        from = M(C_INT), to = M(C_LDOUBLE);
        break;
    case TAC_INSTRUCTION_FLOAT_TO_DOUBLE:
        from = M(C_FLOAT), to = M(C_DOUBLE);
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_FLOAT:
        from = M(C_DOUBLE), to = M(C_FLOAT);
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_DOUBLE:
        from = M(C_LDOUBLE), to = M(C_DOUBLE);
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_LONG_DOUBLE:
        from = M(C_DOUBLE), to = M(C_LDOUBLE);
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_FLOAT:
        from = M(C_LDOUBLE), to = M(C_FLOAT);
        break;
    case TAC_INSTRUCTION_FLOAT_TO_LONG_DOUBLE:
        from = M(C_FLOAT), to = M(C_LDOUBLE);
        break;
    case TAC_INSTRUCTION_PTR_TO_CHAR_PTR:
    case TAC_INSTRUCTION_CHAR_PTR_TO_PTR:
        from = to = M(C_PTR);
        break;
    default:
        return;
    }
    expect_class(v, "src", &src, from);
    expect_class(v, "dst", &dst, to);
    if (src.constant)
        return;
    int ss = size_of(v, src.t), ds = size_of(v, dst.t);
    if ((in->kind == TAC_INSTRUCTION_SIGN_EXTEND || in->kind == TAC_INSTRUCTION_ZERO_EXTEND) &&
        ds <= ss)
        problem(v, "extends %d bytes to %d", ss, ds);
    if (in->kind == TAC_INSTRUCTION_TRUNCATE && ds >= ss)
        problem(v, "truncates %d bytes to %d", ss, ds);
}

static bool is_comparison(Tac_BinaryOperator op)
{
    switch (op) {
    case TAC_BINARY_EQUAL:
    case TAC_BINARY_NOT_EQUAL:
    case TAC_BINARY_LESS_THAN:
    case TAC_BINARY_LESS_OR_EQUAL:
    case TAC_BINARY_GREATER_THAN:
    case TAC_BINARY_GREATER_OR_EQUAL:
    case TAC_BINARY_LESS_THAN_UNSIGNED:
    case TAC_BINARY_LESS_OR_EQUAL_UNSIGNED:
    case TAC_BINARY_GREATER_THAN_UNSIGNED:
    case TAC_BINARY_GREATER_OR_EQUAL_UNSIGNED:
    case TAC_BINARY_LESS_THAN_DOUBLE:
    case TAC_BINARY_LESS_OR_EQUAL_DOUBLE:
    case TAC_BINARY_GREATER_THAN_DOUBLE:
    case TAC_BINARY_GREATER_OR_EQUAL_DOUBLE:
        return true;
    default:
        return false;
    }
}

static bool is_fp_op(Tac_BinaryOperator op)
{
    return op >= TAC_BINARY_ADD_DOUBLE;
}

static void check_binary(Verifier *v, const Tac_Instruction *in)
{
    ValType a, b, d;
    bool ok = val_type(v, in->u.binary.src1, &a);
    ok      = val_type(v, in->u.binary.src2, &b) && ok;
    ok      = val_type(v, in->u.binary.dst, &d) && ok;
    if (!ok)
        return;
    Tac_BinaryOperator op = in->u.binary.op;
    if (is_comparison(op)) {
        expect_class(v, "dst", &d, M(C_INT));
        expect_class(v, "src1", &a, is_fp_op(op) ? M_FP : M_SCALAR);
        expect_same(v, "src1", &a, "src2", &b);
        return;
    }
    if (is_fp_op(op)) {
        expect_class(v, "dst", &d, M_FP);
        expect_same(v, "src1", &a, "dst", &d);
        expect_same(v, "src2", &b, "dst", &d);
        if (a.constant && class_of(a.t) != class_of(d.t))
            problem(v, "src1 constant is %s, dst %s", class_name(class_of(a.t)),
                    class_name(class_of(d.t)));
        if (b.constant && class_of(b.t) != class_of(d.t))
            problem(v, "src2 constant is %s, dst %s", class_name(class_of(b.t)),
                    class_name(class_of(d.t)));
        return;
    }
    expect_class(v, "src1", &a, M_INT);
    expect_class(v, "src2", &b, M_INT);
    expect_class(v, "dst", &d, M_INT);
    expect_same(v, "src1", &a, "dst", &d);
    if (op != TAC_BINARY_LEFT_SHIFT && op != TAC_BINARY_RIGHT_SHIFT &&
        op != TAC_BINARY_RIGHT_SHIFT_LOGICAL)
        expect_same(v, "src2", &b, "dst", &d);
}

static void check_unary(Verifier *v, const Tac_Instruction *in)
{
    ValType s, d;
    bool ok = val_type(v, in->u.unary.src, &s);
    ok      = val_type(v, in->u.unary.dst, &d) && ok;
    if (!ok)
        return;
    switch (in->u.unary.op) {
    case TAC_UNARY_NOT:
        expect_class(v, "src", &s, M_SCALAR);
        expect_class(v, "dst", &d, M(C_INT));
        break;
    case TAC_UNARY_NEGATE_DOUBLE:
        expect_class(v, "dst", &d, M_FP);
        expect_same(v, "src", &s, "dst", &d);
        break;
    case TAC_UNARY_SQRT_DOUBLE:
        expect_class(v, "dst", &d, M(C_DOUBLE));
        expect_same(v, "src", &s, "dst", &d);
        break;
    default:
        expect_class(v, "src", &s, M_INT);
        expect_class(v, "dst", &d, M_INT);
        expect_same(v, "src", &s, "dst", &d);
        break;
    }
}

// An access of `width` bytes at `offset` into aggregate `name` stays inside it.
static void check_offset(Verifier *v, const char *name, int offset, const ValType *val)
{
    const Tac_Type *agg = name_type(v, name);
    if (!agg)
        return;
    int width = val->constant ? 1 : size_of(v, val->t);
    if (offset < 0 || offset + width > size_of(v, agg))
        problem(v, "%d bytes at offset %d of %s, which has %d", width, offset, name,
                size_of(v, agg));
}

static void check_instruction(Verifier *v, const Tac_Instruction *in)
{
    ValType a, b, d;
    switch (in->kind) {
    case TAC_INSTRUCTION_RETURN: {
        const Tac_Type *ft = v->fn->u.function.type;
        if (!in->u.return_.src || !val_type(v, in->u.return_.src, &a) || !ft)
            break;
        const Tac_Type *rt = ft->u.fun_type.ret_type;
        // A struct returned through the hidden pointer returns that pointer.
        if (class_of(rt) == C_AGG && class_of(a.t) == C_PTR)
            break;
        ValType r = { .t = rt };
        expect_same(v, "src", &a, "return type", &r);
        break;
    }
    case TAC_INSTRUCTION_UNARY:
        check_unary(v, in);
        break;
    case TAC_INSTRUCTION_BINARY:
        check_binary(v, in);
        break;
    case TAC_INSTRUCTION_COPY:
        if (both(v, in->u.copy.src, &a, in->u.copy.dst, &d))
            expect_same(v, "src", &a, "dst", &d);
        break;
    case TAC_INSTRUCTION_GET_ADDRESS:
    case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
    case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
        if (both(v, in->u.get_address.src, &a, in->u.get_address.dst, &d))
            expect_class(v, "dst", &d, M(C_PTR));
        break;
    case TAC_INSTRUCTION_LOAD:
    case TAC_INSTRUCTION_LOAD_BYTE:
        if (both(v, in->u.load.src_ptr, &a, in->u.load.dst, &d)) {
            expect_class(v, "src_ptr", &a, M(C_PTR));
            if (in->kind == TAC_INSTRUCTION_LOAD_BYTE && size_of(v, d.t) != 1)
                problem(v, "dst has %d bytes", size_of(v, d.t));
        }
        break;
    case TAC_INSTRUCTION_STORE:
    case TAC_INSTRUCTION_STORE_BYTE:
        if (both(v, in->u.store.src, &a, in->u.store.dst_ptr, &d))
            expect_class(v, "dst_ptr", &d, M(C_PTR));
        break;
    case TAC_INSTRUCTION_ADD_PTR:
        if (both(v, in->u.add_ptr.ptr, &a, in->u.add_ptr.index, &b) &&
            val_type(v, in->u.add_ptr.dst, &d)) {
            expect_class(v, "ptr", &a, M(C_PTR));
            expect_class(v, "index", &b, M(C_INT));
            expect_class(v, "dst", &d, M(C_PTR));
        }
        break;
    case TAC_INSTRUCTION_PTR_DIFF:
        if (both(v, in->u.ptr_diff.ptr_a, &a, in->u.ptr_diff.ptr_b, &b) &&
            val_type(v, in->u.ptr_diff.dst, &d)) {
            expect_class(v, "ptr_a", &a, M(C_PTR));
            expect_class(v, "ptr_b", &b, M(C_PTR));
            expect_class(v, "dst", &d, M(C_INT));
        }
        break;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET:
        if (val_type(v, in->u.copy_to_offset.src, &a))
            check_offset(v, in->u.copy_to_offset.dst, in->u.copy_to_offset.offset, &a);
        break;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
        if (val_type(v, in->u.copy_from_offset.dst, &d))
            check_offset(v, in->u.copy_from_offset.src, in->u.copy_from_offset.offset, &d);
        break;
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
        if (val_type(v, in->u.jump_if_zero.condition, &a))
            expect_class(v, "condition", &a, M_SCALAR);
        break;
    case TAC_INSTRUCTION_JUMP_TABLE:
        if (val_type(v, in->u.jump_table.index, &a))
            expect_class(v, "index", &a, M_SCALAR);
        break;
    case TAC_INSTRUCTION_FUN_CALL:
    case TAC_INSTRUCTION_FUN_CALL_NORETURN: {
        const Tac_Type *ft = in->u.fun_call.fun_type;
        if (!ft)
            problem(v, "%s: no callee type", in->u.fun_call.fun_name);
        if (in->u.fun_call.indirect) {
            const Tac_Type *pt = name_type(v, in->u.fun_call.fun_name);
            if (pt && class_of(pt) != C_PTR)
                problem(v, "%s is not a pointer", in->u.fun_call.fun_name);
        }
        for (const Tac_Val *arg = in->u.fun_call.args; arg; arg = arg->next)
            val_type(v, arg, &a);
        if (in->u.fun_call.dst && val_type(v, in->u.fun_call.dst, &d) && ft) {
            ValType r = { .t = ft->u.fun_type.ret_type };
            expect_same(v, "dst", &d, "return type", &r);
        }
        break;
    }
    case TAC_INSTRUCTION_ALLOCATE_LOCAL: {
        const Tac_Type *t = name_type(v, in->u.allocate_local.name);
        if (t && size_of(v, t) != in->u.allocate_local.size)
            problem(v, "%s has %d bytes, allocated %d", in->u.allocate_local.name, size_of(v, t),
                    in->u.allocate_local.size);
        break;
    }
    case TAC_INSTRUCTION_JUMP:
    case TAC_INSTRUCTION_LABEL:
        break;
    default:
        check_conversion(v, in);
        break;
    }
}

int tac_verify_function(const Tac_TopLevel *fn, const Tac_Layout *layout, Tac_GlobalType global,
                        void *arg, FILE *err)
{
    Verifier v = { fn, layout, global, arg, err, 0, 0, NULL };
    for (const Tac_Param *p = fn->u.function.params; p; p = p->next)
        if (!p->type)
            problem(&v, "parameter %s has no type", p->name);
    for (const Tac_Param *p = fn->u.function.locals; p; p = p->next)
        if (!p->type)
            problem(&v, "local %s has no type", p->name);
    for (const Tac_Instruction *in = fn->u.function.body; in; in = in->next) {
        v.index++;
        v.in = in;
        check_instruction(&v, in);
    }
    return v.errors;
}

// Global names of a program chain, for tac_verify_program.
// cppcheck-suppress constParameterCallback ; the signature is the lookup callback's
static const Tac_Type *program_global(const char *name, void *arg)
{
    for (const Tac_TopLevel *t = arg; t; t = t->next) {
        switch (t->kind) {
        case TAC_TOPLEVEL_FUNCTION:
            if (strcmp(t->u.function.name, name) == 0)
                return t->u.function.type;
            break;
        case TAC_TOPLEVEL_STATIC_VARIABLE:
            if (strcmp(t->u.static_variable.name, name) == 0)
                return t->u.static_variable.type;
            break;
        case TAC_TOPLEVEL_STATIC_CONSTANT:
            if (strcmp(t->u.static_constant.name, name) == 0)
                return t->u.static_constant.type;
            break;
        case TAC_TOPLEVEL_EXTERN:
            if (strcmp(t->u.extern_.name, name) == 0)
                return t->u.extern_.type;
            break;
        }
    }
    return NULL;
}

int tac_verify_program(const Tac_TopLevel *program, const Tac_Layout *layout, FILE *err)
{
    int errors = 0;
    for (const Tac_TopLevel *t = program; t; t = t->next)
        if (t->kind == TAC_TOPLEVEL_FUNCTION)
            errors += tac_verify_function(t, layout, program_global, (void *)program, err);
    return errors;
}
