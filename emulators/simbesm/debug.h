/*
 * The one output path: the operator's console and the trace file.
 *
 * Copyright (c) 2026, Serge Vakulenko
 *
 * Upstream had three ways out -- printf() to the console, `FILE *sim_deb' for
 * the trace, and a FILE * threaded through the disassembler and the dumper.
 * None of them exists on Braam: a write there is a coroutine, and <stdio.h>'s
 * blocking half is declared unavailable by the port kit.  So a destination is a
 * Sink -- a callback and its context -- and the port supplies the two that
 * matter, while the host build supplies one over FILE *.
 *
 * A Sink formats with vsnprintf(), which is pure computation and exists in both
 * worlds; nothing here calls anything that can block.
 */
#ifndef BESM6_DEBUG_H
#define BESM6_DEBUG_H

#include <stdarg.h>

typedef struct Sink {
    /* `n' bytes, not NUL-terminated.  Never called with n == 0. */
    void (*put)(struct Sink *s, const char *buf, int n);
} Sink;

/* The trace file, or NULL when none is open.  Every trace site is a branch on
 * this pointer, which is what it was when it was a FILE *. */
extern Sink *sim_deb;

/* The operator's console: what upstream printed with printf().  Never NULL. */
extern Sink *sim_con;

void sink_printf(Sink *s, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void sink_vprintf(Sink *s, const char *fmt, va_list args);
void sink_write(Sink *s, const char *buf, int n);
void sink_puts(Sink *s, const char *str);

/* Binds sim_con.  First thing an entry point does, so that a failure during
 * startup has somewhere to be reported.  Not to be confused with con_init()
 * in console.h, which is the machine's terminal lines rather than the
 * operator's messages. */
void sink_init(void);

/* BESM6_DEBUG names the trace file ("-" is stderr) and BESM6_TRACE the devices
 * to trace.  Called once at startup; the close flushes and releases the file. */
void sim_debug_from_env(void);
void sim_debug_close(void);

#endif /* BESM6_DEBUG_H */
