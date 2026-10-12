#include "scanner.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "srcloc.h"

// Global variables
static FILE *input_file;
static char yytext[1024]; // Buffer for current lexeme
static int yyleng = 0;    // Length of current lexeme
static int next_char;     // Lookahead character

// Function prototypes
static void consume_char(void);
static void unget_char(void);
static int is_keyword(const char *str);
static void skip_whitespace(void);
static void skip_comment(void);
static int scan_identifier(void);
static int scan_number(void);
static int scan_string(void);
static int scan_char(void);
static int scan_operator(void);
static void scan_line_marker(void);

// Current location in input file: the file and line of the lookahead character,
// as the last line marker gives them, and its column.
int scanner_lineno;
const char *scanner_filename;
static int scanner_col;

// Where the last token returned by yylex() starts.
SrcLoc scanner_token_loc;

// The name of the input, until a line marker names another.
static const char *input_name;

void scanner_set_input_name(const char *name)
{
    input_name = srcloc_intern(name);
}

// Initialize scanner with input file
void init_scanner(FILE *input)
{
    input_file        = input;
    yyleng            = 0;
    yytext[0]         = '\0';
    scanner_lineno    = 1;
    scanner_col       = 1;
    scanner_filename  = input_name;
    scanner_token_loc = (SrcLoc){ input_name, 1, 1 };
    next_char         = input_file ? fgetc(input_file) : EOF;

    if (next_char == '#') {
        consume_char();
        scan_line_marker();
    }
}

// The location of the lookahead character.
static SrcLoc current_loc(void)
{
    return (SrcLoc){ scanner_filename, scanner_lineno, scanner_col };
}

// Report a lexical error at the start of the token being scanned. With error
// recovery on (diag_recover set), count it and return: the caller repairs the
// token in place, so the parser never sees a broken one. Otherwise exit.
static void lex_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    diag_vreport(scanner_token_loc, "error", fmt, ap);
    va_end(ap);
    if (!diag_recover) {
        exit(1);
    }
    diag_count_error();
}

// Main lexer function
int yylex(void)
{
again:
    if (next_char == EOF) {
        scanner_token_loc = current_loc();
        return TOKEN_EOF; // End of input
    }
    skip_whitespace();
    yyleng            = 0;
    yytext[0]         = '\0';
    scanner_token_loc = current_loc();
    if (next_char == EOF) {
        return TOKEN_EOF;
    }

    // Check for comments
    if (next_char == '/') {
        consume_char();
        if (next_char == '*') {
            skip_comment();
            goto again; // Recurse after skipping comment
        } else if (next_char == '/') {
            while (next_char != '\n' && next_char != EOF) {
                consume_char();
            }
            goto again; // Recurse after skipping line comment
        } else {
            unget_char();
            next_char = '/';
        }
    }

    // Scan tokens
    int token;
    if (isalpha(next_char) || next_char == '_' || next_char == '$') {
        token = scan_identifier();
    } else if (isdigit(next_char)) {
        token = scan_number();
    } else if (next_char == '.') {
        // A '.' immediately followed by a digit begins a floating constant with
        // no integer part (e.g. '.5', '.01e+2').  scan_number's decimal path
        // already handles a leading '.', so just route to it; otherwise the '.'
        // is the member-access operator or part of '...'.
        int after = fgetc(input_file);
        if (after != EOF) {
            ungetc(after, input_file);
        }
        if (isdigit(after)) {
            token = scan_number();
        } else {
            token = scan_operator();
        }
    } else if (next_char == '"') {
        token = scan_string();
    } else if (next_char == '\'') {
        token = scan_char();
    } else {
        token = scan_operator();
    }
    if (token == TOKEN_UNKNOWN) {
        goto again; // an invalid character, already reported
    }
    return token;
}

// Consume a character and add to yytext
static void consume_char(void)
{
    if (yyleng < (int)(sizeof(yytext) - 1)) {
        yytext[yyleng++] = next_char;
        yytext[yyleng]   = '\0';
    }
    if (next_char == '\n') {
        scanner_lineno++;
        scanner_col = 1;
    } else if (next_char != EOF) {
        scanner_col++;
    }
    next_char = fgetc(input_file);
}

// Push back the current character: the one consumed before it, never a newline,
// becomes the lookahead again.
static void unget_char(void)
{
    scanner_col--;
    if (next_char != EOF) {
        ungetc(next_char, input_file);
        if (yyleng > 0) {
            yyleng--;
            yytext[yyleng] = '\0';
        }
    }
}

