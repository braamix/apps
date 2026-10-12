#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "c_escape.h"
#include "parser_internal.h"
#include "xalloc.h"

//
// Append to linked list.
// The first field of a struct must be the *next pointer.
//
void append_list(void *head, void *node)
{
    if (!node)
        return;
    *(void **)node = NULL;

    // Find tail.
    void **tail = (void **)head;
    while (*tail) {
        tail = (void **)*tail;
    }
    *tail = node;
}

// Checks if an expression is a constant expression according to C language rules.
static bool is_constant_expression(const Expr *expression)
{
    if (!expression)
        return false;

    switch (expression->kind) {
    case EXPR_LITERAL:
        /* All literals are constant, including enum constants */
        return true;

    case EXPR_VAR:
        /* Variables are not constant, even if they are enum constants
         * (handled in EXPR_LITERAL with LITERAL_ENUM) */
        return false;

    case EXPR_UNARY_OP: {
        /* Check which unary operations are allowed */
        switch (expression->u.unary_op.op) {
        case UNARY_PLUS:
        case UNARY_NEG:
        case UNARY_BIT_NOT:
        case UNARY_LOG_NOT:
            /* These operators are allowed if operand is constant */
            return is_constant_expression(expression->u.unary_op.expr);
        case UNARY_ADDRESS:
        case UNARY_DEREF:
        case UNARY_PRE_INC:
        case UNARY_PRE_DEC:
            /* These involve addresses or modification, not constant */
            return false;
        }
        return false; /* Unreachable, but for safety */
    }

    case EXPR_BINARY_OP: {
        /* Arithmetic, bitwise, relational, and logical ops are allowed, but not the
         * comma operator: C11 6.6p3 forbids it in a constant expression unless it is
         * inside an unevaluated subexpression -- and that carve-out already works,
         * because EXPR_SIZEOF_EXPR below answers true without recursing. */
        if (expression->u.binary_op.op == BINARY_COMMA)
            return false;
        const Expr *left  = expression->u.binary_op.left;
        const Expr *right = expression->u.binary_op.right;
        return is_constant_expression(left) && is_constant_expression(right);
    }

    case EXPR_ASSIGN:
        /* Assignments are not constant */
        return false;

    case EXPR_COND: {
        /* Ternary operator is allowed if all operands are constant */
        const Expr *cond      = expression->u.cond.condition;
        const Expr *then_expr = expression->u.cond.then_expr;
        const Expr *else_expr = expression->u.cond.else_expr;
        return is_constant_expression(cond) && is_constant_expression(then_expr) &&
               is_constant_expression(else_expr);
    }

    case EXPR_CAST: {
        /* Casts are allowed if the operand is constant and the target is scalar.
         * The parser cannot resolve a typedef name, so it rejects only the targets
         * that can never be scalar; typecheck_expr() does the real check once
         * typedef names are resolved. */
        const Type *target_type = expression->u.cast.type;
        if (!target_type)
            return false;
        switch (target_type->kind) {
        case TYPE_ARRAY:
        case TYPE_FUNCTION:
        case TYPE_STRUCT:
        case TYPE_UNION:
            return false;
        default:
            break;
        }
        return is_constant_expression(expression->u.cast.expr);
    }

    case EXPR_CALL:
        /* Function calls are not constant */
        return false;

    case EXPR_COMPOUND:
        /* Compound literals are not constant (may involve runtime init) */
        return false;

    case EXPR_SUBSCRIPT:
        /* Array indexing is not constant */
        return false;

    case EXPR_FIELD_ACCESS:
    case EXPR_PTR_ACCESS:
        /* Member access involves addresses, not constant */
        return false;

    case EXPR_POST_INC:
    case EXPR_POST_DEC:
        /* Increment/decrement are not constant */
        return false;

    case EXPR_SIZEOF_EXPR:
        /* sizeof(expr) is constant if the expression's type is known at compile-time.
         * In C, sizeof doesn't evaluate the expression, so we don't need to check
         * if the operand is constant. */
        return true;

    case EXPR_SIZEOF_TYPE:
        /* sizeof(type) is always constant */
        return true;

    case EXPR_ALIGNOF:
    case EXPR_VA_CLASS:
        /* _Alignof(type) and __builtin_va_class(type) are always constant */
        return true;

    case EXPR_GENERIC:
        /* _Generic is not constant (type selection may be runtime-dependent) */
        return false;

    case EXPR_YIELD:
    case EXPR_AWAIT:
    case EXPR_CO_OP:
        /* Coroutine operations run code; co_sizeof is a link-time value */
        return false;
    }

    return false; /* Unreachable, but for safety */
}

