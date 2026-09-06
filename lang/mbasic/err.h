// The longjmp that is not, and the blocking read that is not.
//
// Upstream's ERROR reset the 6502 stack pointer and jumped to READY, unwinding
// from wherever it was raised (04-interpreter-loop.md). There is no setjmp
// here and none can be written -- wasm's call stack does not live in linear
// memory -- and exceptions are off. So error() records rather than unwinds,
// and the unwinding is done one frame at a time by the macros below. This is
// editors/vi/ex_err.h, which replaced the same mechanism.
//
// The one flag carries a suspension too, because a blocking read has to unwind
// for exactly the same reason and to exactly the same place: only step() looks
// at which it was.
//
//   ERR/SUSPEND  is the raiser. A call plus a return, and the compiler catches
//                a wrong one: `return;` in a value-returning function and
//                `return v;` in a void one are both ill-formed.
//   CHK          is every call site of something that can raise, where going on
//                would commit a change.
//
// A missed CHK must be inert rather than corrupting, so the leaves are
// poisoned: chrget/chrgot answer 0, which is a statement terminator, so every
// scan loop ends; outdo drops the character; ptrget answers a scratch VarRef
// and never a live variable. An unguarded loop runs out of text and stops.
#pragma once

#include "kernel/str.h"
#include "kernel/types.h"

// Why the interpreter stopped. Halt::Suspend rides beside a Reason in want_.
enum class Halt : u8 {
    None,
    Error,   // ERROR: print "?<msg> ERROR [IN n]", then READY
    Break,   // STOP, END or ^C: ERRFIN entered with BRKTXT
    Ready,   // stop without a message: END, or the program ran off the end
    Suspend, // the driver has something to do; want_ says what
    Quit,    // end of input: leave
};

// The error codes are the SYMBOLS, never the numbers. 04-interpreter-loop.md
// §4.3: the numeric value of every code differs between LNGERR=0 and LNGERR=1,
// because they are byte offsets into two unrelated tables. LNGERR=1 here.
enum ErrCode : u8 {
    ERRNF,  // NEXT WITHOUT FOR
    ERRSN,  // SYNTAX
    ERRRG,  // RETURN WITHOUT GOSUB
    ERROD,  // OUT OF DATA
    ERRFC,  // ILLEGAL QUANTITY
    ERROV,  // OVERFLOW
    ERROM,  // OUT OF MEMORY
    ERRUS,  // UNDEF'D STATEMENT
    ERRBS,  // BAD SUBSCRIPT
    ERRDD,  // REDIM'D ARRAY
    ERRDV0, // DIVISION BY ZERO
    ERRID,  // ILLEGAL DIRECT
    ERRTM,  // TYPE MISMATCH
    ERRLS,  // STRING TOO LONG
    ERRFD,  // FILE DATA -- EXTIO only
    ERRST,  // FORMULA TOO COMPLEX
    ERRCN,  // CAN'T CONTINUE
    ERRUF,  // UNDEF'D FUNCTION
    ERR_COUNT,
};

// ERRTAB, spelled out (tables.cpp).
extern const Str ERRTAB[ERR_COUNT];

#define CHK            \
    do {               \
        if (pending()) \
            return;    \
    } while (0)

#define CHKV(v)         \
    do {                \
        if (pending())  \
            return (v); \
    } while (0)

// Inside newstt()'s loop, where the raise lands at the top rather than out of
// the function -- which is where upstream's JMP ERROR put it.
#define CHKC           \
    {                  \
        if (pending()) \
            break;     \
    }

#define ERR(code)    \
    do {             \
        error(code); \
        return;      \
    } while (0)

#define ERRV(v, code) \
    do {              \
        error(code);  \
        return (v);   \
    } while (0)

#define SUSPEND(call) \
    do {              \
        call;         \
        return;       \
    } while (0)
