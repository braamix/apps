//
// Decoding of C backslash escapes and string-literal tokens.
//
// The scanner keeps a character/string literal as its raw source lexeme (quotes and
// backslash escapes intact); these routines turn that lexeme into the bytes it denotes.
// One implementation serves the character-constant path (parser) and the string path
// (semantic analysis, TAC lowering) so the two cannot drift apart.
//
#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//
// Decode a single backslash escape. On entry *ps points at the '\'; on return it has
// been advanced past the whole escape. Returns the escape's value (a hex or octal
// escape may exceed a byte; the caller keeps only the low 8 bits).
//
int c_escape_value(const char **ps);

//
// Decode a string-literal lexeme (leading and trailing quotes, raw escapes) into the
// bytes it denotes. The result is a heap-allocated buffer of *out_len bytes, with one
// extra NUL appended so it is still safe to print; *out_len is the true byte count,
// which is what callers must use, since a decoded string may hold embedded NULs
// ("a\0c" is three bytes, not one). Caller frees with xfree().
//
char *c_decode_string_literal(const char *raw, size_t *out_len);

#ifdef __cplusplus
}
#endif
