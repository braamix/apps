// The tokenizer.
#include "lex.h"

#include "err.h"
#include "gc.h"
#include "kernel/fmt.h"
#include "kernel/text.h"
#include "math/ftoa.h"
#include "ops.h"

namespace {

struct Spelling {
    Tok kind;
    Str word;
};

constexpr Spelling KEYWORDS[] = {
    { Tok::KwFalse, "False" },
    { Tok::KwNone, "None" },
    { Tok::KwTrue, "True" },
    { Tok::KwAnd, "and" },
    { Tok::KwAs, "as" },
    { Tok::KwAssert, "assert" },
    { Tok::KwAsync, "async" },
    { Tok::KwAwait, "await" },
    { Tok::KwBreak, "break" },
    { Tok::KwClass, "class" },
    { Tok::KwContinue, "continue" },
    { Tok::KwDef, "def" },
    { Tok::KwDel, "del" },
    { Tok::KwElif, "elif" },
    { Tok::KwElse, "else" },
    { Tok::KwExcept, "except" },
    { Tok::KwFinally, "finally" },
    { Tok::KwFor, "for" },
    { Tok::KwFrom, "from" },
    { Tok::KwGlobal, "global" },
    { Tok::KwIf, "if" },
    { Tok::KwImport, "import" },
    { Tok::KwIn, "in" },
    { Tok::KwIs, "is" },
    { Tok::KwLambda, "lambda" },
    { Tok::KwNonlocal, "nonlocal" },
    { Tok::KwNot, "not" },
    { Tok::KwOr, "or" },
    { Tok::KwPass, "pass" },
    { Tok::KwRaise, "raise" },
    { Tok::KwReturn, "return" },
    { Tok::KwTry, "try" },
    { Tok::KwWhile, "while" },
    { Tok::KwWith, "with" },
    { Tok::KwYield, "yield" },
};

// Longest first: the scanner takes the first that matches.
constexpr Spelling OPERATORS[] = {
    { Tok::DSlashEq, "//=" }, { Tok::RShiftEq, ">>=" }, { Tok::LShiftEq, "<<=" },
    { Tok::DStarEq, "**=" },  { Tok::Ellipsis, "..." }, { Tok::Arrow, "->" },
    { Tok::PlusEq, "+=" },    { Tok::MinusEq, "-=" },   { Tok::StarEq, "*=" },
    { Tok::SlashEq, "/=" },   { Tok::PercentEq, "%=" }, { Tok::AtEq, "@=" },
    { Tok::AmpEq, "&=" },     { Tok::VbarEq, "|=" },    { Tok::CaretEq, "^=" },
    { Tok::DSlash, "//" },    { Tok::DStar, "**" },     { Tok::LShift, "<<" },
    { Tok::RShift, ">>" },    { Tok::Walrus, ":=" },    { Tok::LessEq, "<=" },
    { Tok::GreaterEq, ">=" }, { Tok::EqEq, "==" },      { Tok::NotEq, "!=" },
    { Tok::LPar, "(" },       { Tok::RPar, ")" },       { Tok::LSqb, "[" },
    { Tok::RSqb, "]" },       { Tok::LBrace, "{" },     { Tok::RBrace, "}" },
    { Tok::Comma, "," },      { Tok::Colon, ":" },      { Tok::Dot, "." },
    { Tok::Semi, ";" },       { Tok::At, "@" },         { Tok::Equal, "=" },
    { Tok::Plus, "+" },       { Tok::Minus, "-" },      { Tok::Star, "*" },
    { Tok::Slash, "/" },      { Tok::Percent, "%" },    { Tok::Amp, "&" },
    { Tok::Vbar, "|" },       { Tok::Caret, "^" },      { Tok::Tilde, "~" },
    { Tok::Less, "<" },       { Tok::Greater, ">" },
};

constexpr usize MAX_INDENT = 100;

bool is_name_start(u8 c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 0x80;
}

bool is_name_char(u8 c)
{
    return is_name_start(c) || (c >= '0' && c <= '9');
}

bool is_digit(u8 c)
{
    return c >= '0' && c <= '9';
}

int hex_value(u8 c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

struct Scanner {
    Str src;
    Lexer *out;

    usize i    = 0; // byte offset
    u32 line   = 1;
    u32 col    = 1;    // codepoints, 1-based
    u32 depth  = 0;    // open brackets
    bool fresh = true; // at the start of a logical line

    // One expression out of an f-string's braces: no indentation, no trailing
    // Newline, Dedent or End, and no complaint that the brackets never closed.
    bool fragment = false;

    usize indents[MAX_INDENT] = { 0 };
    usize alts[MAX_INDENT]    = { 0 }; // the same columns with tabs worth 1
    usize levels              = 1;

    bool fail(Str message) { return fail_at(message, line, col); }

    // Errors point at what is wrong -- the literal, the escape, the character
    // -- and not at wherever scanning happened to stop.
    bool fail_at(Str message, u32 at_line, u32 at_col)
    {
        return err_set_at("SyntaxError", message, at_line, at_col), false;
    }

    bool fail_kind(Str kind, Str message, u32 at_line, u32 at_col)
    {
        return err_set_at(kind, message, at_line, at_col), false;
    }

    bool at_end() const { return i >= src.size(); }

    u8 peek(usize ahead = 0) const { return i + ahead < src.size() ? u8(src[i + ahead]) : 0; }

    // One byte. Columns count codepoints, so a continuation byte is not one.
    void bump()
    {
        if (i >= src.size())
            return;
        u8 c = u8(src[i++]);
        if (c == '\n') {
            line++;
            col = 1;
        } else if ((c & 0xc0) != 0x80) {
            col++;
        }
    }

    bool emit(Tok kind, u32 at_line, u32 at_col)
    {
        Token t;
        t.kind = kind;
        t.line = at_line;
        t.col  = at_col;
        return out->tokens.push(t) ? true : fail("out of memory");
    }

    bool emit_text(Tok kind, u32 at_line, u32 at_col, Str body, u8 flags = 0)
    {
        Token t;
        t.kind  = kind;
        t.line  = at_line;
        t.col   = at_col;
        t.flags = flags;
        t.at    = u32(out->text.size());
        t.len   = u32(body.size());
        if (!out->text.append(body))
            return fail("out of memory");
        return out->tokens.push(t) ? true : fail("out of memory");
    }

    bool run();
    bool line_start();
    bool one_token();
    bool scan_name();
    bool scan_number();
    bool scan_string(u8 flags, u32 at_line, u32 at_col);
    bool string_escape(String &body, bool bytes, u32 at_line, u32 at_col);
};

// Indentation, before the first token of a logical line.
bool Scanner::line_start()
{
    for (;;) {
        usize width = 0, alt = 0;
        while (!at_end() && (peek() == ' ' || peek() == '\t' || peek() == '\f')) {
            if (peek() == ' ') {
                width++;
                alt++;
            } else if (peek() == '\t') {
                width = (width / 8 + 1) * 8;
                alt++;
            } else {
                width = 0;
                alt   = 0;
            }
            bump();
        }
        // A blank or comment-only line has no indentation to speak of.
        if (at_end())
            break;
        if (peek() == '#') {
            while (!at_end() && peek() != '\n')
                bump();
        }
        if (peek() == '\n') {
            bump();
            continue;
        }
        if (peek() == '\r') {
            bump();
            continue;
        }

        if (width > indents[levels - 1]) {
            if (alt <= alts[levels - 1])
                return fail_kind("TabError", "inconsistent use of tabs and spaces in indentation",
                                 line, col);
            if (levels >= MAX_INDENT)
                return fail_kind("IndentationError", "too many levels of indentation", line, col);
            indents[levels] = width;
            alts[levels]    = alt;
            levels++;
            // CPython's INDENT covers the whitespace, so it starts at column 1.
            if (!emit(Tok::Indent, line, 1))
                return false;
        } else {
            while (levels > 1 && width < indents[levels - 1]) {
                levels--;
                if (!emit(Tok::Dedent, line, col))
                    return false;
            }
            if (width != indents[levels - 1])
                return fail_kind("IndentationError",
                                 "unindent does not match any outer indentation level", line, col);
            if (alt != alts[levels - 1])
                return fail_kind("TabError", "inconsistent use of tabs and spaces in indentation",
                                 line, col);
        }
        break;
    }
    fresh = false;
    return true;
}

bool Scanner::scan_name()
{
    u32 at_line = line, at_col = col;
    usize from = i;
    while (!at_end() && is_name_char(peek()))
        bump();
    Str word = src.substr(from, i - from);

    // A string prefix is a name that a quote follows: r, b, u, f and pairs.
    if (!at_end() && (peek() == '"' || peek() == '\'') && word.size() <= 2) {
        u8 flags = 0;
        bool ok  = true;
        for (usize k = 0; k < word.size() && ok; k++) {
            char c = word[k] >= 'A' && word[k] <= 'Z' ? char(word[k] + 32) : word[k];
            if (c == 'r')
                flags |= TOK_STR_RAW;
            else if (c == 'b')
                flags |= TOK_STR_BYTES;
            else if (c == 'f')
                flags |= TOK_STR_F;
            else if (c != 'u')
                ok = false;
        }
        if (ok && !((flags & TOK_STR_BYTES) && (flags & TOK_STR_F)))
            return scan_string(flags, at_line, at_col);
    }

    for (const Spelling &k : KEYWORDS)
        if (word == k.word)
            return emit(k.kind, at_line, at_col);
    return emit_text(Tok::Name, at_line, at_col, word);
}

bool Scanner::scan_number()
{
    u32 at_line = line, at_col = col;
    usize from = i;

    u32 base = 10;
    if (peek() == '0' && (peek(1) | 32) == 'x')
        base = 16;
    else if (peek() == '0' && (peek(1) | 32) == 'o')
        base = 8;
    else if (peek() == '0' && (peek(1) | 32) == 'b')
        base = 2;

    if (base != 10) {
        bump();
        bump();
        i64 value  = 0;
        bool empty = true;
        while (!at_end() && (peek() == '_' || hex_value(peek()) >= 0)) {
            if (peek() == '_') {
                bump();
                continue;
            }
            int d = hex_value(peek());
            if (d < 0 || u32(d) >= base)
                break;
            if (value > (i64(1) << 62) / i64(base))
                return fail_at("int too large (no bignum yet)", at_line, at_col);
            value = value * base + d;
            empty = false;
            bump();
        }
        if (empty || (!at_end() && is_name_char(peek())))
            return fail_at("invalid digit in number", at_line, at_col);
        Token t;
        t.kind = Tok::Int;
        t.line = at_line;
        t.col  = at_col;
        t.ival = value;
        return out->tokens.push(t) ? true : fail("out of memory");
    }

    bool real = false;
    while (!at_end() && (is_digit(peek()) || peek() == '_'))
        bump();
    if (!at_end() && peek() == '.' && is_digit(peek(1))) {
        real = true;
        bump();
        while (!at_end() && (is_digit(peek()) || peek() == '_'))
            bump();
    } else if (!at_end() && peek() == '.' && !is_name_start(peek(1)) && peek(1) != '.') {
        real = true;
        bump();
    }
    if (!at_end() && (peek() | 32) == 'e' &&
        (is_digit(peek(1)) || ((peek(1) == '+' || peek(1) == '-') && is_digit(peek(2))))) {
        real = true;
        bump();
        bump();
        while (!at_end() && is_digit(peek()))
            bump();
    }
    if (!at_end() && (peek() | 32) == 'j')
        return fail_at("complex numbers are not supported", at_line, at_col);
    if (!at_end() && is_name_char(peek()))
        return fail_at("invalid digit in number", at_line, at_col);

    // Underscores are separators only; the value does not see them.
    Buf<64> digits;
    for (usize k = from; k < i; k++)
        if (src[k] != '_')
            digits.put(src[k]);

    Token t;
    t.line = at_line;
    t.col  = at_col;
    if (real) {
        t.kind         = Tok::Float;
        usize used     = 0;
        Option<f64> dv = scan_f64(digits.str(), used);
        if (!dv.has_value())
            return fail_at("invalid number", at_line, at_col);
        t.fval = dv.value();
    } else {
        t.kind         = Tok::Int;
        usize used     = 0;
        Option<i64> iv = scan_i64(digits.str(), used, 10);
        if (!iv.has_value() || used != digits.str().size())
            return fail_at("int too large (no bignum yet)", at_line, at_col);
        t.ival = iv.value();
    }
    return out->tokens.push(t) ? true : fail("out of memory");
}

// One backslash escape, already past the backslash.
bool Scanner::string_escape(String &body, bool bytes, u32 at_line, u32 at_col)
{
    if (at_end())
        return fail_at("EOF in multi-line string", at_line, at_col);
    u8 c = peek();
    bump();
    switch (c) {
    case '\n':
        return true; // a line continuation inside the literal
    case '\\':
    case '\'':
    case '"':
        return body.push(char(c)) || fail("out of memory");
    case 'n':
        return body.push('\n') || fail("out of memory");
    case 't':
        return body.push('\t') || fail("out of memory");
    case 'r':
        return body.push('\r') || fail("out of memory");
    case 'a':
        return body.push('\a') || fail("out of memory");
    case 'b':
        return body.push('\b') || fail("out of memory");
    case 'f':
        return body.push('\f') || fail("out of memory");
    case 'v':
        return body.push('\v') || fail("out of memory");
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7': {
        u32 v = u32(c - '0');
        for (int k = 0; k < 2 && !at_end() && peek() >= '0' && peek() <= '7'; k++) {
            v = v * 8 + u32(peek() - '0');
            bump();
        }
        return body.push(char(v & 0xff)) || fail("out of memory");
    }
    case 'x': {
        int h1 = at_end() ? -1 : hex_value(peek());
        if (h1 < 0)
            return fail_at("truncated \\xXX escape", at_line, at_col);
        bump();
        int h2 = at_end() ? -1 : hex_value(peek());
        if (h2 < 0)
            return fail_at("truncated \\xXX escape", at_line, at_col);
        bump();
        u32 v = u32(h1 * 16 + h2);
        if (bytes)
            return body.push(char(v)) || fail("out of memory");
        char tmp[4];
        usize n = utf8_encode(char32_t(v), tmp);
        return body.append(Str(tmp, n)) || fail("out of memory");
    }
    case 'u':
    case 'U': {
        if (bytes)
            break; // not an escape in a bytes literal
        usize want = c == 'u' ? 4 : 8;
        u32 v      = 0;
        for (usize k = 0; k < want; k++) {
            int h = at_end() ? -1 : hex_value(peek());
            if (h < 0)
                return fail(c == 'u' ? "truncated \\uXXXX escape" : "truncated \\UXXXXXXXX escape");
            v = v * 16 + u32(h);
            bump();
        }
        if (v > 0x10ffff || (v >= 0xd800 && v <= 0xdfff))
            return fail_at("invalid unicode escape", at_line, at_col);
        char tmp[4];
        usize n = utf8_encode(char32_t(v), tmp);
        return body.append(Str(tmp, n)) || fail("out of memory");
    }
    case 'N':
        if (!bytes)
            return fail_at("\\N{...} escapes are not supported", at_line, at_col);
        break;
    default:
        break;
    }
    // Python keeps an unknown escape as the two characters it was written as.
    return (body.push('\\') && body.push(char(c))) || fail("out of memory");
}

bool Scanner::scan_string(u8 flags, u32 at_line, u32 at_col)
{
    u8 quote = peek();
    bump();
    bool triple = peek() == quote && peek(1) == quote;
    if (triple) {
        bump();
        bump();
    }

    bool raw      = (flags & TOK_STR_RAW) != 0;
    bool bytes    = (flags & TOK_STR_BYTES) != 0;
    usize body_at = i;
    String body;
    for (;;) {
        if (at_end())
            return fail_at(
                triple ? "EOF in multi-line string" : "EOL while scanning string literal", at_line,
                at_col);
        if (peek() == quote) {
            if (!triple) {
                bump();
                break;
            }
            if (peek(1) == quote && peek(2) == quote) {
                bump();
                bump();
                bump();
                break;
            }
        }
        if (peek() == '\n' && !triple)
            return fail_at("EOL while scanning string literal", at_line, at_col);
        if (peek() == '\\' && !raw) {
            u32 esc_line = line, esc_col = col;
            bump();
            if (!string_escape(body, bytes, esc_line, esc_col))
                return false;
            continue;
        }
        if (peek() == '\\' && raw) {
            // A raw literal keeps the backslash but the quote after it still
            // does not end the string.
            if (!body.push('\\'))
                return fail("out of memory");
            bump();
            if (!at_end()) {
                if (!body.push(src[i]))
                    return fail("out of memory");
                bump();
            }
            continue;
        }
        if (bytes && peek() >= 0x80)
            return fail("bytes can only contain ASCII literal characters");
        if (!body.push(src[i]))
            return fail("out of memory");
        bump();
    }

    if (flags & TOK_STR_F) {
        // The body is kept as written; phase 4 parses what is inside it.
        usize end = i - (triple ? 3 : 1);
        return emit_text(Tok::FStr, at_line, at_col, src.substr(body_at, end - body_at), flags);
    }
    return emit_text(bytes ? Tok::Bytes : Tok::Str, at_line, at_col, body.str(), flags);
}

bool Scanner::one_token()
{
    u32 at_line = line, at_col = col;
    u8 c = peek();

    if (is_digit(c) || (c == '.' && is_digit(peek(1))))
        return scan_number();
    if (c == '"' || c == '\'')
        return scan_string(0, at_line, at_col);
    if (is_name_start(c))
        return scan_name();

    for (const Spelling &o : OPERATORS)
        if (src.size() - i >= o.word.size() && src.substr(i, o.word.size()) == o.word) {
            for (usize k = 0; k < o.word.size(); k++)
                bump();
            if (o.kind == Tok::LPar || o.kind == Tok::LSqb || o.kind == Tok::LBrace)
                depth++;
            else if (o.kind == Tok::RPar || o.kind == Tok::RSqb || o.kind == Tok::RBrace) {
                if (depth == 0)
                    return fail("unmatched bracket");
                depth--;
            }
            return emit(o.kind, at_line, at_col);
        }

    Buf<48> m;
    m.put("invalid character ");
    if (c < 0x80 && c >= 0x20)
        m.put('\'').put(char(c)).put('\'');
    else
        m.put("in source");
    return fail(m.str());
}

bool Scanner::run()
{
    // A UTF-8 BOM, which an editor may have left at the front.
    if (src.size() >= 3 && u8(src[0]) == 0xef && u8(src[1]) == 0xbb && u8(src[2]) == 0xbf)
        i = 3;

    while (!at_end()) {
        if (fresh && depth == 0) {
            if (!line_start())
                return false;
            if (at_end())
                break;
        }

        // Spaces between tokens, comments, and the two kinds of line join.
        if (peek() == ' ' || peek() == '\t' || peek() == '\f' || peek() == '\r') {
            bump();
            continue;
        }
        if (peek() == '#') {
            while (!at_end() && peek() != '\n')
                bump();
            continue;
        }
        if (peek() == '\\' && (peek(1) == '\n' || (peek(1) == '\r' && peek(2) == '\n'))) {
            bump();
            if (peek() == '\r')
                bump();
            bump();
            continue;
        }
        if (peek() == '\n') {
            u32 at_line = line, at_col = col;
            bump();
            if (depth > 0)
                continue; // implicit joining inside brackets
            usize n = out->tokens.size();
            if (n && out->tokens[n - 1].kind != Tok::Newline &&
                out->tokens[n - 1].kind != Tok::Indent && out->tokens[n - 1].kind != Tok::Dedent) {
                if (!emit(Tok::Newline, at_line, at_col))
                    return false;
            }
            fresh = true;
            continue;
        }

        if (!one_token())
            return false;
    }

    if (fragment)
        return true;
    if (depth > 0)
        return fail("unexpected EOF while parsing");

    usize n = out->tokens.size();
    if (n && out->tokens[n - 1].kind != Tok::Newline && out->tokens[n - 1].kind != Tok::Indent &&
        out->tokens[n - 1].kind != Tok::Dedent) {
        if (!emit(Tok::Newline, line, col))
            return false;
        // The file did not end in one, so what follows is on the next line --
        // which is where CPython puts the dedents and the endmarker.
        line++;
        col = 1;
    }
    while (levels > 1) {
        levels--;
        if (!emit(Tok::Dedent, line, col))
            return false;
    }
    return emit(Tok::End, line, col);
}

} // namespace

