// Tokens, as llvm's AsmLexer makes them for WebAssembly: '#' starts a
// comment, ';' or a newline ends a statement, and an identifier may hold
// dots and dollars.
#pragma once

#include "kernel/str.h"
#include "kernel/types.h"

namespace as {

enum class Tok : u8 {
    Eof,
    Eol,
    Ident,
    Integer,
    Real,
    String,
    Comma,
    Colon,
    Equal,
    LParen,
    RParen,
    LCurly,
    RCurly,
    Arrow, // ->
    Plus,
    Minus,
    At,
    Slash,
    Other, // any other character
    Error,
};

struct Token {
    Tok kind  = Tok::Eof;
    u32 at    = 0; // byte offset in the source
    u32 len   = 0;
    u64 value = 0; // an integer's
    Str why;       // an error's message
};

struct Lexer {
    Str src;
    u32 pos = 0;

    explicit Lexer(Str s) : src(s) {}

    Token next();
    Token peek();
    Str text(const Token &t) const { return src.substr(t.at, t.len); }

private:
    bool has_peeked_ = false;
    Token peeked_;
    Token lex();
    Token number(u32 start);
    Token error(u32 start, Str msg);
};

} // namespace as
