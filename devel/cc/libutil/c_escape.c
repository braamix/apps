//
// Decoding of C backslash escapes and string-literal tokens.
//
#include "c_escape.h"

#include <ctype.h>
#include <string.h>

#include "xalloc.h"

//
// Decode a single backslash escape. On entry *ps points at the '\'; on return
// it has been advanced past the whole escape. Returns the escape's value (a
// hex/octal escape may exceed a byte; the caller keeps only the low 8 bits).
//
int c_escape_value(const char **ps)
{
    const char *s = *ps + 1; // skip backslash
    int val;
    switch (*s) {
    case '\'':
        val = '\'';
        s++;
        break;
    case '"':
        val = '"';
        s++;
        break;
    case '?':
        val = '?';
        s++;
        break;
    case '\\':
        val = '\\';
        s++;
        break;
    case 'a':
        val = '\a';
        s++;
        break;
    case 'b':
        val = '\b';
        s++;
        break;
    case 'f':
        val = '\f';
        s++;
        break;
    case 'n':
        val = '\n';
        s++;
        break;
    case 'r':
        val = '\r';
        s++;
        break;
    case 't':
        val = '\t';
        s++;
        break;
    case 'v':
        val = '\v';
        s++;
        break;
    default:
        if (*s >= '0' && *s <= '7') { // octal, up to 3 digits
            val = *s++ - '0';
            if (*s >= '0' && *s <= '7')
                val = val * 8 + (*s++ - '0');
            if (*s >= '0' && *s <= '7')
                val = val * 8 + (*s++ - '0');
        } else if (*s == 'x') { // hex, any number of digits
            val = 0;
            for (s++; isxdigit((unsigned char)*s); s++)
                val =
                    val * 16 +
                    (isdigit((unsigned char)*s) ? *s - '0' : tolower((unsigned char)*s) - 'a' + 10);
        } else {
            val = (unsigned char)*s;
            s++;
        }
        break;
    }
    *ps = s;
    return val;
}

//
// Decode a string-literal lexeme to the bytes it denotes; see c_escape.h.
//
char *c_decode_string_literal(const char *raw, size_t *out_len)
{
    if (!raw || *raw != '"') {
        // Not a quoted lexeme: hand back a plain copy.
        char *copy = xstrdup(raw);
        *out_len   = copy ? strlen(copy) : 0;
        return copy;
    }
    const char *src = raw + 1;
    // No escape expands: the decoded byte count never exceeds the lexeme length.
    char *buf = xalloc(strlen(raw), __func__, __FILE__, __LINE__);
    char *dst = buf;
    while (*src && *src != '"') {
        if (*src == '\\' && src[1])
            *dst++ = (char)(unsigned char)c_escape_value(&src);
        else
            *dst++ = *src++;
    }
    *dst     = '\0';
    *out_len = (size_t)(dst - buf);
    return buf;
}