struct keyword {
    const char *name;
    int token;
};

// Comparison function for bsearch
static int compare(const void *a, const void *b)
{
    const char *key             = a;
    const struct keyword *entry = b;
    return strcmp(key, entry->name);
}

// Check if yytext is a keyword using bsearch
static int is_keyword(const char *str)
{
    static const struct keyword keywords[] = {
        { "_Alignas", TOKEN_ALIGNAS },
        { "_Alignof", TOKEN_ALIGNOF },
        { "_Atomic", TOKEN_ATOMIC },
        { "_Await", TOKEN_AWAIT },
        { "_Bool", TOKEN_BOOL },
        { "_Complex", TOKEN_COMPLEX },
        { "_Coro", TOKEN_CORO },
        { "_Coro_frame", TOKEN_CORO_FRAME },
        { "_Coro_ptr", TOKEN_CORO_PTR },
        { "_Defer", TOKEN_DEFER },
        { "_Generic", TOKEN_GENERIC },
        { "_Imaginary", TOKEN_IMAGINARY },
        { "_Noreturn", TOKEN_NORETURN },
        { "_Static_assert", TOKEN_STATIC_ASSERT },
        { "_Thread_local", TOKEN_THREAD_LOCAL },
        { "_Yield", TOKEN_YIELD },
        { "__builtin_va_class", TOKEN_VA_CLASS },
        { "__co_alignof", TOKEN_CO_ALIGNOF },
        { "__co_alloca", TOKEN_CO_ALLOCA },
        { "__co_cancel", TOKEN_CO_CANCEL },
        { "__co_destroy", TOKEN_CO_DESTROY },
        { "__co_done", TOKEN_CO_DONE },
        { "__co_init", TOKEN_CO_INIT },
        { "__co_result", TOKEN_CO_RESULT },
        { "__co_resume", TOKEN_CO_RESUME },
        { "__co_sizeof", TOKEN_CO_SIZEOF },
        { "__co_value", TOKEN_CO_VALUE },
        { "__func__", TOKEN_FUNC_NAME },
        { "auto", TOKEN_AUTO },
        { "break", TOKEN_BREAK },
        { "case", TOKEN_CASE },
        { "char", TOKEN_CHAR },
        { "const", TOKEN_CONST },
        { "continue", TOKEN_CONTINUE },
        { "default", TOKEN_DEFAULT },
        { "do", TOKEN_DO },
        { "double", TOKEN_DOUBLE },
        { "else", TOKEN_ELSE },
        { "enum", TOKEN_ENUM },
        { "extern", TOKEN_EXTERN },
        { "float", TOKEN_FLOAT },
        { "for", TOKEN_FOR },
        { "goto", TOKEN_GOTO },
        { "if", TOKEN_IF },
        { "inline", TOKEN_INLINE },
        { "int", TOKEN_INT },
        { "long", TOKEN_LONG },
        { "register", TOKEN_REGISTER },
        { "restrict", TOKEN_RESTRICT },
        { "return", TOKEN_RETURN },
        { "short", TOKEN_SHORT },
        { "signed", TOKEN_SIGNED },
        { "sizeof", TOKEN_SIZEOF },
        { "static", TOKEN_STATIC },
        { "struct", TOKEN_STRUCT },
        { "switch", TOKEN_SWITCH },
        { "typedef", TOKEN_TYPEDEF },
        { "union", TOKEN_UNION },
        { "unsigned", TOKEN_UNSIGNED },
        { "void", TOKEN_VOID },
        { "volatile", TOKEN_VOLATILE },
        { "while", TOKEN_WHILE },
    };

    // Perform binary search: the table is in strcmp order ('_' sorts after 'Z').
    const struct keyword *result = bsearch(str, keywords, sizeof(keywords) / sizeof(keywords[0]),
                                           sizeof(keywords[0]), compare);
    if (!result) {
        return 0;
    }
    return result->token;
}

// Skip whitespace
static void skip_whitespace(void)
{
    while (isspace(next_char)) {
        int c = next_char;
        consume_char();
        if (c == '\n' && next_char == '#') {
            consume_char();
            scan_line_marker();
        }
    }
}

// Skip multi-line comment
static void skip_comment(void)
{
    consume_char(); // Consume '*'
    while (next_char != EOF) {
        if (next_char == '*') {
            consume_char();
            if (next_char == '/') {
                consume_char();
                return;
            }
        } else {
            consume_char();
        }
    }
    lex_error("unterminated comment");
}