//
// primary_expression
//     : IDENTIFIER
//     | constant
//     | string
//     | FUNC_NAME
//     | '(' expression ')'
//     | generic_selection
//     ;
//
Expr *parse_primary_expression()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *expr = NULL;
    switch (current_token) {
    case TOKEN_IDENTIFIER:
        expr        = new_expression(EXPR_VAR);
        expr->u.var = xstrdup(current_lexeme);
        advance_token();
        break;
    case TOKEN_I_CONSTANT:
    case TOKEN_F_CONSTANT:
    case TOKEN_ENUMERATION_CONSTANT:
        expr = parse_constant();
        break;
    case TOKEN_STRING_LITERAL:
        expr = parse_string();
        break;
    case TOKEN_FUNC_NAME: {
        // C11 §6.4.2.2: the function's name as a string; not concatenated, it is no literal.
        if (!current_function_name)
            fatal_error("'__func__' used outside a function");
        size_t len   = strlen(current_function_name);
        char *quoted = xalloc(len + 3, __func__, __FILE__, __LINE__);
        quoted[0]    = '"';
        memcpy(quoted + 1, current_function_name, len);
        quoted[len + 1]               = '"';
        quoted[len + 2]               = '\0';
        expr                          = new_expression(EXPR_LITERAL);
        expr->u.literal               = new_literal(LITERAL_STRING);
        expr->u.literal->u.string_val = quoted;
        advance_token();
        break;
    }
    case TOKEN_LPAREN:
        advance_token();
        expr = parse_expression();
        expect_token(TOKEN_RPAREN);
        break;
    case TOKEN_GENERIC:
        expr = parse_generic_selection();
        break;
    default:
        fatal_error("expected an expression %s", parser_where());
    }
    return expr;
}

// Append one byte to the big-endian packed value, padding from the left with
// zeroes. At most 8 bytes (the host storage) may be packed; whether the value fits
// the target's int is checked by the semantic pass (check_int_literal_width).
static void pack_char_byte(uint64_t *value, int *nbytes, unsigned char b)
{
    *value = (*value << 8) | (uint64_t)b;
    if (++(*nbytes) > 8)
        fatal_error("character constant too long (more than 8 bytes)");
}

// Parse a character-constant lexeme into a packed integer value. Bytes are
// packed big-endian, zero-padded from the left (GCC-style multi-character
// constants):
//   - a byte with bit7 = 0 is a single ASCII byte;
//   - a byte with bit7 = 1 must begin a valid UTF-8 sequence, whose raw bytes
//     are kept verbatim (no codepoint decoding);
//   - a backslash escape contributes its byte value (low 8 bits) with no UTF-8
//     validation.
// Sets *out_nbytes to the number of packed bytes. Fatal error on an invalid
// UTF-8 sequence or more than 8 packed bytes.
static uint64_t parse_char_literal(const char *s, int *out_nbytes)
{
    // Skip optional encoding prefix: L, U, u, u8
    if (*s == 'L' || *s == 'U')
        s++;
    else if (*s == 'u') {
        s++;
        if (*s == '8')
            s++;
    }
    s++; // skip opening '

    uint64_t value = 0;
    int nbytes     = 0;
    while (*s && *s != '\'') {
        unsigned char b = (unsigned char)*s;
        if (b == '\\') {
            pack_char_byte(&value, &nbytes, (unsigned char)c_escape_value(&s));
        } else if (!(b & 0x80)) {
            pack_char_byte(&value, &nbytes, b);
            s++;
        } else {
            // UTF-8 lead byte: determine the sequence length and validate the
            // continuation bytes; the raw bytes are packed as-is.
            int len;
            if ((b & 0xE0) == 0xC0)
                len = 2;
            else if ((b & 0xF0) == 0xE0)
                len = 3;
            else if ((b & 0xF8) == 0xF0)
                len = 4;
            else
                fatal_error("invalid UTF-8 lead byte in character constant");
            pack_char_byte(&value, &nbytes, b);
            s++;
            for (int i = 1; i < len; i++) {
                unsigned char cont = (unsigned char)*s;
                if ((cont & 0xC0) != 0x80)
                    fatal_error("invalid UTF-8 sequence in character constant");
                pack_char_byte(&value, &nbytes, cont);
                s++;
            }
        }
    }
    *out_nbytes = nbytes;
    return value;
}

