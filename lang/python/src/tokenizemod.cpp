// `_tokenize`: CPython's own tokenizer, as tokenize.py drives it.
//
// The lexer the compiler uses (lex.cpp) is not this: it drops comments and
// blank lines, keeps an f-string whole and remembers values rather than
// spellings. tokenize wants every token as written, where it starts and ends
// and the line it came from, so this is a port of Parser/lexer/ -- lexer.c,
// string.c and number.c -- in their own shape, over a reader that is
// Parser/tokenizer/reader.c's readline mode.
//
// The readline is Python, so it cannot be called from inside the lexer. The
// first __next__ calls it until it is exhausted and keeps the lines; the lexer
// then reads them one at a time, as it would have read them from readline,
// and an exception readline raised is raised where the lexer would have met
// it. Only a readline with side effects the program watches can tell.
#include "bigint.h"
#include "builtin.h"
#include "call.h"
#include "codec.h"
#include "exc.h"
#include "gc.h"
#include "intern.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "posix.h"
#include "type.h"
#include "ucd.h"
#include "ustr.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// Token numbers, Include/internal/pycore_token.h's.
enum : i32 {
    ENDMARKER        = 0,
    NAME             = 1,
    NUMBER           = 2,
    STRING           = 3,
    NEWLINE          = 4,
    INDENT           = 5,
    DEDENT           = 6,
    LPAR             = 7,
    RPAR             = 8,
    LSQB             = 9,
    RSQB             = 10,
    COLON            = 11,
    COMMA            = 12,
    SEMI             = 13,
    PLUS             = 14,
    MINUS            = 15,
    STAR             = 16,
    SLASH            = 17,
    VBAR             = 18,
    AMPER            = 19,
    LESS             = 20,
    GREATER          = 21,
    EQUAL            = 22,
    DOT              = 23,
    PERCENT          = 24,
    LBRACE           = 25,
    RBRACE           = 26,
    EQEQUAL          = 27,
    NOTEQUAL         = 28,
    LESSEQUAL        = 29,
    GREATEREQUAL     = 30,
    TILDE            = 31,
    CIRCUMFLEX       = 32,
    LEFTSHIFT        = 33,
    RIGHTSHIFT       = 34,
    DOUBLESTAR       = 35,
    PLUSEQUAL        = 36,
    MINEQUAL         = 37,
    STAREQUAL        = 38,
    SLASHEQUAL       = 39,
    PERCENTEQUAL     = 40,
    AMPEREQUAL       = 41,
    VBAREQUAL        = 42,
    CIRCUMFLEXEQUAL  = 43,
    LEFTSHIFTEQUAL   = 44,
    RIGHTSHIFTEQUAL  = 45,
    DOUBLESTAREQUAL  = 46,
    DOUBLESLASH      = 47,
    DOUBLESLASHEQUAL = 48,
    AT               = 49,
    ATEQUAL          = 50,
    RARROW           = 51,
    ELLIPSIS         = 52,
    COLONEQUAL       = 53,
    EXCLAMATION      = 54,
    OP               = 55,
    FSTRING_START    = 59,
    FSTRING_MIDDLE   = 60,
    FSTRING_END      = 61,
    TSTRING_START    = 62,
    TSTRING_MIDDLE   = 63,
    TSTRING_END      = 64,
    COMMENT          = 65,
    NL               = 66,
    ERRORTOKEN       = 67,
};

// Include/errcode.h.
enum : i32 {
    E_OK        = 10,
    E_EOF       = 11,
    E_TOKEN     = 13,
    E_NOMEM     = 15,
    E_ERROR     = 17,
    E_TABSPACE  = 18,
    E_TOODEEP   = 20,
    E_DEDENT    = 21,
    E_DECODE    = 22,
    E_EOFS      = 23,
    E_EOLS      = 24,
    E_LINECONT  = 25,
    E_COLUMNOVF = 29,
};

constexpr i32 EOF_            = -1;
constexpr i32 TABSIZE         = 8;
constexpr i32 ALTTABSIZE      = 1;
constexpr i32 MAXINDENT       = 100;
constexpr i32 MAXLEVEL        = 200;
constexpr i32 MAXFTLEVEL      = 150;
constexpr u8 MAX_EXPR_NESTING = 3;

i32 one_char(i32 c1)
{
    switch (c1) {
    case '!':
        return EXCLAMATION;
    case '%':
        return PERCENT;
    case '&':
        return AMPER;
    case '(':
        return LPAR;
    case ')':
        return RPAR;
    case '*':
        return STAR;
    case '+':
        return PLUS;
    case ',':
        return COMMA;
    case '-':
        return MINUS;
    case '.':
        return DOT;
    case '/':
        return SLASH;
    case ':':
        return COLON;
    case ';':
        return SEMI;
    case '<':
        return LESS;
    case '=':
        return EQUAL;
    case '>':
        return GREATER;
    case '@':
        return AT;
    case '[':
        return LSQB;
    case ']':
        return RSQB;
    case '^':
        return CIRCUMFLEX;
    case '{':
        return LBRACE;
    case '|':
        return VBAR;
    case '}':
        return RBRACE;
    case '~':
        return TILDE;
    }
    return OP;
}

i32 two_chars(i32 c1, i32 c2)
{
    switch (c1) {
    case '!':
        return c2 == '=' ? NOTEQUAL : OP;
    case '%':
        return c2 == '=' ? PERCENTEQUAL : OP;
    case '&':
        return c2 == '=' ? AMPEREQUAL : OP;
    case '*':
        return c2 == '*' ? DOUBLESTAR : c2 == '=' ? STAREQUAL : OP;
    case '+':
        return c2 == '=' ? PLUSEQUAL : OP;
    case '-':
        return c2 == '=' ? MINEQUAL : c2 == '>' ? RARROW : OP;
    case '/':
        return c2 == '/' ? DOUBLESLASH : c2 == '=' ? SLASHEQUAL : OP;
    case ':':
        return c2 == '=' ? COLONEQUAL : OP;
    case '<':
        return c2 == '<' ? LEFTSHIFT : c2 == '=' ? LESSEQUAL : c2 == '>' ? NOTEQUAL : OP;
    case '=':
        return c2 == '=' ? EQEQUAL : OP;
    case '>':
        return c2 == '=' ? GREATEREQUAL : c2 == '>' ? RIGHTSHIFT : OP;
    case '@':
        return c2 == '=' ? ATEQUAL : OP;
    case '^':
        return c2 == '=' ? CIRCUMFLEXEQUAL : OP;
    case '|':
        return c2 == '=' ? VBAREQUAL : OP;
    }
    return OP;
}

i32 three_chars(i32 c1, i32 c2, i32 c3)
{
    if (c1 == '*' && c2 == '*' && c3 == '=')
        return DOUBLESTAREQUAL;
    if (c1 == '.' && c2 == '.' && c3 == '.')
        return ELLIPSIS;
    if (c1 == '/' && c2 == '/' && c3 == '=')
        return DOUBLESLASHEQUAL;
    if (c1 == '<' && c2 == '<' && c3 == '=')
        return LEFTSHIFTEQUAL;
    if (c1 == '>' && c2 == '>' && c3 == '=')
        return RIGHTSHIFTEQUAL;
    return OP;
}

inline bool is_digit(i32 c)
{
    return c >= '0' && c <= '9';
}

