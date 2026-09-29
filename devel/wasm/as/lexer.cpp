#include "lexer.h"

namespace {

bool is_digit(u8 c)
{
    return c >= '0' && c <= '9';
}

bool is_hex(u8 c)
{
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

u32 hex_value(u8 c)
{
    if (is_digit(c))
        return c - '0';
    return (c | 0x20) - 'a' + 10;
}

bool is_idchar(u8 c)
{
    if (is_digit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
        return true;
    for (const char *p = "!#$%&'*+-./:<=>?@\\^_`|~"; *p; p++)
        if (c == u8(*p))
            return true;
    return false;
}

bool is_lone(u8 c) // reserved on its own
{
    return c == ',' || c == ';' || c == '[' || c == ']' || c == '{' || c == '}';
}

// digit ('_'? digit)*, or the same of hex digits, from s[i]; false for none.
bool digits(Str s, usize &i, bool hex)
{
    auto ok = [&](usize k) { return k < s.size() && (hex ? is_hex(s[k]) : is_digit(s[k])); };
    if (!ok(i))
        return false;
    for (i++;; i++) {
        if (ok(i))
            continue;
        if (i < s.size() && s[i] == '_' && ok(i + 1))
            continue;
        return true;
    }
}

bool is_nat(Str s)
{
    bool hex = s.starts_with("0x");
    usize i  = hex ? 2 : 0;
    return digits(s, i, hex) && i == s.size();
}

bool is_int(Str s)
{
    return s.size() > 1 && (s[0] == '+' || s[0] == '-') && is_nat(s.substr(1));
}

bool is_float(Str s)
{
    usize i = 0;
    if (i < s.size() && (s[i] == '+' || s[i] == '-'))
        i++;
    Str m = s.substr(i);
    if (m == "inf" || m == "nan")
        return true;
    if (m.starts_with("nan:0x")) {
        i += 6;
        return digits(s, i, true) && i == s.size();
    }
    bool hex = m.starts_with("0x");
    if (hex)
        i += 2;
    if (!digits(s, i, hex))
        return false;
    if (i < s.size() && s[i] == '.') {
        i++;
        usize k = i;
        if (!digits(s, i, hex))
            i = k;
    }
    if (i < s.size() && (s[i] | 0x20) == (hex ? 'p' : 'e')) {
        i++;
        if (i < s.size() && (s[i] == '+' || s[i] == '-'))
            i++;
        if (!digits(s, i, false))
            return false;
    }
    return i == s.size();
}

} // namespace

u32 utf8_len(Str s, usize i)
{
    auto at   = [&](usize k) -> u8 { return k < s.size() ? u8(s[k]) : 0; };
    auto cont = [&](usize k, u8 lo = 0x80, u8 hi = 0xbf) { return at(k) >= lo && at(k) <= hi; };
    u8 c      = at(i);
    if (c < 0x80)
        return i < s.size() ? 1 : 0;
    if (c >= 0xc2 && c <= 0xdf)
        return cont(i + 1) ? 2 : 0;
    if (c == 0xe0)
        return cont(i + 1, 0xa0) && cont(i + 2) ? 3 : 0;
    if (c == 0xed)
        return cont(i + 1, 0x80, 0x9f) && cont(i + 2) ? 3 : 0;
    if (c >= 0xe1 && c <= 0xef)
        return cont(i + 1) && cont(i + 2) ? 3 : 0;
    if (c == 0xf0)
        return cont(i + 1, 0x90) && cont(i + 2) && cont(i + 3) ? 4 : 0;
    if (c == 0xf4)
        return cont(i + 1, 0x80, 0x8f) && cont(i + 2) && cont(i + 3) ? 4 : 0;
    if (c >= 0xf1 && c <= 0xf3)
        return cont(i + 1) && cont(i + 2) && cont(i + 3) ? 4 : 0;
    return 0;
}

namespace {

// What goes wrong in a string: the message and where.
struct Bad {
    Str msg;
    u32 at = 0;
};

// The string literal at s[i]: false with `bad` set, or true with `i` past
// its closing quote.
bool scan_string(Str s, usize &i, Bad &bad)
{
    usize open = i;
    for (i++;;) {
        if (i >= s.size() || s[i] == '\n' || s[i] == '\r') {
            bad = { "unclosed string literal", u32(open) };
            return false;
        }
        u8 c = u8(s[i]);
        if (c == '"') {
            i++;
            return true;
        }
        if (c == '\\') {
            usize k = i + 1;
            u8 e    = k < s.size() ? u8(s[k]) : 0;
            if (e == 'n' || e == 'r' || e == 't' || e == '\\' || e == '\'' || e == '"') {
                i += 2;
            } else if (is_hex(e) && k + 1 < s.size() && is_hex(u8(s[k + 1]))) {
                i += 3;
            } else if (e == 'u' && k + 1 < s.size() && s[k + 1] == '{') {
                usize j = k + 2;
                u32 v   = 0;
                bool ok = digits(s, j, true) && j < s.size() && s[j] == '}';
                for (usize d = k + 2; ok && d < j; d++)
                    if (s[d] != '_') {
                        ok = v < 0x110000;
                        v  = v * 16 + hex_value(u8(s[d]));
                    }
                if (!ok || v >= 0x110000 || (v >= 0xd800 && v < 0xe000)) {
                    bad = { "illegal escape", u32(i) };
                    return false;
                }
                i = j + 1;
            } else {
                bad = { "illegal escape", u32(i) };
                return false;
            }
        } else if (c < 0x20 || c == 0x7f) {
            bad = { "illegal control character in string literal", u32(i) };
            return false;
        } else if (c >= 0x80) {
            u32 n = utf8_len(s, i);
            if (!n) {
                bad = { "malformed UTF-8 encoding", u32(i) };
                return false;
            }
            i += n;
        } else {
            i++;
        }
    }
}

// A run of idchars and strings from s[i], to its end or to a string that
// does not close. The first string in it, and whether there are others.
struct Run {
    usize end     = 0;
    bool strings  = false; // it has one
    bool more     = false; // it has another
    usize str_at  = 0;     // where the first starts and ends
    usize str_end = 0;
    Bad bad; // the string that stopped it, if one did
};

Run scan_run(Str s, usize i)
{
    Run r;
    while (i < s.size()) {
        u8 c = u8(s[i]);
        if (is_idchar(c)) {
            i++;
            continue;
        }
        if (c != '"')
            break;
        usize k = i;
        if (!scan_string(s, k, r.bad))
            break;
        r.bad = Bad();
        if (r.strings)
            r.more = true;
        else
            r.strings = true, r.str_at = i, r.str_end = k;
        i = k;
    }
    r.end = i;
    return r;
}

// A name's bytes: valid UTF-8, or false.
bool is_utf8(Str s)
{
    for (usize i = 0; i < s.size();) {
        u32 n = utf8_len(s, i);
        if (!n)
            return false;
        i += n;
    }
    return true;
}

// A validated string literal's bytes.
bool decode(Str lit, Vec<u8> &out)
{
    for (usize i = 1; i + 1 < lit.size(); i++) {
        u8 c = u8(lit[i]);
        if (c != '\\') {
            if (!out.push(c))
                return false;
            continue;
        }
        u8 e = u8(lit[++i]);
        u32 v;
        if (e == 'n')
            v = '\n';
        else if (e == 'r')
            v = '\r';
        else if (e == 't')
            v = '\t';
        else if (e == 'u') {
            v = 0;
            for (i += 2; lit[i] != '}'; i++)
                if (lit[i] != '_')
                    v = v * 16 + hex_value(u8(lit[i]));
            u8 b[4];
            u32 n;
            if (v < 0x80) {
                b[0] = u8(v), n = 1;
            } else if (v < 0x800) {
                b[0] = u8(0xc0 | v >> 6), b[1] = u8(0x80 | (v & 0x3f)), n = 2;
            } else if (v < 0x10000) {
                b[0] = u8(0xe0 | v >> 12), b[1] = u8(0x80 | (v >> 6 & 0x3f));
                b[2] = u8(0x80 | (v & 0x3f)), n = 3;
            } else {
                b[0] = u8(0xf0 | v >> 18), b[1] = u8(0x80 | (v >> 12 & 0x3f));
                b[2] = u8(0x80 | (v >> 6 & 0x3f)), b[3] = u8(0x80 | (v & 0x3f)), n = 4;
            }
            for (u32 k = 0; k < n; k++)
                if (!out.push(b[k]))
                    return false;
            continue;
        } else if (is_hex(e)) {
            v = hex_value(e) * 16 + hex_value(u8(lit[++i]));
        } else {
            v = e; // \\ \' \"
        }
        if (!out.push(u8(v)))
            return false;
    }
    return true;
}

// Whether a quoted string decodes to something empty or not text.
Str check_name(Str lit, Str empty)
{
    Vec<u8> b;
    if (!decode(lit, b))
        return "out of memory";
    if (b.empty())
        return empty;
    if (!is_utf8(Str(reinterpret_cast<const char *>(b.data()), b.size())))
        return "malformed UTF-8 encoding";
    return Str();
}

} // namespace

bool token_bytes(Str src, const Token &t, Vec<u8> &out)
{
    Str s = src.substr(t.at, t.len);
    if (t.kind == Tok::Annot) {
        usize i = 2;
        if (s[i] == '"') {
            Bad bad;
            usize k = i;
            scan_string(s, k, bad);
            return decode(s.substr(i, k - i), out);
        }
        while (i < s.size() && is_idchar(u8(s[i])))
            i++;
        s = s.substr(2, i - 2);
    } else if (t.kind == Tok::Id) {
        s = s.substr(1);
    }
    if (!s.empty() && s[0] == '"')
        return decode(s, out);
    for (char c : s)
        if (!out.push(u8(c)))
            return false;
    return true;
}

namespace {

struct Scan {
    Lexer &x;

    Token fail(Str msg, u32 line, u32 col, Str what = Str())
    {
        x.msg      = msg;
        x.what     = what;
        x.err_line = line;
        x.err_col  = col;
        x.pos      = u32(x.src.size());
        Token t;
        t.kind = Tok::Error;
        t.line = line;
        t.col  = col;
        return t;
    }

    u32 col(usize at) const { return u32(at) - x.bol + 1; }
    Token fail_at(Str msg, usize at, Str what = Str()) { return fail(msg, x.line, col(at), what); }

    u8 at(usize k) const { return k < x.src.size() ? u8(x.src[k]) : 0; }
    bool end(usize k) const { return k >= x.src.size(); }

    // A newline at pos: LF, CR, or CR LF.
    bool newline()
    {
        u8 c = at(x.pos);
        if (end(x.pos) || (c != '\n' && c != '\r'))
            return false;
        x.pos += (c == '\r' && at(x.pos + 1) == '\n') ? 2 : 1;
        x.line++;
        x.bol = x.pos;
        return true;
    }

    // ;; to the end of the line. False on a malformed character.
    bool line_comment()
    {
        while (!end(x.pos) && at(x.pos) != '\n' && at(x.pos) != '\r') {
            u32 n = utf8_len(x.src, x.pos);
            if (!n) {
                fail_at("malformed UTF-8 encoding", x.pos);
                return false;
            }
            x.pos += n;
        }
        return true;
    }

    // (; … ;), nested. False when it does not close.
    bool block_comment()
    {
        u32 line = x.line, c = col(x.pos);
        u32 depth = 0;
        do {
            if (end(x.pos)) {
                fail("unclosed comment", line, c);
                return false;
            }
            if (at(x.pos) == '(' && at(x.pos + 1) == ';') {
                depth++;
                x.pos += 2;
            } else if (at(x.pos) == ';' && at(x.pos + 1) == ')') {
                depth--;
                x.pos += 2;
            } else if (!newline()) {
                u32 n = utf8_len(x.src, x.pos);
                if (!n) {
                    fail_at("malformed UTF-8 encoding", x.pos);
                    return false;
                }
                x.pos += n;
            }
        } while (depth);
        return true;
    }

    // The id after (@ at pos: idchars or a string. False when there is none,
    // or it is not a name.
    bool annot_id()
    {
        usize i = x.pos;
        if (is_idchar(at(i))) {
            while (is_idchar(at(i)))
                i++;
            x.pos = u32(i);
            return true;
        }
        Bad bad;
        if (at(i) != '"' || !scan_string(x.src, i, bad)) {
            fail_at("empty annotation id", x.pos - 2);
            return false;
        }
        Str why = check_name(x.src.substr(x.pos, i - x.pos), "empty annotation id");
        if (!why.empty()) {
            fail_at(why, x.pos - 2);
            return false;
        }
        x.pos = u32(i);
        return true;
    }

    // An id after a (@ inside an annotation. Without one, the ( is a
    // parenthesis and the @ an atom.
    bool nested_id()
    {
        usize i = x.pos + 1;
        Bad bad;
        if (is_idchar(at(i))) {
            while (is_idchar(at(i)))
                i++;
        } else if (at(i) == '"' && scan_string(x.src, i, bad)) {
            Str why = check_name(x.src.substr(x.pos + 1, i - x.pos - 1), "empty annotation id");
            if (!why.empty()) {
                fail_at(why, x.pos - 1);
                return false;
            }
        } else {
            return true;
        }
        x.pos = u32(i);
        return true;
    }

    // (@id …) from its (@, to the parenthesis that closes it.
    Token annotation()
    {
        Token t{ Tok::Annot, x.pos, 0, x.line, col(x.pos) };
        x.pos += 2;
        if (!annot_id())
            return Token{ Tok::Error };
        u32 depth = 1;
        while (depth) {
            u8 c = at(x.pos);
            if (end(x.pos))
                return fail("unclosed annotation", t.line, t.col);
            if (c == ' ' || c == '\t') {
                x.pos++;
            } else if (newline()) {
            } else if (c == '(' && at(x.pos + 1) == ';') {
                if (!block_comment())
                    return Token{ Tok::Error };
            } else if (c == ';' && at(x.pos + 1) == ';') {
                if (!line_comment())
                    return Token{ Tok::Error };
            } else if (c == '(') {
                depth++;
                x.pos++;
                if (at(x.pos) == '@' && !nested_id())
                    return Token{ Tok::Error };
            } else if (c == ')') {
                depth--;
                x.pos++;
            } else if (is_lone(c)) {
                x.pos++;
            } else if (is_idchar(c) || c == '"') {
                Run r = scan_run(x.src, x.pos);
                if (r.end == x.pos)
                    return fail_at(r.bad.msg, r.bad.at);
                Str s = x.src.substr(x.pos, r.end - x.pos);
                if (s == "$" ||
                    (s[0] == '$' && r.strings && r.str_at == x.pos + 1 && r.str_end == r.end &&
                     !check_name(x.src.substr(r.str_at, r.end - r.str_at), "empty identifier")
                          .empty()))
                    return fail_at("empty identifier", x.pos);
                x.pos = u32(r.end);
            } else if (c < 0x80 || utf8_len(x.src, x.pos)) {
                return fail_at("illegal character", x.pos);
            } else {
                return fail_at("malformed UTF-8 encoding", x.pos);
            }
        }
        t.len = x.pos - t.at;
        return t;
    }

    // A run of idchars and strings: a number, keyword, id or string, or
    // reserved.
    Token run()
    {
        usize start = x.pos;
        Token t{ Tok::Error, x.pos, 0, x.line, col(x.pos) };
        Run r = scan_run(x.src, start);
        if (r.end == start)
            return fail_at(r.bad.msg, r.bad.at);
        Str s = x.src.substr(start, r.end - start);
        t.len = u32(s.size());
        x.pos = u32(r.end);
        if (s == "$")
            return fail("empty identifier", t.line, t.col);
        if (!r.strings) {
            if (s[0] == '$')
                t.kind = Tok::Id;
            else if (is_nat(s))
                t.kind = Tok::Nat;
            else if (is_int(s))
                t.kind = Tok::Int;
            else if (is_float(s))
                t.kind = Tok::Float;
            else if (s[0] >= 'a' && s[0] <= 'z')
                t.kind = Tok::Keyword;
        } else if (!r.more && r.str_at == start && r.str_end == r.end) {
            t.kind = Tok::String;
        } else if (!r.more && s[0] == '$' && r.str_at == start + 1 && r.str_end == r.end) {
            Str why = check_name(s.substr(1), "empty identifier");
            if (!why.empty())
                return fail(why, t.line, t.col);
            t.kind = Tok::Id;
        }
        if (t.kind == Tok::Error)
            return fail("unknown operator", t.line, t.col, s);
        return t;
    }

    Token next()
    {
        for (;;) {
            if (x.failed())
                return Token{ Tok::Error, x.pos, 0, x.err_line, x.err_col };
            Token t{ Tok::Eof, x.pos, 0, x.line, col(x.pos) };
            if (end(x.pos))
                return t;
            u8 c = at(x.pos);
            if (c == ' ' || c == '\t') {
                x.pos++;
            } else if (newline()) {
            } else if (c == ';' && at(x.pos + 1) == ';') {
                line_comment();
            } else if (c == '(' && at(x.pos + 1) == ';') {
                block_comment();
            } else if (c == '(' && at(x.pos + 1) == '@') {
                return annotation();
            } else if (c == '(' || c == ')') {
                t.kind = c == '(' ? Tok::LParen : Tok::RParen;
                t.len  = 1;
                x.pos++;
                return t;
            } else if (is_lone(c)) {
                return fail("unknown operator", t.line, t.col, x.src.substr(x.pos, 1));
            } else if (is_idchar(c) || c == '"') {
                return run();
            } else if (c < 0x20 || c == 0x7f) {
                return fail("misplaced control character", t.line, t.col);
            } else if (utf8_len(x.src, x.pos)) {
                return fail("misplaced unicode character", t.line, t.col);
            } else {
                return fail("malformed UTF-8 encoding", t.line, t.col);
            }
        }
    }
};

} // namespace

Token Lexer::next()
{
    Scan s{ *this };
    return s.next();
}