//
// constant
//     : I_CONSTANT             /* includes character_constant */
//     | F_CONSTANT
//     | ENUMERATION_CONSTANT   /* after it has been defined as such */
//     ;
//
Expr *parse_constant()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *expr      = new_expression(EXPR_LITERAL);
    expr->u.literal = new_literal(current_token == TOKEN_I_CONSTANT   ? LITERAL_INT
                                  : current_token == TOKEN_F_CONSTANT ? LITERAL_DOUBLE
                                                                      : LITERAL_ENUM);
    switch (current_token) {
    case TOKEN_I_CONSTANT: {
        const char *p = current_lexeme;
        while (*p && *p != '\'' && !isdigit((unsigned char)*p))
            p++;
        if (*p == '\'') {
            int nbytes;
            uint64_t v = parse_char_literal(current_lexeme, &nbytes);
            // <= 5 bytes (<= 40 bits) is type int; 6 or more bytes is unsigned (a
            // non-standard widening, see docs/Technical_Reference.md).
            if (nbytes <= 5) {
                expr->u.literal->kind      = LITERAL_INT;
                expr->u.literal->u.int_val = (int64_t)v;
                if (nbytes == 1 && current_lexeme[0] == '\'')
                    expr->u.literal->spelling = LITERAL_CHAR_BYTE;
            } else {
                expr->u.literal->kind       = LITERAL_UINT;
                expr->u.literal->u.uint_val = v;
            }
        } else {
            char *end            = NULL;
            unsigned long long v = strtoull(current_lexeme, &end, 0);
            bool is_unsigned     = false;
            int long_count       = 0;
            if (end) {
                while (*end) {
                    if (*end == 'u' || *end == 'U') {
                        is_unsigned = true;
                        end++;
                        continue;
                    }
                    if (*end == 'l' || *end == 'L') {
                        long_count++;
                        end++;
                        if (*end == 'l' || *end == 'L') {
                            long_count++;
                            end++;
                        }
                        continue;
                    }
                    break;
                }
            }
            expr->u.literal->spelling = LITERAL_SPELLED |
                                        (current_lexeme[0] != '0' ? LITERAL_DECIMAL : 0) |
                                        (is_unsigned ? LITERAL_SUFFIX_U : 0) |
                                        (long_count == 1   ? LITERAL_SUFFIX_L
                                         : long_count >= 2 ? LITERAL_SUFFIX_LL
                                                           : 0);
            // C11 §6.4.4.1: pick the first type in the suffix-determined list
            // that can hold the value. A `U` suffix selects the unsigned list
            // (unsigned int → unsigned long → unsigned long long); without it,
            // the signed list (int → long → long long). The `is_unsigned` flag
            // must be honored even when no `L` suffix is present.
            if (is_unsigned) {
                if (long_count >= 2) {
                    expr->u.literal->kind             = LITERAL_ULONG_LONG;
                    expr->u.literal->u.ulong_long_val = v;
                } else if (long_count == 1) {
                    expr->u.literal->kind        = LITERAL_ULONG;
                    expr->u.literal->u.ulong_val = (unsigned long)v;
                } else if (v <= (unsigned long long)UINT_MAX) {
                    expr->u.literal->kind       = LITERAL_UINT;
                    expr->u.literal->u.uint_val = (unsigned int)v;
                } else if (v <= (unsigned long long)ULONG_MAX) {
                    expr->u.literal->kind        = LITERAL_ULONG;
                    expr->u.literal->u.ulong_val = (unsigned long)v;
                } else {
                    expr->u.literal->kind             = LITERAL_ULONG_LONG;
                    expr->u.literal->u.ulong_long_val = v;
                }
            } else {
                if (long_count >= 2) {
                    expr->u.literal->kind            = LITERAL_LONG_LONG;
                    expr->u.literal->u.long_long_val = (long long)v;
                } else if (long_count == 1) {
                    expr->u.literal->kind       = LITERAL_LONG;
                    expr->u.literal->u.long_val = (long)v;
                } else if (v <= (unsigned long long)INT_MAX) {
                    expr->u.literal->kind      = LITERAL_INT;
                    expr->u.literal->u.int_val = (int)v;
                } else if (v <= (unsigned long long)LONG_MAX) {
                    expr->u.literal->kind       = LITERAL_LONG;
                    expr->u.literal->u.long_val = (long)v;
                } else {
                    expr->u.literal->kind            = LITERAL_LONG_LONG;
                    expr->u.literal->u.long_long_val = (long long)v;
                }
            }
        }
        break;
    }
    case TOKEN_F_CONSTANT: {
        char *end        = NULL;
        double v         = strtod(current_lexeme, &end);
        bool is_f_suffix = end && (*end == 'f' || *end == 'F');
        bool is_l_suffix = end && (*end == 'l' || *end == 'L');
        if (is_f_suffix) {
            expr->u.literal->kind       = LITERAL_FLOAT;
            expr->u.literal->u.real_val = strtof(current_lexeme, NULL);
        } else if (is_l_suffix) {
            expr->u.literal->kind              = LITERAL_LONG_DOUBLE;
            expr->u.literal->u.long_double_val = f128_from_string(current_lexeme, NULL);
        } else {
            // A target whose double is single takes strtof's value: rounding v again
            // could round twice.
            expr->u.literal->u.real_val = v;
            expr->u.literal->spelling   = LITERAL_SPELLED;
            expr->u.literal->single_val = strtof(current_lexeme, NULL);
        }
        break;
    }
    case TOKEN_ENUMERATION_CONSTANT:
        expr->u.literal->u.enum_const = xstrdup(current_lexeme);
        break;
    }
    advance_token();
    return expr;
}

//
// string
//     : STRING_LITERAL
//     ;
//
Expr *parse_string()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    // C11 §5.1.1.2 phase 6: concatenate adjacent string-literal tokens.
    // Each lexeme is a quoted token (e.g. "HELLO,"); join by dropping the
    // accumulator's closing quote and the next token's opening quote.
    char *combined = xstrdup(current_lexeme);
    advance_token();
    while (combined[0] == '"' && current_token == TOKEN_STRING_LITERAL) {
        const char *next = current_lexeme;
        size_t clen      = strlen(combined);
        size_t nlen      = strlen(next);
        char *merged     = xalloc(clen + nlen - 1, __func__, __FILE__, __LINE__);
        memcpy(merged, combined, clen - 1);        // drop trailing quote
        memcpy(merged + clen - 1, next + 1, nlen); // drop opening quote, keep NUL
        xfree(combined);
        combined = merged;
        advance_token();
    }
    Expr *expr                    = new_expression(EXPR_LITERAL);
    expr->u.literal               = new_literal(LITERAL_STRING);
    expr->u.literal->u.string_val = combined;
    return expr;
}

