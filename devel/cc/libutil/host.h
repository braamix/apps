//
// C with coroutines on Braam, plain C on the host (Plan.md, decision 1). A call
// that waits is a coroutine on Braam and is awaited; on the host coro(Y) and
// await are nothing, and the same call is C's own.
//
#pragma once

#ifdef __braam__
#include <braam.h> // braam_call, and <coro.h>
#elif defined(__vcc_coroutines__)
#include <coro.h>
#else
#define coro(Y)
#define await

// Braam's park-once, so ^C reaches a long run; on the host there is nothing to do.
static inline int braam_yield(void)
{
    return 0;
}
#endif

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

//
// Debug traces, behind the passes' debug flags: printf, or a call of a printer, on the
// host. On Braam, where those are coroutines and the traces are in plain functions,
// nothing.
//
#ifdef __braam__
#define trace_printf(...) ((void)0)
#define trace_call(c)     ((void)0)
#else
#define trace_printf  printf
#define trace_call(c) (c)
#endif

//
// Diagnostics, from plain functions: written to stderr at once on the host. On Braam
// they are held in memory, written out by diag_flush() and posted by exit().
//
void diag_printf(const char *fmt, ...);
void diag_vprintf(const char *fmt, va_list ap);
coro(braam_call *) int diag_flush(void);

//
// The whole of a file, in memory from malloc with a NUL after its *len bytes; NULL
// with errno set when it cannot be read.
//
coro(braam_call *) char *read_file(const char *path, size_t *len);
