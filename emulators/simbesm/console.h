/*
 * The terminal lines, from the machine's side.
 *
 * Copyright (c) 2026, Serge Vakulenko
 *
 * Nothing here blocks, which is what lets an instruction reach it.  Upstream's
 * non-blocking read(0) and its write(1) per character could be reached for the
 * same reason; on Braam a read and a write are coroutines and cannot.  So
 * output gathers in a buffer the driver drains, and input waits in a ring the
 * platform fills.
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
 * termios VINTR; Braam has neither, and its key task recognises it. */
extern int32 con_stop_char;

/* ---------------------------------------------------------- the machine's side */

/* One byte out.  Nothing leaves until the driver drains it. */
void con_put(int con, int c);

/* The next byte typed, or -1. */
int con_get(int con);

/* ---------------------------------------------------------- the driver's side */

/* Whether anything waits, so the driver can skip the call. */
int con_pending(void);

/* What has gathered for one console, and forgets it: the platform writes it.
 * Zero when there is nothing. */
int con_take(int con, const char **buf);

/* A byte typed, from the platform.  Dropped when the ring is full, which is
 * what a terminal does. */
void con_feed(int con, int c);

/* The platform's: the console's terminal, and the keyboard for a run. */
t_stat con_init(void);
t_stat con_raw(void);
void con_cooked(void);

/* Sends what gathered.  The platform's, because that is where the write is. */
void con_flush(void);

#endif /* BESM6_CONSOLE_H */
