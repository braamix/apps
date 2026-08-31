/*
 * The console buffers: one out, one in, per line.  Only the syscalls that
 * empty and fill them are the platform's.
 *
 * Copyright (c) 2026, Serge Vakulenko
 */
#include "besm6_defs.h"

int32_t con_stop_char = 005; /* ^E */

/* Enough for a screenful; a full buffer flushes early rather than dropping. */
#define CON_OUT 4096

/* A burst of typing between two drains.  Beyond it keys drop, which is what a
 * terminal does. */
#define CON_IN 256

static struct {
    char out[CON_OUT];
    int len;
    unsigned char in[CON_IN];
    int head, tail;
} line[CON_MAX];

void con_put(int con, int c)
{
    if (con < 0 || con >= CON_MAX)
        return;
    if (line[con].len == CON_OUT)
        con_flush();
    line[con].out[line[con].len++] = (char)c;
}

int con_pending(void)
{
    int i;

    for (i = 0; i < CON_MAX; i++)
        if (line[i].len)
            return 1;
    return 0;
}

int con_take(int con, const char **buf)
{
    int n = line[con].len;

    *buf          = line[con].out;
    line[con].len = 0;
    return n;
}

void con_feed(int con, int c)
{
    int next;

    if (con < 0 || con >= CON_MAX)
        return;
    next = (line[con].head + 1) % CON_IN;
    if (next == line[con].tail)
        return; /* full: drop, as a terminal does */
    line[con].in[line[con].head] = (unsigned char)c;
    line[con].head               = next;
}

int con_get(int con)
{
    int c;

    if (con < 0 || con >= CON_MAX)
        return -1;
    if (line[con].tail == line[con].head)
        return -1;
    c              = line[con].in[line[con].tail];
    line[con].tail = (line[con].tail + 1) % CON_IN;
    return c;
}