//
// generic_selection
//     : GENERIC '(' assignment_expression ',' generic_assoc_list ')'
//     ;
//
Expr *parse_generic_selection()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    expect_token(TOKEN_GENERIC);
    expect_token(TOKEN_LPAREN);
    Expr *controlling_expr = parse_assignment_expression();
    expect_token(TOKEN_COMMA);
    GenericAssoc *associations = parse_generic_assoc_list();
    expect_token(TOKEN_RPAREN);
    Expr *expr                       = new_expression(EXPR_GENERIC);
    expr->u.generic.controlling_expr = controlling_expr;
    expr->u.generic.associations     = associations;
    return expr;
}

//
// generic_assoc_list
//     : generic_association
//     | generic_assoc_list ',' generic_association
//     ;
//
GenericAssoc *parse_generic_assoc_list()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    GenericAssoc *assoc = parse_generic_association();
    if (current_token == TOKEN_COMMA) {
        advance_token();
        assoc->next = parse_generic_assoc_list();
    }
    return assoc;
}

//
// generic_association
//     : type_name ':' assignment_expression
//     | DEFAULT ':' assignment_expression
//     ;
//
GenericAssoc *parse_generic_association()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    GenericAssoc *assoc;
    if (current_token == TOKEN_DEFAULT) {
        advance_token();
        expect_token(TOKEN_COLON);
        Expr *expr             = parse_assignment_expression();
        assoc                  = new_generic_assoc(GENERIC_ASSOC_DEFAULT);
        assoc->u.default_assoc = expr;
    } else {
        Type *type = parse_type_name();
        expect_token(TOKEN_COLON);
        Expr *expr               = parse_assignment_expression();
        assoc                    = new_generic_assoc(GENERIC_ASSOC_TYPE);
        assoc->u.type_assoc.type = type;
        assoc->u.type_assoc.expr = expr;
    }
    return assoc;
}

//
// postfix_expression
//     : primary_expression
//     | postfix_expression '[' expression ']'
//     | postfix_expression '(' ')'
//     | postfix_expression '(' argument_expression_list ')'
//     | postfix_expression '.' IDENTIFIER
//     | postfix_expression PTR_OP IDENTIFIER
//     | postfix_expression INC_OP
//     | postfix_expression DEC_OP
//     | '(' type_name ')' '{' initializer_list '}'
//     | '(' type_name ')' '{' initializer_list ',' '}'
//     ;
//
Expr *parse_postfix_expression()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    return parse_postfix_tail(parse_primary_expression());
}

Expr *parse_postfix_tail(Expr *expr)
{
    while (1) {
        SrcLoc loc = diag_loc; // of the operator
        if (current_token == TOKEN_LBRACKET) {
            advance_token();
            Expr *index = parse_expression();
            expect_token(TOKEN_RBRACKET);
            Expr *new_expr              = new_expression(EXPR_SUBSCRIPT);
            new_expr->loc               = loc;
            new_expr->u.subscript.left  = expr;
            new_expr->u.subscript.right = index;
            expr                        = new_expr;
        } else if (current_token == TOKEN_LPAREN) {
            advance_token();
            Expr *args = NULL;
            if (current_token_is_not(TOKEN_RPAREN)) {
                args = parse_argument_expression_list();
            }
            expect_token(TOKEN_RPAREN);
            Expr *new_expr        = new_expression(EXPR_CALL);
            new_expr->loc         = loc;
            new_expr->u.call.func = expr;
            new_expr->u.call.args = args;
            expr                  = new_expr;
        } else if (current_token == TOKEN_DOT) {
            advance_token();
            Ident field = xstrdup(current_lexeme);
            expect_token(TOKEN_IDENTIFIER);
            Expr *new_expr                 = new_expression(EXPR_FIELD_ACCESS);
            new_expr->loc                  = loc;
            new_expr->u.field_access.expr  = expr;
            new_expr->u.field_access.field = field;
            expr                           = new_expr;
        } else if (current_token == TOKEN_PTR_OP) {
            advance_token();
            Ident field = xstrdup(current_lexeme);
            expect_token(TOKEN_IDENTIFIER);
            Expr *new_expr               = new_expression(EXPR_PTR_ACCESS);
            new_expr->loc                = loc;
            new_expr->u.ptr_access.expr  = expr;
            new_expr->u.ptr_access.field = field;
            expr                         = new_expr;
        } else if (current_token == TOKEN_INC_OP) {
            advance_token();
            Expr *new_expr       = new_expression(EXPR_POST_INC);
            new_expr->loc        = loc;
            new_expr->u.post_inc = expr;
            expr                 = new_expr;
        } else if (current_token == TOKEN_DEC_OP) {
            advance_token();
            Expr *new_expr       = new_expression(EXPR_POST_DEC);
            new_expr->loc        = loc;
            new_expr->u.post_dec = expr;
            expr                 = new_expr;
        } else {
            break;
        }
    }
    return expr;
}

