#ifndef SCANNER_H
#define SCANNER_H

#include "srcloc.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

enum {
    TOKEN_EOF,                  // End of file reached
    TOKEN_UNKNOWN,              // Unrecognized symbol
    TOKEN_AUTO,                 // auto
    TOKEN_BREAK,                // break
    TOKEN_CASE,                 // case
    TOKEN_CHAR,                 // char
    TOKEN_CONST,                // const
    TOKEN_CONTINUE,             // continue
    TOKEN_DEFAULT,              // default
    TOKEN_DO,                   // do
    TOKEN_DOUBLE,               // double
    TOKEN_ELSE,                 // else
    TOKEN_ENUM,                 // enum
    TOKEN_EXTERN,               // extern
    TOKEN_FLOAT,                // float
    TOKEN_FOR,                  // for
    TOKEN_GOTO,                 // goto
    TOKEN_IF,                   // if
    TOKEN_INLINE,               // inline
    TOKEN_INT,                  // int
    TOKEN_LONG,                 // long
    TOKEN_REGISTER,             // register
    TOKEN_RESTRICT,             // restrict
    TOKEN_RETURN,               // return
    TOKEN_SHORT,                // short
    TOKEN_SIGNED,               // signed
    TOKEN_SIZEOF,               // sizeof
    TOKEN_STATIC,               // static
    TOKEN_STRUCT,               // struct
    TOKEN_SWITCH,               // switch
    TOKEN_TYPEDEF,              // typedef
    TOKEN_UNION,                // union
    TOKEN_UNSIGNED,             // unsigned
    TOKEN_VOID,                 // void
    TOKEN_VOLATILE,             // volatile
    TOKEN_WHILE,                // while
    TOKEN_ALIGNAS,              // _Alignas
    TOKEN_ALIGNOF,              // _Alignof
    TOKEN_VA_CLASS,             // __builtin_va_class
    TOKEN_ATOMIC,               // _Atomic
    TOKEN_AWAIT,                // _Await (vcc extension)
    TOKEN_BOOL,                 // _Bool
    TOKEN_COMPLEX,              // _Complex
    TOKEN_CORO,                 // _Coro (vcc extension)
    TOKEN_CORO_FRAME,           // _Coro_frame (vcc extension)
    TOKEN_CORO_PTR,             // _Coro_ptr (vcc extension)
    TOKEN_DEFER,                // _Defer (vcc extension)
    TOKEN_GENERIC,              // _Generic
    TOKEN_IMAGINARY,            // _Imaginary
    TOKEN_NORETURN,             // _Noreturn
    TOKEN_STATIC_ASSERT,        // _Static_assert
    TOKEN_THREAD_LOCAL,         // _Thread_local
    TOKEN_YIELD,                // _Yield (vcc extension)
    TOKEN_CO_INIT,              // __co_init: the coroutine operations, in CoOp order
    TOKEN_CO_ALLOCA,            // __co_alloca
    TOKEN_CO_RESUME,            // __co_resume
    TOKEN_CO_CANCEL,            // __co_cancel
    TOKEN_CO_DESTROY,           // __co_destroy
    TOKEN_CO_DONE,              // __co_done
    TOKEN_CO_VALUE,             // __co_value
    TOKEN_CO_RESULT,            // __co_result
    TOKEN_CO_SIZEOF,            // __co_sizeof
    TOKEN_CO_ALIGNOF,           // __co_alignof
    TOKEN_FUNC_NAME,            // __func__
    TOKEN_IDENTIFIER,           // identifier (e.g., variable, _abc123)
    TOKEN_I_CONSTANT,           // integer/character constant (e.g., 123, 0x1a, 'a', L'\n')
    TOKEN_F_CONSTANT,           // floating-point constant (e.g., 3.14, 1e-10, 0x1.2p3)
    TOKEN_STRING_LITERAL,       // string literal (e.g., "hello", L"world")
    TOKEN_ELLIPSIS,             // ...
    TOKEN_RIGHT_ASSIGN,         // >>=
    TOKEN_LEFT_ASSIGN,          // <<=
    TOKEN_ADD_ASSIGN,           // +=
    TOKEN_SUB_ASSIGN,           // -=
    TOKEN_MUL_ASSIGN,           // *=
    TOKEN_DIV_ASSIGN,           // /=
    TOKEN_MOD_ASSIGN,           // %=
    TOKEN_AND_ASSIGN,           // &=
    TOKEN_XOR_ASSIGN,           // ^=
    TOKEN_OR_ASSIGN,            // |=
    TOKEN_RIGHT_OP,             // >>
    TOKEN_LEFT_OP,              // <<
    TOKEN_INC_OP,               // ++
    TOKEN_DEC_OP,               // --
    TOKEN_PTR_OP,               // ->
    TOKEN_AND_OP,               // &&
    TOKEN_OR_OP,                // ||
    TOKEN_LE_OP,                // <=
    TOKEN_GE_OP,                // >=
    TOKEN_EQ_OP,                // ==
    TOKEN_NE_OP,                // !=
    TOKEN_SEMICOLON,            // ;
    TOKEN_LBRACE,               // {
    TOKEN_RBRACE,               // }
    TOKEN_COMMA,                // ,
    TOKEN_COLON,                // :
    TOKEN_ASSIGN,               // =
    TOKEN_LPAREN,               // (
    TOKEN_RPAREN,               // )
    TOKEN_LBRACKET,             // [
    TOKEN_RBRACKET,             // ]
    TOKEN_DOT,                  // .
    TOKEN_AMPERSAND,            // &
    TOKEN_NOT,                  // !
    TOKEN_TILDE,                // ~
    TOKEN_MINUS,                // -
    TOKEN_PLUS,                 // +
    TOKEN_STAR,                 // *
    TOKEN_SLASH,                // /
    TOKEN_PERCENT,              // %
    TOKEN_LT,                   // <
    TOKEN_GT,                   // >
    TOKEN_CARET,                // ^
    TOKEN_PIPE,                 // |
    TOKEN_QUESTION,             // ?
    TOKEN_TYPEDEF_NAME,         // previously defined typedef
    TOKEN_ENUMERATION_CONSTANT, // previously defined enumerator constant
};

void init_scanner(FILE *input);
int yylex(void);

// Get current lexeme
char *get_yytext(void);

// Human-readable name for a token code (e.g. "';'", "'return'", "identifier").
const char *token_name(int token);

// Current location: the file and line of the lookahead character.
extern int scanner_lineno;
extern const char *scanner_filename;

// Where the last token returned by yylex() starts.
extern SrcLoc scanner_token_loc;

// Name the input for locations, until a line marker names another file.
// Taken by every init_scanner() after it.
void scanner_set_input_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif // SCANNER_H
