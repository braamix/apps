// The one output path: the operator's console and the trace file.
//
// Copyright (c) 2026, Serge Vakulenko
//
// Upstream had three -- printf(), a FILE *sim_deb, and a FILE * threaded
// through the disassembler and the dumper.  None exists on Braam, where a write
// is a coroutine, so a destination is a callback.  Formatting is vsnprintf(),
// which is pure computation; nothing here can block.
#ifndef BESM6_DEBUG_H
#define BESM6_DEBUG_H

#include <stdarg.h>

typedef struct Sink {
    // `n' bytes, not NUL-terminated.  Never called with n == 0.
    void (*put)(struct Sink *s, const char *buf, int n);
} Sink;

// The trace file, or NULL.  Every trace site branches on it, as before.
extern Sink *sim_deb;

// The operator's console: what upstream printed with printf().  Never NULL.
extern Sink *sim_con;

void sink_printf(Sink *s, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void sink_vprintf(Sink *s, const char *fmt, va_list args);
void sink_write(Sink *s, const char *buf, int n);
void sink_puts(Sink *s, const char *str);

// Binds sim_con, first, so a startup failure has somewhere to go.  Not
// con_init(), which is the machine's terminal lines.
void sink_init(void);

// BESM6_DEBUG names the trace file ("-" is stderr), BESM6_TRACE the devices.
void sim_debug_from_env(void);
void sim_debug_close(void);

// The trace file itself: the platform's, like the image and the console.
Sink *deb_file_open(const char *path);
void deb_file_close(void);

#endif // BESM6_DEBUG_H