//
// argument_expression_list
//     : assignment_expression
//     | argument_expression_list ',' assignment_expression
//     ;
//
Expr *parse_argument_expression_list()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *expr = parse_assignment_expression();
    if (current_token == TOKEN_COMMA) {
        advance_token();
        expr->next = parse_argument_expression_list();
    }
    return expr;
}

// Compound literal: (type-name) { initializer-list }, after the ')'; with its postfix tail.
static Expr *parse_compound_literal(Type *type)
{
    SrcLoc loc = diag_loc; // of the '{'
    expect_token(TOKEN_LBRACE);
    InitItem *items = NULL;
    if (current_token != TOKEN_RBRACE) {
        items = parse_initializer_list();
        if (current_token == TOKEN_COMMA)
            advance_token();
    }
    expect_token(TOKEN_RBRACE);
    Expr *compound                    = new_expression(EXPR_COMPOUND);
    compound->loc                     = loc;
    compound->u.compound_literal.type = type;
    compound->u.compound_literal.init = items;
    return parse_postfix_tail(compound);
}

// The operand of prefix ++/--: a unary expression, or a compound literal (a postfix
// expression, which parse_cast_expression recognizes).
static Expr *parse_prefix_operand()
{
    if (current_token == TOKEN_LPAREN &&
        (is_type_specifier(next_token()) || is_type_qualifier(next_token()) ||
         next_token() == TOKEN_ATOMIC))
        return parse_cast_expression();
    return parse_unary_expression();
}

// Can this token begin an expression?  A bare `_Yield` is followed by one that cannot.
static bool starts_expression(int token)
{
    switch (token) {
    case TOKEN_IDENTIFIER:
    case TOKEN_I_CONSTANT:
    case TOKEN_F_CONSTANT:
    case TOKEN_STRING_LITERAL:
    case TOKEN_ENUMERATION_CONSTANT:
    case TOKEN_FUNC_NAME:
    case TOKEN_GENERIC:
    case TOKEN_LPAREN:
    case TOKEN_INC_OP:
    case TOKEN_DEC_OP:
    case TOKEN_AMPERSAND:
    case TOKEN_STAR:
    case TOKEN_PLUS:
    case TOKEN_MINUS:
    case TOKEN_TILDE:
    case TOKEN_NOT:
    case TOKEN_SIZEOF:
    case TOKEN_ALIGNOF:
    case TOKEN_VA_CLASS:
    case TOKEN_YIELD:
    case TOKEN_AWAIT:
        return true;
    default:
        return token >= TOKEN_CO_INIT && token <= TOKEN_CO_ALIGNOF;
    }
}

// The coroutine operations (vcc extension): keywords with call syntax.  The number of
// arguments is checked here; their types, and which name a coroutine, in semantic.
static Expr *parse_co_op()
{
    CoOp op    = (CoOp)(current_token - TOKEN_CO_INIT);
    SrcLoc loc = diag_loc;
    advance_token();
    expect_token(TOKEN_LPAREN);
    Expr *result         = new_expression(EXPR_CO_OP);
    result->loc          = loc;
    result->u.co_op.op   = op;
    result->u.co_op.args = current_token == TOKEN_RPAREN ? NULL : parse_argument_expression_list();
    expect_token(TOKEN_RPAREN);
    int n = 0;
    for (const Expr *a = result->u.co_op.args; a; a = a->next)
        n++;
    int min = op == CO_OP_INIT ? 3 : op == CO_OP_ALLOCA ? 2 : 1;
    if (n < min || (min == 1 && n > 1))
        fatal_error("'%s' takes %s%d argument%s", co_op_name[op], min > 1 ? "at least " : "", min,
                    min > 1 ? "s" : "");
    return result;
}

