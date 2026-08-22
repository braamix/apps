// The porting layer: printf, getchar, rand and time, none of which exist here.
//
// Output is buffered. A write is an asynchronous syscall and speak(), rspeak()
// and the verb handlers are plain functions, which cannot co_await one; so they
// append here and the loop flushes once a turn.
#pragma once

#include "kernel/str.h"
#include "kernel/string.h"
#include "kernel/task.h"
#include "kernel/types.h"

// printf, putchar, puts. The conversions are the ones upstream uses: %d %u %s
// %c %%.
void adv_printf(const char *fmt, ...);
void adv_putc(char c);
void adv_puts(Str s);

// Everything written since the last flush, in one write_all.
Task<void> adv_flush(void);

bool adv_write_failed(void);

// How a line ended. Interrupt is ^C, which upstream caught with SIGINT.
enum class AdvEnd : u8 {
    Enter,
    Interrupt,
    Eof,
};

// Claims the keyboard if there is a console, and picks the reader to match.
Task<void> adv_input_init(void);

// Hands the keyboard back.
Task<void> adv_input_done(void);

// One line, without its newline.
Task<AdvEnd> adv_readline(String &out);

// Seeds ran() from the wall clock.
Task<void> adv_seed(void);

// Reads the wall clock datime() then reports.
Task<void> adv_clock(void);
