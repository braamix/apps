#include "as.h"

#include "emit.h"
#include "lexer.h"

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
