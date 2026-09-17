// The tokenizer: source text to a stream of tokens, with the indentation
// turned into Indent and Dedent.
#pragma once

#include "kernel/str.h"
#include "kernel/string.h"
#include "kernel/vec.h"
#include "value.h"

enum class Tok : u8 {
    End,
    Newline,
    Indent,
    Dedent,

    Name,
    Int,
    Float,
    Str,
    Bytes,
    FStr,
    Imag,

    KwFalse,
    KwNone,
    KwTrue,
    KwAnd,
    KwAs,
    KwAssert,
    KwAsync,
    KwAwait,
    KwBreak,
    KwClass,
    KwContinue,
    KwDef,
    KwDel,
    KwElif,
    KwElse,
    KwExcept,
    KwFinally,
    KwFor,
    KwFrom,
    KwGlobal,
    KwIf,
    KwImport,
    KwIn,
    KwIs,
    KwLambda,
    KwNonlocal,
    KwNot,
    KwOr,
    KwPass,
    KwRaise,
    KwReturn,
    KwTry,
    KwWhile,
    KwWith,
    KwYield,

    LPar,
    RPar,
    LSqb,
    RSqb,
    LBrace,
    RBrace,
    Comma,
    Colon,
    Dot,
    Semi,
    At,
    Equal,
    Arrow,
    PlusEq,
    MinusEq,
    StarEq,
    SlashEq,
    DSlashEq,
    PercentEq,
    AtEq,
    AmpEq,
    VbarEq,
    CaretEq,
    RShiftEq,
    LShiftEq,
    DStarEq,
    Plus,
    Minus,
    Star,
    DStar,
    Slash,
    DSlash,
    Percent,
    LShift,
    RShift,
    Amp,
    Vbar,
    Caret,
    Tilde,
    Walrus,
    Less,
    Greater,
    LessEq,
    GreaterEq,
    EqEq,
    NotEq,
    Ellipsis,
};

// The keyword or operator spelling, empty for a token that carries text.
Str tok_text(Tok t);

// The name used by --dump-tokens and by error messages.
Str tok_label(Tok t);

bool tok_is_keyword(Tok t);

// An Int token past i64 keeps its digits in `text` instead of `ival`, and
// says so here; the base travels with them, the 0x/0o/0b having been eaten.
enum : u8 {
    TOK_INT_WIDE = 1 << 3,
    TOK_INT_HEX  = 1 << 4,
    TOK_INT_OCT  = 1 << 5,
    TOK_INT_BIN  = 1 << 6,
};

// The base an Int token's digits are written in.
inline u32 tok_int_base(u8 flags)
{
    return (flags & TOK_INT_HEX) ? 16 : (flags & TOK_INT_OCT) ? 8 : (flags & TOK_INT_BIN) ? 2 : 10;
}

// A string literal's prefix, as its token carries it in `flags`.
enum : u8 {
    TOK_STR_RAW   = 1 << 0,
    TOK_STR_BYTES = 1 << 1,
    TOK_STR_F     = 1 << 2,
    TOK_STR_T     = 1 << 3, // 3.14's t-string, which is an FStr too
};

// `line`/`col` are what a SyntaxError points at. `bcol`, `eline` and `ecol`
// are the span CPython's ast reports instead: a byte offset into the line,
// counted from zero, and the position one past the token's last byte.
struct Token {
    Tok kind  = Tok::End;
    u8 flags  = 0; // string literals: raw, and which quote was used
    u32 line  = 0; // 1-based
    u32 col   = 0; // 1-based, counted in codepoints
    u32 at    = 0; // offset into Lexer::text
    u32 len   = 0;
    u32 bcol  = 0;
    u32 eline = 0;
    u32 ecol  = 0;
    i64 ival  = 0;
    f64 fval  = 0;
};

// The whole source in one pass. False leaves a SyntaxError pending, with the
// line and column in the error channel.
struct Lexer {
    Vec<Token> tokens;
    String text;  // the decoded bytes of every Name, Str, Bytes and FStr
    String own;   // the source as UTF-8, where it was declared otherwise
    String codec; // a declared encoding only the codec registry can decode

    // `decoded` is a str's text, whose coding cookie no longer means
    // anything. Otherwise the source is bytes, and PEP 263 decides how they
    // are read: a BOM, a cookie in the first two lines, and UTF-8 by default.
    // Where the cookie names a codec written in Python, `codec` says which,
    // and the SyntaxError pending says it is unknown.
    bool run(Str source, bool decoded = false);

    // One expression's tokens appended after everything already scanned, for
    // what is inside an f-string's braces: the body arrives as written and is
    // taken apart at parse time, so its expressions are lexed then. Every
    // token is stamped with the f-string's own line and column, which is where
    // an error in one should point. Returns the index of the first, or 0 with
    // the error pending; the run ends with an End the sub-parse stops at.
    usize sublex(Str fragment, u32 line, u32 col);

    Str text_of(const Token &t) const { return Str(text.data() + t.at, t.len); }
};

// PEP 263: the encoding a source's first two lines declare, normalized as
// CPython's tokenizer does it -- "utf-8", "iso-8859-1", or the name as
// written -- appended to `out`. False when there is none.
bool lex_cookie(Str source, String &out);

// A source that cannot be decoded at all, pending: CPython's SyntaxError at
// line 0, offset -1. Always false.
bool lex_undecodable(Str message);

// source_decode(bytes, encoding): the source as a str, through the codec
// registry, or the SyntaxError CPython raises where that fails. A ContObj;
// compile() and import call it where a cookie names a codec written in
// Python.
struct CallArgs;
enum class R : u8;
R lex_source_decode(const CallArgs &a, Value &out);

// One literal run's escapes decoded, appended to `out`. An f-string's body is
// kept as written, so its literal halves are decoded at parse time -- and by
// this, so that there is one escape table and not two. False leaves a
// SyntaxError pending, pointing at the literal.
bool lex_unescape(Str raw, u32 line, u32 col, String &out);

// PEP 701: an f-string's fields may hold any string, the same quote
// included. Offsets into `s`, npos where the text never closes.
//
// lex_fstr_end: the closing quote of an f-string whose text starts at `at`.
// lex_field_end: the `}` closing a field whose text starts at `at`.
// lex_skip_string: just past the literal whose quote is at `at`, its prefix
// read from the letters before it.
usize lex_fstr_end(Str s, usize at, char quote, bool triple, bool raw);
usize lex_field_end(Str s, usize at);
usize lex_skip_string(Str s, usize at);

// One token per line, for --dump-tokens. False leaves the error pending, with
// what was tokenized so far already in `out`.
bool lex_dump(Str source, String &out);
