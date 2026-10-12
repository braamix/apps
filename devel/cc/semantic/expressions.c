//
// Type-checking for expressions.
//
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "c_escape.h"
#include "semantic.h"
#include "structtab.h"
#include "symtab.h"
#include "target.h"
#include "typecheck.h"
#include "typetab.h"
#include "xalloc.h"

//
// Messages name the types of the operands as C writes them (docs/Technical_Reference.md,
// "Diagnostics"). The strings leak, but only on the way out: fatal_error() exits.
//
static const char *type_of(const Expr *e)
{
    return type_to_c(e->type);
}

static _Noreturn void invalid_operands(const char *op, const Expr *e1, const Expr *e2)
{
    fatal_error("invalid operands to '%s' ('%s' and '%s')", op, type_of(e1), type_of(e2));
}

// Is type t const-qualified, directly or through its typedef?
static bool is_const_type(const Type *t)
{
    while (t) {
        for (const TypeQualifier *q = t->qualifiers; q; q = q->next)
            if (q->kind == TYPE_QUALIFIER_CONST)
                return true;
        if (t->kind != TYPE_TYPEDEF_NAME || !typetab_exists(t->u.typedef_name.name))
            return false;
        t = typetab_resolve(t->u.typedef_name.name);
    }
    return false;
}

// Is lvalue e read-only (C11 §6.3.2.1p1): of a const-qualified type, or a member of a
// const-qualified structure or union, however deep?
static bool is_read_only(const Expr *e)
{
    if (is_const_type(e->type))
        return true;
    if (e->kind == EXPR_FIELD_ACCESS)
        return is_read_only(e->u.field_access.expr);
    if (e->kind == EXPR_PTR_ACCESS) {
        const Type *p = unalias(e->u.ptr_access.expr->type);
        return p->kind == TYPE_POINTER && is_const_type(p->u.pointer.target);
    }
    return false;
}

// Reject a change to read-only lvalue e by assignment, ++ or --.
static void check_modifiable(const Expr *e)
{
    if (!is_read_only(e))
        return;
    if (e->kind == EXPR_VAR) {
        // A local's backend name may carry a "$N" suffix; the user wrote the name before it.
        const char *dollar = strchr(e->u.var, '$');
        int len            = dollar ? (int)(dollar - e->u.var) : (int)strlen(e->u.var);
        fatal_error("cannot assign to variable '%.*s' with const-qualified type '%s'", len,
                    e->u.var, type_of(e));
    }
    fatal_error("cannot assign to a read-only location of type '%s'", type_of(e));
}

static const char *binary_op_text(BinaryOp op)
{
    switch (op) {
    case BINARY_MUL:
        return "*";
    case BINARY_DIV:
        return "/";
    case BINARY_MOD:
        return "%";
    case BINARY_ADD:
        return "+";
    case BINARY_SUB:
        return "-";
    case BINARY_LEFT_SHIFT:
        return "<<";
    case BINARY_RIGHT_SHIFT:
        return ">>";
    case BINARY_LT:
        return "<";
    case BINARY_GT:
        return ">";
    case BINARY_LE:
        return "<=";
    case BINARY_GE:
        return ">=";
    case BINARY_EQ:
        return "==";
    case BINARY_NE:
        return "!=";
    case BINARY_BIT_AND:
        return "&";
    case BINARY_BIT_XOR:
        return "^";
    case BINARY_BIT_OR:
        return "|";
    case BINARY_LOG_AND:
        return "&&";
    case BINARY_LOG_OR:
        return "||";
    case BINARY_COMMA:
        return ",";
    }
    return "?";
}

static const char *assign_op_text(AssignOp op)
{
    switch (op) {
    case ASSIGN_SIMPLE:
        return "=";
    case ASSIGN_MUL:
        return "*=";
    case ASSIGN_DIV:
        return "/=";
    case ASSIGN_MOD:
        return "%=";
    case ASSIGN_ADD:
        return "+=";
    case ASSIGN_SUB:
        return "-=";
    case ASSIGN_LEFT:
        return "<<=";
    case ASSIGN_RIGHT:
        return ">>=";
    case ASSIGN_AND:
        return "&=";
    case ASSIGN_XOR:
        return "^=";
    case ASSIGN_OR:
        return "|=";
    }
    return "?";
}

// Parser represents f(void) as a single unnamed TYPE_VOID param; treat as no params.
static const Param *params_for_call(const Type *fn_type)
{
    const Param *params = fn_type->u.function.params;
    if (params && !params->next && unalias(params->type)->kind == TYPE_VOID && !params->name)
        return NULL;
    return params;
}

static Expr *decay_expr(Expr *typed);
static TypeKind bitfield_promoted_kind(const Expr *e);

// True if the (pre-decay) operand has array type but is not a string literal:
// a named array, an array element/member, or a *ptr-to-array is never a
// modifiable lvalue (C11 6.3.2.1), so =, compound assignment, and ++/-- must
// reject it before it decays to an assignable-looking pointer.  A string
// literal is also an array object, but it falls through to the generic
// is_lvalue() path so its existing diagnostic stays unchanged.
static bool is_array_lvalue_operand(const Expr *e)
{
    return e->kind != EXPR_LITERAL && is_array(e->type);
}

// Resolve and validate a type name (cast, sizeof, _Alignof, _Generic, compound literal),
// registering any struct/union it defines.
Type *check_type_name(Type *t)
{
    t = resolve_typedef_names(t);
    register_inline_struct_defs(t);
    validate_type(t);
    return t;
}

// Check if an expression is an lvalue.
// True if the (un-decayed) expression is a function designator: a bare
// identifier that names a function.  Such an operand decays to a function
// pointer, so it slips past the lvalue/scalar checks; callers that require a
// modifiable lvalue (++/--/compound assignment) must reject it explicitly,
// looking at the raw operand before typecheck_and_decay() rewrites its type.
static bool is_function_designator(const Expr *e)
{
    if (e->kind != EXPR_VAR)
        return false;
    const Symbol *sym = symtab_get_opt(e->u.var);
    return sym && sym->type && unalias(sym->type)->kind == TYPE_FUNCTION;
}

static bool is_lvalue(const Expr *e)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    switch (e->kind) {
    case EXPR_VAR:
    case EXPR_PTR_ACCESS:
    case EXPR_SUBSCRIPT:
    case EXPR_COMPOUND: // C11 6.5.2.5p4
        return true;
    case EXPR_FIELD_ACCESS:
        // `E.member` is an lvalue iff E is an lvalue (e.g. f().m and (c?a:b).m are not).
        return is_lvalue(e->u.field_access.expr);
    case EXPR_BINARY_OP:
        // No binary operator yields an lvalue in C.  Unlike C++, the comma operator
        // is no exception (C11 6.5.17p2 gives its result the *value* of the right
        // operand, not the object); compound assignment is a separate EXPR_ASSIGN node.
        return false;
    case EXPR_UNARY_OP:
        if (e->u.unary_op.op == UNARY_DEREF) {
            return true;
        }
        return false;
    default:
        return false;
    }
}