//
// Process line markers:
//      # line_number "filename" [flag1 [flag2 ...]]
// Fields:
//  - line_number: The line number in the original source file
//                 that the following code corresponds to.
//  - filename: The name of the source file (in quotes) where
//              the code originated.
//  - flags (optional): Numeric flags that provide additional context.
//
// Common flags include:
//  1: Indicates the start of a new file
//     (e.g., after an #include).
//  2: Indicates a return to the previous file
//     (e.g., after finishing an included file).
//  3: Indicates the code is from a system header file
//     (e.g., <stdio.h>).
//  4: Indicates the code should be treated as an implicit
//     extern "C" block (relevant for C++).
//
static void scan_line_marker()
{
    // Skip whitespace after '#'
    while (isspace(next_char)) {
        // Handle newline or spaces
        if (next_char == '\n')
            return; // Empty # line (null directive)
        consume_char();
    }

    // Expect a number (line_number)
    if (!isdigit(next_char)) {
        return; // Not a line marker, just a # (null directive)
    }

    yyleng = 0;
    while (isdigit(next_char)) {
        consume_char();
    }
    yytext[yyleng] = '\0';
    int line_num   = atoi(yytext); // Convert to integer

    // Skip whitespace
    while (isspace(next_char) && next_char != '\n') {
        consume_char();
    }

    // An optional quoted filename
    if (next_char == '"') {
        yyleng = 0;
        scan_string();

        // Strip the quotes
        if (yyleng >= 2) {
            yytext[yyleng - 1] = '\0';
        }
        scanner_filename = srcloc_intern(yytext + 1);
    }

    // Skip optional flags and rest of the line
    while (next_char != EOF && next_char != '\n') {
        consume_char();
    }

    // The newline ending the marker steps the count to the line it names.
    scanner_lineno = line_num - 1;
}

// Scan identifier or keyword
static int scan_identifier(void)
{
    if (next_char == 'L' || next_char == 'u' || next_char == 'U') {
        consume_char();
        if (next_char == '"') {
            // String with L/u/U prefix.
            return scan_string();
        }
        if (next_char == '\'') {
            // Character with L/u/U prefix.
            return scan_char();
        }
        if (yytext[0] == 'u' && next_char == '8') {
            consume_char();
            if (next_char == '"') {
                // String with u8 prefix.
                return scan_string();
            }
            if (next_char == '\'') {
                // Character with u8 prefix.
                return scan_char();
            }
        }
    }
    while (isalnum(next_char) || next_char == '_' || next_char == '$') {
        consume_char();
    }
    int token = is_keyword(yytext);
    if (token) {
        return token;
    }
    return TOKEN_IDENTIFIER;
}

// Validate an integer-constant suffix: an optional 'u'/'U' and an optional
// 'l'/'L' or 'll'/'LL', in either order; the two letters of 'll'/'LL' must share
// case.  Rejects 'lL', 'Ll', 'LLL', 'lul', 'uu', a stray 'f', etc.
static int valid_int_suffix(const char *s)
{
    int have_u = 0, have_l = 0;
    while (*s) {
        char c = *s;
        if (c == 'u' || c == 'U') {
            if (have_u) {
                return 0;
            }
            have_u = 1;
            s++;
        } else if (c == 'l' || c == 'L') {
            if (have_l) {
                return 0;
            }
            have_l = 1;
            if (s[1] == c) {
                s += 2; // 'll' or 'LL' (same case)
            } else if (s[1] == 'l' || s[1] == 'L') {
                return 0; // mixed-case 'lL'/'Ll'
            } else {
                s++; // single 'l'/'L'
            }
        } else {
            return 0; // e.g. a stray 'f'
        }
    }
    return 1;
}

// Validate a floating-constant suffix: empty, a single 'f'/'F', or a single
// 'l'/'L' (long double).
static int valid_float_suffix(const char *s)
{
    if (s[0] == '\0') {
        return 1;
    }
    if (s[1] != '\0') {
        return 0;
    }
    char c = s[0];
    return c == 'f' || c == 'F' || c == 'l' || c == 'L';
}

