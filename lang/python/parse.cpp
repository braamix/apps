// Recursive descent, one function per level of the expression grammar.
//
// Every list of children is collected into a local Vec and pushed into the
// kids arena in one go: the arena is append-only, and a nested parse pushes
// its own runs while this one is still being built.
#include "parse.h"

#include "err.h"
#include "kernel/fmt.h"

namespace {

struct BinLevel {
    Tok tok;
    Op op;
};

// Lowest precedence first; each level is one loop in binary(). A short row is
// padded with Tok::End, which is the sentinel.
constexpr BinLevel LEVELS[][5] = {
    { { Tok::Vbar, Op::Or } },
    { { Tok::Caret, Op::Xor } },
    { { Tok::Amp, Op::And } },
    { { Tok::LShift, Op::Lsh }, { Tok::RShift, Op::Rsh } },
    { { Tok::Plus, Op::Add }, { Tok::Minus, Op::Sub } },
    { { Tok::Star, Op::Mul },
      { Tok::Slash, Op::Div },
      { Tok::DSlash, Op::FloorDiv },
      { Tok::Percent, Op::Mod },
      { Tok::At, Op::MatMul } },
};

constexpr usize NLEVELS = sizeof(LEVELS) / sizeof(LEVELS[0]);

struct AugOp {
    Tok tok;
    Op op;
};

constexpr AugOp AUGMENTED[] = {
    { Tok::PlusEq, Op::Add },  { Tok::MinusEq, Op::Sub },       { Tok::StarEq, Op::Mul },
    { Tok::SlashEq, Op::Div }, { Tok::DSlashEq, Op::FloorDiv }, { Tok::PercentEq, Op::Mod },
    { Tok::DStarEq, Op::Pow }, { Tok::AmpEq, Op::And },         { Tok::VbarEq, Op::Or },
    { Tok::CaretEq, Op::Xor }, { Tok::LShiftEq, Op::Lsh },      { Tok::RShiftEq, Op::Rsh },
    { Tok::AtEq, Op::MatMul },
};

using List = Vec<u32>;

struct Parser {
    Ast *ast;
    usize i     = 0; // the token being looked at
    u32 depth   = 0;
    bool failed = false;

    const Token &tok(usize ahead = 0) const
    {
        usize k = i + ahead;
        usize n = ast->lex.tokens.size();
        return ast->lex.tokens[k < n ? k : n - 1];
    }

    Tok kind(usize ahead = 0) const { return tok(ahead).kind; }

    bool at(Tok k) const { return kind() == k; }

    void bump() { i += kind() == Tok::End ? 0 : 1; }

    bool take(Tok k)
    {
        if (!at(k))
            return false;
        bump();
        return true;
    }

    u32 fail(Str message) { return fail_kind("SyntaxError", message); }

    // The same, for the one kind that is not a plain SyntaxError.
    u32 fail_kind(Str kind, Str message)
    {
        if (!failed) {
            failed = true;
            err_set_at(kind, message, tok().line, tok().col);
        }
        return 0;
    }

    // At the node's own token, rather than wherever the parser now stands.
    u32 fail_node(Str message, u32 n)
    {
        const Token &t = ast->lex.tokens[ast->at(n).tok];
        if (!failed) {
            failed = true;
            err_set_at("SyntaxError", message, t.line, t.col);
        }
        return 0;
    }

    bool expect(Tok k, Str what)
    {
        if (take(k))
            return true;
        // A `:=` where the grammar has none is CPython's plain complaint.
        return fail(at(Tok::Walrus) ? Str("invalid syntax") : what), false;
    }

    u32 add(Nd kind, u32 at_tok)
    {
        Node n;
        n.kind = kind;
        n.tok  = at_tok;
        if (!ast->nodes.push(n))
            return fail("out of memory");
        return u32(ast->nodes.size() - 1);
    }

    Node &node(u32 n) { return ast->nodes[n]; }

    bool hold(List &v, u32 n)
    {
        if (!n)
            return false;
        if (!v.push(n))
            return fail("out of memory"), false;
        return true;
    }

    // The collected lists, end to end, become this node's run.
    bool run_of(u32 n, const List **lists, usize count)
    {
        usize from = ast->kids.size();
        for (usize l = 0; l < count; l++)
            for (usize k = 0; k < lists[l]->size(); k++)
                if (!ast->kids.push((*lists[l])[k]))
                    return fail("out of memory"), false;
        node(n).kid0 = u32(from);
        node(n).nkid = u32(ast->kids.size() - from);
        return true;
    }

    bool run_of(u32 n, const List &v)
    {
        const List *one[1] = { &v };
        return run_of(n, one, 1);
    }

    bool enter()
    {
        if (++depth > MAX_NEST)
            return fail("too deeply nested"), false;
        return true;
    }

    void leave() { depth--; }

    bool statements_line(List &into);
    bool block(List &into);
    u32 small_statement();
    u32 compound_statement();
    u32 if_statement();
    u32 while_statement();
    u32 for_statement(bool is_async);
    u32 try_statement();
    u32 with_statement(bool is_async);
    u32 funcdef(bool is_async, List &decorators);
    u32 classdef(List &decorators);
    u32 decorated();
    u32 import_statement();
    u32 from_statement();
    u32 arguments(bool lambda);

    u32 exprlist(bool allow_star);
    // `*x` where x is a bitwise-or operand, or an expression in arguments.
    u32 starred_or(bool allow_star, u32 (Parser::*inner)(), bool arg = false);
    u32 expression();
    u32 named();
    u32 walrus(u32 target);
    Str expr_name(u32 n);
    u32 ternary();
    u32 lambda_expr();
    u32 or_expr();
    u32 and_expr();
    u32 not_expr();
    u32 comparison();
    u32 binary(usize level);
    u32 unary();
    u32 power();
    u32 await_expr();
    u32 postfix(u32 value);
    u32 atom();
    u32 fstring(u32 at_tok);
    bool fstring_parts(Str body, bool raw, u32 at_tok, String &pending, List &into,
                       bool tmpl = false);
    bool fstring_field(Str field, u32 at_tok, String &pending, List &into, bool tmpl);
    u32 str_const(Str text, u32 at_tok);
    u32 sub_expression(Str source, u32 at_tok);
    u32 bracketed();
    u32 subscript_item();
    u32 target_list();
    u32 comprehension_tail(Nd kind, u32 elt, u32 value, u32 at_tok);
    bool call_args(u32 call);
    u32 target();

    // match
    bool soft(Str word, usize ahead = 0) const
    {
        const Token &t = tok(ahead);
        return t.kind == Tok::Name && ast->lex.text_of(t) == word;
    }
    bool starts_expression(usize ahead) const;
    bool match_ahead() const;
    u32 match_statement();
    u32 match_case();
    u32 patterns();
    u32 pattern();
    u32 or_pattern();
    u32 closed_pattern();
    u32 star_pattern();
    u32 number_pattern();
    u32 signed_number();
    u32 name_or_attr();
    u32 sequence_pattern(Tok close, u32 at_tok);
    u32 mapping_pattern();
    u32 class_pattern(u32 cls);
    u32 capture_target();

    // PEP 695
    bool type_params(List &into);
    u32 type_param();
    bool type_alias_ahead() const;
    u32 type_alias();