//
// unary_expression
//     : postfix_expression
//     | INC_OP unary_expression
//     | DEC_OP unary_expression
//     | unary_operator cast_expression
//     | SIZEOF unary_expression
//     | SIZEOF '(' type_name ')'
//     | ALIGNOF '(' type_name ')'
//     | YIELD relational_expression          /* vcc extension */
//     | YIELD                                /* vcc extension */
//     | AWAIT cast_expression                /* vcc extension */
//     | CO_INIT ... CO_ALIGNOF '(' argument_expression_list ')'   /* vcc extension */
//     ;
// The operand of _Yield is a relational expression, so `yield i == CO_CANCEL` means
// `(yield i) == CO_CANCEL`; that of _Await binds like a cast's, so `await f(x) + 1`
// means `(await f(x)) + 1`.
//
Expr *parse_unary_expression()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    SrcLoc loc = diag_loc; // of the operator
    if (current_token == TOKEN_INC_OP) {
        advance_token();
        Expr *result            = new_expression(EXPR_UNARY_OP);
        result->u.unary_op.op   = UNARY_PRE_INC;
        result->u.unary_op.expr = parse_prefix_operand();
        result->loc             = loc;
        return result;
    } else if (current_token == TOKEN_DEC_OP) {
        advance_token();
        Expr *result            = new_expression(EXPR_UNARY_OP);
        result->u.unary_op.op   = UNARY_PRE_DEC;
        result->u.unary_op.expr = parse_prefix_operand();
        result->loc             = loc;
        return result;
    } else if (current_token == TOKEN_AMPERSAND || current_token == TOKEN_STAR ||
               current_token == TOKEN_PLUS || current_token == TOKEN_MINUS ||
               current_token == TOKEN_TILDE || current_token == TOKEN_NOT) {
        Expr *result            = new_expression(EXPR_UNARY_OP);
        result->u.unary_op.op   = parse_unary_operator();
        result->u.unary_op.expr = parse_cast_expression();
        result->loc             = loc;
        return result;
    } else if (current_token == TOKEN_SIZEOF) {
        advance_token();
        if (current_token == TOKEN_LPAREN &&
            (is_type_specifier(next_token()) || is_type_qualifier(next_token()) ||
             next_token() == TOKEN_ATOMIC)) {
            expect_token(TOKEN_LPAREN);
            Type *type = parse_type_name();
            expect_token(TOKEN_RPAREN);
            if (current_token == TOKEN_LBRACE) {
                Expr *result          = new_expression(EXPR_SIZEOF_EXPR);
                result->u.sizeof_expr = parse_compound_literal(type);
                result->loc           = loc;
                return result;
            }
            Expr *result          = new_expression(EXPR_SIZEOF_TYPE);
            result->u.sizeof_type = type;
            result->loc           = loc;
            return result;
        } else {
            Expr *result          = new_expression(EXPR_SIZEOF_EXPR);
            result->u.sizeof_expr = parse_unary_expression();
            result->loc           = loc;
            return result;
        }
    } else if (current_token == TOKEN_ALIGNOF) {
        advance_token();
        expect_token(TOKEN_LPAREN);
        Expr *result       = new_expression(EXPR_ALIGNOF);
        result->u.align_of = parse_type_name();
        expect_token(TOKEN_RPAREN);
        result->loc = loc;
        return result;
    } else if (current_token == TOKEN_VA_CLASS) {
        advance_token();
        expect_token(TOKEN_LPAREN);
        Expr *result       = new_expression(EXPR_VA_CLASS);
        result->u.va_class = parse_type_name();
        expect_token(TOKEN_RPAREN);
        result->loc = loc;
        return result;
    } else if (current_token == TOKEN_YIELD) {
        advance_token();
        Expr *result = new_expression(EXPR_YIELD);
        if (starts_expression(current_token))
            result->u.yield_expr = parse_relational_expression();
        result->loc = loc;
        return result;
    } else if (current_token == TOKEN_AWAIT) {
        advance_token();
        Expr *result         = new_expression(EXPR_AWAIT);
        result->u.await_expr = parse_cast_expression();
        result->loc          = loc;
        return result;
    } else if (current_token >= TOKEN_CO_INIT && current_token <= TOKEN_CO_ALIGNOF) {
        return parse_co_op();
    } else {
        return parse_postfix_expression();
    }
}

//
// unary_operator
//     : '&'
//     | '*'
//     | '+'
//     | '-'
//     | '~'
//     | '!'
//     ;
//
UnaryOp parse_unary_operator()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    UnaryOp op = current_token == TOKEN_AMPERSAND ? UNARY_ADDRESS
                 : current_token == TOKEN_STAR    ? UNARY_DEREF
                 : current_token == TOKEN_PLUS    ? UNARY_PLUS
                 : current_token == TOKEN_MINUS   ? UNARY_NEG
                 : current_token == TOKEN_TILDE   ? UNARY_BIT_NOT
                                                  : UNARY_LOG_NOT;
    advance_token();
    return op;
}

//
// cast_expression
//     : unary_expression
//     | '(' type_name ')' cast_expression
//     ;
//
Expr *parse_cast_expression()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    if (current_token == TOKEN_LPAREN &&
        (is_type_specifier(next_token()) || is_type_qualifier(next_token()) ||
         next_token() == TOKEN_ATOMIC)) {
        SrcLoc loc = diag_loc; // of the '('
        advance_token();
        Type *type = parse_type_name();
        expect_token(TOKEN_RPAREN);
        if (current_token == TOKEN_LBRACE)
            return parse_compound_literal(type);
        Expr *expr            = parse_cast_expression();
        Expr *new_expr        = new_expression(EXPR_CAST);
        new_expr->loc         = loc;
        new_expr->u.cast.type = type;
        new_expr->u.cast.expr = expr;
        return new_expr;
    }
    return parse_unary_expression();
}

//
// multiplicative_expression
//     : cast_expression
//     | multiplicative_expression '*' cast_expression
//     | multiplicative_expression '/' cast_expression
//     | multiplicative_expression '%' cast_expression
//     ;
//
Expr *parse_multiplicative_expression()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *left = parse_cast_expression();
    while (current_token == TOKEN_STAR || current_token == TOKEN_SLASH ||
           current_token == TOKEN_PERCENT) {
        Expr *expr           = new_expression(EXPR_BINARY_OP);
        expr->u.binary_op.op = current_token == TOKEN_STAR    ? BINARY_MUL
                               : current_token == TOKEN_SLASH ? BINARY_DIV
                                                              : BINARY_MOD;
        advance_token();
        expr->u.binary_op.left  = left;
        expr->u.binary_op.right = parse_cast_expression();
        left                    = expr;
    }
    return left;
}