// Type-check a variable reference.
static Expr *typecheck_var(Expr *e)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    const Symbol *sym = symtab_get(e->u.var);
    if (coroutine_value(e, sym))
        return e; // a coroutine's name as a value: its coro_ptr
    if (strcmp(e->u.var, "__builtin_alloca") == 0)
        fatal_error("'__builtin_alloca' can only be called");

    // A block-scope static is keyed in the symtab by its source name but carries a distinct
    // backend name (so sibling-block repeats stay unique); rewrite the reference to it so the
    // translator and backend see the same name as the storage definition.  For every other
    // symbol the names match, making this a no-op.
    if (sym->name && strcmp(sym->name, e->u.var) != 0) {
        xfree(e->u.var);
        e->u.var = xstrdup(sym->name);
    }

    free_type(e->type);
    e->type = clone_type(sym->type, __func__, __FILE__, __LINE__);
    return e;
}

// Type-check a string literal.
Expr *typecheck_string(Expr *e)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    Type *array            = new_type(TYPE_ARRAY, __func__, __FILE__, __LINE__);
    array->u.array.element = new_type(TYPE_CHAR, __func__, __FILE__, __LINE__);
    // Size from the decoded bytes (quotes stripped, escapes processed), not the raw
    // lexeme, so sizeof "Hello, World!" == 14 (byte length incl. NUL), not 16.  The
    // decoded byte count comes from the decoder, not strlen: an embedded NUL is a byte
    // of the string like any other, so sizeof "a\0c" == 4.
    size_t decoded_length;
    char *decoded = c_decode_string_literal(e->u.literal->u.string_val, &decoded_length);
    set_array_size(array, decoded_length + 1);
    xfree(decoded);
    free_type(e->type);
    e->type = array;
    return e;
}

// A literal the compiler made itself (no spelling) is typed as if long had 64 bits.
// Where long is narrower than long long, a long one that does not fit is a long long.
static void widen_long_literal(Literal *lit)
{
    if (target_config->long_size >= target_config->llong_size)
        return;
    int bits = (int)target_config->long_size * 8;
    if (lit->kind == LITERAL_LONG &&
        (lit->u.long_val > (1LL << (bits - 1)) - 1 || lit->u.long_val < -(1LL << (bits - 1)))) {
        long long v          = lit->u.long_val;
        lit->kind            = LITERAL_LONG_LONG;
        lit->u.long_long_val = v;
    } else if (lit->kind == LITERAL_ULONG && lit->u.ulong_val > (1ULL << bits) - 1) {
        unsigned long long v  = lit->u.ulong_val;
        lit->kind             = LITERAL_ULONG_LONG;
        lit->u.ulong_long_val = v;
    }
}

// Type-check a constant literal.
static Expr *typecheck_literal(Expr *e)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    free_type(e->type);
    e->type = NULL; // prevent double-free: typecheck_string also calls free_type(e->type)
    check_int_literal_width(e->u.literal);
    type_int_literal(e->u.literal);
    if (e->u.literal->kind == LITERAL_DOUBLE) // as the target's double holds it
        e->u.literal->u.real_val = literal_to_double(e->u.literal);
    widen_long_literal(e->u.literal);
    switch (e->u.literal->kind) {
    case LITERAL_INT:
        e->type = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
        break;
    case LITERAL_LONG:
        e->type = new_type(TYPE_LONG, __func__, __FILE__, __LINE__);
        break;
    case LITERAL_LONG_LONG:
        e->type = new_type(TYPE_LONG_LONG, __func__, __FILE__, __LINE__);
        break;
    case LITERAL_UINT:
        e->type = new_type(TYPE_UINT, __func__, __FILE__, __LINE__);
        break;
    case LITERAL_ULONG:
        e->type = new_type(TYPE_ULONG, __func__, __FILE__, __LINE__);
        break;
    case LITERAL_ULONG_LONG:
        e->type = new_type(TYPE_ULONG_LONG, __func__, __FILE__, __LINE__);
        break;
    case LITERAL_CHAR:
        e->type = new_type(TYPE_CHAR, __func__, __FILE__, __LINE__);
        break;
    case LITERAL_FLOAT:
        e->type = new_type(TYPE_FLOAT, __func__, __FILE__, __LINE__);
        break;
    case LITERAL_DOUBLE:
        e->type = new_type(TYPE_DOUBLE, __func__, __FILE__, __LINE__);
        break;
    case LITERAL_LONG_DOUBLE:
        e->type = new_type(TYPE_LONG_DOUBLE, __func__, __FILE__, __LINE__);
        break;
    case LITERAL_STRING: {
        e = typecheck_string(e);
        break;
    }
    case LITERAL_ENUM: {
        const Symbol *sym = symtab_get(e->u.literal->u.enum_const);
        int val           = sym->u.enum_val;
        xfree(e->u.literal->u.enum_const);
        e->u.literal->kind      = LITERAL_INT;
        e->u.literal->u.int_val = val;
        e->type                 = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
        break;
    }
    default:
        internal_error("unsupported literal kind %d", e->u.literal->kind);
    }
    return e;
}

//
// Evaluate an immediate first argument of a target intrinsic (Target.immediate_args), diagnose
// a non-constant or out-of-range one, and replace it with the folded literal.
//
// THE FOLD HAS TO HAPPEN HERE, not at instruction selection.  eval_const() is the language's
// own constant-expression evaluator and is fully recursive, so it sees through an arbitrarily
// nested constant expression — `(PSW_MMAP_DISABLE | PSW_PROT_DISABLE) | PSW_INTR_DISABLE`, the
// way a kernel actually spells a mode-word mask.  The TAC-level folding the back end used to
// rely on is neither: it collapsed one level and left anything deeper as a live OR node, so a
// three-term mask reached the back end unfolded and was rejected as "not a constant" — with a
// diagnostic pointing at the one thing that was not wrong.  Folding in the front end also makes
// the contract independent of the optimizer flags.
//
// Returns the new argument list head; `args` must be non-NULL (arity is checked by the
// prototype in the target's header before this runs).
//
static Expr *fold_immediate_arg0(Expr *args, const char *name)
{
    const ImmediateArg *imm = target_config->immediate_args;
    while (imm && imm->name && strcmp(name, imm->name) != 0)
        imm++;
    if (!imm || !imm->name)
        return args; // not one of them

    long val = 0;
    if (!try_eval_const_int(args, &val))
        fatal_error("intrinsic '%s': the %s must be a constant", name, imm->what);
    if (val < imm->lo || val > imm->hi)
        fatal_error("intrinsic '%s': %s 0%lo %s", name, imm->what, val, imm->range);

    Expr *lit                 = new_expression(EXPR_LITERAL);
    lit->u.literal            = new_literal(LITERAL_INT);
    lit->u.literal->u.int_val = (int)val;
    lit                       = typecheck_literal(lit);

    lit->next  = args->next;
    args->next = NULL; // free_expression walks the ->next chain
    free_expression(args);
    return lit;
}

