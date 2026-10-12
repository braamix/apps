#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"

//
// Evaluator for the constant expression on a "#if" line.  The scanner
// (lex_if_token, in yylex.c) turns the line into a stream of tokens; this file
// reads them one at a time and computes the integer result, which the directive
// loop treats as true (nonzero) or false.  It is a small recursive-descent
// parser with one-token lookahead kept in cpp.look_token / cpp.look_value.
//

// A value: §6.10.1p4 computes in intmax_t, or in uintmax_t when either operand
// of an operator is unsigned (the usual arithmetic conversions).
typedef struct {
    intmax_t v;
    int u;
} Value;

static coro(braam_call *) Value eval_expr(void);
static coro(braam_call *) Value eval_binary(int min_prec);
static coro(braam_call *) Value eval_term(void);

#define BITS (int)(8 * sizeof(intmax_t)) // a shift by this or more is out of range

static Value value(intmax_t v, int u)
{
    Value r = { v, u };
    return r;
}

coro(braam_call *) void advance(void)
{
    cpp.look_token    = await lex_if_token();
    cpp.look_value    = cpp.tok_value;
    cpp.look_unsigned = cpp.tok_unsigned;
}

coro(braam_call *) int match(int token)
{
    if (cpp.look_token == token) {
        await advance();
        return 1;
    }
    return 0;
}

int precedence(int token)
{
    switch (token) {
    case ',':
        return 1;
    case '=':
        return 2;
    case '?':
    case ':':
        return 3;
    case OROR:
        return 4;
    case ANDAND:
        return 5;
    case '|':
    case '^':
        return 6;
    case '&':
        return 7;
    case EQ:
    case NE:
        return 8;
    case '<':
    case '>':
    case LE:
    case GE:
        return 9;
    case LS:
    case RS:
        return 10;
    case '+':
    case '-':
        return 11;
    case '*':
    case '/':
    case '%':
        return 12;
    case '!':
    case '~':
    case UMINUS:
        return 13;
    case '(':
    case '.':
        return 14;
    default:
        return 0;
    }
}

static Value apply_op(int op, Value a, Value b)
{
    int u        = a.u || b.u;
    uintmax_t ua = (uintmax_t)a.v, ub = (uintmax_t)b.v;
    intmax_t sa = a.v, sb = b.v;

    switch (op) {
    case '*':
        return value((intmax_t)(ua * ub), u);
    case '/':
    case '%':
        if (sb == 0) {
            pperror(op == '/' ? "division by zero in '#if'" : "remainder by zero in '#if'");
            return value(0, u);
        }
        if (u)
            return value((intmax_t)(op == '/' ? ua / ub : ua % ub), 1);
        if (sb == -1) // INTMAX_MIN / -1 would trap
            return value(op == '/' ? (intmax_t)(0 - ua) : 0, 0);
        return value(op == '/' ? sa / sb : sa % sb, 0);
    case '+':
        return value((intmax_t)(ua + ub), u);
    case '-':
        return value((intmax_t)(ua - ub), u);
    case LS: // the type of the left operand
        return value(ub >= BITS ? 0 : (intmax_t)(ua << ub), a.u);
    case RS:
        if (a.u)
            return value(ub >= BITS ? 0 : (intmax_t)(ua >> ub), 1);
        if (ub >= BITS)
            return value(sa < 0 ? -1 : 0, 0);
        // arithmetic shift done unsigned
        return value(sa < 0 ? (intmax_t) ~(~ua >> ub) : (intmax_t)(ua >> ub), 0);
    case '<':
        return value(u ? ua < ub : sa < sb, 0);
    case '>':
        return value(u ? ua > ub : sa > sb, 0);
    case LE:
        return value(u ? ua <= ub : sa <= sb, 0);
    case GE:
        return value(u ? ua >= ub : sa >= sb, 0);
    case EQ:
        return value(sa == sb, 0);
    case NE:
        return value(sa != sb, 0);
    case '&':
        return value(sa & sb, u);
    case '^':
        return value(sa ^ sb, u);
    case '|':
        return value(sa | sb, u);
    case ANDAND:
        return value(sa && sb, 0);
    case OROR:
        return value(sa || sb, 0);
    case ',':
        return b;
    default:
        pperror("unexpected operator in '#if'");
        return a;
    }
}

static coro(braam_call *) Value eval_binary(int min_prec)
{
    Value val = await eval_term();

    for (;;) {
        int op   = cpp.look_token;
        int prec = precedence(op);

        if (prec == 0 || prec < min_prec)
            break;
        if (op == ':') // belongs to an enclosing '?', not a binary operator here
            break;

        await advance(); // consume the operator

        if (op == '?') {
            Value mid = await eval_binary(precedence(','));
            if (!await match(':'))
                pperror("expected ':' in '#if'");
            Value els = await eval_binary(prec);
            val       = val.v ? mid : els;
            val.u     = mid.u || els.u;
        } else if (op == '=') {
            pperror("assignment is not allowed in '#if'");
            await eval_binary(prec); // consume the right-hand side to stay in sync
        } else {
            Value rhs = await eval_binary(prec + 1);
            val       = apply_op(op, val, rhs);
        }
    }

    return val;
}

static coro(braam_call *) Value eval_expr(void)
{
    return await eval_binary(precedence(','));
}

static coro(braam_call *) Value eval_term(void)
{
    Value val;

    if (await match('-')) {
        val = await eval_term();
        return value((intmax_t)(0 - (uintmax_t)val.v), val.u);
    } else if (await match('+')) {
        return await eval_term();
    } else if (await match('!')) {
        val = await eval_term();
        return value(!val.v, 0);
    } else if (await match('~')) {
        val = await eval_term();
        return value(~val.v, val.u);
    } else if (await match('(')) {
        val = await eval_expr();
        if (!await match(')'))
            pperror("expected ')' in '#if'");
        return val;
    } else if (await match(DEFINED)) {
        if (await match('(')) {
            if (cpp.look_token != number)
                pperror("expected an identifier after 'defined'");
            val = value(cpp.look_value, 0);
            await advance();
            if (!await match(')'))
                pperror("expected ')' after the name in 'defined'");
            return val;
        } else if (cpp.look_token == number) {
            val = value(cpp.look_value, 0);
            await advance();
            return val;
        } else {
            pperror("expected an identifier or '(' after 'defined'");
        }
    } else if (cpp.look_token == number) {
        val = value(cpp.look_value, cpp.look_unsigned);
        await advance();
        return val;
    }

    pperror("expected a value in '#if'");
    return value(0, 0);
}

coro(braam_call *) int eval_if(void)
{
    await advance();
    Value result = await eval_expr();
    if (cpp.look_token != stop)
        pperror("extra tokens at the end of '#if'");
    return result.v != 0;
}