//
// additive_expression
//     : multiplicative_expression
//     | additive_expression '+' multiplicative_expression
//     | additive_expression '-' multiplicative_expression
//     ;
//
Expr *parse_additive_expression()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *left = parse_multiplicative_expression();
    while (current_token == TOKEN_PLUS || current_token == TOKEN_MINUS) {
        Expr *expr           = new_expression(EXPR_BINARY_OP);
        expr->u.binary_op.op = current_token == TOKEN_PLUS ? BINARY_ADD : BINARY_SUB;
        advance_token();
        expr->u.binary_op.left  = left;
        expr->u.binary_op.right = parse_multiplicative_expression();
        left                    = expr;
    }
    return left;
}

//
// shift_expression
//     : additive_expression
//     | shift_expression LEFT_OP additive_expression
//     | shift_expression RIGHT_OP additive_expression
//     ;
//
Expr *parse_shift_expression()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *left = parse_additive_expression();
    while (current_token == TOKEN_LEFT_OP || current_token == TOKEN_RIGHT_OP) {
        Expr *expr = new_expression(EXPR_BINARY_OP);
        expr->u.binary_op.op =
            current_token == TOKEN_LEFT_OP ? BINARY_LEFT_SHIFT : BINARY_RIGHT_SHIFT;
        advance_token();
        expr->u.binary_op.left  = left;
        expr->u.binary_op.right = parse_additive_expression();
        left                    = expr;
    }
    return left;
}

//
// relational_expression
//     : shift_expression
//     | relational_expression '<' shift_expression
//     | relational_expression '>' shift_expression
//     | relational_expression LE_OP shift_expression
//     | relational_expression GE_OP shift_expression
//     ;
//
Expr *parse_relational_expression()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *left = parse_shift_expression();
    while (current_token == TOKEN_LT || current_token == TOKEN_GT || current_token == TOKEN_LE_OP ||
           current_token == TOKEN_GE_OP) {
        Expr *expr           = new_expression(EXPR_BINARY_OP);
        expr->u.binary_op.op = current_token == TOKEN_LT      ? BINARY_LT
                               : current_token == TOKEN_GT    ? BINARY_GT
                               : current_token == TOKEN_LE_OP ? BINARY_LE
                                                              : BINARY_GE;
        advance_token();
        expr->u.binary_op.left  = left;
        expr->u.binary_op.right = parse_shift_expression();
        left                    = expr;
    }
    return left;
}

//
// equality_expression
//     : relational_expression
//     | equality_expression EQ_OP relational_expression
//     | equality_expression NE_OP relational_expression
//     ;
//
Expr *parse_equality_expression()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *left = parse_relational_expression();
    while (current_token == TOKEN_EQ_OP || current_token == TOKEN_NE_OP) {
        Expr *expr           = new_expression(EXPR_BINARY_OP);
        expr->u.binary_op.op = current_token == TOKEN_EQ_OP ? BINARY_EQ : BINARY_NE;
        advance_token();
        expr->u.binary_op.left  = left;
        expr->u.binary_op.right = parse_relational_expression();
        left                    = expr;
    }
    return left;
}

//
// and_expression
//     : equality_expression
//     | and_expression '&' equality_expression
//     ;
//
Expr *parse_and_expression()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *left = parse_equality_expression();
    while (current_token == TOKEN_AMPERSAND) {
        Expr *expr           = new_expression(EXPR_BINARY_OP);
        expr->u.binary_op.op = BINARY_BIT_AND;
        advance_token();
        expr->u.binary_op.left  = left;
        expr->u.binary_op.right = parse_equality_expression();
        left                    = expr;
    }
    return left;
}

//
// exclusive_or_expression
//     : and_expression
//     | exclusive_or_expression '^' and_expression
//     ;
//
Expr *parse_exclusive_or_expression()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *left = parse_and_expression();
    while (current_token == TOKEN_CARET) {
        Expr *expr           = new_expression(EXPR_BINARY_OP);
        expr->u.binary_op.op = BINARY_BIT_XOR;
        advance_token();
        expr->u.binary_op.left  = left;
        expr->u.binary_op.right = parse_and_expression();
        left                    = expr;
    }
    return left;
}

//
// inclusive_or_expression
//     : exclusive_or_expression
//     | inclusive_or_expression '|' exclusive_or_expression
//     ;
//
Expr *parse_inclusive_or_expression()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *left = parse_exclusive_or_expression();
    while (current_token == TOKEN_PIPE) {
        Expr *expr           = new_expression(EXPR_BINARY_OP);
        expr->u.binary_op.op = BINARY_BIT_OR;
        advance_token();
        expr->u.binary_op.left  = left;
        expr->u.binary_op.right = parse_exclusive_or_expression();
        left                    = expr;
    }
    return left;
}

