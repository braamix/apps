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

// A string literal's prefix, as its token carries it in `flags`.
enum : u8 {
    TOK_STR_RAW   = 1 << 0,
    TOK_STR_BYTES = 1 << 1,
    TOK_STR_F     = 1 << 2,
};

struct Token {
    Tok kind = Tok::End;
    u8 flags = 0; // string literals: raw, and which quote was used
    u32 line = 0; // 1-based
    u32 col  = 0; // 1-based, counted in codepoints
    u32 at   = 0; // offset into Lexer::text
    u32 len  = 0;
    i64 ival = 0;
    f64 fval = 0;
};

// The whole source in one pass. False leaves a SyntaxError pending, with the
// line and column in the error channel.
struct Lexer {
    Vec<Token> tokens;
    String text; // the decoded bytes of every Name, Str, Bytes and FStr

    bool run(Str source);

    // One expression's tokens appended after everything already scanned, for
    // what is inside an f-string's braces: the body arrives as written and is
    // taken apart at parse time, so its expressions are lexed then. Every
    // token is stamped with the f-string's own line and column, which is where
    // an error in one should point. Returns the index of the first, or 0 with
    // the error pending; the run ends with an End the sub-parse stops at.
    usize sublex(Str fragment, u32 line, u32 col);

    Str text_of(const Token &t) const { return Str(text.data() + t.at, t.len); }
};

// One literal run's escapes decoded, appended to `out`. An f-string's body is
// kept as written, so its literal halves are decoded at parse time -- and by
// this, so that there is one escape table and not two. False leaves a
// SyntaxError pending, pointing at the literal.
bool lex_unescape(Str raw, u32 line, u32 col, String &out);

// One token per line, for --dump-tokens. False leaves the error pending, with
// what was tokenized so far already in `out`.
bool lex_dump(Str source, String &out);
