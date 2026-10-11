#include "lexer.h"

namespace as {

namespace {

bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

bool is_hex(char c)
{
    char l = char(c | 0x20);
    return is_digit(c) || (l >= 'a' && l <= 'f');
}

bool is_alpha(char c)
{
    char l = char(c | 0x20);
    return l >= 'a' && l <= 'z';
}

bool ident_start(char c)
{
    return is_alpha(c) || c == '_' || c == '$' || c == '.' || c == '?';
}

bool ident_char(char c)
{
    return ident_start(c) || is_digit(c);
}

u32 hex_value(char c)
{
    return is_digit(c) ? u32(c - '0') : u32((c | 0x20) - 'a' + 10);
}

} // namespace

Token Lexer::error(u32 start, Str msg)
{
    return Token{ Tok::Error, start, pos - start, 0, msg };
}

Token Lexer::peek()
{
    if (!has_peeked_) {
        peeked_     = lex();
        has_peeked_ = true;
    }
    return peeked_;
}

Token Lexer::next()
{
    if (has_peeked_) {
        has_peeked_ = false;
        return peeked_;
    }
    return lex();
}

// A number from its first digit: an integer in any of four bases, or a
// float, decimal or hex.
Token Lexer::number(u32 start)
{
    auto at   = [&](u32 k) { return k < src.size() ? src[k] : '\0'; };
    auto real = [&]() { return Token{ Tok::Real, start, pos - start, 0, {} }; };
    // The fraction and exponent of a decimal float, from the dot or the e.
    auto decimal_float = [&]() {
        if (at(pos) == '.') {
            pos++;
            while (is_digit(at(pos)))
                pos++;
        }
        if (at(pos) == '-' || at(pos) == '+') {
            pos++;
            return error(start, "invalid sign in float literal");
        }
        if (at(pos) == 'e' || at(pos) == 'E') {
            pos++;
            if (at(pos) == '-' || at(pos) == '+')
                pos++;
            while (is_digit(at(pos)))
                pos++;
        }
        return real();
    };

    u64 v       = 0;
    bool over   = false;
    auto accrue = [&](u32 base, u32 d) {
        if (v > (~u64(0) - d) / base)
            over = true;
        v = v * base + d;
    };

    if (src[start] == '0' && (at(start + 1) == 'x' || at(start + 1) == 'X')) {
        pos         = start + 2;
        u32 digits0 = pos;
        while (is_hex(at(pos)))
            accrue(16, hex_value(src[pos++]));
        bool no_int = pos == digits0;
        if (at(pos) == '.' || at(pos) == 'p' || at(pos) == 'P') {
            bool no_frac = true;
            if (at(pos) == '.') {
                pos++;
                u32 f = pos;
                while (is_hex(at(pos)))
                    pos++;
                no_frac = pos == f;
            }
            if (no_int && no_frac)
                return error(start,
                             "invalid hexadecimal floating-point constant: expected at "
                             "least one significand digit");
            if (at(pos) != 'p' && at(pos) != 'P')
                return error(start,
                             "invalid hexadecimal floating-point constant: expected "
                             "exponent part 'p'");
            pos++;
            if (at(pos) == '+' || at(pos) == '-')
                pos++;
            u32 e = pos;
            while (is_digit(at(pos)))
                pos++;
            if (pos == e)
                return error(start,
                             "invalid hexadecimal floating-point constant: expected at "
                             "least one exponent digit");
            return real();
        }
        if (no_int)
            return error(start, "invalid hexadecimal number");
    } else if (src[start] == '0' && (at(start + 1) == 'b' || at(start + 1) == 'B') &&
               (at(start + 2) == '0' || at(start + 2) == '1')) {
        pos = start + 2;
        while (at(pos) == '0' || at(pos) == '1')
            accrue(2, u32(src[pos++] - '0'));
        if (is_digit(at(pos)))
            return error(start, "invalid binary number");
    } else {
        pos = start;
        while (is_digit(at(pos)))
            pos++;
        if (at(pos) == '.' || at(pos) == 'e' || at(pos) == 'E')
            return decimal_float();
        bool octal = src[start] == '0' && pos - start > 1;
        for (u32 k = start; k < pos; k++) {
            u32 d = u32(src[k] - '0');
            if (octal && d > 7)
                return error(start, "invalid octal number");
            accrue(octal ? 8 : 10, d);
        }
    }
    if (over)
        return error(start, "integer literal is too large to be represented in any integer type");
    // AsmLexer skips the suffixes a C integer may carry.
    while (at(pos) == 'u' || at(pos) == 'U' || at(pos) == 'l' || at(pos) == 'L')
        pos++;
    return Token{ Tok::Integer, start, pos - start, v, {} };
}

Token Lexer::lex()
{
    for (;;) {
        while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\r'))
            pos++;
        if (pos >= src.size())
            return Token{ Tok::Eof, pos, 0, 0, {} };
        char c = src[pos];
        if (c == '#' || (c == '/' && pos + 1 < src.size() && src[pos + 1] == '/')) {
            while (pos < src.size() && src[pos] != '\n')
                pos++;
            continue;
        }
        if (c == '/' && pos + 1 < src.size() && src[pos + 1] == '*') {
            u32 start = pos;
            pos += 2;
            while (pos + 1 < src.size() && !(src[pos] == '*' && src[pos + 1] == '/'))
                pos++;
            if (pos + 1 >= src.size()) {
                pos = src.size();
                return error(start, "unterminated comment");
            }
            pos += 2;
            continue;
        }
        break;
    }

    u32 start = pos;
    char c    = src[pos++];
    auto one  = [&](Tok k) { return Token{ k, start, 1, 0, {} }; };
    auto at   = [&](u32 k) { return k < src.size() ? src[k] : '\0'; };

    if (c == '\n' || c == ';')
        return one(Tok::Eol);
    if (is_digit(c))
        return number(start);
    if (ident_start(c)) {
        // .5 and the like are floats, .5foo an identifier.
        if (c == '.' && is_digit(at(pos))) {
            while (is_digit(at(pos)))
                pos++;
            if (!ident_char(at(pos)) || at(pos) == 'e' || at(pos) == 'E') {
                u32 k = pos;
                if (at(k) == 'e' || at(k) == 'E') {
                    k++;
                    if (at(k) == '-' || at(k) == '+')
                        k++;
                    while (is_digit(at(k)))
                        k++;
                    pos = k;
                }
                return Token{ Tok::Real, start, pos - start, 0, {} };
            }
        }
        while (ident_char(at(pos)))
            pos++;
        return Token{ Tok::Ident, start, pos - start, 0, {} };
    }
    switch (c) {
    case '"':
        // As AsmLexer, to the closing quote or the end of the file.
        while (pos < src.size() && src[pos] != '"') {
            if (src[pos] == '\\' && pos + 1 < src.size())
                pos++;
            pos++;
        }
        if (pos >= src.size() || src[pos] != '"')
            return error(start, "unterminated string constant");
        pos++;
        return Token{ Tok::String, start, pos - start, 0, {} };
    case ',':
        return one(Tok::Comma);
    case ':':
        return one(Tok::Colon);
    case '=':
        return one(Tok::Equal);
    case '(':
        return one(Tok::LParen);
    case ')':
        return one(Tok::RParen);
    case '{':
        return one(Tok::LCurly);
    case '}':
        return one(Tok::RCurly);
    case '+':
        return one(Tok::Plus);
    case '-':
        if (at(pos) == '>') {
            pos++;
            return Token{ Tok::Arrow, start, 2, 0, {} };
        }
        return one(Tok::Minus);
    case '@':
        return one(Tok::At);
    case '/':
        return one(Tok::Slash);
    default:
        return one(Tok::Other);
    }
}

} // namespace as