// Scan number (integer or floating-point)
static int scan_number(void)
{
    int is_float = 0;

    // Handle hexadecimal prefix
    if (next_char == '0') {
        consume_char(); // '0'
        if (tolower(next_char) != 'x') {
            goto decimal;
        }
        consume_char(); // 'x' or 'X'
        while (isxdigit(next_char)) {
            consume_char();
        }
        if (next_char == '.') {
            is_float = 1;
            consume_char();
            while (isxdigit(next_char)) {
                consume_char();
            }
        }
        if (tolower(next_char) == 'p') {
            is_float = 1;
            consume_char();
            if (next_char == '+' || next_char == '-') {
                consume_char();
            }
            if (!isdigit(next_char)) {
                lex_error("missing exponent in numeric constant '%s'", yytext);
            }
            while (isdigit(next_char)) {
                consume_char();
            }
        }
    } else {
        // Decimal or octal
    decimal:
        while (isdigit(next_char)) {
            consume_char();
        }
        if (next_char == '.') {
            is_float = 1;
            consume_char();
            while (isdigit(next_char)) {
                consume_char();
            }
        }
        if (tolower(next_char) == 'e') {
            is_float = 1;
            consume_char();
            if (next_char == '+' || next_char == '-') {
                consume_char();
            }
            if (!isdigit(next_char)) {
                lex_error("missing exponent in numeric constant '%s'", yytext);
            }
            while (isdigit(next_char)) {
                consume_char();
            }
        }
    }

    // Handle suffixes
    int suffix_start = yyleng;
    while (tolower(next_char) == 'u' || tolower(next_char) == 'l' || tolower(next_char) == 'f') {
        consume_char();
    }

    // A numeric constant may not run straight into an identifier character or a
    // second '.': '1foo' is a single invalid token, not '1f' followed by 'oo',
    // and '1.0e10.0' is one malformed preprocessing number, not '1.0e10' '.' '0'.
    if (isalpha(next_char) || next_char == '_' || next_char == '.') {
        while (isalnum(next_char) || next_char == '_' || next_char == '.') {
            consume_char();
        }
        lex_error("invalid suffix on numeric constant '%s'", yytext);
        return is_float ? TOKEN_F_CONSTANT : TOKEN_I_CONSTANT;
    }

    // Validate the suffix combination itself: an integer accepts an optional
    // 'u'/'U' and an optional 'l'/'L' or 'll'/'LL' in either order; a float
    // accepts a single 'f'/'F' or 'l'/'L'.  Reject e.g. '0lL', '0LLL', '0lul'.
    const char *suffix = yytext + suffix_start;
    if (is_float ? !valid_float_suffix(suffix) : !valid_int_suffix(suffix)) {
        lex_error("invalid suffix on numeric constant '%s'", yytext);
    }

    return is_float ? TOKEN_F_CONSTANT : TOKEN_I_CONSTANT;
}

// Scan string literal
static int scan_string(void)
{
    if (next_char == 'L' || next_char == 'u' || next_char == 'U') {
        consume_char(); // Consume prefix
    }
    if (next_char != '"') {
        lex_error("expected string literal");
    }
    consume_char(); // Consume opening quote
    while (next_char != '"' && next_char != '\n' && next_char != EOF) {
        if (next_char == '\\') {
            consume_char();
            // Handle escape sequences
            if (next_char == '\'' || next_char == '"' || next_char == '?' || next_char == '\\' ||
                next_char == 'a' || next_char == 'b' || next_char == 'f' || next_char == 'n' ||
                next_char == 'r' || next_char == 't' || next_char == 'v') {
                consume_char();
            } else if (next_char >= '0' && next_char <= '7') {
                consume_char();
                if (next_char >= '0' && next_char <= '7') {
                    consume_char();
                    if (next_char >= '0' && next_char <= '7') {
                        consume_char();
                    }
                }
            } else if (next_char == 'x') {
                consume_char();
                while (isxdigit(next_char)) {
                    consume_char();
                }
            } else if (next_char == EOF || next_char == '\n') {
                // Backslash at end of line / input: let the unterminated-string
                // check below report it.
                break;
            } else {
                lex_error("invalid escape sequence '\\%c'", next_char);
            }
        } else {
            consume_char();
        }
    }
    if (next_char == '"') {
        consume_char(); // Consume closing quote
    } else {
        lex_error("unterminated string literal");
    }
    return TOKEN_STRING_LITERAL;
}