// Type-check an expression.
// C11 §6.5.2.2: default argument promotions for variadic trailing arguments.
static Expr *promote_variadic_arg(Expr *e)
{
    e              = typecheck_and_decay(e);
    const Type *et = unalias(e->type);
    if (is_promotable_narrow(et))
        e = convert_to_kind(e, promoted_kind(et));
    else if (et->kind == TYPE_FLOAT)
        e = convert_to_kind(e, TYPE_DOUBLE);
    return e;
}

// Check the arguments of a call against function type `fn_type` (C11 §6.5.2.2): their
// number, then each converted as by assignment to its parameter's type, or promoted
// past the last one of a variadic function.  Returns the new argument list.
Expr *typecheck_call_args(const Type *fn_type, Expr *args, const char *name)
{
    const Param *params = params_for_call(fn_type);
    const bool variadic = fn_type->u.function.variadic;
    int param_count = 0, arg_count = 0;
    for (const Param *p = params; p; p = p->next)
        param_count++;
    for (const Expr *a = args; a; a = a->next)
        arg_count++;
    if (arg_count < param_count || (!variadic && arg_count > param_count)) {
        char callee[256] = "";
        if (name)
            snprintf(callee, sizeof(callee), " '%s'", name);
        diag_error(diag_loc, "too %s arguments to function%s (expected %s%d, have %d)",
                   arg_count < param_count ? "few" : "many", callee, variadic ? "at least " : "",
                   param_count, arg_count);
        const Symbol *sym = name ? symtab_get_opt(name) : NULL;
        if (sym && sym->loc.line > 0)
            diag_note(sym->loc, "'%s' declared here", name);
        diag_unwind();
    }
    int arg_number = 0;
    Expr *arg = args, *prev = NULL, *new_args = NULL;
    const Param *p = params;
    while (arg) {
        Expr *arg_next = arg->next;
        arg->next      = NULL;
        Expr *new_arg;
        arg_number++;
        if (p) {
            char context[300];
            if (name)
                snprintf(context, sizeof(context), "passing argument %d of '%s'", arg_number, name);
            else
                snprintf(context, sizeof(context), "passing argument %d", arg_number);
            new_arg = coerce_for_assignment(typecheck_and_decay(arg), p->type, context);
            p       = p->next;
        } else {
            new_arg = promote_variadic_arg(arg);
        }
        if (!new_args)
            new_args = new_arg;
        if (prev)
            prev->next = new_arg;
        prev = new_arg;
        arg  = arg_next;
    }
    return new_args;
}

static Expr *typecheck_expr(Expr *e);

