// The text cut into tokens, §3 of the language. Plain C++ over a source
// already in memory: a token is a span of it, and a string's bytes are
// decoded only when asked for. Messages are the reference interpreter's.
#pragma once

#include "kernel/str.h"
#include "kernel/types.h"
#include "kernel/vec.h"

enum class Tok : u8 {
    Eof,
    LParen,
    RParen,
    Keyword, // a…z idchar*: module, i32.add, offset=16
    Nat,     // uN: 7, 0x1_f
    Int,     // sN with its sign: -7, +0x1f
    Float,   // any other fN: 1.5, 1e3, -inf, nan:0x1
    String,
    Id,    // $x or $"x"
    Annot, // (@id …), the whole of it
    Error,
};

struct Token {
    Tok kind = Tok::Eof;
    u32 at   = 0; // the span in the source
    u32 len  = 0;
    u32 line = 0; // from 1
    u32 col  = 0; // from 1, in bytes
};

struct Lexer {
    Str src;
    u32 pos  = 0;
    u32 line = 1;
    u32 bol  = 0; // where the line begins

    // The first error: after it every token is Error.
    Str msg;
    Str what; // the text it names, if any
    u32 err_line = 0;
    u32 err_col  = 0;

    explicit Lexer(Str s) : src(s) {}

    Token next();
    bool failed() const { return !msg.empty(); }
    Str text(const Token &t) const { return src.substr(t.at, t.len); }
};

// A String token's bytes, an Id's name ($x is x, $"x" is x too), or an
// Annot's id, appended to `out`. False out of memory.
bool token_bytes(Str src, const Token &t, Vec<u8> &out);

// The length of the valid UTF-8 character at s[i], or 0.
u32 utf8_len(Str s, usize i);
