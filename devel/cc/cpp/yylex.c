// UNIX V7 source code: see /COPYRIGHT or www.tuhs.org for details.

#include <string.h>

#include "defs.h"

static intmax_t str_to_int(char *st, int b, int *is_unsigned);

//
// The lexer for #if expressions: return the next token to the parser
// (parser.c).  It reads from the scan buffer via skip_blanks/scan_token,
// recognizing two-character operators (||, &&, ==, ...), single-character
// operators, numbers, character constants, the "defined" keyword, and
// identifiers.  A number's value is left in cpp.tok_value.  An identifier is
// looked up: a defined macro name becomes 1, an undefined one becomes 0 -- which
// is exactly how "#if SOMENAME" is supposed to behave.  Returns "stop" at the
// end of the line.
//
int lex_if_token()
{
    static int ifdef        = 0; // 1 while the operand of "defined" is expected
    static char *op2[]      = { "||", "&&", ">>", "<<", ">=", "<=", "!=", "==" }; // 2-char ops
    static const int val2[] = { OROR, ANDAND, RS, LS, GE, LE, NE, EQ }; // their token codes
    static const char *opc  = "b\bt\tn\nf\fr\r\\\\"; // escape letter -> value pairs (\n, \t, ...)
    char savc;
    const char *s;
    int val;
    char **p2;
    const struct symtab *sp;

    for (;;) {
        cpp.scan_ptr = skip_blanks(cpp.scan_ptr);
        if (*cpp.tok_ptr == '\n')
            return (stop); // end of #if
        savc          = *cpp.scan_ptr;
        *cpp.scan_ptr = '\0';
        for (p2 = op2 + 8; --p2 >= op2;) // check 2-char ops
            if (0 == strcmp(*p2, cpp.tok_ptr)) {
                val = val2[p2 - op2];
                goto ret;
            }
        s = "+-*/%<>&^|?:!~(),"; // check 1-char ops
        while (*s)
            if (*s++ == *cpp.tok_ptr) {
                val = *--s;
                goto ret;
            }
        cpp.tok_unsigned = 0;
        if (*cpp.tok_ptr <= '9' && *cpp.tok_ptr >= '0') { // a number
            if (*cpp.tok_ptr == '0')
                cpp.tok_value = (cpp.tok_ptr[1] == 'x' || cpp.tok_ptr[1] == 'X')
                                    ? str_to_int(cpp.tok_ptr + 2, 16, &cpp.tok_unsigned)
                                    : str_to_int(cpp.tok_ptr + 1, 8, &cpp.tok_unsigned);
            else
                cpp.tok_value = str_to_int(cpp.tok_ptr, 10, &cpp.tok_unsigned);
            val = number;
        } else if (ISID(*cpp.tok_ptr)) {
            if (0 == strcmp(cpp.tok_ptr, "defined")) {
                ifdef = 1;
                ++cpp.false_level;
                val = DEFINED;
            } else {
                sp = lookup(cpp.tok_ptr, -1);
                if (ifdef != 0) {
                    ifdef = 0;
                    --cpp.false_level;
                }
                cpp.tok_value = (sp->value == 0) ? 0 : 1;
                val           = number;
            }
        } else if (*cpp.tok_ptr == '\'') { // character constant
            val = number;
            if (cpp.tok_ptr[1] == '\\') { // escaped
                if (cpp.scan_ptr[-1] == '\'')
                    cpp.scan_ptr[-1] = '\0';
                s = opc;
                while (*s)
                    if (*s++ != cpp.tok_ptr[2])
                        ++s;
                    else {
                        cpp.tok_value = *s;
                        goto ret;
                    }
                if (cpp.tok_ptr[2] <= '9' && cpp.tok_ptr[2] >= '0')
                    cpp.tok_value = str_to_int(cpp.tok_ptr + 2, 8, &cpp.tok_unsigned);
                else
                    cpp.tok_value = cpp.tok_ptr[2];
            } else
                cpp.tok_value = cpp.tok_ptr[1];
        } else if (0 == strcmp("\\\n", cpp.tok_ptr)) {
            *cpp.scan_ptr = savc;
            continue;
        } else {
            *cpp.scan_ptr = savc;
            pperror("invalid character '%c' in '#if'", *cpp.tok_ptr);
            continue;
        }
    ret:
        *cpp.scan_ptr = savc;
        cpp.out_ptr = cpp.tok_ptr = cpp.scan_ptr;
        return (val);
    }
}

//
// Convert the numeric string st to an integer in base b (8, 10 or 16), the way
// #if numeric literals are written, with an optional u/U and l/L/ll/LL suffix in
// either order.  *is_unsigned is set for a 'u' suffix, or for an octal or hex value
// that fits only uintmax_t (§6.4.4.1p5).  Returns the value.
//
static intmax_t str_to_int(char *st, int b, int *is_unsigned)
{
    uintmax_t n = 0;
    int c, t, u = 0, l = 0;
    const char *s = st;

    while ((c = *s++)) {
        if (c >= '0' && c <= '9')
            t = c - '0';
        else if (c >= 'a' && c <= 'f')
            t = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            t = c - 'A' + 10;
        else
            t = 99;
        if (t < b && !u && !l) {
            n = n * b + t;
            continue;
        }
        if ((c == 'u' || c == 'U') && !u) {
            u = 1;
        } else if ((c == 'l' || c == 'L') && !l) {
            l = 1;
            if (*s == c)
                ++s; // ll, LL
        } else {
            pperror("invalid number '%s' in '#if'", st);
            break;
        }
    }
    *is_unsigned = u || (b != 10 && n > (uintmax_t)INTMAX_MAX);
    return (intmax_t)n;
}