Str tok_text(Tok t)
{
    for (const Spelling &k : KEYWORDS)
        if (k.kind == t)
            return k.word;
    for (const Spelling &o : OPERATORS)
        if (o.kind == t)
            return o.word;
    return Str();
}

bool tok_is_keyword(Tok t)
{
    return t >= Tok::KwFalse && t <= Tok::KwYield;
}

Str tok_label(Tok t)
{
    switch (t) {
    case Tok::End:
        return "endmarker";
    case Tok::Newline:
        return "newline";
    case Tok::Indent:
        return "indent";
    case Tok::Dedent:
        return "dedent";
    case Tok::Name:
        return "name";
    case Tok::Int:
        return "int";
    case Tok::Float:
        return "float";
    case Tok::Str:
        return "str";
    case Tok::Bytes:
        return "bytes";
    case Tok::FStr:
        return "fstring";
    default:
        break;
    }
    return tok_is_keyword(t) ? Str("kw") : Str("op");
}

bool Lexer::run(Str source)
{
    Scanner s{ source, this };
    return s.run();
}

bool lex_unescape(Str raw, u32 line, u32 col, String &out)
{
    Lexer sink;
    Scanner s{ raw, &sink };
    s.line     = line;
    s.col      = col;
    s.fragment = true;
    while (!s.at_end()) {
        if (s.peek() == '\\') {
            s.bump();
            if (!s.string_escape(out, false, line, col))
                return false;
            continue;
        }
        if (!out.push(char(s.peek())))
            return err_set("MemoryError", "out of memory"), false;
        s.bump();
    }
    return true;
}

