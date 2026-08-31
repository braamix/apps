/*
 * The terminal lines, from the machine's side.
 *
 * Copyright (c) 2026, Serge Vakulenko
 *
 * Nothing here blocks, which is what lets an instruction reach it.  Upstream's
 * non-blocking read(0) and its write(1) per character could be reached for the
 * same reason; on Braam a read and a write are coroutines and cannot.  So
 * output gathers in a buffer the driver empties, and input waits in a ring a
 * task of its own fills.
 */
#ifndef BESM6_CONSOLE_H
#define BESM6_CONSOLE_H

/* Which console carries a line.  One so far; the second screen adds another. */
enum {
    CON_NONE   = -1,
    CON_SCREEN = 0,
    CON_MAX    = 1,
};

/* The byte that stops the machine: ^E.  The host gets it as SIGINT, through
 * termios VINTR; Braam has neither, and con_get() recognises it. */
extern int32 con_stop_char;

/* con_raw() takes the keyboard for a run; con_cooked() gives it back. */
t_stat con_init(void);
t_stat con_raw(void);
void con_cooked(void);

/* One byte out.  Nothing leaves until con_flush(). */
void con_put(int con, int c);

/* Whether anything waits, so the driver can skip the call. */
int con_pending(void);

/* The driver's, never an instruction's: on Braam the write happens here. */
void con_flush(void);

/* The next byte typed, or -1. */
int con_get(int con);

#endif /* BESM6_CONSOLE_H */
