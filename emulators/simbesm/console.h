/*
 * The terminal lines, from the machine's side.
 *
 * Copyright (c) 2026, Serge Vakulenko
 *
 * Two Consul lines carry a session: `tty25' is the console and `tty26' a second
 * one.  Neither call below blocks, which is what lets both be reached from
 * inside an instruction -- upstream's non-blocking read(0) and its write(1) per
 * character were reachable from there for the same reason, and on Braam a read
 * and a write are coroutines, so they cannot be.
 *
 * So output gathers in a buffer and leaves in con_flush(), which the driver
 * calls between two instructions; and input waits in a ring that a task of its
 * own fills, so con_get() only looks.
 */
#ifndef BESM6_CONSOLE_H
#define BESM6_CONSOLE_H

/* Which console carries a line.  There is one so far; the second screen adds
 * CON_SCREEN2. */
enum {
    CON_NONE   = -1,
    CON_SCREEN = 0,
    CON_MAX    = 1,
};

/* The byte that stops the machine: ^E.  Upstream let termios turn it into
 * SIGINT before sim_poll_kbd() could see it, which is why nothing tested for
 * it here; Braam has neither termios nor a signal for it, so the console layer
 * is where it has to be recognised. */
extern int32 con_stop_char;

/* Binds the consoles.  con_raw() takes the keyboard for the duration of a run
 * and con_cooked() gives it back. */
t_stat con_init(void);
t_stat con_raw(void);
void con_cooked(void);

/* One byte to a console.  Buffered; nothing leaves until con_flush(). */
void con_put(int con, int c);

/* Whether anything is waiting to leave, so the driver can skip the call. */
int con_pending(void);

/* Sends what con_put() gathered.  Called from the driver, never from inside an
 * instruction: on Braam this is where the write happens. */
void con_flush(void);

/* The next byte typed at a console, or -1 when nothing is waiting. */
int con_get(int con);

#endif /* BESM6_CONSOLE_H */