// Scan character literal
static int scan_char(void)
{
    if (next_char == 'L' || next_char == 'u' || next_char == 'U') {
        consume_char(); // Consume optional prefix
    }
    if (next_char != '\'') {
        lex_error("expected character literal");
    }
    consume_char(); // Consume opening quote
    int has_content = 0;
    while (next_char != '\'' && next_char != '\n' && next_char != EOF) {
        has_content = 1;
        if (next_char == '\\') {
            consume_char();
            // Handle escape sequences
            if (next_char == '\'' || next_char == '"' || next_char == '?' || next_char == '\\' ||
                next_char == 'a' || next_char == 'b' || next_char == 'f' || next_char == 'n' ||
                next_char == 'r' || next_char == 't' || next_char == 'v') {
                consume_char();
            } else if (next_char >= '0' && next_char <= '7') {
                consume_char();
                if (next_char >= '0' && next_char <= '7') {
                    consume_char();
                    if (next_char >= '0' && next_char <= '7') {
                        consume_char();
                    }
                }
            } else if (next_char == 'x') {
                consume_char();
                while (isxdigit(next_char)) {
                    consume_char();
                }
            } else {
                lex_error("invalid escape sequence '\\%c'", next_char);
            }
        } else {
            consume_char();
        }
    }
    if (next_char == '\'') {
        consume_char(); // Consume closing quote
        if (!has_content) {
            lex_error("empty character literal");
        }
    } else {
        lex_error("unterminated character literal");
    }
    return TOKEN_I_CONSTANT;
}

// Scan operators and punctuation
static int scan_operator(void)
{
    int c = next_char;
    consume_char();
    int c2 = next_char;

    // Multi-character operators
    if (c == '.' && c2 == '.') {
        consume_char();
        if (next_char == '.') {
            consume_char();
        } else {
            lex_error("expected '...'");
        }
        return TOKEN_ELLIPSIS;
    }
    if (c == '>' && c2 == '>') {
        consume_char();
        if (next_char == '=') {
            consume_char();
            return TOKEN_RIGHT_ASSIGN;
        }
        return TOKEN_RIGHT_OP;
    }
    if (c == '<' && c2 == '<') {
        consume_char();
        if (next_char == '=') {
            consume_char();
            return TOKEN_LEFT_ASSIGN;
        }
        return TOKEN_LEFT_OP;
    }
    if (c == '+' && c2 == '=') {
        consume_char();
        return TOKEN_ADD_ASSIGN;
    }
    if (c == '-' && c2 == '=') {
        consume_char();
        return TOKEN_SUB_ASSIGN;
    }
    if (c == '*' && c2 == '=') {
        consume_char();
        return TOKEN_MUL_ASSIGN;
    }
    if (c == '/' && c2 == '=') {
        consume_char();
        return TOKEN_DIV_ASSIGN;
    }
    if (c == '%' && c2 == '=') {
        consume_char();
        return TOKEN_MOD_ASSIGN;
    }
    if (c == '&' && c2 == '=') {
        consume_char();
        return TOKEN_AND_ASSIGN;
    }
    if (c == '^' && c2 == '=') {
        consume_char();
        return TOKEN_XOR_ASSIGN;
    }
    if (c == '|' && c2 == '=') {
        consume_char();
        return TOKEN_OR_ASSIGN;
    }
    if (c == '+' && c2 == '+') {
        consume_char();
        return TOKEN_INC_OP;
    }
    if (c == '-' && c2 == '-') {
        consume_char();
        return TOKEN_DEC_OP;
    }
    if (c == '-' && c2 == '>') {
        consume_char();
        return TOKEN_PTR_OP;
    }
    if (c == '&' && c2 == '&') {
        consume_char();
        return TOKEN_AND_OP;
    }
    if (c == '|' && c2 == '|') {
        consume_char();
        return TOKEN_OR_OP;
    }
    if (c == '<' && c2 == '=') {
        consume_char();
        return TOKEN_LE_OP;
    }
    if (c == '>' && c2 == '=') {
        consume_char();
        return TOKEN_GE_OP;
    }
    if (c == '=' && c2 == '=') {
        consume_char();
        return TOKEN_EQ_OP;
    }
    if (c == '!' && c2 == '=') {
        consume_char();
        return TOKEN_NE_OP;
    }

    // Single-character tokens
    switch (c) {
    case ';':
        return TOKEN_SEMICOLON;
    case '{':
        return TOKEN_LBRACE;
    case '}':
        return TOKEN_RBRACE;
    case ',':
        return TOKEN_COMMA;
    case ':':
        return TOKEN_COLON;
    case '=':
        return TOKEN_ASSIGN;
    case '(':
        return TOKEN_LPAREN;
    case ')':
        return TOKEN_RPAREN;
    case '[':
        return TOKEN_LBRACKET;
    case ']':
        return TOKEN_RBRACKET;
    case '.':
        return TOKEN_DOT;
    case '&':
        return TOKEN_AMPERSAND;
    case '!':
        return TOKEN_NOT;
    case '~':
        return TOKEN_TILDE;
    case '-':
        return TOKEN_MINUS;
    case '+':
        return TOKEN_PLUS;
    case '*':
        return TOKEN_STAR;
    case '/':
        return TOKEN_SLASH;
    case '%':
        return TOKEN_PERCENT;
    case '<':
        return TOKEN_LT;
    case '>':
        return TOKEN_GT;
    case '^':
        return TOKEN_CARET;
    case '|':
        return TOKEN_PIPE;
    case '?':
        return TOKEN_QUESTION;
    default:
        // Unknown character: not part of any C token outside a literal.
        if (isprint(c)) {
            lex_error("invalid character '%c'", c);
        } else {
            lex_error("invalid character '\\x%02x'", (unsigned char)c);
        }
        return TOKEN_UNKNOWN;
    }
}

