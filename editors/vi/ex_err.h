/*
 * The longjmp that is not.
 *
 * Upstream carried two jmp_bufs: resetlab, thrown to from the ex command
 * loop's error(), and vreslab, thrown to from a CATCH inside open or visual.
 * error1() picked between them. There is no setjmp here and none can be
 * written — wasm's call stack does not live in linear memory — and exceptions
 * are off.
 *
 * So error() records rather than unwinds, and the unwinding is done one frame
 * at a time by the macros below. Two halves:
 *
 *   THROW*  is the call site of error(). It is a call plus a return, and the
 *           compiler catches a wrong one: `return;` in a value-returning
 *           function and `return v;` in a void one are both ill-formed.
 *   CHK*    is every call site of something that can throw, where continuing
 *           would commit a change.
 *
 * A missed CHK must be inert rather than corrupting, so a dozen leaf routines
 * are poisoned while ex_thrown: the input routines answer EOF or ESCAPE,
 * putline() appends nothing and answers a sentinel, putchar() drops. An
 * unguarded loop therefore runs out of input and stops instead of editing.
 */
#pragma once

EXTERN int ex_thrown;        /* an error is pending */
EXTERN exbool ex_thrown_msg; /* and it printed a message */
EXTERN exbool ex_quitting;   /* exit() was called; unwind and go */
EXTERN int ex_status;        /* what it asked to exit with */

#define THROW(call) \
    do {            \
        call;       \
        return;     \
    } while (0)
#define THROWV(v, call) \
    do {                \
        call;           \
        return (v);     \
    } while (0)
#define COTHROW(call) \
    do {              \
        call;         \
        co_return;    \
    } while (0)
#define COTHROWV(v, call) \
    do {                  \
        call;             \
        co_return (v);    \
    } while (0)

/*
 * Inside commands(), where the throw lands at the top of the loop rather than
 * out of the function -- which is exactly where upstream's longjmp(resetlab)
 * put it. A co_return there would end the editor instead of the command.
 */
#define THROWC(call) \
    {                \
        call;        \
        continue;    \
    }

#define CHK            \
    do {               \
        if (ex_thrown) \
            return;    \
    } while (0)
#define CHKV(v)         \
    do {                \
        if (ex_thrown)  \
            return (v); \
    } while (0)
#define COCHK          \
    do {               \
        if (ex_thrown) \
            co_return; \
    } while (0)
#define COCHKV(v)          \
    do {                   \
        if (ex_thrown)     \
            co_return (v); \
    } while (0)

/*
 * Upstream's CATCH/ONERR/ENDCATCH bracketed a block whose errors came back
 * here rather than to the command loop. The block is now a function of its
 * own and the handler an if, because nothing unwinds by itself.
 */
#define CATCHIT(call) (vcatch = 1, call, vcatch = 0, excatch())