inline bool is_xdigit(i32 c)
{
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

inline bool id_start(i32 c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 128;
}

inline bool id_char(i32 c)
{
    return id_start(c) || is_digit(c);
}

inline i32 to_lower(i32 c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

struct Loc {
    i32 lineno   = -1;
    i32 byte_col = -1;
};

struct Tok {
    i32 type    = -1;
    i32 level   = 0;
    bool is_raw = false;
    i64 start = -1, end = -1;
    Loc start_loc, end_loc;
};

enum : u8 { FT_MIDDLE, FT_EXPRESSION, FT_FORMAT_SPEC };
enum : u8 { FSTRING, RAW_FSTRING, TSTRING, RAW_TSTRING };

struct FtState {
    u8 mode              = FT_MIDDLE;
    u8 kind              = FSTRING;
    char quote           = 0;
    u8 quote_size        = 0;
    u8 debug_expr        = 0;
    u8 replacement_depth = 0;
    i32 paren_level      = 0;
    i64 start            = 0;
    Loc start_loc;
};

inline bool is_tstring(u8 kind)
{
    return kind == TSTRING || kind == RAW_TSTRING;
}

inline bool is_raw(u8 kind)
{
    return kind == RAW_FSTRING || kind == RAW_TSTRING;
}

inline char prefix_of(u8 kind)
{
    return is_tstring(kind) ? 't' : 'f';
}

// One line of the input, as readline gave it.
struct Line {
    usize at, len;
    bool implicit;
};

// A warning the lexer owes, made after the token is.
struct Owed {
    String msg;
    i32 lineno;
};

// struct tok_state, with the whole input kept.
struct Lexer {
    String input; // every line readline gave, in order
    Vec<Line> lines;
    String partial;    // an open line waiting for the rest of it
    String utf8_carry; // the start of a UTF-8 sequence the next chunk ends
    usize next_line = 0;
    bool read_error = false; // readline raised after the last line
    bool have_error = false; // ... and that exception is `error` on the object

    String buf; // what the lexer has read so far
    i64 buf_offset = 0, cur = 0, inp = 0, start = -1, line_start = 0;
    i32 done                   = E_OK;
    i32 indent                 = 0;
    i32 indstack[MAXINDENT]    = {};
    i32 altindstack[MAXINDENT] = {};
    i32 atbol = 1, pendin = 0, lineno = 0, level = 0;
    Loc start_loc;
    char parenstack[MAXLEVEL]      = {};
    i32 parenlinenostack[MAXLEVEL] = {};
    i32 parencolstack[MAXLEVEL]    = {};
    Vec<FtState> ft;
    bool extra_tokens     = false;
    bool comment_newline  = false;
    bool implicit_newline = false;
    Vec<Owed> owed;
    Value *error_slot = nullptr; // where the readline's exception lives

    FtState *current() { return ft.empty() ? nullptr : &ft[ft.size() - 1]; }

    i32 bracket_depth(const FtState *s) const { return level - s->paren_level; }

    i32 byte_column() const { return i32(cur - line_start); }

    bool failed() const { return done != E_OK && done != E_EOF; }

    // ------------------------------------------------------------ reading

    bool underflow()
    {
        bool reset = start < 0 && ft.empty();
        if (next_line >= lines.size()) {
            if (reset) {
                buf_offset = cur = inp = line_start = i64(buf.size());
            }
            if (read_error) {
                done = E_ERROR;
                if (have_error && error_slot)
                    err_set_value(*error_slot);
            } else {
                done = E_EOF;
            }
            return false;
        }
        const Line &l    = lines[next_line++];
        i64 source_start = i64(buf.size());
        if (!buf.append(Str(input.data() + l.at, l.len))) {
            done = E_NOMEM;
            return false;
        }
        if (reset) {
            cur        = source_start;
            buf_offset = source_start;
            line_start = buf_offset;
            start      = -1;
        }
        inp              = source_start + i64(l.len);
        implicit_newline = l.implicit;
        lineno++;
        return true;
    }

    bool refill()
    {
        if (done != E_OK)
            return false;
        if (!underflow()) {
            cur = inp;
            return false;
        }
        line_start = cur;
        for (i64 i = line_start; i < inp; i++)
            if (buf[usize(i)] == 0) {
                syntaxerror("source code cannot contain null bytes");
                cur = inp;
                return false;
            }
        return true;
    }

    i32 nextc()
    {
        while (cur == inp)
            if (!refill())
                return EOF_;
        return u8(buf[usize(cur++)]);
    }

    void backup(i32 c)
    {
        if (c != EOF_)
            --cur;
    }

    // ------------------------------------------------------------ errors

    // _syntaxerror_range: SyntaxError(msg, (filename, lineno, offset, text,
    // lineno, end_offset)), pending.
    i32 syntaxerror_range(Str msg, i32 col, i32 end_col, Str kind = "SyntaxError")
    {
        if (done == E_ERROR)
            return ERRORTOKEN;
        Str line     = Str(buf.data() + line_start, usize(inp - line_start));
        usize cursor = usize(cur - line_start);
        if (cursor > line.size())
            cursor = line.size();
        auto chars = [&](usize bytes) {
            i32 n = 0;
            for (usize i = 0; i < bytes && i < line.size(); i++)
                if ((u8(line[i]) & 0xc0) != 0x80)
                    n++;
            return n;
        };
        auto byte_to_char = [&](i32 byte_col) {
            i32 c = 1;
            for (i32 i = 0; i < byte_col - 1 && usize(i) < line.size(); i++)
                if ((u8(line[usize(i)]) & 0xc0) != 0x80)
                    c++;
            return c;
        };
        if (col == -1)
            col = chars(cursor);
        else if (col > 0)
            col = byte_to_char(col);
        if (end_col == -1)
            end_col = col;
        else if (end_col > 0)
            end_col = byte_to_char(end_col);
        usize nl = 0;
        while (nl < line.size() && line[nl] != '\n')
            nl++;
        raise(kind, msg, lineno, col, line.substr(0, nl), lineno, end_col);
        done = E_ERROR;
        return ERRORTOKEN;
    }

    i32 syntaxerror(Str msg) { return syntaxerror_range(msg, -1, -1); }

    static void raise(Str kind, Str msg, i32 line, i32 col, Str text, i32 end_line, i32 end_col,
                      bool ends = true)
    {
        Root m{ str_new(msg) };
        Root f{ str_new("<string>") };
        Root x{ str_lossy(text) };
        TupleObj *d = tuple_new(ends ? 6 : 6);
        if (m.v.is_nil() || f.v.is_nil() || x.v.is_nil() || !d) {
            if (!err_pending())
                oom();
            return;
        }
        Root rd{ obj_value(d) };
        d->items()[0]  = f.v;
        d->items()[1]  = int_from_i64(line);
        d->items()[2]  = int_from_i64(col);
        d->items()[3]  = x.v;
        d->items()[4]  = ends ? int_from_i64(end_line) : value_none();
        d->items()[5]  = ends ? int_from_i64(end_col) : value_none();
        TupleObj *args = tuple_new(2);
        if (!args) {
            oom();
            return;
        }
        args->items()[0] = m.v;
        args->items()[1] = rd.v;
        Root ra{ obj_value(args) };
        Value e = exc_new(exc_find(kind), ra.v);
        if (!e.is_nil())
            err_set_value(e);
    }

    i32 indenterror()
    {
        done = E_TABSPACE;
        cur  = inp;
        return ERRORTOKEN;
    }

    void warn_invalid_escape(i32 c)
    {
        Owed o;
        Buf<200> b;
        b.put("\"\\").put(char(c)).put("\" is an invalid escape sequence. Such sequences will ");
        b.put("not work in the future. Did you mean \"\\\\").put(char(c));
        b.put("\"? A raw string is also an option.");
        o.msg.assign(b.str());
        o.lineno = lineno;
        owed.push(static_cast<Owed &&>(o));
    }

    // ------------------------------------------------------------ tokens

    i32 make(Tok &t, i32 type, i64 p_start, i64 p_end)
    {
        t.level  = level;
        t.is_raw = false;
        t.start  = p_start;
        t.end    = p_end;
        if (p_start >= 0) {
            t.start_loc = start_loc;
            t.end_loc   = { lineno, byte_column() };
        } else {
            t.start_loc = t.end_loc = { lineno, -1 };
        }
        return type;
    }

    i32 continuation_line()
    {
        i32 c = nextc();
        if (c == '\r')
            c = nextc();
        if (c != '\n') {
            done = E_LINECONT;
            return -1;
        }
        c = nextc();
        if (c == EOF_) {
            done = E_EOF;
            cur  = inp;
            return -1;
        }
        backup(c);
        return c;
    }

    bool verify_identifier()
    {
        if (extra_tokens)
            return true;
        if (failed())
            return false;
        Str s      = Str(buf.data() + start, usize(cur - start));
        usize at   = 0;
        bool first = true;
        while (at < s.size()) {
            u32 cp  = 0;
            usize n = cp_decode(s, at, cp);
            if (n == 0) {
                done = E_DECODE;
                return false;
            }
            bool ok =
                first ? (ucd_is(cp, UCD_XID_START) || cp == '_') : ucd_is(cp, UCD_XID_CONTINUE);
            if (!ok) {
                if (at + n < s.size())
                    cur = start + i64(at + n);
                Buf<96> b;
                char hex[16];
                if (ucd_is(cp, UCD_PRINTABLE)) {
                    char enc[4];
                    b.put("invalid character '").put(Str(enc, cp_encode(cp, enc)));
                    b.put("' (U+").put(hex4(hex, cp)).put(")");
                } else {
                    b.put("invalid non-printable character U+").put(hex4(hex, cp));
                }
                syntaxerror(b.str());
                return false;
            }
            first = false;
            at += n;
        }
        return true;
    }

    // %04X.
    static Str hex4(char *out, u32 v)
    {
        char tmp[8];
        usize n = 0;
        do {
            tmp[n++] = "0123456789ABCDEF"[v & 15];
            v >>= 4;
        } while (v);
        while (n < 4)
            tmp[n++] = '0';
        for (usize i = 0; i < n; i++)
            out[i] = tmp[n - 1 - i];
        return Str(out, n);
    }

    // ------------------------------------------------------------ numbers

    bool lookahead(Str test)
    {
        usize s = 0;
        for (;;) {
            i32 c = nextc();
            if (s == test.size()) {
                bool res = !id_char(c);
                backup(c);
                while (s)
                    backup(test[--s]);
                return res;
            }
            if (c == test[s]) {
                s++;
                continue;
            }
            backup(c);
            while (s)
                backup(test[--s]);
            return false;
        }
    }

    bool verify_end_of_number(i32 c, Str kind)
    {
        if (extra_tokens)
            return true;
        bool r = false;
        if (c == 'a')
            r = lookahead("nd");
        else if (c == 'e')
            r = lookahead("lse");
        else if (c == 'f')
            r = lookahead("or");
        else if (c == 'i') {
            i32 c2 = nextc();
            if (c2 == 'f' || c2 == 'n' || c2 == 's')
                r = true;
            backup(c2);
        } else if (c == 'o')
            r = lookahead("r");
        else if (c == 'n')
            r = lookahead("ot");
        if (r) {
            backup(c);
            Owed o;
            Buf<64> b;
            b.put("invalid ").put(kind).put(" literal");
            o.msg.assign(b.str());
            o.lineno = lineno;
            owed.push(static_cast<Owed &&>(o));
            nextc();
        } else if (c < 128 && c >= 0 && id_char(c)) {
            backup(c);
            Buf<64> b;
            b.put("invalid ").put(kind).put(" literal");
            syntaxerror(b.str());
            return false;
        }
        return true;
    }

    i32 decimal_tail()
    {
        i32 c;
        for (;;) {
            do {
                c = nextc();
            } while (is_digit(c));
            if (c != '_')
                break;
            c = nextc();
            if (!is_digit(c)) {
                backup(c);
                syntaxerror("invalid decimal literal");
                return 0;
            }
        }
        return c;
    }

    i32 digit_error(Str kind, i32 c)
    {
        Buf<64> b;
        b.put("invalid digit '").put(char(c)).put("' in ").put(kind).put(" literal");
        return syntaxerror(b.str());
    }

    i32 scan_number(Tok &t, i32 c, bool leading_dot)
    {
        if (leading_dot)
            goto fraction;
        if (c == '0') {
            c = nextc();
            if (c == 'x' || c == 'X') {
                c = nextc();
                do {
                    if (c == '_')
                        c = nextc();
                    if (!is_xdigit(c)) {
                        backup(c);
                        return make(t, syntaxerror("invalid hexadecimal literal"), -1, -1);
                    }
                    do {
                        c = nextc();
                    } while (is_xdigit(c));
                } while (c == '_');
                if (!verify_end_of_number(c, "hexadecimal"))
                    return make(t, ERRORTOKEN, -1, -1);
            } else if (c == 'o' || c == 'O') {
                c = nextc();
                do {
                    if (c == '_')
                        c = nextc();
                    if (c < '0' || c >= '8') {
                        if (is_digit(c))
                            return make(t, digit_error("octal", c), -1, -1);
                        backup(c);
                        return make(t, syntaxerror("invalid octal literal"), -1, -1);
                    }
                    do {
                        c = nextc();
                    } while ('0' <= c && c < '8');
                } while (c == '_');
                if (is_digit(c))
                    return make(t, digit_error("octal", c), -1, -1);
                if (!verify_end_of_number(c, "octal"))
                    return make(t, ERRORTOKEN, -1, -1);
            } else if (c == 'b' || c == 'B') {
                c = nextc();
                do {
                    if (c == '_')
                        c = nextc();
                    if (c != '0' && c != '1') {
                        if (is_digit(c))
                            return make(t, digit_error("binary", c), -1, -1);
                        backup(c);
                        return make(t, syntaxerror("invalid binary literal"), -1, -1);
                    }
                    do {
                        c = nextc();
                    } while (c == '0' || c == '1');
                } while (c == '_');
                if (is_digit(c))
                    return make(t, digit_error("binary", c), -1, -1);
                if (!verify_end_of_number(c, "binary"))
                    return make(t, ERRORTOKEN, -1, -1);
            } else {
                bool nonzero = false;
                for (;;) {
                    if (c == '_') {
                        c = nextc();
                        if (!is_digit(c)) {
                            backup(c);
                            return make(t, syntaxerror("invalid decimal literal"), -1, -1);
                        }
                    }
                    if (c != '0')
                        break;
                    c = nextc();
                }
                i64 zeros_end = cur;
                if (is_digit(c)) {
                    nonzero = true;
                    c       = decimal_tail();
                    if (c == 0)
                        return make(t, ERRORTOKEN, -1, -1);
                }
                if (c == '.') {
                    c = nextc();
                    goto fraction;
                } else if (c == 'e' || c == 'E') {
                    goto exponent;
                } else if (c == 'j' || c == 'J') {
                    goto imaginary;
                } else if (nonzero && !extra_tokens) {
                    backup(c);
                    return make(
                        t,
                        syntaxerror_range("leading zeros in decimal integer literals are "
                                          "not permitted; use an 0o prefix for octal "
                                          "integers",
                                          i32(start + 1 - line_start), i32(zeros_end - line_start)),
                        -1, -1);
                }
                if (!verify_end_of_number(c, "decimal"))
                    return make(t, ERRORTOKEN, -1, -1);
            }
        } else {
            c = decimal_tail();
            if (c == 0)
                return make(t, ERRORTOKEN, -1, -1);
            if (c == '.') {
                c = nextc();
            fraction:
                if (is_digit(c)) {
                    c = decimal_tail();
                    if (c == 0)
                        return make(t, ERRORTOKEN, -1, -1);
                }
            }
            if (c == 'e' || c == 'E') {
                i32 e;
            exponent:
                e = c;
                c = nextc();
                if (c == '+' || c == '-') {
                    c = nextc();
                    if (!is_digit(c)) {
                        backup(c);
                        return make(t, syntaxerror("invalid decimal literal"), -1, -1);
                    }
                } else if (!is_digit(c)) {
                    backup(c);
                    if (!verify_end_of_number(e, "decimal"))
                        return make(t, ERRORTOKEN, -1, -1);
                    backup(e);
                    return make(t, NUMBER, start, cur);
                }
                c = decimal_tail();
                if (c == 0)
                    return make(t, ERRORTOKEN, -1, -1);
            }
            if (c == 'j' || c == 'J') {
            imaginary:
                c = nextc();
                if (!verify_end_of_number(c, "imaginary"))
                    return make(t, ERRORTOKEN, -1, -1);
            } else if (!verify_end_of_number(c, "decimal")) {
                return make(t, ERRORTOKEN, -1, -1);
            }
        }
        backup(c);
        return make(t, NUMBER, start, cur);
    }

    // ------------------------------------------------------------ strings

    void rewind_to_string_start(i64 at, Loc loc)
    {
        cur        = at + 1;
        line_start = at - loc.byte_col;
        lineno     = loc.lineno;
    }

    i32 check_string_prefixes(bool b, bool r, bool u, bool f, bool t)
    {
        struct Pair {
            bool on;
            Str a, b;
        };
        const Pair PAIRS[] = { { u && b, "u", "b" }, { u && r, "u", "r" }, { u && f, "u", "f" },
                               { u && t, "u", "t" }, { b && f, "b", "f" }, { b && t, "b", "t" },
                               { f && t, "f", "t" } };
        for (const Pair &p : PAIRS)
            if (p.on) {
                Buf<64> m;
                m.put('\'').put(p.a).put("' and '").put(p.b).put("' prefixes are incompatible");
                syntaxerror_range(m.str(), i32(start + 1 - line_start), i32(cur - line_start));
                return -1;
            }
        return 0;
    }

    i32 scan_fstring_start(Tok &t, i32 c)
    {
        i32 quote      = c;
        i32 quote_size = 1;
        i32 after      = nextc();
        if (after == quote) {
            i32 after2 = nextc();
            if (after2 == quote) {
                quote_size = 3;
            } else {
                backup(after2);
                backup(after);
            }
        }
        if (after != quote)
            backup(after);
        i64 p_start = start, p_end = cur;
        if (ft.size() + 1 >= usize(MAXFTLEVEL)) {
            syntaxerror("too many nested f-strings or t-strings");
            return make(t, ERRORTOKEN, -1, -1);
        }
        FtState s;
        s.mode        = FT_MIDDLE;
        s.quote       = char(quote);
        s.quote_size  = u8(quote_size);
        s.paren_level = level;
        s.start       = start;
        s.start_loc   = start_loc;
        char p0       = buf[usize(start)];
        char p1       = char(to_lower(buf[usize(start + 1)]));
        bool raw = false, tstr = false;
        if (p0 == 't' || p0 == 'T') {
            raw  = p1 == 'r';
            tstr = true;
        } else if (p0 == 'f' || p0 == 'F') {
            raw = p1 == 'r';
        } else {
            raw  = true;
            tstr = p1 == 't';
        }
        s.kind = tstr ? (raw ? RAW_TSTRING : TSTRING) : (raw ? RAW_FSTRING : FSTRING);
        if (!ft.push(s)) {
            done = E_NOMEM;
            return make(t, ERRORTOKEN, -1, -1);
        }
        return make(t, tstr ? TSTRING_START : FSTRING_START, p_start, p_end);
    }

    i32 scan_string(Tok &t, i32 c)
    {
        i32 quote          = c;
        i32 quote_size     = 1;
        i32 end_quote_size = 0;
        bool escaped_quote = false;
        c                  = nextc();
        if (c == quote) {
            c = nextc();
            if (c == quote)
                quote_size = 3;
            else
                end_quote_size = 1;
        }
        if (c != quote)
            backup(c);
        while (end_quote_size != quote_size) {
            c = nextc();
            if (done == E_ERROR)
                return make(t, ERRORTOKEN, -1, -1);
            if (done == E_DECODE)
                break;
            if (c == EOF_ || (quote_size == 1 && c == '\n')) {
                i32 end_lineno = lineno;
                rewind_to_string_start(start, start_loc);
                const FtState *s = current();
                char tmp[24];
                if (s && s->quote == quote && s->quote_size == quote_size) {
                    Buf<64> b;
                    b.put(prefix_of(s->kind)).put("-string: expecting '}'");
                    return make(t, syntaxerror(b.str()), -1, -1);
                }
                Buf<128> b;
                if (quote_size == 3) {
                    b.put("unterminated triple-quoted string literal (detected at line ");
                    b.put(int_text(tmp, sizeof tmp, end_lineno)).put(')');
                    syntaxerror(b.str());
                    if (c != '\n')
                        done = E_EOFS;
                    return make(t, ERRORTOKEN, -1, -1);
                }
                b.put("unterminated string literal (detected at line ");
                b.put(int_text(tmp, sizeof tmp, end_lineno)).put(')');
                if (escaped_quote)
                    b.put("; perhaps you escaped the end quote?");
                syntaxerror(b.str());
                if (c != '\n')
                    done = E_EOLS;
                return make(t, ERRORTOKEN, -1, -1);
            }
            if (c == quote) {
                end_quote_size += 1;
            } else {
                end_quote_size = 0;
                if (c == '\\') {
                    c = nextc();
                    if (c == quote)
                        escaped_quote = true;
                    if (c == '\r')
                        c = nextc();
                }
            }
        }
        return make(t, STRING, start, cur);
    }

    i32 begin_expr(FtState *s)
    {
        if (s->replacement_depth >= MAX_EXPR_NESTING) {
            Buf<64> b;
            b.put(prefix_of(s->kind)).put("-string: expressions nested too deeply");
            syntaxerror(b.str());
            return -1;
        }
        s->replacement_depth++;
        s->mode       = FT_EXPRESSION;
        s->debug_expr = 0;
        return 0;
    }

    i32 ftstring_punctuation(FtState *s, i32 c)
    {
        if (bracket_depth(s) != s->replacement_depth)
            return 0;
        if (c == '!') {
            i32 next = nextc();
            backup(next);
            if (next == '=')
                return 0;
        }
        if (c == ':') {
            s->mode = FT_FORMAT_SPEC;
            return COLON;
        }
        return 0;
    }

    i32 close_expr(FtState *s, i32 c)
    {
        i32 depth = bracket_depth(s);
        if (depth < 0) {
            Buf<64> b;
            b.put(prefix_of(s->kind)).put("-string: unmatched '").put(char(c)).put('\'');
            syntaxerror(b.str());
            return -1;
        }
        if (c == '}' && depth == s->replacement_depth - 1) {
            s->replacement_depth--;
            s->mode       = FT_MIDDLE;
            s->debug_expr = 0;
        }
        return 0;
    }

    i32 get_ftstring(FtState *s, Tok &t)
    {
        i64 p_start = -1, p_end = -1;
        i32 end_quote_size  = 0;
        bool unicode_escape = false;
        i32 quote           = s->quote;
        i32 quote_size      = s->quote_size;
        bool in_spec        = s->mode == FT_FORMAT_SPEC;
        bool raw            = is_raw(s->kind);
        char tmp[24];

        start     = cur;
        start_loc = { lineno, byte_column() };

        while (end_quote_size != quote_size) {
            i32 c = nextc();
            if (done == E_ERROR || done == E_DECODE)
                return make(t, ERRORTOKEN, -1, -1);
            if (c == EOF_ || (quote_size == 1 && c == '\n')) {
                if (failed())
                    return make(t, ERRORTOKEN, -1, -1);
                if (in_spec && c == '\n') {
                    Buf<128> b;
                    b.put(prefix_of(s->kind)).put("-string: newlines are not allowed in format ");
                    b.put("specifiers for single quoted ").put(prefix_of(s->kind)).put("-strings");
                    return make(t, syntaxerror(b.str()), -1, -1);
                }
                i32 end_lineno = lineno;
                rewind_to_string_start(s->start, s->start_loc);
                Buf<128> b;
                if (quote_size == 3) {
                    b.put("unterminated triple-quoted ").put(prefix_of(s->kind));
                    b.put("-string literal (detected at line ");
                    b.put(int_text(tmp, sizeof tmp, end_lineno)).put(')');
                    syntaxerror(b.str());
                    if (c != '\n')
                        done = E_EOFS;
                    return make(t, ERRORTOKEN, -1, -1);
                }
                b.put("unterminated ").put(prefix_of(s->kind)).put("-string literal (detected at");
                b.put(" line ").put(int_text(tmp, sizeof tmp, end_lineno)).put(')');
                return make(t, syntaxerror(b.str()), -1, -1);
            }
            if (c == quote) {
                end_quote_size += 1;
                continue;
            }
            end_quote_size = 0;
            if (c == '{') {
                i32 peek = nextc();
                if (peek != '{' || in_spec) {
                    backup(peek);
                    backup(c);
                    if (begin_expr(s) < 0)
                        return make(t, ERRORTOKEN, -1, -1);
                    p_start = start;
                    p_end   = cur;
                    if (p_start == p_end)
                        return get_normal(s, t);
                } else {
                    p_start = start;
                    p_end   = cur - 1;
                }
                return emit_middle(s, t, p_start, p_end, raw);
            } else if (c == '}') {
                if (unicode_escape)
                    return emit_middle(s, t, start, cur, raw);
                i32 peek  = nextc();
                i32 depth = bracket_depth(s);
                if (peek == '}' && !in_spec && depth == 0) {
                    p_start = start;
                    p_end   = cur - 1;
                } else {
                    backup(peek);
                    if (!in_spec && depth == 0) {
                        if (start == cur - 1) {
                            Buf<64> b;
                            b.put(prefix_of(s->kind)).put("-string: single '}' is not allowed");
                            return make(t, syntaxerror(b.str()), -1, -1);
                        }
                        backup(c);
                        return emit_middle(s, t, start, cur, raw);
                    }
                    backup(c);
                    s->mode = FT_EXPRESSION;
                    p_start = start;
                    p_end   = cur;
                }
                return emit_middle(s, t, p_start, p_end, raw);
            } else if (c == '\\') {
                i32 peek = nextc();
                if (peek == '\r')
                    peek = nextc();
                if (peek == '{' || peek == '}') {
                    if (!raw)
                        warn_invalid_escape(peek);
                    backup(peek);
                    continue;
                }
                if (!raw && peek == 'N') {
                    peek = nextc();
                    if (peek == '{')
                        unicode_escape = true;
                    else
                        backup(peek);
                }
            }
        }
        p_start = start;
        p_end   = cur;
        if (p_end - quote_size == p_start) {
            i32 end_token = is_tstring(s->kind) ? TSTRING_END : FSTRING_END;
            ft.pop();
            return make(t, end_token, p_start, p_end);
        }
        for (i32 i = 0; i < quote_size; i++)
            backup(quote);
        return emit_middle(s, t, p_start, cur, raw);
    }

    i32 emit_middle(FtState *s, Tok &t, i64 p_start, i64 p_end, bool raw)
    {
        i32 type = make(t, is_tstring(s->kind) ? TSTRING_MIDDLE : FSTRING_MIDDLE, p_start, p_end);
        t.is_raw = raw;
        return type;
    }

    // ------------------------------------------------------------ normal

    i32 get_normal(FtState *current, Tok &t)
    {
        i32 c;
        bool blankline;
        i64 p_start = -1, p_end = -1;
    nextline:
        start     = -1;
        start_loc = { lineno, -1 };
        blankline = false;

        if (atbol) {
            i32 col = 0, altcol = 0, cont_line_col = 0;
            atbol = 0;
            for (;;) {
                c = nextc();
                if (c == ' ') {
                    col++, altcol++;
                } else if (c == '\t') {
                    col    = (col / TABSIZE + 1) * TABSIZE;
                    altcol = (altcol / ALTTABSIZE + 1) * ALTTABSIZE;
                } else if (c == '\014') {
                    col = altcol = 0;
                } else if (c == '\\') {
                    cont_line_col = cont_line_col ? cont_line_col : col;
                    if ((c = continuation_line()) == -1)
                        return make(t, ERRORTOKEN, -1, -1);
                } else if (c == EOF_ && err_pending()) {
                    return make(t, ERRORTOKEN, -1, -1);
                } else {
                    break;
                }
            }
            backup(c);
            if (c == '#' || c == '\n' || c == '\r')
                blankline = true;
            if (!blankline && level == 0) {
                col    = cont_line_col ? cont_line_col : col;
                altcol = cont_line_col ? cont_line_col : altcol;
                if (col == indstack[indent]) {
                    if (altcol != altindstack[indent])
                        return make(t, indenterror(), -1, -1);
                } else if (col > indstack[indent]) {
                    if (indent + 1 >= MAXINDENT) {
                        done = E_TOODEEP;
                        cur  = inp;
                        return make(t, ERRORTOKEN, -1, -1);
                    }
                    if (altcol <= altindstack[indent])
                        return make(t, indenterror(), -1, -1);
                    pendin++;
                    indstack[++indent]  = col;
                    altindstack[indent] = altcol;
                } else {
                    while (indent > 0 && col < indstack[indent]) {
                        pendin--;
                        indent--;
                    }
                    if (col != indstack[indent]) {
                        done = E_DEDENT;
                        cur  = inp;
                        return make(t, ERRORTOKEN, -1, -1);
                    }
                    if (altcol != altindstack[indent])
                        return make(t, indenterror(), -1, -1);
                }
            }
        }

        start     = cur;
        start_loc = { lineno, byte_column() };

        if (pendin != 0) {
            if (pendin < 0) {
                if (extra_tokens) {
                    p_start = cur;
                    p_end   = cur;
                }
                pendin++;
                return make(t, DEDENT, p_start, p_end);
            }
            if (extra_tokens) {
                p_start = buf_offset;
                p_end   = cur;
            }
            pendin--;
            return make(t, INDENT, p_start, p_end);
        }

        c = nextc();
        backup(c);

    again:
        start = -1;
        do {
            c = nextc();
        } while (c == ' ' || c == '\t' || c == '\014');

        start     = cur - 1;
        start_loc = { lineno, byte_column() - 1 };

        if (c == '#') {
            while (c != EOF_ && c != '\n' && c != '\r')
                c = nextc();
            if (extra_tokens) {
                i64 p = start;
                backup(c);
                comment_newline = blankline;
                return make(t, COMMENT, p, cur);
            }
        }

        if (c == EOF_) {
            if (level)
                return make(t, ERRORTOKEN, -1, -1);
            return make(t, done == E_EOF ? ENDMARKER : ERRORTOKEN, -1, -1);
        }

        bool nonascii = false;
        if (id_start(c)) {
            bool saw_b = false, saw_r = false, saw_u = false, saw_f = false, saw_t = false;
            for (;;) {
                if (!saw_b && (c == 'b' || c == 'B'))
                    saw_b = true;
                else if (!saw_u && (c == 'u' || c == 'U'))
                    saw_u = true;
                else if (!saw_r && (c == 'r' || c == 'R'))
                    saw_r = true;
                else if (!saw_f && (c == 'f' || c == 'F'))
                    saw_f = true;
                else if (!saw_t && (c == 't' || c == 'T'))
                    saw_t = true;
                else
                    break;
                c = nextc();
                if (c == '"' || c == '\'') {
                    if (check_string_prefixes(saw_b, saw_r, saw_u, saw_f, saw_t) < 0)
                        return make(t, ERRORTOKEN, -1, -1);
                    if (saw_f || saw_t)
                        return scan_fstring_start(t, c);
                    return scan_string(t, c);
                }
            }
            while (id_char(c)) {
                if (c >= 128)
                    nonascii = true;
                c = nextc();
            }
            backup(c);
            if (nonascii && !verify_identifier())
                return make(t, ERRORTOKEN, -1, -1);
            return make(t, NAME, start, cur);
        }

        if (c == '\r')
            c = nextc();

        if (c == '\n') {
            atbol = 1;
            if (blankline || level > 0) {
                if (extra_tokens) {
                    if (comment_newline)
                        comment_newline = false;
                    return make(t, NL, start, cur);
                }
                goto nextline;
            }
            if (comment_newline && extra_tokens) {
                comment_newline = false;
                return make(t, NL, start, cur);
            }
            return make(t, NEWLINE, start, cur - 1);
        }

        if (c == '.') {
            c = nextc();
            if (is_digit(c))
                return scan_number(t, c, true);
            if (c == '.') {
                c = nextc();
                if (c == '.')
                    return make(t, ELLIPSIS, start, cur);
                backup(c);
                backup('.');
            } else {
                backup(c);
            }
            return make(t, DOT, start, cur);
        }

        if (is_digit(c))
            return scan_number(t, c, false);

        if (c == '\'' || c == '"')
            return scan_string(t, c);

        if (c == '\\') {
            if ((c = continuation_line()) == -1)
                return make(t, ERRORTOKEN, -1, -1);
            goto again;
        }

        if ((c == ':' || c == '}' || c == '!') && current) {
            i32 type = ftstring_punctuation(current, c);
            if (type < 0)
                return make(t, ERRORTOKEN, -1, -1);
            if (type != 0)
                return make(t, type, start, cur);
        }

        {
            i32 c2  = nextc();
            i32 two = two_chars(c, c2);
            if (two != OP) {
                i32 c3    = nextc();
                i32 three = three_chars(c, c2, c3);
                if (three != OP)
                    two = three;
                else
                    backup(c3);
                return make(t, two, start, cur);
            }
            backup(c2);
        }

        switch (c) {
        case '(':
        case '[':
        case '{':
            if (level >= MAXLEVEL)
                return make(t, syntaxerror("too many nested parentheses"), -1, -1);
            parenstack[level]       = char(c);
            parenlinenostack[level] = lineno;
            parencolstack[level]    = i32(start - line_start);
            level++;
            break;
        case ')':
        case ']':
        case '}':
            if (current && bracket_depth(current) == 0) {
                Buf<64> b;
                if (c == '}')
                    b.put(prefix_of(current->kind)).put("-string: single '}' is not allowed");
                else
                    b.put(prefix_of(current->kind))
                        .put("-string: unmatched '")
                        .put(char(c))
                        .put('\'');
                return make(t, syntaxerror(b.str()), -1, -1);
            }
            if (!extra_tokens && !level) {
                Buf<32> b;
                b.put("unmatched '").put(char(c)).put('\'');
                return make(t, syntaxerror(b.str()), -1, -1);
            }
            if (level > 0) {
                level--;
                i32 opening = parenstack[level];
                if (!extra_tokens &&
                    !((opening == '(' && c == ')') || (opening == '[' && c == ']') ||
                      (opening == '{' && c == '}'))) {
                    if (current && opening == '{' &&
                        bracket_depth(current) == current->replacement_depth - 1) {
                        Buf<64> b;
                        b.put(prefix_of(current->kind)).put("-string: unmatched '").put(char(c));
                        b.put('\'');
                        return make(t, syntaxerror(b.str()), -1, -1);
                    }
                    char tmp[24];
                    Buf<128> b;
                    b.put("closing parenthesis '").put(char(c));
                    b.put("' does not match opening parenthesis '").put(char(opening)).put('\'');
                    if (parenlinenostack[level] != lineno)
                        b.put(" on line ").put(int_text(tmp, sizeof tmp, parenlinenostack[level]));
                    return make(t, syntaxerror(b.str()), -1, -1);
                }
            }
            if (current && close_expr(current, c) < 0)
                return make(t, ERRORTOKEN, -1, -1);
            break;
        default:
            break;
        }

        if (c < 0x20 || c > 0x7e) {
            Buf<64> b;
            char hex[16];
            b.put("invalid non-printable character U+").put(hex4(hex, u32(c)));
            return make(t, syntaxerror(b.str()), -1, -1);
        }

        if (c == '=' && current && bracket_depth(current) == current->replacement_depth)
            current->debug_expr = 1;

        return make(t, one_char(c), start, cur);
    }

    i32 get(Tok &t)
    {
        t          = Tok{};
        FtState *s = current();
        i32 result;
        if (!s || s->mode == FT_EXPRESSION)
            result = get_normal(s, t);
        else
            result = get_ftstring(s, t);
        if (failed())
            result = ERRORTOKEN;
        t.type = result;
        return result;
    }
};

// ------------------------------------------------------------ the object

struct IterObj : Obj {
    Value readline;
    Value encoding;  // a str, or Nil for a readline that answers str
    Value error;     // what readline raised, raised again where it is met
    Value last_line; // the line the previous token came from
    Lexer *lx;
    i64 last_lineno, last_end_lineno, col_diff;
    bool extra_tokens, done, drained, finished;
};

void iter_trace(Obj *o)
{
    IterObj *it = static_cast<IterObj *>(o);
    gc_mark(it->readline);
    gc_mark(it->encoding);
    gc_mark(it->error);
    gc_mark(it->last_line);
}

void iter_fini(Obj *o)
{
    IterObj *it = static_cast<IterObj *>(o);
    if (it->lx)
        heap_delete(it->lx);
    it->lx = nullptr;
}

R iter_next(Value v, Value &out);

Value iter_self(Value v)
{
    return v;
}

extern const Type iter_type;

IterObj *iter_of(Value v)
{
    return static_cast<IterObj *>(v.obj());
}

R stop_iteration()
{
    Value e = exc_new(exc_find("StopIteration"), Value());
    return e.is_nil() ? R::Err : err_set_value(e);
}

// The length of a UTF-8 sequence that `s` ends in the middle of, to be
// completed by the next chunk; zero when it ends on a boundary.
usize incomplete_tail(Str s)
{
    usize n = s.size();
    for (usize back = 1; back <= 3 && back <= n; back++) {
        u8 c = u8(s[n - back]);
        if ((c & 0xc0) == 0x80)
            continue;
        usize need = c >= 0xf0 ? 4 : c >= 0xe0 ? 3 : c >= 0xc0 ? 2 : 1;
        return need > back && c >= 0xc2 && c <= 0xf4 ? back : 0;
    }
    return 0;
}

// One chunk of decoded text into the lexer's lines. A line the chunk leaves
// open is ended with a newline of its own, unless `may_end` is false because
// a decoder still holds its tail; then it waits for the next chunk.
bool take_text(Lexer *lx, Str text, bool may_end)
{
    String joined;
    if (!lx->partial.empty()) {
        if (!joined.append(lx->partial.str()) || !joined.append(text))
            return false;
        lx->partial.clear();
        text = joined.str();
    }
    usize at = 0;
    while (at < text.size()) {
        usize e = at;
        while (e < text.size() && text[e] != '\n')
            e++;
        bool nl = e < text.size();
        if (!nl && !may_end)
            return lx->partial.assign(text.substr(at));
        usize len   = e - at + (nl ? 1 : 0);
        usize start = lx->input.size();
        if (!lx->input.append(text.substr(at, len)))
            return false;
        if (!nl && !lx->input.push('\n'))
            return false;
        if (!lx->lines.push(Line{ start, len + (nl ? 0 : 1), !nl }))
            return false;
        at += len;
    }
    return true;
}

// The UTF-8 decoding with "replace" a chunk goes through, natively.
R decode_utf8(Str bytes, Value &out)
{
    Root in{ bytes_new(bytes) };
    Root errors{ str_new("replace") };
    if (in.v.is_nil() || errors.v.is_nil())
        return R::Err;
    CodecCall c;
    c.codec  = Codec::Utf8;
    c.input  = in.v;
    c.errors = errors.v;
    return codec_run(c, out);
}

// s[0] the iterator, s[1] the chunk being decoded, s[2] the text carried
// between chunks; i the state.
enum : u32 { DR_CALL, DR_GOT, DR_DECODED, DR_TOKEN };

R iter_produce(Value self, Value &out);

R finish_drain(ContObj *k)
{
    IterObj *it = iter_of(k->s[0]);
    it->drained = true;
    // The decoder's held tail, and the open line, end the input.
    Lexer *lx = it->lx;
    if (!lx->utf8_carry.empty()) {
        Root text;
        Value got;
        if (decode_utf8(lx->utf8_carry.str(), got) != R::Ok)
            return R::Err;
        text = got;
        lx   = iter_of(k->s[0])->lx;
        lx->utf8_carry.clear();
        if (is_str(text.v) && !take_text(lx, str_of(text.v)->str(), true))
            return oom();
    }
    if (!lx->partial.empty()) {
        String rest;
        if (!rest.append(lx->partial.str()))
            return oom();
        lx->partial.clear();
        if (!take_text(lx, rest.str(), true))
            return oom();
    }
    Value got;
    k->i = DR_TOKEN;
    R r  = iter_produce(k->s[0], got);
    if (r != R::Ok)
        return r;
    if (is_cont(got))
        return cont_await(k, got);
    return cont_done(k, got);
}

R drain_step(ContObj *k, Value in)
{
    for (;;) {
        IterObj *it = iter_of(k->s[0]);
        switch (k->i) {
        case DR_CALL:
            k->i        = DR_GOT;
            k->catching = CATCH_ANY;
            return cont_call(k, it->readline, Value(), 0);
        case DR_GOT: {
            k->catching = CATCH_NONE;
            if (in.is_nil()) {
                // Raised: StopIteration is the end, anything else is kept.
                Value e          = k->caught;
                k->caught        = Value();
                const ExcType *t = exc_type_of(e);
                if (!t || !exc_is(t, exc_find("StopIteration"))) {
                    it->error          = e;
                    it->lx->read_error = true;
                    it->lx->have_error = true;
                }
                return finish_drain(k);
            }
            bool text = it->encoding.is_nil();
            if (text ? !is_str(in) : !is_bytes(in)) {
                Root e{ exc_make("TypeError",
                                 text ? Str("readline() returned a non-string object")
                                      : Str("readline() returned a non-bytes object")) };
                if (e.v.is_nil())
                    return R::Err;
                it                 = iter_of(k->s[0]);
                it->error          = e.v;
                it->lx->read_error = true;
                it->lx->have_error = true;
                return finish_drain(k);
            }
            if (text) {
                Str s = str_of(in)->str();
                if (s.empty())
                    return finish_drain(k);
                if (!take_text(it->lx, s, true))
                    return oom();
                k->i = DR_CALL;
                in   = Value();
                continue;
            }
            Str b = static_cast<BytesObj *>(in.obj())->str();
            if (b.empty())
                return finish_drain(k);
            Str enc = str_of(it->encoding)->str();
            if (enc == "utf-8") {
                // A sequence split between two chunks is held, as an
                // incremental decoder would hold it.
                Lexer *lx = it->lx;
                String joined;
                if (!joined.append(lx->utf8_carry.str()) || !joined.append(b))
                    return oom();
                lx->utf8_carry.clear();
                usize tail = incomplete_tail(joined.str());
                Str whole  = joined.str();
                if (!lx->utf8_carry.assign(whole.substr(whole.size() - tail)))
                    return oom();
                Value got;
                if (decode_utf8(whole.substr(0, whole.size() - tail), got) != R::Ok)
                    return R::Err;
                if (!is_str(got))
                    return err_set("SystemError", "tokenizer: utf-8 decoding suspended");
                if (!take_text(iter_of(k->s[0])->lx, str_of(got)->str(), tail == 0))
                    return oom();
                k->i = DR_CALL;
                in   = Value();
                continue;
            }
            k->i = DR_DECODED;
            Root errs{ str_new("replace") };
            if (errs.v.is_nil())
                return R::Err;
            return cont_method(k, in, "decode", 2, it->encoding, errs.v);
        }
        case DR_DECODED:
            if (!is_str(in))
                return err_set2("TypeError", "decoder should return a string result, not",
                                type_name(in));
            if (!take_text(it->lx, str_of(in)->str(), true))
                return oom();
            k->i = DR_CALL;
            in   = Value();
            continue;
        default:
            return cont_done(k, in);
        }
    }
}

// The next token of an iterator whose input is in, as the tuple.
R iter_produce(Value self, Value &out)
{
    Root rs{ self };
    IterObj *it = iter_of(rs.v);
    Lexer *lx   = it->lx;
    if (it->finished)
        return stop_iteration();
    lx->error_slot = &it->error;
    Tok t;
    i32 type = lx->get(t);
    it       = iter_of(rs.v);
    if (type == ERRORTOKEN) {
        if (!err_pending()) {
            Str msg;
            Str kind = "SyntaxError";
            switch (lx->done) {
            case E_TOKEN:
                msg = "invalid token";
                break;
            case E_EOF: {
                i64 span = lx->inp - lx->buf_offset;
                Lexer::raise("SyntaxError", "unexpected EOF in multi-line statement", lx->lineno,
                             i32(span > 0 ? span : 0), Str(), 0, 0, false);
                return R::Err;
            }
            case E_DEDENT:
                msg  = "unindent does not match any outer indentation level";
                kind = "IndentationError";
                break;
            case E_NOMEM:
                return oom();
            case E_TABSPACE:
                kind = "TabError";
                msg  = "inconsistent use of tabs and spaces in indentation";
                break;
            case E_TOODEEP:
                kind = "IndentationError";
                msg  = "too many levels of indentation";
                break;
            case E_LINECONT:
                msg = "unexpected character after line continuation character";
                break;
            default:
                msg = "unknown tokenization error";
            }
            Str span  = Str(lx->buf.data() + lx->buf_offset, usize(lx->inp - lx->buf_offset));
            Str line  = span.size() ? span.substr(0, span.size() - 1) : span;
            i32 chars = 1;
            for (usize i = 0; i < line.size(); i++)
                if ((u8(line[i]) & 0xc0) != 0x80)
                    chars++;
            Lexer::raise(kind, msg, lx->lineno, chars, line, 0, 0, false);
        }
        return R::Err;
    }
    if (it->done) {
        it->finished = true;
        return stop_iteration();
    }

    // The token's text, and the line it is on.
    Root str;
    if (t.start < 0)
        str = str_new("");
    else
        str = str_new(Str(lx->buf.data() + t.start, usize(t.end - t.start)));
    if (str.v.is_nil())
        return R::Err;
    bool trailing     = type == ENDMARKER || (type == DEDENT && lx->done == E_EOF);
    bool literal      = type == STRING || type == FSTRING_MIDDLE || type == TSTRING_MIDDLE;
    i64 end_line      = lx->line_start;
    i64 line_at       = literal && t.start >= 0 ? t.start - t.start_loc.byte_col : end_line;
    i64 line_len      = lx->inp - lx->line_start + (end_line - line_at);
    bool line_changed = true;
    Root line;
    if (it->extra_tokens && trailing) {
        line = str_new("");
    } else {
        i64 size = line_len;
        if (size >= 1 && lx->implicit_newline)
            size -= 1;
        if (t.end_loc.lineno != it->last_lineno) {
            line          = str_lossy(Str(lx->buf.data() + line_at, usize(size)));
            it            = iter_of(rs.v);
            it->last_line = line.v;
            it->col_diff  = 0;
        } else {
            line         = it->last_line;
            line_changed = false;
        }
    }
    if (line.v.is_nil())
        return R::Err;
    it = iter_of(rs.v);

    i64 lineno = t.start_loc.lineno, end_lineno = t.end_loc.lineno;
    i64 col = -1, end_col = -1;
    // _get_col_offsets: characters, counted from the bytes.
    Str ltext          = is_str(line.v) ? str_of(line.v)->str() : Str();
    auto chars_between = [&](i64 from, i64 to) {
        i64 n = 0;
        while (from < to) {
            u8 ch = from < i64(ltext.size()) ? u8(ltext[usize(from)]) : 0;
            from += ch < 0x80 ? 1 : (ch & 0xe0) == 0xc0 ? 2 : (ch & 0xf0) == 0xe0 ? 3 : 4;
            n++;
        }
        return n;
    };
    i64 byte_offset = -1;
    if (t.start >= 0 && t.start >= line_at) {
        byte_offset = t.start - line_at;
        if (line_changed) {
            col          = chars_between(0, byte_offset);
            it->col_diff = byte_offset - col;
        } else {
            col = byte_offset - it->col_diff;
        }
    }
    if (t.end >= 0 && t.end >= end_line) {
        i64 end_byte = t.end - end_line;
        if (lineno == end_lineno) {
            i64 tc  = chars_between(byte_offset, end_byte);
            end_col = col + tc;
            it->col_diff += (t.end - t.start) - tc;
        } else {
            Str raw = Str(lx->buf.data() + end_line, usize(lx->inp - end_line));
            i64 n   = 0;
            for (i64 i = 0; i < end_byte && usize(i) < raw.size(); i++)
                if ((u8(raw[usize(i)]) & 0xc0) != 0x80)
                    n++;
            end_col = n;
            it->col_diff += end_byte - end_col;
        }
    }
    it->last_lineno     = lineno;
    it->last_end_lineno = end_lineno;

    i32 kind = type;
    if (it->extra_tokens) {
        if (trailing) {
            lineno = end_lineno = lineno + 1;
            col = end_col = 0;
        }
        if (kind > DEDENT && kind < OP) {
            kind = OP;
        } else if (kind == NEWLINE) {
            if (!lx->implicit_newline)
                str = str_new(t.start >= 0 && lx->buf[usize(t.start)] == '\r' ? Str("\r\n")
                                                                              : Str("\n"));
            else
                str = str_new("");
            end_col++;
        } else if (kind == NL) {
            if (lx->implicit_newline)
                str = str_new("");
        }
        if (str.v.is_nil())
            return R::Err;
    }
    if (type == ENDMARKER)
        iter_of(rs.v)->done = true;

    Root sp, ep;
    TupleObj *a = tuple_new(2);
    if (!a)
        return oom();
    sp                                              = obj_value(a);
    static_cast<TupleObj *>(sp.v.obj())->items()[0] = int_from_i64(lineno);
    static_cast<TupleObj *>(sp.v.obj())->items()[1] = int_from_i64(col);
    TupleObj *b                                     = tuple_new(2);
    if (!b)
        return oom();
    ep                                              = obj_value(b);
    static_cast<TupleObj *>(ep.v.obj())->items()[0] = int_from_i64(end_lineno);
    static_cast<TupleObj *>(ep.v.obj())->items()[1] = int_from_i64(end_col);
    TupleObj *tup                                   = tuple_new(5);
    if (!tup)
        return oom();
    tup->items()[0] = Value::of_int(kind);
    tup->items()[1] = str.v;
    tup->items()[2] = sp.v;
    tup->items()[3] = ep.v;
    tup->items()[4] = line.v;
    Root rt{ obj_value(tup) };

    // A warning the lexer owes goes out before the token does.
    lx = iter_of(rs.v)->lx;
    if (!lx->owed.empty()) {
        Owed o = static_cast<Owed &&>(lx->owed[0]);
        lx->owed.erase(0);
        Root w{ warn_explicit_cont("SyntaxWarning", o.msg.str(), "<string>", o.lineno) };
        if (w.v.is_nil())
            return R::Err;
        return warn_then_cont(w.v, rt.v, out);
    }
    out = rt.v;
    return R::Ok;
}

R iter_next(Value v, Value &out)
{
    IterObj *it = iter_of(v);
    if (!it->drained) {
        Root rv{ v };
        Root kv{ cont_new(drain_step) };
        if (kv.v.is_nil())
            return R::Err;
        cont_of(kv.v)->s[0] = rv.v;
        out                 = kv.v;
        return R::Ok;
    }
    return iter_produce(v, out);
}

R m_next(const CallArgs &a, Value &out)
{
    if (!a.nargs || !a.args[0].is_obj() || a.args[0].obj()->type != &iter_type)
        return err_set("TypeError", "a _tokenize.TokenizerIter is required");
    return iter_next(a.args[0], out);
}

R m_iter(const CallArgs &a, Value &out)
{
    if (!a.nargs)
        return err_set("TypeError", "__iter__ needs self");
    out = a.args[0];
    return R::Ok;
}

constexpr Method ITER_METHODS[] = {
    { "__next__", m_next },
    { "__iter__", m_iter },
};

constexpr Type iter_type{ .name   = "_tokenize.TokenizerIter",
                          .trace  = iter_trace,
                          .fini   = iter_fini,
                          .iter   = iter_self,
                          .next   = iter_next,
                          .final  = true,
                          .vmnext = true };

R b_tokenizer_iter(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "readline", "extra_tokens", "encoding" };
    Value v[3];
    if (a.nargs > 1) {
        char tmp[24];
        Buf<96> b;
        b.put("TokenizerIter() takes exactly 1 positional argument (");
        b.put(int_text(tmp, sizeof tmp, a.nargs)).put(" given)");
        return err_set("TypeError", b.str());
    }
    if (!fn_take(a, "TokenizerIter", NAMES, 1, v))
        return R::Err;
    if (v[1].is_nil())
        return err_set("TypeError",
                       "TokenizerIter() missing required keyword-only argument: "
                       "'extra_tokens'");
    if (!v[2].is_nil() && !is_str(v[2])) {
        Buf<96> b;
        b.put("TokenizerIter() argument 'encoding' must be str, not ").put(type_name(v[2]));
        return err_set("TypeError", b.str());
    }
    Root rl{ v[0] }, enc{ v[2] };
    Lexer *lx = heap_new<Lexer>();
    if (!lx)
        return oom();
    lx->extra_tokens = py_truth(v[1]);
    IterObj *it      = static_cast<IterObj *>(obj_alloc(&iter_type, sizeof(IterObj)));
    if (!it) {
        heap_delete(lx);
        return oom();
    }
    it->readline        = rl.v;
    it->encoding        = enc.v;
    it->error           = Value();
    it->last_line       = Value();
    it->lx              = lx;
    it->last_lineno     = 0;
    it->last_end_lineno = 0;
    it->col_diff        = 0;
    it->extra_tokens    = lx->extra_tokens;
    it->done            = false;
    it->drained         = false;
    it->finished        = false;
    out                 = obj_value(it);
    return R::Ok;
}

} // namespace

bool tokenize_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    if (!method_install(&iter_type, ITER_METHODS))
        return false;
    return mod_type(static_cast<DictObj *>(rd.v.obj()), &iter_type, b_tokenizer_iter);
}