static Expr *typecheck_expr_at(Expr *e)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    if (!e)
        return NULL;
    switch (e->kind) {
    case EXPR_VAR:
        return typecheck_var(e);
    case EXPR_LITERAL:
        return typecheck_literal(e);
    case EXPR_CAST: {
        e->u.cast.type       = check_type_name(e->u.cast.type);
        Expr *inner          = typecheck_and_decay(e->u.cast.expr);
        const Type *cast_ty  = unalias(e->u.cast.type);
        const Type *inner_ty = unalias(inner->type);
        if ((cast_ty->kind == TYPE_DOUBLE && is_pointer(inner_ty)) ||
            (is_pointer(cast_ty) && inner_ty->kind == TYPE_DOUBLE)) {
            fatal_error("cannot cast '%s' to '%s'", type_of(inner), type_to_c(e->u.cast.type));
        }
        if (cast_ty->kind == TYPE_VOID) {
            free_type(e->type);
            e->type        = clone_type(e->u.cast.type, __func__, __FILE__, __LINE__);
            e->u.cast.expr = inner;
            return e;
        }
        if (!is_scalar(e->u.cast.type) || !is_scalar(inner->type)) {
            fatal_error("cannot cast '%s' to '%s'", type_of(inner), type_to_c(e->u.cast.type));
        }
        free_type(e->type);
        e->type        = clone_type(e->u.cast.type, __func__, __FILE__, __LINE__);
        e->u.cast.expr = inner;
        return e;
    }
    case EXPR_UNARY_OP: {
        switch (e->u.unary_op.op) {
        case UNARY_LOG_NOT: {
            free_type(e->type);
            Expr *inner        = typecheck_scalar(e->u.unary_op.expr);
            e->type            = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
            e->u.unary_op.expr = inner;
            return e;
        }
        case UNARY_BIT_NOT: {
            Expr *inner = typecheck_and_decay(e->u.unary_op.expr);
            if (!is_integer(inner->type)) {
                fatal_error("invalid argument type '%s' to unary '~'", type_of(inner));
            }
            const Type *it = unalias(inner->type);
            if (is_promotable_narrow(it))
                inner = convert_to_kind(inner, promoted_kind(it));
            free_type(e->type);
            e->type            = clone_type(inner->type, __func__, __FILE__, __LINE__);
            e->u.unary_op.expr = inner;
            return e;
        }
        case UNARY_PLUS:
        case UNARY_NEG: {
            Expr *inner = typecheck_and_decay(e->u.unary_op.expr);
            if (!is_arithmetic(inner->type)) {
                fatal_error("invalid argument type '%s' to unary '%s'", type_of(inner),
                            e->u.unary_op.op == UNARY_NEG ? "-" : "+");
            }
            const Type *it = unalias(inner->type);
            if (is_promotable_narrow(it))
                inner = convert_to_kind(inner, promoted_kind(it));
            free_type(e->type);
            e->type            = clone_type(inner->type, __func__, __FILE__, __LINE__);
            e->u.unary_op.expr = inner;
            return e;
        }
        case UNARY_DEREF: {
            Expr *inner = typecheck_and_decay(e->u.unary_op.expr);
            if (!is_pointer(inner->type)) {
                fatal_error("invalid argument type '%s' to unary '*'", type_of(inner));
            }
            const Type *ptr_type = unalias(inner->type);
            if (unalias(ptr_type->u.pointer.target)->kind == TYPE_VOID) {
                fatal_error("cannot dereference '%s'", type_of(inner));
            }
            free_type(e->type);
            e->type = clone_type(ptr_type->u.pointer.target, __func__, __FILE__, __LINE__);
            e->u.unary_op.expr = inner;
            return e;
        }
        case UNARY_ADDRESS: {
            Expr *inner = typecheck_expr(e->u.unary_op.expr);
            // A string literal is an lvalue (an array object with static storage), so
            // &"..." yields a pointer to its char[N] type.  inner->type is already char[N].
            bool is_string_literal =
                inner->kind == EXPR_LITERAL && inner->u.literal->kind == LITERAL_STRING;
            if (!is_lvalue(inner) && !is_string_literal) {
                fatal_error("cannot take the address of an rvalue of type '%s'", type_of(inner));
            }
            if (access_bitfield(inner)) {
                fatal_error("cannot take the address of a bit-field");
            }
            Type *ptr             = new_type(TYPE_POINTER, __func__, __FILE__, __LINE__);
            ptr->u.pointer.target = clone_type(inner->type, __func__, __FILE__, __LINE__);
            free_type(e->type);
            e->type            = ptr;
            e->u.unary_op.expr = inner;
            return e;
        }
        case UNARY_PRE_INC:
        case UNARY_PRE_DEC: {
            const char *what = e->u.unary_op.op == UNARY_PRE_INC ? "increment" : "decrement";
            if (is_function_designator(e->u.unary_op.expr)) {
                fatal_error("expression is not assignable");
            }
            Expr *inner = typecheck_expr(e->u.unary_op.expr);
            if (is_array_lvalue_operand(inner)) {
                fatal_error("array type '%s' is not assignable", type_of(inner));
            }
            inner = decay_expr(inner);
            if (!is_lvalue(inner)) {
                fatal_error("expression is not assignable");
            }
            check_modifiable(inner);
            if (!is_scalar(inner->type)) {
                fatal_error("cannot %s value of type '%s'", what, type_of(inner));
            }
            if (is_pointer(inner->type) && !is_complete_pointer(inner->type)) {
                fatal_error("arithmetic on a pointer to an incomplete type '%s'", type_of(inner));
            }
            free_type(e->type);
            e->type            = clone_type(inner->type, __func__, __FILE__, __LINE__);
            e->u.unary_op.expr = inner;
            return e;
        }
        default:
            internal_error("unsupported unary op %d", e->u.unary_op.op);
        }
    }
    case EXPR_BINARY_OP: {
        Expr *e1 = e->u.binary_op.left, *e2 = e->u.binary_op.right;
        switch (e->u.binary_op.op) {
        case BINARY_LOG_AND:
        case BINARY_LOG_OR: {
            e1 = typecheck_scalar(e1);
            e2 = typecheck_scalar(e2);
            free_type(e->type);
            e->type              = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
            e->u.binary_op.left  = e1;
            e->u.binary_op.right = e2;
            return e;
        }
        case BINARY_COMMA: {
            // C11 6.5.17p2: the left operand is evaluated as a void expression, then
            // discarded; the result has the type and value of the right operand after
            // lvalue conversion.  So: no usual arithmetic conversions, no scalar
            // requirement, and either operand may be void -- decay_expr() rewrites only
            // array and function types, so a void operand passes straight through.
            e1 = typecheck_and_decay(e1);
            e2 = typecheck_and_decay(e2);
            free_type(e->type);
            e->type              = clone_type(e2->type, __func__, __FILE__, __LINE__);
            e->u.binary_op.left  = e1;
            e->u.binary_op.right = e2;
            return e;
        }
        case BINARY_ADD: {
            e1 = typecheck_and_decay(e1);
            e2 = typecheck_and_decay(e2);
            free_type(e->type);
            if (is_arithmetic(e1->type) && is_arithmetic(e2->type)) {
                const Type *common = get_common_type(e1->type, e2->type);
                e1                 = convert_to_type(e1, common);
                e2                 = convert_to_type(e2, common);
                e->type            = clone_type(common, __func__, __FILE__, __LINE__);
            } else if (is_complete_pointer(e1->type) && is_integer(e2->type)) {
                e2      = convert_to_kind(e2, ptrdiff_kind());
                e->type = clone_type(e1->type, __func__, __FILE__, __LINE__);
            } else if (is_complete_pointer(e2->type) && is_integer(e1->type)) {
                e1      = convert_to_kind(e1, ptrdiff_kind());
                e->type = clone_type(e2->type, __func__, __FILE__, __LINE__);
            } else {
                invalid_operands("+", e1, e2);
            }
            e->u.binary_op.left  = e1;
            e->u.binary_op.right = e2;
            return e;
        }
        case BINARY_SUB: {
            e1 = typecheck_and_decay(e1);
            e2 = typecheck_and_decay(e2);
            free_type(e->type);
            if (is_arithmetic(e1->type) && is_arithmetic(e2->type)) {
                const Type *common = get_common_type(e1->type, e2->type);
                e1                 = convert_to_type(e1, common);
                e2                 = convert_to_type(e2, common);
                e->type            = clone_type(common, __func__, __FILE__, __LINE__);
            } else if (is_complete_pointer(e1->type) && is_integer(e2->type)) {
                e2      = convert_to_kind(e2, ptrdiff_kind());
                e->type = clone_type(e1->type, __func__, __FILE__, __LINE__);
            } else if (is_complete_pointer(e1->type) &&
                       unalias(e1->type)->kind == unalias(e2->type)->kind) {
                if (!compatible_type(e1->type, e2->type))
                    fatal_error("incompatible pointer types ('%s' and '%s')", type_of(e1),
                                type_of(e2));
                e->type = new_type(ptrdiff_kind(), __func__, __FILE__, __LINE__);
            } else {
                invalid_operands("-", e1, e2);
            }
            e->u.binary_op.left  = e1;
            e->u.binary_op.right = e2;
            return e;
        }
        case BINARY_MUL:
        case BINARY_DIV:
        case BINARY_MOD: {
            e1 = typecheck_and_decay(e1);
            e2 = typecheck_and_decay(e2);
            if (!is_arithmetic(e1->type) || !is_arithmetic(e2->type) ||
                (e->u.binary_op.op == BINARY_MOD &&
                 (!is_integer(e1->type) || !is_integer(e2->type)))) {
                invalid_operands(binary_op_text(e->u.binary_op.op), e1, e2);
            }
            const Type *common = get_common_type(e1->type, e2->type);
            e1                 = convert_to_type(e1, common);
            e2                 = convert_to_type(e2, common);
            free_type(e->type);
            e->type              = clone_type(common, __func__, __FILE__, __LINE__);
            e->u.binary_op.left  = e1;
            e->u.binary_op.right = e2;
            return e;
        }
        case BINARY_EQ:
        case BINARY_NE: {
            e1 = typecheck_and_decay(e1);
            e2 = typecheck_and_decay(e2);
            if (unalias(e1->type)->kind == TYPE_VOID || unalias(e2->type)->kind == TYPE_VOID) {
                invalid_operands(binary_op_text(e->u.binary_op.op), e1, e2);
            }
            if (!is_scalar(e1->type) || !is_scalar(e2->type)) {
                invalid_operands(binary_op_text(e->u.binary_op.op), e1, e2);
            }
            const Type *common = is_pointer(e1->type) || is_pointer(e2->type)
                                     ? common_pointer_type(e1, e2)
                                     : get_common_type(e1->type, e2->type);
            e1                 = convert_to_type(e1, common);
            e2                 = convert_to_type(e2, common);
            free_type(e->type);
            e->type              = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
            e->u.binary_op.left  = e1;
            e->u.binary_op.right = e2;
            return e;
        }
        case BINARY_LT:
        case BINARY_GT:
        case BINARY_LE:
        case BINARY_GE: {
            e1 = typecheck_and_decay(e1);
            e2 = typecheck_and_decay(e2);
            if (is_complete_pointer(e1->type) && is_complete_pointer(e2->type) &&
                !compatible_type(e1->type, e2->type))
                fatal_error("incompatible pointer types ('%s' and '%s')", type_of(e1), type_of(e2));
            const Type *common =
                is_arithmetic(e1->type) && is_arithmetic(e2->type)
                    ? get_common_type(e1->type, e2->type)
                    : (is_complete_pointer(e1->type) && is_complete_pointer(e2->type) ? e1->type
                                                                                      : NULL);
            if (!common) {
                invalid_operands(binary_op_text(e->u.binary_op.op), e1, e2);
            }
            e1 = convert_to_type(e1, common);
            e2 = convert_to_type(e2, common);
            free_type(e->type);
            e->type              = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
            e->u.binary_op.left  = e1;
            e->u.binary_op.right = e2;
            return e;
        }
        case BINARY_BIT_AND:
        case BINARY_BIT_XOR:
        case BINARY_BIT_OR: {
            e1 = typecheck_and_decay(e1);
            e2 = typecheck_and_decay(e2);
            if (!is_integer(e1->type) || !is_integer(e2->type)) {
                invalid_operands(binary_op_text(e->u.binary_op.op), e1, e2);
            }
            const Type *common = get_common_type(e1->type, e2->type);
            e1                 = convert_to_type(e1, common);
            e2                 = convert_to_type(e2, common);
            free_type(e->type);
            e->type              = clone_type(common, __func__, __FILE__, __LINE__);
            e->u.binary_op.left  = e1;
            e->u.binary_op.right = e2;
            return e;
        }
        case BINARY_LEFT_SHIFT:
        case BINARY_RIGHT_SHIFT: {
            e1 = typecheck_and_decay(e1);
            e2 = typecheck_and_decay(e2);
            if (!is_integer(e1->type) || !is_integer(e2->type)) {
                invalid_operands(binary_op_text(e->u.binary_op.op), e1, e2);
            }
            const Type *t1 = unalias(e1->type), *t2 = unalias(e2->type);
            if (is_promotable_narrow(t1)) {
                e1 = convert_to_kind(e1, promoted_kind(t1));
            }
            if (is_promotable_narrow(t2)) {
                e2 = convert_to_kind(e2, promoted_kind(t2));
            }
            free_type(e->type);
            e->type              = clone_type(e1->type, __func__, __FILE__, __LINE__);
            e->u.binary_op.left  = e1;
            e->u.binary_op.right = e2;
            return e;
        }
        default:
            internal_error("unsupported binary op %d", e->u.binary_op.op);
        }
    }
    case EXPR_ASSIGN: {
        if (is_function_designator(e->u.assign.target)) {
            fatal_error("expression is not assignable");
        }
        Expr *lhs = typecheck_expr(e->u.assign.target);
        if (is_array_lvalue_operand(lhs)) {
            fatal_error("array type '%s' is not assignable", type_of(lhs));
        }
        lhs = decay_expr(lhs);
        if (!is_lvalue(lhs)) {
            fatal_error("expression is not assignable");
        }
        check_modifiable(lhs);
        Expr *rhs = typecheck_and_decay(e->u.assign.value);
        if (e->u.assign.op == ASSIGN_SIMPLE) {
            rhs = coerce_for_assignment(rhs, lhs->type, "assigning");
            if (lhs->kind == EXPR_VAR)
                coro_lint_bind(lhs->u.var, symtab_level(lhs->u.var), rhs);
            else
                coro_lint_bind(NULL, -1, rhs); // through a pointer or into a member
        } else if ((e->u.assign.op == ASSIGN_ADD || e->u.assign.op == ASSIGN_SUB) &&
                   is_complete_pointer(lhs->type)) {
            if (!is_integer(rhs->type))
                invalid_operands(assign_op_text(e->u.assign.op), lhs, rhs);
            rhs = convert_to_kind(rhs, ptrdiff_kind());
        } else {
            if (!is_arithmetic(lhs->type) || !is_arithmetic(rhs->type))
                invalid_operands(assign_op_text(e->u.assign.op), lhs, rhs);
            // Bitwise, shift, and remainder compound assignments are integer-only.
            switch (e->u.assign.op) {
            case ASSIGN_MOD:
            case ASSIGN_LEFT:
            case ASSIGN_RIGHT:
            case ASSIGN_AND:
            case ASSIGN_XOR:
            case ASSIGN_OR:
                if (!is_integer(lhs->type) || !is_integer(rhs->type))
                    invalid_operands(assign_op_text(e->u.assign.op), lhs, rhs);
                break;
            default:
                break;
            }
            // An arithmetic compound op (+= -= *= /= %=) is `lhs = lhs op rhs` (C11
            // §6.5.16.2p3): computed in get_common_type(lhs, rhs) — the promoted lvalue,
            // or a wider or floating rhs type (`int i; i /= 1L << 40`, `i *= 2.5`) — and
            // the result converted back to the lvalue type.  The translator notices the
            // differing operand type and widens/narrows around the op.  Shift and bitwise
            // ops keep converting the rhs to the lvalue type (a shift count is promoted
            // independently; a bitwise result truncated to the lvalue has the right bits).
            const Type *lt   = unalias(lhs->type);
            bool is_arith_op = e->u.assign.op == ASSIGN_ADD || e->u.assign.op == ASSIGN_SUB ||
                               e->u.assign.op == ASSIGN_MUL || e->u.assign.op == ASSIGN_DIV ||
                               e->u.assign.op == ASSIGN_MOD;
            // A _Bool lvalue promotes for *every* compound operator, not just the
            // arithmetic ones: `b <<= 1` and `b |= 4` are `b = b op x` with the result
            // converted back to _Bool (C11 §6.5.16.2p3, §6.3.1.2), and the translator
            // performs that conversion — the emit_cast that re-normalises to 0/1 — only
            // on this promoted path, where the operation type differs from the lvalue's.
            // A same-size integer common type changes nothing for + - * (the low bits
            // agree) or when the signedness also agrees, so the lvalue type is kept: on
            // BESM-6 a 48-bit unsigned result copied back to a 41-bit int would not be a
            // valid int.
            // A bit-field lvalue takes part with its promoted type (`unsigned u:3; u /= -2`
            // divides as int), and is always converted back.
            TypeKind bpk  = bitfield_promoted_kind(lhs);
            Type promoted = { .kind = bpk };
            const Type *common =
                get_common_type(bpk != TYPE_VOID ? &promoted : lhs->type, rhs->type);
            bool additive    = e->u.assign.op == ASSIGN_ADD || e->u.assign.op == ASSIGN_SUB ||
                               e->u.assign.op == ASSIGN_MUL;
            bool same_as_lhs = bpk == TYPE_VOID && is_integer(common) && is_integer(lt) &&
                               get_size(common) == get_size(lt) &&
                               (additive || is_signed(common) == is_signed(lt)) &&
                               lt->kind != TYPE_BOOL && !is_promotable_narrow(lt);
            if ((is_arith_op && !same_as_lhs) || lt->kind == TYPE_BOOL) {
                rhs = convert_to_type(rhs, common);
            } else {
                rhs = convert_to_type(rhs, lhs->type);
            }
        }
        free_type(e->type);
        e->type            = clone_type(lhs->type, __func__, __FILE__, __LINE__);
        e->u.assign.target = lhs;
        e->u.assign.value  = rhs;
        return e;
    }
    case EXPR_COND: {
        Expr *cond      = typecheck_scalar(e->u.cond.condition);
        Expr *then_expr = typecheck_and_decay(e->u.cond.then_expr);
        Expr *else_expr = typecheck_and_decay(e->u.cond.else_expr);
        const Type *result_type;
        const Type *then_ty = unalias(then_expr->type);
        const Type *else_ty = unalias(else_expr->type);
        if (then_ty->kind == TYPE_VOID && else_ty->kind == TYPE_VOID) {
            // A void/void conditional has type void; both operands stay as-is
            // (no conversion needed).  Own the result type directly so it is not
            // leaked by the clone below.
            free_type(e->type);
            e->type             = new_type(TYPE_VOID, __func__, __FILE__, __LINE__);
            e->u.cond.condition = cond;
            e->u.cond.then_expr = then_expr;
            e->u.cond.else_expr = else_expr;
            return e;
        } else if (is_pointer(then_expr->type) || is_pointer(else_expr->type)) {
            result_type = common_pointer_type(then_expr, else_expr);
        } else if (is_arithmetic(then_expr->type) && is_arithmetic(else_expr->type)) {
            result_type = get_common_type(then_expr->type, else_expr->type);
        } else if (then_ty->kind == else_ty->kind) {
            // For struct/union operands the tags must match, too.
            if ((then_ty->kind == TYPE_STRUCT || then_ty->kind == TYPE_UNION) &&
                strcmp(then_ty->u.struct_t.name, else_ty->u.struct_t.name) != 0) {
                fatal_error("incompatible operand types in '?:' ('%s' and '%s')",
                            type_of(then_expr), type_of(else_expr));
            }
            result_type = then_expr->type;
        } else {
            fatal_error("incompatible operand types in '?:' ('%s' and '%s')", type_of(then_expr),
                        type_of(else_expr));
        }
        free_type(e->type);
        e->type             = clone_type(result_type, __func__, __FILE__, __LINE__);
        e->u.cond.condition = cond;
        e->u.cond.then_expr = convert_to_type(then_expr, result_type);
        e->u.cond.else_expr = convert_to_type(else_expr, result_type);
        return e;
    }
    case EXPR_CALL: {
        Expr *func = e->u.call.func;
        const Type *fn_type;
        if (func->kind == EXPR_VAR) {
            const Symbol *sym = symtab_get(func->u.var);
            if (!coroutine_call_allowed(e))
                check_coroutine_name(sym);
            if (strcmp(func->u.var, "__builtin_alloca") == 0)
                check_alloca_call();
            // va_start expands to __va_start(&ap) on the targets that have it.
            if (strcmp(func->u.var, "__va_start") == 0 &&
                !(typecheck_function && typecheck_function->u.function.variadic))
                fatal_error("'va_start' used in a function with fixed arguments");
            // Type the callee node from its symbol.  A bare name is not decayed here (the
            // call names it directly), but it must still carry its type: a function
            // designator's is a function type and a function-pointer variable's is a
            // pointer, and that is the only thing that later tells a direct call from a
            // call through a pointer.  The symbol table cannot answer it later — locals and
            // parameters are scoped and purged on block exit, long before TAC lowering runs.
            free_type(func->type);
            func->type = clone_type(sym->type, __func__, __FILE__, __LINE__);
            fn_type    = unalias(func->type);
            if (coro_desc_target(fn_type)) {
                free_type(e->type);
                e->type = clone_type(typecheck_coro_ptr_call(e, coro_desc_target(fn_type)),
                                     __func__, __FILE__, __LINE__);
                return e;
            }
            if (fn_type->kind == TYPE_POINTER)
                fn_type = unalias(fn_type->u.pointer.target); // function pointer decay
            if (fn_type->kind != TYPE_FUNCTION)
                fatal_error("called object type '%s' is not a function or function pointer",
                            type_of(func));
        } else {
            func    = typecheck_and_decay(func);
            fn_type = unalias(func->type);
            if (coro_desc_target(fn_type)) {
                e->u.call.func = func;
                free_type(e->type);
                e->type = clone_type(typecheck_coro_ptr_call(e, coro_desc_target(fn_type)),
                                     __func__, __FILE__, __LINE__);
                return e;
            }
            if (fn_type->kind == TYPE_POINTER)
                fn_type = unalias(fn_type->u.pointer.target);
            if (fn_type->kind != TYPE_FUNCTION)
                fatal_error("called object type '%s' is not a function or function pointer",
                            type_of(func));
            e->u.call.func = func;
        }
        // A function is named by its own name; a local pointer to one has a backend name.
        const char *callee = func->kind == EXPR_VAR && unalias(func->type)->kind == TYPE_FUNCTION
                                 ? func->u.var
                                 : NULL;
        Expr *new_args     = typecheck_call_args(fn_type, e->u.call.args, callee);
        // The intrinsics whose first argument the front end must constant-fold: an extracode's
        // opcode, a mode-word mask and a halt code are immediate fields of the instruction
        // word, not values.
        if (func->kind == EXPR_VAR && new_args)
            new_args = fold_immediate_arg0(new_args, func->u.var);

        free_type(e->type);
        e->type        = clone_type(fn_type->u.function.return_type, __func__, __FILE__, __LINE__);
        e->u.call.args = new_args;
        return e;
    }
    case EXPR_SUBSCRIPT: {
        Expr *ptr   = typecheck_and_decay(e->u.subscript.left);
        Expr *index = typecheck_and_decay(e->u.subscript.right);
        const Type *result_type;
        if (is_complete_pointer(ptr->type) && is_integer(index->type)) {
            result_type = unalias(ptr->type)->u.pointer.target;
            index       = convert_to_kind(index, ptrdiff_kind());
        } else if (is_complete_pointer(index->type) && is_integer(ptr->type)) {
            result_type = unalias(index->type)->u.pointer.target;
            ptr         = convert_to_kind(ptr, ptrdiff_kind());
        } else {
            invalid_operands("[]", ptr, index);
        }
        free_type(e->type);
        e->type              = clone_type(result_type, __func__, __FILE__, __LINE__);
        e->u.subscript.left  = ptr;
        e->u.subscript.right = index;
        return e;
    }
    case EXPR_SIZEOF_EXPR: {
        Expr *inner = typecheck_expr(e->u.sizeof_expr);
        if (unalias(inner->type)->kind == TYPE_FUNCTION) {
            fatal_error("invalid application of 'sizeof' to a function type");
        }
        if (access_bitfield(inner)) {
            fatal_error("invalid application of 'sizeof' to a bit-field");
        }
        if (!is_complete(inner->type)) {
            fatal_error("invalid application of 'sizeof' to an incomplete type '%s'",
                        type_of(inner));
        }
        free_type(e->type);
        e->type          = new_type(size_kind(), __func__, __FILE__, __LINE__);
        e->u.sizeof_expr = inner;
        return e;
    }
    case EXPR_SIZEOF_TYPE: {
        e->u.sizeof_type = check_type_name(e->u.sizeof_type);
        if (!is_complete(e->u.sizeof_type)) {
            fatal_error("invalid application of 'sizeof' to an incomplete type '%s'",
                        type_to_c(e->u.sizeof_type));
        }
        free_type(e->type);
        e->type = new_type(size_kind(), __func__, __FILE__, __LINE__);
        return e;
    }
    case EXPR_ALIGNOF: {
        e->u.align_of = check_type_name(e->u.align_of);
        if (!is_complete(e->u.align_of)) {
            fatal_error("invalid application of '_Alignof' to an incomplete type '%s'",
                        type_to_c(e->u.align_of));
        }
        free_type(e->type);
        e->type = new_type(size_kind(), __func__, __FILE__, __LINE__);
        return e;
    }
    case EXPR_VA_CLASS: {
        if (!target_config->va_class)
            fatal_error("'__builtin_va_class' is not supported on target '%s'",
                        target_config->name);
        e->u.va_class = check_type_name(e->u.va_class);
        if (!is_complete(e->u.va_class)) {
            fatal_error("invalid application of '__builtin_va_class' to an incomplete type '%s'",
                        type_to_c(e->u.va_class));
        }
        free_type(e->type);
        e->type = new_type(TYPE_INT, __func__, __FILE__, __LINE__);
        return e;
    }
    case EXPR_YIELD:
        return typecheck_yield(e);
    case EXPR_AWAIT:
        return typecheck_await(e);
    case EXPR_CO_OP:
        return typecheck_co_op(e);
    case EXPR_FIELD_ACCESS: {
        Expr *strct          = typecheck_and_decay(e->u.field_access.expr);
        const Type *strct_ty = unalias(strct->type);
        if (strct_ty->kind != TYPE_STRUCT && strct_ty->kind != TYPE_UNION) {
            fatal_error("member reference base type '%s' is not a structure or union",
                        type_of(strct));
        }
        const StructDef *entry = structtab_find(strct_ty->u.struct_t.name);
        const FieldDef *member = entry->members;
        for (; member; member = member->next) {
            if (member->name && strcmp(member->name, e->u.field_access.field) == 0) {
                break;
            }
        }
        if (!member) {
            fatal_error("no member named '%s' in '%s'", e->u.field_access.field,
                        type_to_c(strct_ty));
        }
        assert(member);
        free_type(e->type);
        e->type                  = clone_type(member->type, __func__, __FILE__, __LINE__);
        e->u.field_access.offset = member->offset;
        e->u.field_access.bf     = member->bf;
        // Stash the member's declared type alongside its offset: the tag may be block-local
        // and purged by the time the translator needs to know how the member is addressed,
        // and e->type is about to be decayed to a pointer for an array-typed member.
        free_type(e->u.field_access.member_type);
        e->u.field_access.member_type = clone_type(member->type, __func__, __FILE__, __LINE__);
        e->u.field_access.expr        = strct;
        return e;
    }
    case EXPR_PTR_ACCESS: {
        Expr *strct_ptr      = typecheck_and_decay(e->u.ptr_access.expr);
        const Type *ptr_type = unalias(strct_ptr->type);
        if (!is_pointer(ptr_type) || (unalias(ptr_type->u.pointer.target)->kind != TYPE_STRUCT &&
                                      unalias(ptr_type->u.pointer.target)->kind != TYPE_UNION)) {
            fatal_error("member reference type '%s' is not a pointer to a structure or union",
                        type_of(strct_ptr));
        }
        const Type *target_type = unalias(ptr_type->u.pointer.target);
        const StructDef *entry  = structtab_find(target_type->u.struct_t.name);
        const FieldDef *member  = entry->members;
        for (; member; member = member->next) {
            if (member->name && strcmp(member->name, e->u.ptr_access.field) == 0) {
                break;
            }
        }
        if (!member) {
            fatal_error("no member named '%s' in '%s'", e->u.ptr_access.field,
                        type_to_c(target_type));
        }
        assert(member);
        free_type(e->type);
        e->type                = clone_type(member->type, __func__, __FILE__, __LINE__);
        e->u.ptr_access.offset = member->offset;
        e->u.ptr_access.bf     = member->bf;
        // See EXPR_FIELD_ACCESS above.
        free_type(e->u.ptr_access.member_type);
        e->u.ptr_access.member_type = clone_type(member->type, __func__, __FILE__, __LINE__);
        e->u.ptr_access.expr        = strct_ptr;
        return e;
    }
    case EXPR_POST_INC: {
        if (is_function_designator(e->u.post_inc)) {
            fatal_error("expression is not assignable");
        }
        Expr *inner = typecheck_expr(e->u.post_inc);
        if (is_array_lvalue_operand(inner)) {
            fatal_error("array type '%s' is not assignable", type_of(inner));
        }
        inner = decay_expr(inner);
        if (!is_lvalue(inner)) {
            fatal_error("expression is not assignable");
        }
        check_modifiable(inner);
        if (!is_scalar(inner->type)) {
            fatal_error("cannot increment value of type '%s'", type_of(inner));
        }
        if (is_pointer(inner->type) && !is_complete_pointer(inner->type)) {
            fatal_error("arithmetic on a pointer to an incomplete type '%s'", type_of(inner));
        }
        free_type(e->type);
        e->type       = clone_type(inner->type, __func__, __FILE__, __LINE__);
        e->u.post_inc = inner;
        return e;
    }
    case EXPR_POST_DEC: {
        if (is_function_designator(e->u.post_dec)) {
            fatal_error("expression is not assignable");
        }
        Expr *inner = typecheck_expr(e->u.post_dec);
        if (is_array_lvalue_operand(inner)) {
            fatal_error("array type '%s' is not assignable", type_of(inner));
        }
        inner = decay_expr(inner);
        if (!is_lvalue(inner)) {
            fatal_error("expression is not assignable");
        }
        check_modifiable(inner);
        if (!is_scalar(inner->type)) {
            fatal_error("cannot decrement value of type '%s'", type_of(inner));
        }
        if (is_pointer(inner->type) && !is_complete_pointer(inner->type)) {
            fatal_error("arithmetic on a pointer to an incomplete type '%s'", type_of(inner));
        }
        free_type(e->type);
        e->type       = clone_type(inner->type, __func__, __FILE__, __LINE__);
        e->u.post_dec = inner;
        return e;
    }
    case EXPR_GENERIC: {
        // Controlling expression is not evaluated; only its type is used for matching.
        const Expr *ctrl      = typecheck_and_decay(e->u.generic.controlling_expr);
        const Type *ctrl_type = ctrl->type;

        GenericAssoc *selected      = NULL;
        GenericAssoc *default_assoc = NULL;
        for (GenericAssoc *ga = e->u.generic.associations; ga; ga = ga->next) {
            if (ga->kind == GENERIC_ASSOC_TYPE) {
                ga->u.type_assoc.type = check_type_name(ga->u.type_assoc.type);
                ga->u.type_assoc.expr = typecheck_and_decay(ga->u.type_assoc.expr);
                if (!selected && compare_type(unalias(ctrl_type), unalias(ga->u.type_assoc.type))) {
                    selected = ga;
                }
            } else {
                if (default_assoc)
                    fatal_error("duplicate 'default' association in '_Generic'");
                ga->u.default_assoc = typecheck_and_decay(ga->u.default_assoc);
                default_assoc       = ga;
            }
        }

        GenericAssoc *match = selected ? selected : default_assoc;
        if (!match)
            fatal_error("no association in '_Generic' matches type '%s'", type_to_c(ctrl_type));
        assert(match);

        const Expr *match_expr =
            (match->kind == GENERIC_ASSOC_TYPE) ? match->u.type_assoc.expr : match->u.default_assoc;
        free_type(e->type);
        e->type = clone_type(match_expr->type, __func__, __FILE__, __LINE__);

        // Prune to the selected association so TAC lowering sees exactly one branch.
        for (GenericAssoc *ga = e->u.generic.associations, *nxt; ga; ga = nxt) {
            nxt = ga->next;
            if (ga == match) {
                match->next = NULL;
                continue;
            }
            if (ga->kind == GENERIC_ASSOC_TYPE) {
                free_type(ga->u.type_assoc.type);
                free_expression(ga->u.type_assoc.expr);
            } else {
                free_expression(ga->u.default_assoc);
            }
            xfree(ga);
        }
        e->u.generic.associations = match;
        free_expression(e->u.generic.controlling_expr);
        e->u.generic.controlling_expr = NULL;
        return e;
    }
    case EXPR_COMPOUND: {
        e->u.compound_literal.type = check_type_name(e->u.compound_literal.type);
        Type *lit_type             = e->u.compound_literal.type;
        if (!is_complete(lit_type)) {
            fatal_error("compound literal has incomplete type '%s'", type_to_c(lit_type));
        }
        TypeKind kind = unalias(lit_type)->kind;
        if (kind == TYPE_ARRAY || kind == TYPE_STRUCT || kind == TYPE_UNION) {
            // Wrap InitItem list in a temporary INITIALIZER_COMPOUND to reuse typecheck_init.
            Initializer *wrap   = new_initializer(INITIALIZER_COMPOUND);
            wrap->u.items       = e->u.compound_literal.init;
            Initializer *result = typecheck_init(lit_type, wrap);
            if (result->kind == INITIALIZER_SINGLE) {
                // A string for a char array: keep it as the only item.
                e->u.compound_literal.init = new_init_item(NULL, result);
            } else {
                // Detach the type-checked items and free the wrapper shell.
                e->u.compound_literal.init = result->u.items;
                result->u.items            = NULL;
                free_initializer(result);
            }
        } else {
            // Scalar: C11 allows {expr} for a scalar type; typecheck the single item.
            InitItem *item = e->u.compound_literal.init;
            if (!item || item->next) {
                fatal_error("a scalar compound literal takes exactly one initializer");
            }
            assert(item);
            item->init = typecheck_init(lit_type, item->init);
        }
        free_type(e->type);
        e->type = clone_type(lit_type, __func__, __FILE__, __LINE__);
        return e;
    }
    default:
        internal_error("unsupported expression kind %d", e->kind);
    }
}

