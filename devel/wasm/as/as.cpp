#include "as.h"

#include "emit.h"
#include "lexer.h"
#include "number.h"

namespace {

// name:line:col: error: msg
void lex_error(Diag &diag, Str name, const Lexer &x)
{
    Out where, msg;
    where.put(name).put(':').num(x.err_line).put(':').num(x.err_col);
    msg.put(x.msg);
    if (!x.what.empty())
        msg.put(' ').put(x.what);
    diag.error_at(where.str(), msg.str());
}

// Bytes as a string literal would spell them, one way only.
void quoted(Out &out, const Vec<u8> &b)
{
    out.put('"');
    for (u8 c : b)
        if (c >= 0x20 && c < 0x7f && c != '"' && c != '\\')
            out.put(char(c));
        else
            out.put('\\').hex(c, 2);
    out.put('"');
}

constexpr Str KIND[] = { "eof",   "(",      ")",  "keyword", "nat",  "int",
                         "float", "string", "id", "annot",   "error" };

} // namespace

bool dump_tokens(Str name, Str source, Out &out, Diag &diag)
{
    Lexer x(source);
    for (;;) {
        Token t = x.next();
        if (t.kind == Tok::Error) {
            lex_error(diag, name, x);
            return false;
        }
        out.num(t.line).put(':').num(t.col).put(' ').put(KIND[u32(t.kind)]);
        Vec<u8> b;
        switch (t.kind) {
        case Tok::Keyword:
        case Tok::Nat:
        case Tok::Int:
        case Tok::Float:
            out.put(' ').put(x.text(t));
            break;
        case Tok::String:
        case Tok::Id:
        case Tok::Annot:
            if (!token_bytes(source, t, b)) {
                diag.error("out of memory");
                return false;
            }
            out.put(' ');
            if (t.kind != Tok::String)
                out.put(t.kind == Tok::Id ? '$' : '@');
            quoted(out, b);
            if (t.kind == Tok::Annot) {
                Str s = x.text(t);
                b.clear();
                for (char c : s)
                    if (!b.push(u8(c))) {
                        diag.error("out of memory");
                        return false;
                    }
                out.put(' ');
                quoted(out, b);
            }
            break;
        default:
            break;
        }
        out.put('\n');
        if (t.kind == Tok::Eof)
            return true;
    }
}

bool dump_numbers(Str name, Str source, Out &out, Diag &diag)
{
    u32 line = 0;
    while (!source.empty()) {
        Str text = source.split('\n', source);
        line++;
        Lexer x(text);
        Token t = x.next();
        if (t.kind == Tok::Eof)
            continue;
        Str type    = x.text(t);
        u32 width   = 0;
        bool is_int = false, is_uint = false;
        if (t.kind == Tok::Keyword && type.size() > 1 &&
            (type[0] == 'i' || type[0] == 'u' || type[0] == 'f')) {
            Str w   = type.substr(1);
            width   = w == "8" ? 8 : w == "16" ? 16 : w == "32" ? 32 : w == "64" ? 64 : 0;
            is_int  = type[0] == 'i';
            is_uint = type[0] == 'u';
            if (type[0] == 'f' && width < 32)
                width = 0;
        }
        Token n  = x.next();
        Token n2 = n.kind == Tok::Error ? n : x.next();
        if (!width || n.kind == Tok::Eof || (n.kind != Tok::Error && n2.kind != Tok::Eof)) {
            Out m;
            m.put(name).put(':').num(line).put(':').num(t.col);
            diag.error_at(m.str(),
                          "expected a type, i8 to i64, u8 to u64, f32 or f64, and a "
                          "literal");
            return false;
        }
        Str lit = text.substr(t.at + t.len);
        while (!lit.empty() && (lit[0] == ' ' || lit[0] == '\t'))
            lit = lit.substr(1);
        while (!lit.empty() && (lit[lit.size() - 1] == ' ' || lit[lit.size() - 1] == '\r'))
            lit = lit.substr(0, lit.size() - 1);
        out.put(type).put(' ').put(lit).put(' ');
        bool number = n.kind == Tok::Nat || n.kind == Tok::Int || n.kind == Tok::Float;
        u64 v       = 0;
        u32 v32     = 0;
        bool ok     = false;
        if (!number)
            ;
        else if (is_int)
            ok = parse_int(lit, width, v);
        else if (is_uint)
            ok = parse_uint(lit, width, v);
        else if (width == 32 && (ok = parse_f32(lit, v32)))
            v = v32;
        else if (width == 64)
            ok = parse_f64(lit, v);
        if (n.kind == Tok::Error) {
            out.put(x.msg);
            if (!x.what.empty())
                out.put(' ').put(x.what);
        } else if (!number)
            out.put("not a number");
        else if (!ok)
            out.put("out of range");
        else if (width == 64)
            out.put("0x").hex(u32(v >> 32), 8).hex(u32(v), 8);
        else
            out.put("0x").hex(u32(v), width / 4);
        out.put('\n');
    }
    return true;
}

bool assemble(Str name, Str source, const AsConfig &c, Vec<u8> &out, Diag &diag)
{
    (void)c;
    // Nothing is parsed yet: a source that lexes is the empty module.
    Lexer x(source);
    for (;;) {
        Token t = x.next();
        if (t.kind == Tok::Error) {
            lex_error(diag, name, x);
            return false;
        }
        if (t.kind == Tok::Eof)
            break;
    }
    Emit e{ out };
    e.u32le(0x6d736100); // \0asm
    e.u32le(1);          // version
    if (e.oom) {
        diag.error("out of memory");
        return false;
    }
    return true;
}