usize Lexer::sublex(Str fragment, u32 at_line, u32 at_col)
{
    usize first = tokens.size();
    Scanner s{ fragment, this };
    // As if inside brackets: no indentation, and a newline joins rather than
    // ending a statement. An expression is balanced, so the count stays up.
    s.depth    = 1;
    s.fresh    = false;
    s.fragment = true;
    if (!s.run())
        return 0;
    for (usize k = first; k < tokens.size(); k++) {
        tokens[k].line = at_line;
        tokens[k].col  = at_col;
    }
    Token end;
    end.kind = Tok::End;
    end.line = at_line;
    end.col  = at_col;
    if (!tokens.push(end))
        return err_set("MemoryError", "out of memory"), 0;
    return first;
}

// One token per line: `line:col label value`, the value written as repr.
bool lex_dump(Str source, String &out)
{
    Lexer lx;
    bool ok = lx.run(source);

    for (usize k = 0; k < lx.tokens.size(); k++) {
        const Token &t = lx.tokens[k];
        Buf<64> head;
        head.put(u64(t.line)).put(':').put(u64(t.col)).put(' ').put(tok_label(t.kind));
        if (!out.append(head.str()))
            return false;

        Str word = tok_text(t.kind);
        if (!word.empty()) {
            if (!out.push(' ') || !out.append(word))
                return false;
        } else if (t.kind == Tok::Name) {
            if (!out.push(' ') || !out.append(lx.text_of(t)))
                return false;
        } else if (t.kind == Tok::Int) {
            Buf<24> n;
            i64 v = t.ival;
            if (v < 0) {
                n.put('-');
                v = -v;
            }
            n.put(u64(v));
            if (!out.push(' ') || !out.append(n.str()))
                return false;
        } else if (t.kind == Tok::Float) {
            char tmp[48];
            if (!out.push(' ') || !out.append(float_text(tmp, sizeof tmp, t.fval)))
                return false;
        } else if (t.kind == Tok::Str || t.kind == Tok::FStr || t.kind == Tok::Bytes) {
            Root v{ t.kind == Tok::Bytes ? bytes_new(lx.text_of(t))
                                         : obj_value(str_raw(lx.text_of(t))) };
            if (v.v.is_nil() || !out.push(' ') || py_repr(v.v, out) != R::Ok)
                return false;
        }
        if (!out.push('\n'))
            return false;
    }
    return ok;
}