// typecheck_expr with diag_loc at the node, for the errors found in it.
static Expr *typecheck_expr(Expr *e)
{
    SrcLoc saved = diag_enter(e ? e->loc : diag_loc);
    Expr *result = typecheck_expr_at(e);
    diag_loc     = saved;
    return result;
}

// Type-check an expression and apply array-to-pointer decay.
// Apply the lvalue conversions of C11 6.3.2.1 to an already-type-checked
// expression: an array decays to a pointer to its element, a function to a
// function pointer; an incomplete struct/union is rejected.  Split out of
// typecheck_and_decay() so callers that must inspect the *pre-decay* type
// (e.g. to reject an array as a modifiable lvalue) can decay after their check.
static Expr *decay_expr(Expr *typed)
{
    const Type *vt = unalias(typed->type);
    if ((vt->kind == TYPE_STRUCT || vt->kind == TYPE_UNION) && !is_complete(typed->type)) {
        fatal_error("incomplete type '%s' where a complete type is required", type_of(typed));
    }
    if (vt->kind == TYPE_ARRAY) {
        // A typedef'd array decays through its resolved element type.
        Type *ptr             = new_type(TYPE_POINTER, __func__, __FILE__, __LINE__);
        ptr->u.pointer.target = clone_type(vt->u.array.element, __func__, __FILE__, __LINE__);
        free_type(typed->type);
        typed->type = ptr; // Modify in place
    } else if (vt->kind == TYPE_FUNCTION) {
        Type *ptr             = new_type(TYPE_POINTER, __func__, __FILE__, __LINE__);
        ptr->u.pointer.target = clone_type(vt, __func__, __FILE__, __LINE__);
        free_type(typed->type);
        typed->type = ptr;
    }
    return typed;
}