//
// logical_and_expression
//     : inclusive_or_expression
//     | logical_and_expression AND_OP inclusive_or_expression
//     ;
//
Expr *parse_logical_and_expression()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *left = parse_inclusive_or_expression();
    while (current_token == TOKEN_AND_OP) {
        Expr *expr           = new_expression(EXPR_BINARY_OP);
        expr->u.binary_op.op = BINARY_LOG_AND;
        advance_token();
        expr->u.binary_op.left  = left;
        expr->u.binary_op.right = parse_inclusive_or_expression();
        left                    = expr;
    }
    return left;
}

//
// logical_or_expression
//     : logical_and_expression
//     | logical_or_expression OR_OP logical_and_expression
//     ;
//
Expr *parse_logical_or_expression()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *left = parse_logical_and_expression();
    while (current_token == TOKEN_OR_OP) {
        Expr *expr           = new_expression(EXPR_BINARY_OP);
        expr->u.binary_op.op = BINARY_LOG_OR;
        advance_token();
        expr->u.binary_op.left  = left;
        expr->u.binary_op.right = parse_logical_and_expression();
        left                    = expr;
    }
    return left;
}

//
// conditional_expression
//     : logical_or_expression
//     | logical_or_expression '?' expression ':' conditional_expression
//     ;
//
Expr *parse_conditional_expression()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *expr = parse_logical_or_expression();
    if (current_token == TOKEN_QUESTION) {
        SrcLoc loc = diag_loc; // of the '?'
        advance_token();
        Expr *then_expr = parse_expression();
        expect_token(TOKEN_COLON);
        Expr *else_expr            = parse_conditional_expression();
        Expr *new_expr             = new_expression(EXPR_COND);
        new_expr->loc              = loc;
        new_expr->u.cond.condition = expr;
        new_expr->u.cond.then_expr = then_expr;
        new_expr->u.cond.else_expr = else_expr;
        expr                       = new_expr;
    }
    return expr;
}

//
// assignment_expression
//     : conditional_expression
//     | unary_expression assignment_operator assignment_expression
//     ;
//
Expr *parse_assignment_expression()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *target = parse_conditional_expression();
    if (current_token == TOKEN_ASSIGN || current_token == TOKEN_MUL_ASSIGN ||
        current_token == TOKEN_DIV_ASSIGN || current_token == TOKEN_MOD_ASSIGN ||
        current_token == TOKEN_ADD_ASSIGN || current_token == TOKEN_SUB_ASSIGN ||
        current_token == TOKEN_LEFT_ASSIGN || current_token == TOKEN_RIGHT_ASSIGN ||
        current_token == TOKEN_AND_ASSIGN || current_token == TOKEN_XOR_ASSIGN ||
        current_token == TOKEN_OR_ASSIGN) {
        Expr *expr            = new_expression(EXPR_ASSIGN);
        expr->u.assign.target = target;
        expr->u.assign.op     = parse_assignment_operator();
        expr->u.assign.value  = parse_assignment_expression();
        target                = expr;
    }
    return target;
}

//
// assignment_operator
//     : '='
//     | MUL_ASSIGN
//     | DIV_ASSIGN
//     | MOD_ASSIGN
//     | ADD_ASSIGN
//     | SUB_ASSIGN
//     | LEFT_ASSIGN
//     | RIGHT_ASSIGN
//     | AND_ASSIGN
//     | XOR_ASSIGN
//     | OR_ASSIGN
//     ;
//
AssignOp parse_assignment_operator()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    AssignOp op = current_token == TOKEN_ASSIGN         ? ASSIGN_SIMPLE
                  : current_token == TOKEN_MUL_ASSIGN   ? ASSIGN_MUL
                  : current_token == TOKEN_DIV_ASSIGN   ? ASSIGN_DIV
                  : current_token == TOKEN_MOD_ASSIGN   ? ASSIGN_MOD
                  : current_token == TOKEN_ADD_ASSIGN   ? ASSIGN_ADD
                  : current_token == TOKEN_SUB_ASSIGN   ? ASSIGN_SUB
                  : current_token == TOKEN_LEFT_ASSIGN  ? ASSIGN_LEFT
                  : current_token == TOKEN_RIGHT_ASSIGN ? ASSIGN_RIGHT
                  : current_token == TOKEN_AND_ASSIGN   ? ASSIGN_AND
                  : current_token == TOKEN_XOR_ASSIGN   ? ASSIGN_XOR
                                                        : ASSIGN_OR;
    advance_token();
    return op;
}

//
// expression
//     : assignment_expression
//     | expression ',' assignment_expression
//     ;
//
Expr *parse_expression()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *left = parse_assignment_expression();
    while (current_token == TOKEN_COMMA) {
        Expr *expr           = new_expression(EXPR_BINARY_OP);
        expr->u.binary_op.op = BINARY_COMMA;
        advance_token();
        expr->u.binary_op.left  = left;
        expr->u.binary_op.right = parse_assignment_expression();
        left                    = expr;
    }
    return left;
}

//
// constant_expression
//     : conditional_expression    /* with constraints */
//     ;
// Returns non-NULL value.
//
Expr *parse_constant_expression()
{
    if (parser_debug) {
        printf("--- %s()\n", __func__);
    }
    Expr *expression = parse_conditional_expression();
    if (!is_constant_expression(expression)) {
        fatal_error("expected a constant expression");
    }
    return expression;
}
