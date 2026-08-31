/*
 * The driver: what runs the machine, and where anything that blocks happens.
 *
 * Copyright (c) 2026, Serge Vakulenko
 *
 * A guest disk exchange starts with an `увв' instruction and finishes with a
 * ГРП interrupt some time later; the guest waits for the interrupt either way.
 * Upstream nevertheless performed the host fread() *inside* the instruction and
 * deferred only the interrupt.  On Braam that read is a `co_await', and
 * cpu_one_inst() is a plain function -- making it a coroutine would put a
 * co_await in the instruction loop, and a co_await is a call and not a tail
 * call, so the native stack would grow until the process trapped.
 *
 * So the transfer is deferred with the interrupt, which is also the more
 * faithful model: the instruction posts a request and returns, the driver
 * performs it between two instructions, and then arms the completion event.
 * No instruction runs in between, so nothing the guest can observe moved.
 */
#ifndef BESM6_MACHINE_H
#define BESM6_MACHINE_H

/*
 * The transfer the machine has asked for and the driver has not performed yet.
 * One is enough: the guest cannot start a second exchange before the driver has
 * run, because the driver runs before the next instruction.
 */
typedef struct {
    UNIT *unit;               /* NULL when nothing is pending */
    t_stat (*serve)(UNIT *u); /* what performs the transfer */
    int32 delay;              /* model time until the completion event */
} IoRequest;

extern IoRequest io_request;

/* Asks for a transfer.  Called from inside an instruction, and returns at once. */
void io_post(UNIT *u, t_stat (*serve)(UNIT *u), int32 delay);

/* Performs it and arms the completion event.  Called from the driver between
 * two instructions; a no-op when nothing is pending. */
t_stat io_service(void);

#endif /* BESM6_MACHINE_H */