    bool run();
};

bool is_target(const Ast *ast, u32 n)
{
    Nd k = ast->at(n).kind;
    return k == Nd::Name || k == Nd::Attribute || k == Nd::Subscript || k == Nd::Starred ||
           k == Nd::Tuple || k == Nd::List;
}

// ------------------------------------------------------------- expressions

// `*x` where a star is allowed, or whatever `inner` parses.
u32 Parser::starred_or(bool allow_star, u32 (Parser::*inner)(), bool arg)
{
    if (!allow_star || !at(Tok::Star))
        return (this->*inner)();
    u32 t = u32(i);
    bump();
    u32 n = add(Nd::Starred, t);
    if (!n)
        return 0;
    node(n).a = arg ? ternary() : binary(0);
    return node(n).a ? n : 0;
}

u32 Parser::atom()
{
    u32 t = u32(i);
    if (at(Tok::LPar) || at(Tok::LSqb) || at(Tok::LBrace)) {
        if (!enter())
            return 0;
        u32 n = bracketed();
        leave();
        return n;
    }
    switch (kind()) {
    case Tok::Name:
        bump();
        return add(Nd::Name, t);

    case Tok::Int:
    case Tok::Float:
    case Tok::Imag: {
        Const c = at(Tok::Int) ? Const::Int : at(Tok::Imag) ? Const::Imag : Const::Float;
        bump();
        u32 n = add(Nd::Constant, t);
        if (n)
            node(n).flags = u8(c);
        return n;
    }

    case Tok::Str:
    case Tok::Bytes:
    case Tok::FStr:
        return fstring(t);

    case Tok::KwNone:
    case Tok::KwTrue:
    case Tok::KwFalse:
    case Tok::Ellipsis: {
        Const c = at(Tok::KwNone)    ? Const::None
                  : at(Tok::KwTrue)  ? Const::True
                  : at(Tok::KwFalse) ? Const::False
                                     : Const::Ellipsis;
        bump();
        u32 n = add(Nd::Constant, t);
        if (n)
            node(n).flags = u8(c);
        return n;
    }

    default:
        break;
    }
    return fail("invalid syntax");
}

// ------------------------------------------------------------- the literals
//
// A run of adjacent literals is one node, as in Python: all plain is one
// Constant, and anything with an f in it is a JoinedStr over the whole run.
// The f-string's body arrives as written -- the lexer keeps it raw, because
// what is inside the braces is source and not text -- so this is where it is
// taken apart, and where the literal halves have their escapes decoded.

// A literal run turned into a Constant, at the f-string's own position.
u32 Parser::str_const(Str text, u32 at_tok)
{
    Token t = ast->lex.tokens[at_tok];
    t.kind  = Tok::Str;
    t.flags = 0;
    t.at    = u32(ast->lex.text.size());
    t.len   = u32(text.size());
    if (!ast->lex.text.append(text))
        return fail("out of memory");
    if (!ast->lex.tokens.push(t))
        return fail("out of memory");
    u32 n = add(Nd::Constant, u32(ast->lex.tokens.size() - 1));
    if (n)
        node(n).flags = u8(Const::Str);
    return n;
}

// One expression, lexed out of the f-string's body and parsed where it stands.
// The tokens go after everything already scanned, so the cursor is put there
// and brought back; ground rule 4 still holds, this being ordinary recursion.
u32 Parser::sub_expression(Str source, u32 at_tok)
{
    bool blank = true;
    for (usize k = 0; k < source.size(); k++)
        if (source[k] != ' ' && source[k] != '\t' && source[k] != '\n')
            blank = false;
    if (blank)
        return fail("f-string: valid expression required before '}'");

    const Token &ft = ast->lex.tokens[at_tok];
    u32 line = ft.line, col = ft.col;
    usize start = ast->lex.sublex(source, line, col);
    if (!start) {
        failed = true;
        return 0;
    }

    usize save = i;
    i          = start;
    u32 n      = exprlist(true);
    if (n && !at(Tok::End))
        n = fail("f-string: invalid syntax");
    i = save;
    return n;
}

// The top-level `!` of a conversion or `:` of a spec, or npos. Brackets and
// quotes hide both, and `!=` is an operator rather than a conversion.
usize field_suffix(Str field, char want)
{
    u32 depth = 0;
    for (usize k = 0; k < field.size(); k++) {
        char c = field[k];
        if (c == '\'' || c == '"') {
            k = lex_skip_string(field, k);
            if (k == Str::npos)
                return Str::npos;
            k--;
            continue;
        }
        if (c == '(' || c == '[' || c == '{') {
            depth++;
            continue;
        }
        if (c == ')' || c == ']' || c == '}') {
            if (depth)
                depth--;
            continue;
        }
        if (depth || c != want)
            continue;
        if (want == '!' && k + 1 < field.size() && field[k + 1] == '=')
            continue;
        return k;
    }
    return Str::npos;
}

// The `=` of f"{x=}": top level, and the last thing before the conversion or
// the spec. Comparison operators are spelled with one too, so they are skipped.
usize field_debug_eq(Str field)
{
    u32 depth   = 0;
    usize found = Str::npos;
    for (usize k = 0; k < field.size(); k++) {
        char c = field[k];
        if (c == '\'' || c == '"') {
            k = lex_skip_string(field, k);
            if (k == Str::npos)
                return found;
            k--;
            continue;
        }
        if (c == '(' || c == '[' || c == '{') {
            depth++;
            continue;
        }
        if (c == ')' || c == ']' || c == '}') {
            if (depth)
                depth--;
            continue;
        }
        if (depth || c != '=')
            continue;
        if (k + 1 < field.size() && field[k + 1] == '=')
            continue; // ==
        if (k && (field[k - 1] == '=' || field[k - 1] == '!' || field[k - 1] == '<' ||
                  field[k - 1] == '>'))
            continue; // ==, !=, <=, >=
        found = k;
    }
    return found;
}

// What an Interpolation says its expression was: the source, less a comment
// and the blank space after it.
Str interp_text(Str expr)
{
    for (usize k = 0; k < expr.size(); k++) {
        if (expr[k] == '\'' || expr[k] == '"') {
            k = lex_skip_string(expr, k);
            if (k == Str::npos)
                break;
            k--;
        } else if (expr[k] == '#') {
            expr = expr.substr(0, k);
            break;
        }
    }
    usize n = expr.size();
    while (n && (expr[n - 1] == ' ' || expr[n - 1] == '\t' || expr[n - 1] == '\n' ||
                 expr[n - 1] == '\r' || expr[n - 1] == '\f' ||
                 (expr[n - 1] == '\\' && n < expr.size() && expr[n] == '\n')))
        n--;
    return expr.substr(0, n);
}

// One `{...}` field. `pending` holds the literal text built so far, which the
// `=` form adds the expression's own source to before it is flushed. In a
// t-string it is an Interpolation, which keeps the source too.
bool Parser::fstring_field(Str field, u32 at_tok, String &pending, List &into, bool tmpl)
{
    Str expr = field;
    Str spec;
    bool has_spec = false;
    u8 conv       = FCONV_NONE;

    usize colon = field_suffix(field, ':');
    if (colon != Str::npos) {
        expr     = field.substr(0, colon);
        spec     = field.substr(colon + 1);
        has_spec = true;
    }
    usize bang = field_suffix(expr, '!');
    if (bang != Str::npos) {
        Str c = expr.substr(bang + 1);
        // PEP 701 lets whitespace follow the conversion character.
        while (c.size() > 1 && (c[c.size() - 1] == ' ' || c[c.size() - 1] == '\t'))
            c = c.substr(0, c.size() - 1);
        if (c.size() != 1 || (c[0] != 's' && c[0] != 'r' && c[0] != 'a'))
            return fail("f-string: invalid conversion character"), false;
        conv = u8(c[0]);
        expr = expr.substr(0, bang);
    }

    // f"{x=}" prints the expression as written and then its value, and takes
    // repr unless a conversion or a spec says otherwise.
    usize eq = field_debug_eq(expr);
    if (eq != Str::npos) {
        usize end = eq + 1;
        while (end < expr.size() && (expr[end] == ' ' || expr[end] == '\t'))
            end++;
        if (!pending.append(expr.substr(0, end)))
            return fail("out of memory"), false;
        expr = expr.substr(0, eq);
        if (conv == FCONV_NONE && !has_spec)
            conv = FCONV_REPR;
    }

    if (pending.size()) {
        if (!hold(into, str_const(pending.str(), at_tok)))
            return false;
        pending.clear();
    }

    u32 value = sub_expression(expr, at_tok);
    if (!value)
        return false;
    u32 text = tmpl ? str_const(interp_text(expr), at_tok) : 0;
    if (tmpl && !text)
        return false;

    u32 n = add(tmpl ? Nd::Interpolation : Nd::FormattedValue, at_tok);
    if (!n)
        return false;
    node(n).a     = value;
    node(n).c     = text;
    node(n).flags = conv;
    if (has_spec) {
        // The spec is an f-string of its own: it may hold fields, and it may
        // hold nothing at all, which is still a spec.
        String inner;
        List kids;
        if (!fstring_parts(spec, false, at_tok, inner, kids))
            return false;
        if (inner.size() && !hold(kids, str_const(inner.str(), at_tok)))
            return false;
        u32 j = add(Nd::JoinedStr, at_tok);
        if (!j || !run_of(j, kids))
            return false;
        node(n).b = j;
    }
    return hold(into, n);
}

// The body of one f-string, split into literals and fields. Doubled braces are
// one brace and start nothing; the literal runs between them have their
// escapes decoded here, which is why the braces are found in the raw text
// first -- an escape that yields a brace must not open a field.
bool Parser::fstring_parts(Str body, bool raw, u32 at_tok, String &pending, List &into, bool tmpl)
{
    usize at = 0, run = 0;
    for (;;) {
        bool end = at == body.size();
        char c   = end ? 0 : body[at];
        if (!end && c != '{' && c != '}') {
            // An escape is not a brace, and \N{...} is a name, not a field.
            if (c == '\\' && !raw && at + 1 < body.size()) {
                if (body[at + 1] == 'N' && at + 2 < body.size() && body[at + 2] == '{') {
                    usize close = body.find('}', at + 3);
                    at          = close == Str::npos ? body.size() : close + 1;
                    continue;
                }
                if (body[at + 1] != '{' && body[at + 1] != '}') {
                    at += 2;
                    continue;
                }
            }
            at++;
            continue;
        }
        if (at > run) {
            Str piece = body.substr(run, at - run);
            if (raw) {
                if (!pending.append(piece))
                    return fail("out of memory"), false;
            } else {
                const Token &t = ast->lex.tokens[at_tok];
                if (!lex_unescape(piece, t.line, t.col, pending))
                    return failed = true, false;
            }
        }
        if (end)
            return true;

        if (at + 1 < body.size() && body[at + 1] == c) {
            if (!pending.push(c))
                return fail("out of memory"), false;
            at += 2;
            run = at;
            continue;
        }
        if (c == '}')
            return fail("f-string: single '}' is not allowed"), false;

        usize stop = lex_field_end(body, at + 1);
        if (stop == Str::npos)
            return fail("f-string: expecting '}'"), false;
        if (!fstring_field(body.substr(at + 1, stop - at - 1), at_tok, pending, into, tmpl))
            return false;
        at  = stop + 1;
        run = at;
    }
}

u32 Parser::fstring(u32 at_tok)
{
    bool bytes = at(Tok::Bytes);
    bool any_f = false;
    // A t-string joins only with t-strings.
    bool tmpl = at(Tok::FStr) && (tok().flags & TOK_STR_T);
    String pending;
    List parts;

    while (at(Tok::Str) || at(Tok::Bytes) || at(Tok::FStr)) {
        if (tmpl != (at(Tok::FStr) && (tok().flags & TOK_STR_T)))
            return fail("cannot mix t-string literals with string or bytes literals");
        if ((kind() == Tok::Bytes) != bytes)
            return fail("cannot mix bytes and nonbytes literals");
        if (at(Tok::FStr)) {
            any_f = true;
            u32 t = u32(i);
            u8 fl = tok().flags;
            // text_of points into lex.text, which sublex appends to: copy.
            String body;
            if (!body.append(ast->lex.text_of(tok())))
                return fail("out of memory");
            bump();
            if (!fstring_parts(body.str(), (fl & TOK_STR_RAW) != 0, t, pending, parts, tmpl))
                return 0;
            continue;
        }
        if (!pending.append(ast->lex.text_of(tok())))
            return fail("out of memory");
        bump();
    }

    if (!any_f) {
        u32 n = add(Nd::Constant, at_tok);
        if (!n)
            return 0;
        node(n).flags = u8(bytes ? Const::Bytes : Const::Str);
        // The joined text goes to the end of the arena; the first token now
        // names it.
        Token &head = ast->lex.tokens[at_tok];
        head.at     = u32(ast->lex.text.size());
        head.len    = u32(pending.size());
        if (!ast->lex.text.append(pending.str()))
            return fail("out of memory");
        return n;
    }

    if (pending.size() && !hold(parts, str_const(pending.str(), at_tok)))
        return 0;
    u32 n = add(tmpl ? Nd::TemplateStr : Nd::JoinedStr, at_tok);
    if (!n || !run_of(n, parts))
        return 0;
    return n;
}

// The three bracketed forms, which are where the parser's depth is counted.
u32 Parser::bracketed()
{
    u32 t = u32(i);
    switch (kind()) {
    case Tok::LPar: {
        bump();
        if (take(Tok::RPar))
            return add(Nd::Tuple, t);
        if (at(Tok::KwYield)) {
            u32 y = expression();
            return expect(Tok::RPar, "expected ')'") ? y : 0;
        }
        u32 first = starred_or(true, &Parser::named);
        if (!first)
            return 0;
        if (at(Tok::KwFor) || (at(Tok::KwAsync) && kind(1) == Tok::KwFor)) {
            u32 g = comprehension_tail(Nd::GeneratorExp, first, 0, t);
            return expect(Tok::RPar, "expected ')'") ? g : 0;
        }
        if (!at(Tok::Comma))
            return expect(Tok::RPar, "expected ')'") ? first : 0;

        List elts;
        if (!hold(elts, first))
            return 0;
        while (take(Tok::Comma)) {
            if (at(Tok::RPar))
                break;
            if (!hold(elts, starred_or(true, &Parser::named)))
                return 0;
        }
        if (!expect(Tok::RPar, "expected ')'"))
            return 0;
        u32 n = add(Nd::Tuple, t);
        return n && run_of(n, elts) ? n : 0;
    }

    case Tok::LSqb: {
        bump();
        if (take(Tok::RSqb))
            return add(Nd::List, t);
        u32 first = starred_or(true, &Parser::named);
        if (!first)
            return 0;
        if (at(Tok::KwFor) || (at(Tok::KwAsync) && kind(1) == Tok::KwFor)) {
            u32 g = comprehension_tail(Nd::ListComp, first, 0, t);
            return expect(Tok::RSqb, "expected ']'") ? g : 0;
        }
        List elts;
        if (!hold(elts, first))
            return 0;
        while (take(Tok::Comma)) {
            if (at(Tok::RSqb))
                break;
            if (!hold(elts, starred_or(true, &Parser::named)))
                return 0;
        }
        if (!expect(Tok::RSqb, "expected ']'"))
            return 0;
        u32 n = add(Nd::List, t);
        return n && run_of(n, elts) ? n : 0;
    }

    case Tok::LBrace: {
        bump();
        if (take(Tok::RBrace))
            return add(Nd::Dict, t);

        // `{**a}` and `{k: v}` are dicts; anything else is a set.
        List items;
        bool dict = false;
        u32 key = 0, val = 0;
        if (take(Tok::DStar)) {
            dict = true;
            val  = or_expr();
            if (!val)
                return 0;
        } else {
            key = starred_or(true, &Parser::named);
            if (!key)
                return 0;
            if (take(Tok::Colon)) {
                dict = true;
                val  = ternary();
                if (!val)
                    return 0;
            }
        }

        if (at(Tok::KwFor) || (at(Tok::KwAsync) && kind(1) == Tok::KwFor)) {
            u32 g = dict ? comprehension_tail(Nd::DictComp, key, val, t)
                         : comprehension_tail(Nd::SetComp, key, 0, t);
            return expect(Tok::RBrace, "expected '}'") ? g : 0;
        }

        if (dict) {
            if (!items.push(key) || !items.push(val))
                return fail("out of memory");
        } else if (!hold(items, key)) {
            return 0;
        }

        while (take(Tok::Comma)) {
            if (at(Tok::RBrace))
                break;
            if (dict) {
                if (take(Tok::DStar)) {
                    u32 v = or_expr();
                    if (!v || !items.push(0) || !items.push(v))
                        return fail("out of memory");
                    continue;
                }
                u32 k2 = ternary();
                if (!k2 || !expect(Tok::Colon, "expected ':'"))
                    return 0;
                u32 v2 = ternary();
                if (!v2 || !items.push(k2) || !items.push(v2))
                    return fail("out of memory");
            } else if (!hold(items, starred_or(true, &Parser::named))) {
                return 0;
            }
        }
        if (!expect(Tok::RBrace, "expected '}'"))
            return 0;
        u32 n = add(dict ? Nd::Dict : Nd::Set, t);
        return n && run_of(n, items) ? n : 0;
    }

    default:
        break;
    }
    return fail("invalid syntax");
}

u32 Parser::comprehension_tail(Nd kind_of, u32 elt, u32 value, u32 at_tok)
{
    List clauses;
    while (at(Tok::KwFor) || (at(Tok::KwAsync) && kind(1) == Tok::KwFor)) {
        bool is_async = take(Tok::KwAsync);
        u32 t         = u32(i);
        bump(); // for
        u32 tgt = target_list();
        if (!tgt || !expect(Tok::KwIn, "expected 'in'"))
            return 0;
        u32 it = or_expr();
        if (!it)
            return 0;
        List ifs;
        while (take(Tok::KwIf))
            if (!hold(ifs, or_expr()))
                return 0;
        u32 c = add(Nd::Comprehen, t);
        if (!c || !run_of(c, ifs))
            return 0;
        node(c).a     = tgt;
        node(c).b     = it;
        node(c).flags = is_async ? 1 : 0;
        if (!hold(clauses, c))
            return 0;
    }
    if (!clauses.size())
        return fail("expected 'for' in a comprehension");

    u32 n = add(kind_of, at_tok);
    if (!n || !run_of(n, clauses))
        return 0;
    node(n).a = elt;
    node(n).b = value;
    return n;
}

bool Parser::call_args(u32 call)
{
    List args, keywords;
    bool dstar = false;
    while (!at(Tok::RPar)) {
        if (at(Tok::DStar))
            dstar = true;
        if (take(Tok::DStar)) {
            u32 k = add(Nd::Keyword, u32(i - 1));
            if (!k)
                return false;
            node(k).a = ternary();
            if (!node(k).a || !hold(keywords, k))
                return false;
        } else if (at(Tok::Name) && kind(1) == Tok::Equal) {
            u32 t = u32(i);
            bump();
            bump();
            u32 k = add(Nd::Keyword, t);
            if (!k)
                return false;
            node(k).flags = 1;
            node(k).a     = ternary();
            if (!node(k).a || !hold(keywords, k))
                return false;
        } else {
            bool star = at(Tok::Star);
            if (star && dstar)
                return fail("iterable argument unpacking follows keyword argument unpacking"),
                       false;
            if (!star && keywords.size())
                return fail(dstar ? Str("positional argument follows keyword argument unpacking")
                                  : Str("positional argument follows keyword argument")),
                       false;
            u32 e = starred_or(true, &Parser::named, true);
            if (!e)
                return false;
            if (at(Tok::KwFor) || (at(Tok::KwAsync) && kind(1) == Tok::KwFor)) {
                e = comprehension_tail(Nd::GeneratorExp, e, 0, u32(i));
                if (!e)
                    return false;
            }
            if (!hold(args, e))
                return false;
        }
        if (!take(Tok::Comma))
            break;
    }
    if (!expect(Tok::RPar, "expected ')'"))
        return false;
    node(call).b       = u32(args.size());
    const List *two[2] = { &args, &keywords };
    return run_of(call, two, 2);
}

// One item of a subscript: a slice, a named expression, or since 3.11 a
// starred one.
u32 Parser::subscript_item()
{
    u32 t = u32(i);
    if (at(Tok::Star))
        return starred_or(true, &Parser::ternary);
    u32 lower = at(Tok::Colon) ? 0 : ternary();
    if (failed)
        return 0;
    if (at(Tok::Walrus))
        return walrus(lower);
    if (!at(Tok::Colon))
        return lower;

    bump();
    u32 upper = 0, step = 0;
    if (!at(Tok::RSqb) && !at(Tok::Colon) && !at(Tok::Comma))
        upper = ternary();
    if (take(Tok::Colon) && !at(Tok::RSqb) && !at(Tok::Comma))
        step = ternary();
    if (failed)
        return 0;
    u32 n = add(Nd::Slice, t);
    if (n) {
        node(n).a = lower;
        node(n).b = upper;
        node(n).c = step;
    }
    return n;
}

u32 Parser::postfix(u32 value)
{
    while (value) {
        if (at(Tok::Dot)) {
            bump();
            if (!at(Tok::Name))
                return fail("expected a name after '.'");
            u32 n = add(Nd::Attribute, u32(i));
            bump();
            if (!n)
                return 0;
            node(n).a = value;
            value     = n;
        } else if (at(Tok::LPar)) {
            u32 t = u32(i);
            bump();
            u32 n = add(Nd::Call, t);
            if (!n)
                return 0;
            node(n).a = value;
            if (!call_args(n))
                return 0;
            value = n;
        } else if (at(Tok::LSqb)) {
            u32 t = u32(i);
            bump();
            u32 index = subscript_item();
            if (failed)
                return 0;
            // `x[*a]` is a tuple of one, as `x[a,]` is.
            if (at(Tok::Comma) || ast->at(index).kind == Nd::Starred) {
                List elts;
                if (!hold(elts, index))
                    return 0;
                while (take(Tok::Comma)) {
                    if (at(Tok::RSqb))
                        break;
                    if (!hold(elts, subscript_item()))
                        return 0;
                }
                index = add(Nd::Tuple, t);
                if (!index || !run_of(index, elts))
                    return 0;
            }
            if (!expect(Tok::RSqb, "expected ']'"))
                return 0;
            u32 n = add(Nd::Subscript, t);
            if (!n)
                return 0;
            node(n).a = value;
            node(n).b = index;
            value     = n;
        } else {
            break;
        }
    }
    return value;
}

u32 Parser::await_expr()
{
    if (at(Tok::KwAwait)) {
        u32 t = u32(i);
        bump();
        u32 n = add(Nd::Await, t);
        if (!n)
            return 0;
        // The operand is a primary: `await x ** 2` is `(await x) ** 2`.
        node(n).a = postfix(atom());
        return node(n).a ? n : 0;
    }
    return postfix(atom());
}

// `**` binds tighter than a unary on its left and looser on its right, so
// -2**2 is -4 and 2**-1 is a float.
u32 Parser::power()
{
    u32 base = await_expr();
    if (!base || !at(Tok::DStar))
        return base;
    u32 t = u32(i);
    bump();
    u32 n = add(Nd::BinOp, t);
    if (!n)
        return 0;
    node(n).flags = u8(Op::Pow);
    node(n).a     = base;
    node(n).b     = unary();
    return node(n).b ? n : 0;
}

u32 Parser::unary()
{
    if (at(Tok::Minus) || at(Tok::Plus) || at(Tok::Tilde)) {
        Un u  = at(Tok::Minus) ? Un::USub : at(Tok::Plus) ? Un::UAdd : Un::Invert;
        u32 t = u32(i);
        bump();
        u32 n = add(Nd::UnaryOp, t);
        if (!n)
            return 0;
        node(n).flags = u8(u);
        node(n).a     = unary();
        return node(n).a ? n : 0;
    }
    return power();
}

u32 Parser::binary(usize level)
{
    if (level >= NLEVELS)
        return unary();
    u32 left = binary(level + 1);
    while (left) {
        bool matched = false;
        for (const BinLevel &b : LEVELS[level]) {
            if (b.tok == Tok::End)
                break;
            if (!at(b.tok))
                continue;
            u32 t = u32(i);
            bump();
            u32 n = add(Nd::BinOp, t);
            if (!n)
                return 0;
            node(n).flags = u8(b.op);
            node(n).a     = left;
            node(n).b     = binary(level + 1);
            if (!node(n).b)
                return 0;
            left    = n;
            matched = true;
            break;
        }
        if (!matched)
            break;
    }
    return left;
}

u32 Parser::comparison()
{
    u32 left = binary(0);
    if (!left)
        return 0;
    List ops;
    for (;;) {
        Cmp op;
        u32 t = u32(i);
        if (at(Tok::Less))
            op = Cmp::Lt;
        else if (at(Tok::Greater))
            op = Cmp::Gt;
        else if (at(Tok::LessEq))
            op = Cmp::Le;
        else if (at(Tok::GreaterEq))
            op = Cmp::Ge;
        else if (at(Tok::EqEq))
            op = Cmp::Eq;
        else if (at(Tok::NotEq))
            op = Cmp::Ne;
        else if (at(Tok::KwIn))
            op = Cmp::In;
        else if (at(Tok::KwNot) && kind(1) == Tok::KwIn)
            op = Cmp::NotIn;
        else if (at(Tok::KwIs) && kind(1) == Tok::KwNot)
            op = Cmp::IsNot;
        else if (at(Tok::KwIs))
            op = Cmp::Is;
        else
            break;
        bump();
        if (op == Cmp::NotIn || op == Cmp::IsNot)
            bump();
        u32 rhs = binary(0);
        if (!rhs)
            return 0;
        u32 c = add(Nd::CmpOp, t);
        if (!c)
            return 0;
        node(c).flags = u8(op);
        node(c).a     = rhs;
        if (!hold(ops, c))
            return 0;
    }
    if (!ops.size())
        return left;
    u32 n = add(Nd::Compare, node(left).tok);
    if (!n || !run_of(n, ops))
        return 0;
    node(n).a = left;
    return n;
}

u32 Parser::not_expr()
{
    if (at(Tok::KwNot)) {
        u32 t = u32(i);
        bump();
        u32 n = add(Nd::UnaryOp, t);
        if (!n)
            return 0;
        node(n).flags = u8(Un::Not);
        node(n).a     = not_expr();
        return node(n).a ? n : 0;
    }
    return comparison();
}

u32 Parser::and_expr()
{
    u32 left = not_expr();
    if (!left || !at(Tok::KwAnd))
        return left;
    List values;
    if (!hold(values, left))
        return 0;
    u32 t = u32(i);
    while (take(Tok::KwAnd))
        if (!hold(values, not_expr()))
            return 0;
    u32 n = add(Nd::BoolOp, t);
    if (!n || !run_of(n, values))
        return 0;
    node(n).flags = u8(Bool::And);
    return n;
}

u32 Parser::or_expr()
{
    u32 left = and_expr();
    if (!left || !at(Tok::KwOr))
        return left;
    List values;
    if (!hold(values, left))
        return 0;
    u32 t = u32(i);
    while (take(Tok::KwOr))
        if (!hold(values, and_expr()))
            return 0;
    u32 n = add(Nd::BoolOp, t);
    if (!n || !run_of(n, values))
        return 0;
    node(n).flags = u8(Bool::Or);
    return n;
}

u32 Parser::lambda_expr()
{
    u32 t = u32(i);
    bump();
    u32 args = arguments(true);
    if (!args || !expect(Tok::Colon, "expected ':'"))
        return 0;
    u32 body = ternary();
    if (!body)
        return 0;
    u32 n = add(Nd::Lambda, t);
    if (n) {
        node(n).a = args;
        node(n).b = body;
    }
    return n;
}

u32 Parser::ternary()
{
    if (at(Tok::KwLambda))
        return lambda_expr();
    u32 body = or_expr();
    if (!body || !at(Tok::KwIf))
        return body;
    u32 t = u32(i);
    bump();
    u32 test = or_expr();
    if (!test || !expect(Tok::KwElse, "expected 'else'"))
        return 0;
    u32 orelse = ternary();
    if (!orelse)
        return 0;
    u32 n = add(Nd::IfExp, t);
    if (n) {
        node(n).a = test;
        node(n).b = body;
        node(n).c = orelse;
    }
    return n;
}

u32 Parser::expression()
{
    if (at(Tok::KwYield)) {
        u32 t = u32(i);
        bump();
        if (take(Tok::KwFrom)) {
            u32 n = add(Nd::YieldFrom, t);
            if (!n)
                return 0;
            node(n).a = ternary();
            return node(n).a ? n : 0;
        }
        u32 n = add(Nd::Yield, t);
        if (!n)
            return 0;
        if (!at(Tok::Newline) && !at(Tok::RPar) && !at(Tok::RSqb) && !at(Tok::RBrace) &&
            !at(Tok::Comma) && !at(Tok::Semi) && !at(Tok::End)) {
            node(n).a = exprlist(true);
            if (!node(n).a)
                return 0;
        }
        return n;
    }

    return ternary();
}

// An expression where `:=` may stand unparenthesized: a condition, an
// element of a display, a positional argument, a subscript, a decorator.
u32 Parser::named()
{
    u32 e = ternary();
    if (!e || !at(Tok::Walrus))
        return e;
    return walrus(e);
}

// `target := value`, with the target already parsed.
u32 Parser::walrus(u32 target)
{
    u32 t = u32(i);
    bump();
    if (ast->at(target).kind != Nd::Name) {
        Buf<96> b;
        b.put("cannot use assignment expressions with ").put(expr_name(target));
        return fail_node(b.str(), target);
    }
    u32 n = add(Nd::NamedExpr, t);
    if (!n)
        return 0;
    node(n).a = target;
    node(n).b = ternary();
    return node(n).b ? n : 0;
}

// What CPython calls an expression in a complaint about assigning to it.
Str Parser::expr_name(u32 n)
{
    const Node &x = ast->at(n);
    switch (x.kind) {
    case Nd::Attribute:
        return "attribute";
    case Nd::Subscript:
        return "subscript";
    case Nd::Starred:
        return "starred";
    case Nd::Name:
        return "name";
    case Nd::List:
        return "list";
    case Nd::Tuple:
        return "tuple";
    case Nd::Lambda:
        return "lambda";
    case Nd::Call:
        return "function call";
    case Nd::BoolOp:
    case Nd::BinOp:
    case Nd::UnaryOp:
        return "expression";
    case Nd::GeneratorExp:
        return "generator expression";
    case Nd::Yield:
    case Nd::YieldFrom:
        return "yield expression";
    case Nd::Await:
        return "await expression";
    case Nd::ListComp:
        return "list comprehension";
    case Nd::SetComp:
        return "set comprehension";
    case Nd::DictComp:
        return "dict comprehension";
    case Nd::Dict:
        return "dict literal";
    case Nd::Set:
        return "set display";
    case Nd::JoinedStr:
    case Nd::FormattedValue:
        return "f-string expression";
    case Nd::TemplateStr:
    case Nd::Interpolation:
        return "t-string expression";
    case Nd::Compare:
        return "comparison";
    case Nd::IfExp:
        return "conditional expression";
    case Nd::NamedExpr:
        return "named expression";
    case Nd::Constant:
        switch (Const(x.flags)) {
        case Const::None:
            return "None";
        case Const::True:
            return "True";
        case Const::False:
            return "False";
        case Const::Ellipsis:
            return "ellipsis";
        default:
            return "literal";
        }
    default:
        return "expression";
    }
}

// One expression, or the tuple a comma makes of several.
u32 Parser::exprlist(bool allow_star)
{
    u32 t     = u32(i);
    u32 first = starred_or(allow_star, &Parser::expression);
    if (!first || !at(Tok::Comma))
        return first;

    List elts;
    if (!hold(elts, first))
        return 0;
    while (take(Tok::Comma)) {
        if (at(Tok::Newline) || at(Tok::Equal) || at(Tok::RPar) || at(Tok::RSqb) ||
            at(Tok::RBrace) || at(Tok::Colon) || at(Tok::Semi) || at(Tok::End))
            break;
        if (!hold(elts, starred_or(allow_star, &Parser::expression)))
            return 0;
    }
    u32 n = add(Nd::Tuple, t);
    return n && run_of(n, elts) ? n : 0;
}

// -------------------------------------------------------------- statements

u32 Parser::arguments(bool lambda)
{
    u32 t = u32(i);
    u32 n = add(Nd::Arguments, t);
    if (!n)
        return 0;

    List posonly, args, vararg, kwonly, kwdefaults, kwarg, defaults;
    bool after_star = false;

    for (;;) {
        if (lambda ? at(Tok::Colon) : at(Tok::RPar))
            break;
        if (take(Tok::Slash)) {
            for (usize k = 0; k < args.size(); k++)
                if (!posonly.push(args[k]))
                    return fail("out of memory");
            args.clear();
            if (!take(Tok::Comma))
                break;
            continue;
        }
        if (take(Tok::Star)) {
            after_star = true;
            if (at(Tok::Name)) {
                u32 a = add(Nd::Arg, u32(i));
                bump();
                if (!a)
                    return 0;
                // PEP 646: `*args: *Ts`.
                if (!lambda && take(Tok::Colon) &&
                    !(node(a).a = starred_or(true, &Parser::ternary)))
                    return 0;
                if (!hold(vararg, a))
                    return 0;
            }
            if (!take(Tok::Comma))
                break;
            continue;
        }
        if (take(Tok::DStar)) {
            if (!at(Tok::Name))
                return fail("expected a name after '**'");
            u32 a = add(Nd::Arg, u32(i));
            bump();
            if (!a)
                return 0;
            if (!lambda && take(Tok::Colon) && !(node(a).a = ternary()))
                return 0;
            if (!hold(kwarg, a))
                return 0;
            take(Tok::Comma);
            break;
        }
        if (!at(Tok::Name))
            return fail("expected a parameter name");
        u32 a = add(Nd::Arg, u32(i));
        bump();
        if (!a)
            return 0;
        if (!lambda && take(Tok::Colon) && !(node(a).a = ternary()))
            return 0;
        u32 dflt = 0;
        if (take(Tok::Equal) && !(dflt = ternary()))
            return 0;
        if (after_star) {
            if (!kwonly.push(a) || !kwdefaults.push(dflt))
                return fail("out of memory");
        } else {
            if (!args.push(a))
                return fail("out of memory");
            if (dflt) {
                if (!defaults.push(dflt))
                    return fail("out of memory");
            } else if (defaults.size()) {
                return fail("non-default argument follows default argument");
            }
        }
        if (!take(Tok::Comma))
            break;
    }

    node(n).a            = u32(posonly.size());
    node(n).b            = u32(args.size());
    node(n).c            = u32(kwonly.size());
    node(n).d            = u32(defaults.size());
    node(n).flags        = u8((vararg.size() ? ARG_VARARG : 0) | (kwarg.size() ? ARG_KWARG : 0));
    const List *seven[7] = { &posonly, &args, &vararg, &kwonly, &kwdefaults, &kwarg, &defaults };
    return run_of(n, seven, 7) ? n : 0;
}

// One logical line: a compound statement, or simple statements split by `;`.
bool Parser::statements_line(List &into)
{
    u32 c = compound_statement();
    if (failed)
        return false;
    if (c)
        return hold(into, c);
    for (;;) {
        if (!hold(into, small_statement()))
            return false;
        if (!take(Tok::Semi))
            break;
        if (at(Tok::Newline) || at(Tok::End) || at(Tok::Dedent))
            break;
    }
    if (!at(Tok::End) && !at(Tok::Dedent) && !take(Tok::Newline))
        return fail("invalid syntax"), false;
    return true;
}

// An indented block, or the simple statements after a colon on one line.
bool Parser::block(List &into)
{
    if (!expect(Tok::Colon, "expected ':'"))
        return false;
    if (!take(Tok::Newline))
        return statements_line(into);
    if (!take(Tok::Indent))
        return fail_kind("IndentationError", "expected an indented block"), false;
    if (!enter())
        return false;
    while (!at(Tok::Dedent) && !at(Tok::End)) {
        if (take(Tok::Newline))
            continue;
        if (!statements_line(into)) {
            leave();
            return false;
        }
    }
    leave();
    take(Tok::Dedent);
    return true;
}

u32 Parser::if_statement()
{
    u32 t = u32(i);
    bump(); // if, or the elif of an enclosing if
    u32 test = named();
    if (!test)
        return 0;
    List body, orelse;
    if (!block(body))
        return 0;
    if (at(Tok::KwElif)) {
        if (!hold(orelse, if_statement()))
            return 0;
    } else if (take(Tok::KwElse)) {
        if (!block(orelse))
            return 0;
    }
    u32 n = add(Nd::If, t);
    if (!n)
        return 0;
    node(n).a          = test;
    node(n).b          = u32(body.size());
    node(n).c          = u32(orelse.size());
    const List *two[2] = { &body, &orelse };
    return run_of(n, two, 2) ? n : 0;
}

u32 Parser::while_statement()
{
    u32 t = u32(i);
    bump();
    u32 test = named();
    if (!test)
        return 0;
    List body, orelse;
    if (!block(body))
        return 0;
    if (take(Tok::KwElse) && !block(orelse))
        return 0;
    u32 n = add(Nd::While, t);
    if (!n)
        return 0;
    node(n).a          = test;
    node(n).b          = u32(body.size());
    node(n).c          = u32(orelse.size());
    const List *two[2] = { &body, &orelse };
    return run_of(n, two, 2) ? n : 0;
}

u32 Parser::for_statement(bool is_async)
{
    u32 t = u32(i);
    bump();
    u32 tgt = target_list();
    if (!tgt || !expect(Tok::KwIn, "expected 'in'"))
        return 0;
    u32 it = exprlist(true);
    if (!it)
        return 0;
    List body, orelse;
    if (!block(body))
        return 0;
    if (take(Tok::KwElse) && !block(orelse))
        return 0;
    u32 n = add(is_async ? Nd::AsyncFor : Nd::For, t);
    if (!n)
        return 0;
    node(n).a          = tgt;
    node(n).b          = it;
    node(n).c          = u32(body.size());
    node(n).d          = u32(orelse.size());
    const List *two[2] = { &body, &orelse };
    return run_of(n, two, 2) ? n : 0;
}

u32 Parser::try_statement()
{
    u32 t = u32(i);
    bump();
    List body, handlers, orelse, finalbody;
    if (!block(body))
        return 0;

    i32 star = -1; // whether the handlers are except*, once one has said
    while (at(Tok::KwExcept)) {
        u32 ht = u32(i);
        bump();
        bool is_star = take(Tok::Star);
        if (star >= 0 && is_star != (star == 1))
            return fail("cannot have both 'except' and 'except*' on the same 'try'");
        star     = is_star ? 1 : 0;
        u32 type = 0, name_tok = 0;
        bool named = false;
        if (is_star && at(Tok::Colon))
            return fail("expected one or more exception types");
        if (!at(Tok::Colon)) {
            u32 tt = u32(i);
            type   = expression();
            if (!type)
                return 0;
            // PEP 758: `except A, B:` is a tuple, where no `as` follows.
            if (at(Tok::Comma)) {
                List types;
                if (!hold(types, type))
                    return 0;
                while (take(Tok::Comma))
                    if (!hold(types, expression()))
                        return 0;
                if (at(Tok::KwAs))
                    return fail("multiple exception types must be parenthesized when using 'as'");
                type = add(Nd::Tuple, tt);
                if (!type || !run_of(type, types))
                    return 0;
            }
            if (take(Tok::KwAs)) {
                if (!at(Tok::Name))
                    return fail("expected a name after 'as'");
                name_tok = u32(i);
                named    = true;
                bump();
            }
        }
        List hbody;
        if (!block(hbody))
            return 0;
        u32 h = add(Nd::ExceptHandler, named ? name_tok : ht);
        if (!h || !run_of(h, hbody))
            return 0;
        node(h).a     = type;
        node(h).b     = u32(hbody.size());
        node(h).flags = named ? 1 : 0;
        if (!hold(handlers, h))
            return 0;
    }
    if (take(Tok::KwElse) && !block(orelse))
        return 0;
    if (take(Tok::KwFinally) && !block(finalbody))
        return 0;
    if (!handlers.size() && !finalbody.size())
        return fail("expected 'except' or 'finally' block");

    u32 n = add(star == 1 ? Nd::TryStar : Nd::Try, t);
    if (!n)
        return 0;
    node(n).a           = u32(body.size());
    node(n).b           = u32(handlers.size());
    node(n).c           = u32(orelse.size());
    node(n).d           = u32(finalbody.size());
    const List *four[4] = { &body, &handlers, &orelse, &finalbody };
    return run_of(n, four, 4) ? n : 0;
}

u32 Parser::with_statement(bool is_async)
{
    u32 t = u32(i);
    bump();
    List items, body;
    // 3.9's `with (a as b, c):`: a bracket whose close a colon follows.
    bool paren = false;
    if (at(Tok::LPar)) {
        u32 nest = 0;
        usize k  = i;
        for (; k < ast->lex.tokens.size(); k++) {
            Tok x = ast->lex.tokens[k].kind;
            if (x == Tok::LPar || x == Tok::LSqb || x == Tok::LBrace)
                nest++;
            else if ((x == Tok::RPar || x == Tok::RSqb || x == Tok::RBrace) && !--nest)
                break;
            else if (x == Tok::End || x == Tok::Newline)
                break;
        }
        paren = k + 1 < ast->lex.tokens.size() && ast->lex.tokens[k].kind == Tok::RPar &&
                ast->lex.tokens[k + 1].kind == Tok::Colon;
        if (paren) {
            bump();
            if (!enter())
                return 0;
        }
    }
    for (;;) {
        u32 it = u32(i);
        u32 cm = expression();
        if (!cm)
            return 0;
        u32 vars = 0;
        if (take(Tok::KwAs) && !(vars = target()))
            return 0;
        u32 w = add(Nd::WithItem, it);
        if (!w)
            return 0;
        node(w).a = cm;
        node(w).b = vars;
        if (!hold(items, w))
            return 0;
        if (!take(Tok::Comma) || (paren && at(Tok::RPar)))
            break;
    }
    if (paren) {
        leave();
        if (!expect(Tok::RPar, "expected ')'"))
            return 0;
    }
    if (!block(body))
        return 0;
    u32 n = add(is_async ? Nd::AsyncWith : Nd::With, t);
    if (!n)
        return 0;
    node(n).a          = u32(items.size());
    node(n).b          = u32(body.size());
    const List *two[2] = { &items, &body };
    return run_of(n, two, 2) ? n : 0;
}

u32 Parser::target()
{
    u32 e = at(Tok::Star) ? starred_or(true, &Parser::target) : postfix(atom());
    if (!e)
        return 0;
    if (!is_target(ast, e))
        return fail_node("cannot assign to this", e);
    return e;
}

// The target of a `for`, which is not a full expression: `in` would otherwise
// be taken for the comparison operator and eat the iterable.
u32 Parser::target_list()
{
    u32 t     = u32(i);
    u32 first = target();
    if (!first || !at(Tok::Comma))
        return first;
    List elts;
    if (!hold(elts, first))
        return 0;
    while (take(Tok::Comma)) {
        if (at(Tok::KwIn) || at(Tok::Equal) || at(Tok::Newline) || at(Tok::End))
            break;
        if (!hold(elts, target()))
            return 0;
    }
    u32 n = add(Nd::Tuple, t);
    return n && run_of(n, elts) ? n : 0;
}

u32 Parser::funcdef(bool is_async, List &decorators)
{
    bump(); // def
    if (!at(Tok::Name))
        return fail("expected a function name");
    u32 name = u32(i);
    bump();
    List params;
    if (!type_params(params) || !expect(Tok::LPar, "expected '('"))
        return 0;
    u32 args = arguments(false);
    if (!args || !expect(Tok::RPar, "expected ')'"))
        return 0;
    u32 returns = 0;
    if (take(Tok::Arrow) && !(returns = ternary()))
        return 0;
    List body;
    if (!block(body))
        return 0;

    u32 n = add(is_async ? Nd::AsyncFunctionDef : Nd::FunctionDef, name);
    if (!n)
        return 0;
    node(n).a            = args;
    node(n).b            = u32(body.size());
    node(n).c            = u32(decorators.size());
    node(n).d            = returns;
    node(n).pad          = u16(params.size());
    const List *three[3] = { &body, &decorators, &params };
    return run_of(n, three, 3) ? n : 0;
}

u32 Parser::classdef(List &decorators)
{
    bump(); // class
    if (!at(Tok::Name))
        return fail("expected a class name");
    u32 name = u32(i);
    bump();

    List bases, keywords, body, params;
    if (!type_params(params))
        return 0;
    if (take(Tok::LPar)) {
        while (!at(Tok::RPar)) {
            if (take(Tok::DStar)) {
                u32 k = add(Nd::Keyword, u32(i - 1));
                if (!k)
                    return 0;
                node(k).a = ternary();
                if (!node(k).a || !hold(keywords, k))
                    return 0;
            } else if (at(Tok::Name) && kind(1) == Tok::Equal) {
                u32 kt = u32(i);
                bump();
                bump();
                u32 k = add(Nd::Keyword, kt);
                if (!k)
                    return 0;
                node(k).flags = 1;
                node(k).a     = ternary();
                if (!node(k).a || !hold(keywords, k))
                    return 0;
            } else if (!hold(bases, starred_or(true, &Parser::ternary, true))) {
                return 0;
            }
            if (!take(Tok::Comma))
                break;
        }
        if (!expect(Tok::RPar, "expected ')'"))
            return 0;
    }
    if (!block(body))
        return 0;

    u32 n = add(Nd::ClassDef, name);
    if (!n)
        return 0;
    node(n).a           = u32(bases.size());
    node(n).b           = u32(keywords.size());
    node(n).c           = u32(body.size());
    node(n).d           = u32(decorators.size());
    node(n).pad         = u16(params.size());
    const List *five[5] = { &bases, &keywords, &body, &decorators, &params };
    return run_of(n, five, 5) ? n : 0;
}

u32 Parser::decorated()
{
    List decorators;
    while (take(Tok::At)) {
        if (!hold(decorators, named()))
            return 0;
        if (!take(Tok::Newline))
            return fail("expected a newline after a decorator");
    }
    if (at(Tok::KwDef))
        return funcdef(false, decorators);
    if (at(Tok::KwClass))
        return classdef(decorators);
    if (at(Tok::KwAsync) && kind(1) == Tok::KwDef) {
        bump();
        return funcdef(true, decorators);
    }
    return fail("expected a definition after a decorator");
}

u32 Parser::import_statement()
{
    u32 t = u32(i);
    bump();
    List names;
    for (;;) {
        if (!at(Tok::Name))
            return fail("expected a module name");
        u32 first = u32(i);
        bump();
        while (at(Tok::Dot) && kind(1) == Tok::Name) {
            bump();
            bump();
        }
        u32 a = add(Nd::Alias, first);
        if (!a)
            return 0;
        // A dotted name is kept as the span of tokens it was written as.
        node(a).b = u32(i);
        if (take(Tok::KwAs)) {
            if (!at(Tok::Name))
                return fail("expected a name after 'as'");
            node(a).a = u32(i) + 1;
            bump();
        }
        if (!hold(names, a))
            return 0;
        if (!take(Tok::Comma))
            break;
    }
    u32 n = add(Nd::Import, t);
    return n && run_of(n, names) ? n : 0;
}

u32 Parser::from_statement()
{
    u32 t = u32(i);
    bump();
    u32 level = 0;
    while (at(Tok::Dot) || at(Tok::Ellipsis)) {
        level += at(Tok::Ellipsis) ? 3 : 1;
        bump();
    }
    u32 module = 0, module_end = 0;
    bool named = false;
    if (at(Tok::Name)) {
        module = u32(i);
        named  = true;
        bump();
        while (at(Tok::Dot) && kind(1) == Tok::Name) {
            bump();
            bump();
        }
        module_end = u32(i);
    }
    if (!expect(Tok::KwImport, "expected 'import'"))
        return 0;

    List names;
    bool paren = take(Tok::LPar);
    if (at(Tok::Star)) {
        u32 a = add(Nd::Alias, u32(i));
        bump();
        if (!a)
            return 0;
        node(a).flags = 1; // `*`
        if (!hold(names, a))
            return 0;
    } else {
        for (;;) {
            if (!at(Tok::Name))
                return fail("expected a name");
            u32 a = add(Nd::Alias, u32(i));
            bump();
            if (!a)
                return 0;
            if (take(Tok::KwAs)) {
                if (!at(Tok::Name))
                    return fail("expected a name after 'as'");
                node(a).a = u32(i) + 1;
                bump();
            }
            if (!hold(names, a))
                return 0;
            if (!take(Tok::Comma))
                break;
            if (paren && at(Tok::RPar))
                break;
        }
    }
    if (paren && !expect(Tok::RPar, "expected ')'"))
        return 0;

    u32 n = add(Nd::ImportFrom, named ? module : t);
    if (!n || !run_of(n, names))
        return 0;
    node(n).a     = level;
    node(n).b     = module_end;
    node(n).flags = named ? 1 : 0;
    return n;
}

u32 Parser::small_statement()
{
    u32 t = u32(i);
    switch (kind()) {
    case Tok::KwPass:
        bump();
        return add(Nd::Pass, t);
    case Tok::KwBreak:
        bump();
        return add(Nd::Break, t);
    case Tok::KwContinue:
        bump();
        return add(Nd::Continue, t);
    case Tok::KwReturn: {
        bump();
        u32 n = add(Nd::Return, t);
        if (!n)
            return 0;
        if (!at(Tok::Newline) && !at(Tok::Semi) && !at(Tok::End) && !at(Tok::Dedent) &&
            !(node(n).a = exprlist(true)))
            return 0;
        return n;
    }
    case Tok::KwRaise: {
        bump();
        u32 n = add(Nd::Raise, t);
        if (!n)
            return 0;
        if (!at(Tok::Newline) && !at(Tok::Semi) && !at(Tok::End) && !at(Tok::Dedent)) {
            if (!(node(n).a = expression()))
                return 0;
            if (take(Tok::KwFrom) && !(node(n).b = expression()))
                return 0;
        }
        return n;
    }
    case Tok::KwDel: {
        bump();
        List targets;
        for (;;) {
            if (!hold(targets, target()))
                return 0;
            if (!take(Tok::Comma) || at(Tok::Newline) || at(Tok::Semi) || at(Tok::End))
                break;
        }
        u32 n = add(Nd::Delete, t);
        return n && run_of(n, targets) ? n : 0;
    }
    case Tok::KwGlobal:
    case Tok::KwNonlocal: {
        bool global = at(Tok::KwGlobal);
        bump();
        List names;
        for (;;) {
            if (!at(Tok::Name))
                return fail("expected a name");
            u32 nm = add(Nd::Name, u32(i));
            bump();
            if (!hold(names, nm))
                return 0;
            if (!take(Tok::Comma))
                break;
        }
        u32 n = add(global ? Nd::Global : Nd::Nonlocal, t);
        return n && run_of(n, names) ? n : 0;
    }
    case Tok::KwAssert: {
        bump();
        u32 n = add(Nd::Assert, t);
        if (!n || !(node(n).a = ternary()))
            return 0;
        if (take(Tok::Comma) && !(node(n).b = ternary()))
            return 0;
        return n;
    }
    case Tok::KwImport:
        return import_statement();
    case Tok::KwFrom:
        return from_statement();
    case Tok::Name:
        if (type_alias_ahead())
            return type_alias();
        // PEP 810: `lazy` is soft, and only before an import.
        if (soft("lazy") && (kind(1) == Tok::KwImport || kind(1) == Tok::KwFrom)) {
            u32 lt = u32(i);
            bump();
            bool from = at(Tok::KwFrom);
            u32 n     = from ? from_statement() : import_statement();
            if (!n)
                return 0;
            if (from && node(n).flags && ast->text(n) == "__future__") {
                const Token &tk = ast->lex.tokens[lt];
                if (!failed) {
                    failed = true;
                    err_set_at("SyntaxError", "lazy from __future__ import is not allowed", tk.line,
                               tk.col);
                }
                return 0;
            }
            node(n).pad |= 1;
            return n;
        }
        break;
    default:
        break;
    }

    // An expression statement, an assignment, or an augmented assignment.
    u32 first = exprlist(true);
    if (!first)
        return 0;

    for (const AugOp &a : AUGMENTED)
        if (at(a.tok)) {
            u32 at_tok = u32(i);
            bump();
            if (!is_target(ast, first))
                return fail_node("cannot assign to this", first);
            u32 n = add(Nd::AugAssign, at_tok);
            if (!n)
                return 0;
            node(n).flags = u8(a.op);
            node(n).a     = first;
            node(n).b     = at(Tok::KwYield) ? expression() : exprlist(true);
            return node(n).b ? n : 0;
        }

    if (at(Tok::Colon)) {
        u32 at_tok = u32(i);
        bump();
        if (!is_target(ast, first))
            return fail_node("cannot annotate this", first);
        u32 ann = ternary();
        if (!ann)
            return 0;
        u32 value = 0;
        if (take(Tok::Equal)) {
            value = at(Tok::KwYield) ? expression() : exprlist(true);
            if (!value)
                return 0;
        }
        u32 n = add(Nd::AnnAssign, at_tok);
        if (!n)
            return 0;
        node(n).a     = first;
        node(n).b     = ann;
        node(n).c     = value;
        node(n).flags = ast->at(first).kind == Nd::Name ? 1 : 0;
        return n;
    }

    if (!at(Tok::Equal)) {
        u32 n = add(Nd::Expr, ast->at(first).tok);
        if (!n)
            return 0;
        node(n).a = first;
        return n;
    }

    // A chain: a = b = value. Everything before the last `=` is a target.
    List targets;
    if (!hold(targets, first))
        return 0;
    u32 value = 0;
    while (take(Tok::Equal)) {
        u32 e = at(Tok::KwYield) ? expression() : exprlist(true);
        if (!e)
            return 0;
        if (at(Tok::Equal)) {
            if (!hold(targets, e))
                return 0;
        } else {
            value = e;
        }
    }
    for (usize k = 0; k < targets.size(); k++)
        if (!is_target(ast, targets[k]))
            return fail_node("cannot assign to this", targets[k]);
    u32 n = add(Nd::Assign, ast->at(targets[0]).tok);
    if (!n || !run_of(n, targets))
        return 0;
    node(n).a = value;
    return n;
}

u32 Parser::compound_statement()
{
    List none;
    switch (kind()) {
    case Tok::KwIf:
        return if_statement();
    case Tok::KwWhile:
        return while_statement();
    case Tok::KwFor:
        return for_statement(false);
    case Tok::KwTry:
        return try_statement();
    case Tok::KwWith:
        return with_statement(false);
    case Tok::KwDef:
        return funcdef(false, none);
    case Tok::KwClass:
        return classdef(none);
    case Tok::At:
        return decorated();
    case Tok::Name:
        if (match_ahead())
            return match_statement();
        break;
    case Tok::KwAsync:
        if (kind(1) == Tok::KwDef) {
            bump();
            return funcdef(true, none);
        }
        if (kind(1) == Tok::KwFor) {
            bump();
            return for_statement(true);
        }
        if (kind(1) == Tok::KwWith) {
            bump();
            return with_statement(true);
        }
        return fail("expected 'def', 'for' or 'with' after 'async'");
    default:
        break;
    }
    return 0;
}

// ------------------------------------------------------------------ match
//
// `match` and `case` are soft keywords: a line is a match statement only when
// `match` is followed by an expression, a colon at bracket depth zero, and a
// block that opens with `case`. Anything else is the name `match`.

bool Parser::starts_expression(usize ahead) const
{
    switch (kind(ahead)) {
    case Tok::Name:
    case Tok::Int:
    case Tok::Float:
    case Tok::Imag:
    case Tok::Str:
    case Tok::Bytes:
    case Tok::FStr:
    case Tok::KwNone:
    case Tok::KwTrue:
    case Tok::KwFalse:
    case Tok::Ellipsis:
    case Tok::LPar:
    case Tok::LSqb:
    case Tok::LBrace:
    case Tok::Minus:
    case Tok::Plus:
    case Tok::Tilde:
    case Tok::KwNot:
    case Tok::KwAwait:
    case Tok::KwLambda:
    case Tok::Star:
        return true;
    default:
        return false;
    }
}

bool Parser::match_ahead() const
{
    if (!soft("match") || !starts_expression(1))
        return false;
    u32 nest = 0;
    for (usize k = 1;; k++) {
        switch (kind(k)) {
        case Tok::LPar:
        case Tok::LSqb:
        case Tok::LBrace:
            nest++;
            break;
        case Tok::RPar:
        case Tok::RSqb:
        case Tok::RBrace:
            if (!nest)
                return false;
            nest--;
            break;
        case Tok::Newline:
        case Tok::End:
            return false;
        case Tok::Colon:
            if (!nest)
                return kind(k + 1) == Tok::Newline && kind(k + 2) == Tok::Indent &&
                       soft("case", k + 3);
            break;
        default:
            break;
        }
    }
}

u32 Parser::match_statement()
{
    u32 t = u32(i);
    bump(); // match
    u32 first = starred_or(true, &Parser::named);
    if (!first)
        return 0;
    u32 subject = first;
    if (at(Tok::Comma)) {
        List elts;
        if (!hold(elts, first))
            return 0;
        while (take(Tok::Comma)) {
            if (at(Tok::Colon))
                break;
            if (!hold(elts, starred_or(true, &Parser::named)))
                return 0;
        }
        subject = add(Nd::Tuple, node(first).tok);
        if (!subject || !run_of(subject, elts))
            return 0;
    }
    if (!expect(Tok::Colon, "expected ':'") || !expect(Tok::Newline, "expected a newline"))
        return 0;
    if (!take(Tok::Indent))
        return fail_kind("IndentationError", "expected an indented block");
    if (!enter())
        return 0;
    List cases;
    while (!at(Tok::Dedent) && !at(Tok::End)) {
        if (take(Tok::Newline))
            continue;
        if (!soft("case")) {
            leave();
            return fail("expected 'case'");
        }
        if (!hold(cases, match_case())) {
            leave();
            return 0;
        }
    }
    leave();
    take(Tok::Dedent);
    u32 n = add(Nd::Match, t);
    if (!n || !run_of(n, cases))
        return 0;
    node(n).a = subject;
    return n;
}

u32 Parser::match_case()
{
    u32 t = u32(i);
    bump(); // case
    u32 p = patterns();
    if (!p)
        return 0;
    u32 guard = 0;
    if (take(Tok::KwIf) && !(guard = named()))
        return 0;
    List body;
    if (!block(body))
        return 0;
    u32 n = add(Nd::MatchCase, t);
    if (!n || !run_of(n, body))
        return 0;
    node(n).a = p;
    node(n).b = guard;
    return n;
}

// The pattern after `case`: an open sequence, without brackets, is a
// sequence pattern.
u32 Parser::patterns()
{
    u32 t     = u32(i);
    u32 first = at(Tok::Star) ? star_pattern() : pattern();
    if (!first)
        return 0;
    if (!at(Tok::Comma)) {
        if (ast->at(first).kind == Nd::MatchStar)
            return fail_node("can't use starred expression here", first);
        return first;
    }
    List elts;
    if (!hold(elts, first))
        return 0;
    while (take(Tok::Comma)) {
        if (at(Tok::Colon) || at(Tok::KwIf))
            break;
        if (!hold(elts, at(Tok::Star) ? star_pattern() : pattern()))
            return 0;
    }
    u32 n = add(Nd::MatchSequence, t);
    return n && run_of(n, elts) ? n : 0;
}

// A name a pattern binds: not `_`, and not the start of something longer.
u32 Parser::capture_target()
{
    if (!at(Tok::Name))
        return fail("invalid pattern target");
    if (soft("_"))
        return fail("cannot use '_' as a target");
    u32 t = u32(i);
    bump();
    return t;
}

u32 Parser::pattern()
{
    u32 p = or_pattern();
    if (!p || !at(Tok::KwAs))
        return p;
    bump();
    u32 name = capture_target();
    if (!name)
        return 0;
    u32 n = add(Nd::MatchAs, name);
    if (!n)
        return 0;
    node(n).a     = p;
    node(n).flags = 1;
    return n;
}

u32 Parser::or_pattern()
{
    u32 t     = u32(i);
    u32 first = closed_pattern();
    if (!first || !at(Tok::Vbar))
        return first;
    List alts;
    if (!hold(alts, first))
        return 0;
    while (take(Tok::Vbar))
        if (!hold(alts, closed_pattern()))
            return 0;
    u32 n = add(Nd::MatchOr, t);
    return n && run_of(n, alts) ? n : 0;
}

u32 Parser::star_pattern()
{
    u32 t = u32(i);
    bump(); // *
    if (!at(Tok::Name))
        return fail("invalid syntax");
    bool wild = soft("_");
    u32 name  = u32(i);
    bump();
    u32 n = add(Nd::MatchStar, wild ? t : name);
    if (n)
        node(n).flags = wild ? 0 : 1;
    return n;
}

// A number, or `-` and a number, as a Constant or a UnaryOp over one. A `+`
// is taken and dropped, as CPython's main branch does since 3.15.
u32 Parser::signed_number()
{
    u32 t    = u32(i);
    bool neg = take(Tok::Minus);
    if (!neg)
        take(Tok::Plus);
    if (!at(Tok::Int) && !at(Tok::Float) && !at(Tok::Imag))
        return fail("invalid syntax");
    u32 c = atom();
    if (!c || !neg)
        return c;
    u32 n = add(Nd::UnaryOp, t);
    if (!n)
        return 0;
    node(n).flags = u8(Un::USub);
    node(n).a     = c;
    return n;
}

// A literal number, or `real ± imaginary`.
u32 Parser::number_pattern()
{
    u32 left = signed_number();
    if (!left || (!at(Tok::Plus) && !at(Tok::Minus)))
        return left;
    const Node &l  = ast->at(left);
    const Node &lc = l.kind == Nd::UnaryOp ? ast->at(l.a) : l;
    if (Const(lc.flags) == Const::Imag)
        return fail_node("real number required in complex literal", left);
    u32 t = u32(i);
    Op op = at(Tok::Plus) ? Op::Add : Op::Sub;
    bump();
    if (!at(Tok::Imag))
        return fail("imaginary number required in complex literal");
    u32 right = atom();
    if (!right)
        return 0;
    u32 n = add(Nd::BinOp, t);
    if (!n)
        return 0;
    node(n).flags = u8(op);
    node(n).a     = left;
    node(n).b     = right;
    return n;
}

u32 Parser::name_or_attr()
{
    u32 v = add(Nd::Name, u32(i));
    bump();
    while (v && at(Tok::Dot)) {
        bump();
        if (!at(Tok::Name))
            return fail("expected a name after '.'");
        u32 n = add(Nd::Attribute, u32(i));
        bump();
        if (!n)
            return 0;
        node(n).a = v;
        v         = n;
    }
    return v;
}

u32 Parser::closed_pattern()
{
    if (!enter())
        return 0;
    u32 t = u32(i);
    u32 r = 0;
    switch (kind()) {
    case Tok::Int:
    case Tok::Float:
    case Tok::Imag:
    case Tok::Minus:
    case Tok::Plus: {
        u32 v = number_pattern();
        r     = v ? add(Nd::MatchValue, t) : 0;
        if (r)
            node(r).a = v;
        break;
    }
    case Tok::Str:
    case Tok::Bytes:
    case Tok::FStr: {
        for (usize k = 0; kind(k) == Tok::Str || kind(k) == Tok::Bytes || kind(k) == Tok::FStr; k++)
            if (kind(k) == Tok::FStr) {
                leave();
                return fail("patterns may not match formatted string literals");
            }
        u32 v = atom();
        r     = v ? add(Nd::MatchValue, t) : 0;
        if (r)
            node(r).a = v;
        break;
    }
    case Tok::KwNone:
    case Tok::KwTrue:
    case Tok::KwFalse: {
        Const c = at(Tok::KwNone) ? Const::None : at(Tok::KwTrue) ? Const::True : Const::False;
        bump();
        r = add(Nd::MatchSingleton, t);
        if (r)
            node(r).flags = u8(c);
        break;
    }
    case Tok::Name: {
        bool wild   = soft("_");
        u32 v       = name_or_attr();
        bool dotted = v && ast->at(v).kind == Nd::Attribute;
        if (!v)
            break;
        if (at(Tok::LPar)) {
            r = class_pattern(v);
        } else if (dotted) {
            r = add(Nd::MatchValue, t);
            if (r)
                node(r).a = v;
        } else {
            r = add(Nd::MatchAs, t);
            if (r)
                node(r).flags = wild ? 0 : 1;
        }
        break;
    }
    case Tok::LPar: {
        bump();
        if (take(Tok::RPar)) {
            r = add(Nd::MatchSequence, t);
            break;
        }
        u32 first = at(Tok::Star) ? star_pattern() : pattern();
        if (!first)
            break;
        if (at(Tok::Comma)) {
            List elts;
            if (!hold(elts, first))
                break;
            while (take(Tok::Comma)) {
                if (at(Tok::RPar))
                    break;
                if (!hold(elts, at(Tok::Star) ? star_pattern() : pattern())) {
                    leave();
                    return 0;
                }
            }
            if (!expect(Tok::RPar, "expected ')'"))
                break;
            r = add(Nd::MatchSequence, t);
            if (r && !run_of(r, elts))
                r = 0;
            break;
        }
        if (!expect(Tok::RPar, "expected ')'"))
            break;
        if (ast->at(first).kind == Nd::MatchStar) {
            leave();
            return fail_node("can't use starred expression here", first);
        }
        r = first;
        break;
    }
    case Tok::LSqb:
        bump();
        r = sequence_pattern(Tok::RSqb, t);
        break;
    case Tok::LBrace:
        r = mapping_pattern();
        break;
    default:
        r = fail("invalid syntax");
        break;
    }
    leave();
    return r;
}

// `[p, *rest, q]`, the opening bracket already taken.
u32 Parser::sequence_pattern(Tok close, u32 at_tok)
{
    List elts;
    while (!at(close)) {
        if (!hold(elts, at(Tok::Star) ? star_pattern() : pattern()))
            return 0;
        if (!take(Tok::Comma))
            break;
    }
    if (!expect(close, "expected ']'"))
        return 0;
    u32 n = add(Nd::MatchSequence, at_tok);
    return n && run_of(n, elts) ? n : 0;
}

u32 Parser::mapping_pattern()
{
    u32 t = u32(i);
    bump(); // {
    List keys, pats;
    u32 rest = 0;
    while (!at(Tok::RBrace)) {
        if (take(Tok::DStar)) {
            rest = capture_target();
            if (!rest)
                return 0;
            take(Tok::Comma);
            break;
        }
        u32 key = 0;
        switch (kind()) {
        case Tok::Int:
        case Tok::Float:
        case Tok::Imag:
        case Tok::Minus:
        case Tok::Plus:
            key = number_pattern();
            break;
        case Tok::Str:
        case Tok::Bytes:
            key = atom();
            break;
        case Tok::FStr:
            return fail("patterns may not match formatted string literals");
        case Tok::KwNone:
        case Tok::KwTrue:
        case Tok::KwFalse:
            key = atom();
            break;
        case Tok::Name:
            key = name_or_attr();
            if (key && ast->at(key).kind != Nd::Attribute)
                return fail_node("invalid syntax", key);
            break;
        default:
            return fail("invalid syntax");
        }
        if (!key || !expect(Tok::Colon, "expected ':'"))
            return 0;
        u32 p = pattern();
        if (!p || !keys.push(key) || !pats.push(p))
            return p ? fail("out of memory") : 0;
        if (!take(Tok::Comma))
            break;
    }
    if (!expect(Tok::RBrace, "expected '}'"))
        return 0;
    u32 n = add(Nd::MatchMapping, rest ? rest : t);
    if (!n)
        return 0;
    const List *two[2] = { &keys, &pats };
    if (!run_of(n, two, 2))
        return 0;
    node(n).a     = u32(keys.size());
    node(n).flags = rest ? 1 : 0;
    return n;
}

// `Point(x, y=0)`, the class already parsed.
u32 Parser::class_pattern(u32 cls)
{
    u32 t = u32(i);
    bump(); // (
    List args, kws;
    while (!at(Tok::RPar)) {
        if (at(Tok::Name) && kind(1) == Tok::Equal) {
            u32 kt = u32(i);
            bump();
            bump();
            u32 p = pattern();
            u32 k = p ? add(Nd::Keyword, kt) : 0;
            if (!k)
                return 0;
            node(k).flags = 1;
            node(k).a     = p;
            if (!hold(kws, k))
                return 0;
        } else {
            if (kws.size())
                return fail("positional patterns follow keyword patterns");
            if (!hold(args, pattern()))
                return 0;
        }
        if (!take(Tok::Comma))
            break;
    }
    if (!expect(Tok::RPar, "expected ')'"))
        return 0;
    u32 n = add(Nd::MatchClass, t);
    if (!n)
        return 0;
    const List *two[2] = { &args, &kws };
    if (!run_of(n, two, 2))
        return 0;
    node(n).a = cls;
    node(n).b = u32(args.size());
    return n;
}

// ---------------------------------------------------------------- PEP 695

// `[T, *Ts, **P]` after a name, when there is one; `into` is left empty
// otherwise.
bool Parser::type_params(List &into)
{
    if (!take(Tok::LSqb))
        return true;
    if (at(Tok::RSqb))
        return fail("Type parameter list cannot be empty"), false;
    while (!at(Tok::RSqb)) {
        if (!hold(into, type_param()))
            return false;
        if (!take(Tok::Comma))
            break;
    }
    return expect(Tok::RSqb, "expected ']'");
}

u32 Parser::type_param()
{
    Nd kind = take(Tok::DStar) ? Nd::ParamSpec : take(Tok::Star) ? Nd::TypeVarTuple : Nd::TypeVar;
    if (!at(Tok::Name))
        return fail("expected a type parameter name");
    u32 n = add(kind, u32(i));
    bump();
    if (!n)
        return 0;
    if (at(Tok::Colon)) {
        if (kind == Nd::TypeVarTuple)
            return fail("cannot use bound with TypeVarTuple");
        if (kind == Nd::ParamSpec)
            return fail("cannot use bound with ParamSpec");
        bump();
        if (!(node(n).a = ternary()))
            return 0;
    }
    if (take(Tok::Equal)) {
        // A TypeVarTuple's default may be starred: `*Ts = *tuple[int]`.
        u32 d = kind == Nd::TypeVarTuple ? starred_or(true, &Parser::ternary) : ternary();
        if (!d)
            return 0;
        node(n).b = d;
    }
    return n;
}

// `type X = ...` and `type X[T] = ...`: `type` is soft.
bool Parser::type_alias_ahead() const
{
    return soft("type") && kind(1) == Tok::Name && (kind(2) == Tok::Equal || kind(2) == Tok::LSqb);
}

u32 Parser::type_alias()
{
    u32 t = u32(i);
    bump(); // type
    u32 name = add(Nd::Name, u32(i));
    bump();
    if (!name)
        return 0;
    List params;
    if (!type_params(params) || !expect(Tok::Equal, "expected '='"))
        return 0;
    u32 value = expression();
    if (!value)
        return 0;
    u32 n = add(Nd::TypeAlias, t);
    if (!n || !run_of(n, params))
        return 0;
    node(n).a = name;
    node(n).b = value;
    node(n).c = u32(params.size());
    return n;
}

bool Parser::run()
{
    List body;
    while (!at(Tok::End)) {
        if (take(Tok::Newline))
            continue;
        if (at(Tok::Indent))
            return fail_kind("IndentationError", "unexpected indent"), false;
        if (!statements_line(body))
            return false;
    }
    u32 n = add(Nd::Module, u32(i));
    if (!n || !run_of(n, body))
        return false;
    ast->root = n;
    return true;
}

} // namespace

bool Ast::parse(Str source, bool decoded)
{
    if (!lex.run(source, decoded))
        return false;
    // Node 0 is Nop, so an index of 0 reads as "nothing".
    if (!nodes.push(Node{}))
        return err_set("MemoryError", "out of memory"), false;
    Parser p{ this };
    return p.run();
}