// Get current lexeme
char *get_yytext(void)
{
    return yytext;
}

// Human-readable name for a token code, used in parser diagnostics.
const char *token_name(int token)
{
    switch (token) {
    case TOKEN_EOF:
        return "end of file";
    case TOKEN_UNKNOWN:
        return "unknown token";
    case TOKEN_AUTO:
        return "'auto'";
    case TOKEN_BREAK:
        return "'break'";
    case TOKEN_CASE:
        return "'case'";
    case TOKEN_CHAR:
        return "'char'";
    case TOKEN_CONST:
        return "'const'";
    case TOKEN_CONTINUE:
        return "'continue'";
    case TOKEN_DEFAULT:
        return "'default'";
    case TOKEN_DO:
        return "'do'";
    case TOKEN_DOUBLE:
        return "'double'";
    case TOKEN_ELSE:
        return "'else'";
    case TOKEN_ENUM:
        return "'enum'";
    case TOKEN_EXTERN:
        return "'extern'";
    case TOKEN_FLOAT:
        return "'float'";
    case TOKEN_FOR:
        return "'for'";
    case TOKEN_GOTO:
        return "'goto'";
    case TOKEN_IF:
        return "'if'";
    case TOKEN_INLINE:
        return "'inline'";
    case TOKEN_INT:
        return "'int'";
    case TOKEN_LONG:
        return "'long'";
    case TOKEN_REGISTER:
        return "'register'";
    case TOKEN_RESTRICT:
        return "'restrict'";
    case TOKEN_RETURN:
        return "'return'";
    case TOKEN_SHORT:
        return "'short'";
    case TOKEN_SIGNED:
        return "'signed'";
    case TOKEN_SIZEOF:
        return "'sizeof'";
    case TOKEN_STATIC:
        return "'static'";
    case TOKEN_STRUCT:
        return "'struct'";
    case TOKEN_SWITCH:
        return "'switch'";
    case TOKEN_TYPEDEF:
        return "'typedef'";
    case TOKEN_UNION:
        return "'union'";
    case TOKEN_UNSIGNED:
        return "'unsigned'";
    case TOKEN_VOID:
        return "'void'";
    case TOKEN_VOLATILE:
        return "'volatile'";
    case TOKEN_WHILE:
        return "'while'";
    case TOKEN_ALIGNAS:
        return "'_Alignas'";
    case TOKEN_ALIGNOF:
        return "'_Alignof'";
    case TOKEN_VA_CLASS:
        return "'__builtin_va_class'";
    case TOKEN_ATOMIC:
        return "'_Atomic'";
    case TOKEN_AWAIT:
        return "'_Await'";
    case TOKEN_BOOL:
        return "'_Bool'";
    case TOKEN_COMPLEX:
        return "'_Complex'";
    case TOKEN_CORO:
        return "'_Coro'";
    case TOKEN_CORO_FRAME:
        return "'_Coro_frame'";
    case TOKEN_CORO_PTR:
        return "'_Coro_ptr'";
    case TOKEN_DEFER:
        return "'_Defer'";
    case TOKEN_GENERIC:
        return "'_Generic'";
    case TOKEN_IMAGINARY:
        return "'_Imaginary'";
    case TOKEN_NORETURN:
        return "'_Noreturn'";
    case TOKEN_STATIC_ASSERT:
        return "'_Static_assert'";
    case TOKEN_THREAD_LOCAL:
        return "'_Thread_local'";
    case TOKEN_YIELD:
        return "'_Yield'";
    case TOKEN_CO_INIT:
        return "'__co_init'";
    case TOKEN_CO_ALLOCA:
        return "'__co_alloca'";
    case TOKEN_CO_RESUME:
        return "'__co_resume'";
    case TOKEN_CO_CANCEL:
        return "'__co_cancel'";
    case TOKEN_CO_DESTROY:
        return "'__co_destroy'";
    case TOKEN_CO_DONE:
        return "'__co_done'";
    case TOKEN_CO_VALUE:
        return "'__co_value'";
    case TOKEN_CO_RESULT:
        return "'__co_result'";
    case TOKEN_CO_SIZEOF:
        return "'__co_sizeof'";
    case TOKEN_CO_ALIGNOF:
        return "'__co_alignof'";
    case TOKEN_FUNC_NAME:
        return "'__func__'";
    case TOKEN_IDENTIFIER:
        return "identifier";
    case TOKEN_I_CONSTANT:
        return "integer constant";
    case TOKEN_F_CONSTANT:
        return "floating constant";
    case TOKEN_STRING_LITERAL:
        return "string literal";
    case TOKEN_ELLIPSIS:
        return "'...'";
    case TOKEN_RIGHT_ASSIGN:
        return "'>>='";
    case TOKEN_LEFT_ASSIGN:
        return "'<<='";
    case TOKEN_ADD_ASSIGN:
        return "'+='";
    case TOKEN_SUB_ASSIGN:
        return "'-='";
    case TOKEN_MUL_ASSIGN:
        return "'*='";
    case TOKEN_DIV_ASSIGN:
        return "'/='";
    case TOKEN_MOD_ASSIGN:
        return "'%='";
    case TOKEN_AND_ASSIGN:
        return "'&='";
    case TOKEN_XOR_ASSIGN:
        return "'^='";
    case TOKEN_OR_ASSIGN:
        return "'|='";
    case TOKEN_RIGHT_OP:
        return "'>>'";
    case TOKEN_LEFT_OP:
        return "'<<'";
    case TOKEN_INC_OP:
        return "'++'";
    case TOKEN_DEC_OP:
        return "'--'";
    case TOKEN_PTR_OP:
        return "'->'";
    case TOKEN_AND_OP:
        return "'&&'";
    case TOKEN_OR_OP:
        return "'||'";
    case TOKEN_LE_OP:
        return "'<='";
    case TOKEN_GE_OP:
        return "'>='";
    case TOKEN_EQ_OP:
        return "'=='";
    case TOKEN_NE_OP:
        return "'!='";
    case TOKEN_SEMICOLON:
        return "';'";
    case TOKEN_LBRACE:
        return "'{'";
    case TOKEN_RBRACE:
        return "'}'";
    case TOKEN_COMMA:
        return "','";
    case TOKEN_COLON:
        return "':'";
    case TOKEN_ASSIGN:
        return "'='";
    case TOKEN_LPAREN:
        return "'('";
    case TOKEN_RPAREN:
        return "')'";
    case TOKEN_LBRACKET:
        return "'['";
    case TOKEN_RBRACKET:
        return "']'";
    case TOKEN_DOT:
        return "'.'";
    case TOKEN_AMPERSAND:
        return "'&'";
    case TOKEN_NOT:
        return "'!'";
    case TOKEN_TILDE:
        return "'~'";
    case TOKEN_MINUS:
        return "'-'";
    case TOKEN_PLUS:
        return "'+'";
    case TOKEN_STAR:
        return "'*'";
    case TOKEN_SLASH:
        return "'/'";
    case TOKEN_PERCENT:
        return "'%'";
    case TOKEN_LT:
        return "'<'";
    case TOKEN_GT:
        return "'>'";
    case TOKEN_CARET:
        return "'^'";
    case TOKEN_PIPE:
        return "'|'";
    case TOKEN_QUESTION:
        return "'?'";
    case TOKEN_TYPEDEF_NAME:
        return "typedef name";
    case TOKEN_ENUMERATION_CONSTANT:
        return "enumeration constant";
    default:
        return "token";
    }
}
