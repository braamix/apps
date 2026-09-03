// The terminal lines, from the machine's side.
//
// Copyright (c) 2026, Serge Vakulenko
//
// Nothing here blocks, which is what lets an instruction reach it.  Upstream's
// non-blocking read(0) and its write(1) per character could be reached for the
// same reason; on Braam a read and a write are coroutines and cannot.  So
// output gathers in a buffer the driver drains, and input waits in a ring the
// platform fills.
#ifndef BESM6_CONSOLE_H
#define BESM6_CONSOLE_H

#include "types.h"

// Which console carries a line: the program's own terminal, or a second screen
// it opened (Sys::TermOpen).  The host build has only the first.
enum {
    CON_NONE    = -1,
    CON_SCREEN  = 0,
    CON_SCREEN2 = 1,
    CON_MAX     = 2,
};

// ---------------------------------------------------------- the machine's side

// One byte out.  Nothing leaves until the driver drains it.
void con_put(int con, int c);

// The next byte typed, or -1.
int con_get(int con);

// ---------------------------------------------------------- the driver's side

// Whether anything waits, so the driver can skip the call.
int con_pending(void);

// What has gathered for one console, and forgets it: the platform writes it.
// Zero when there is nothing.
int con_take(int con, const char **buf);

// A byte typed, from the platform.  Dropped when the ring is full, which is
// what a terminal does.
void con_feed(int con, int c);

// A whole key's bytes, or none of them: half an escape sequence is worse than
// no key at all.  Zero when it does not fit.
int con_feed_all(int con, const char *buf, int len);

// Whether a second screen was opened, which decides what tty26 is attached to.
// The platform's: only Braam has one.
int con_second(void);

// The platform's: the console's terminal, and the keyboard for a run.
status_t con_init(void);
status_t con_raw(void);
void con_cooked(void);

// Sends what gathered.  The platform's, because that is where the write is.
void con_flush(void);

#endif // BESM6_CONSOLE_H