// The type a bit-field promotes to, as GCC and clang promote it (C11 §6.3.1.1p2 for
// _Bool, int and unsigned int): int when it is narrower than int, whatever its declared
// type, int or unsigned int by its signedness when it is as wide, else no promotion
// (TYPE_VOID).
static TypeKind bitfield_promoted_kind(const Expr *e)
{
    const BitField *bf = access_bitfield(e);
    if (!bf)
        return TYPE_VOID;
    int int_bits = target_config ? target_config->int_bits : 32;
    if (bf->width < int_bits)
        return TYPE_INT;
    if (bf->width == int_bits)
        return is_signed(e->type) ? TYPE_INT : TYPE_UINT;
    return TYPE_VOID;
}

// A bit-field read as a value has its promoted type right away: every operator would
// otherwise promote its declared type, which is wrong for `unsigned u:3`, whose value
// promotes to int.  Lvalue operands (assignment, ++, &) do not come through here.
Expr *typecheck_and_decay(Expr *e)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    if (!e)
        return NULL;
    e           = decay_expr(typecheck_expr(e));
    TypeKind pk = bitfield_promoted_kind(e);
    if (pk != TYPE_VOID && unalias(e->type)->kind != pk)
        e = convert_to_kind(e, pk);
    return e;
}

// Type-check an expression and require it to be scalar.
Expr *typecheck_scalar(Expr *e)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *typed = typecheck_and_decay(e);
    if (!is_scalar(typed->type)) {
        fatal_error("a value of scalar type is required, not '%s'", type_of(typed));
    }
    return typed;
}
