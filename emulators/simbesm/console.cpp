/*
 * The host build's consoles: stdin and stdout in raw mode.  The Braam build
 * replaces this file, where a console is a screen.
 *
 * Copyright (c) 2026, Serge Vakulenko
 */
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>

#include "besm6_defs.h"

int32 con_stop_char = 005; /* ^E */

/* Enough for a screenful; a full buffer flushes early rather than dropping. */
#define CON_BUF 4096

static struct {
    char buf[CON_BUF];
    int len;
} out[CON_MAX];

static struct termios cmdtty, runtty;
static int cmdfl, runfl; /* TTY flags */

static t_bool con_isatty(void)
{
    static int answer = -1;

    if (answer == -1)
        answer = isatty(0);
    return (t_bool)(answer != 0);
}

/* ------------------------------------------------------------------ output */

void con_flush(void)
{
    int i;

    for (i = 0; i < CON_MAX; i++) {
        if (out[i].len == 0)
            continue;
        /* Console 0 is stdout; there is nowhere else to write on the host. */
        if (i == CON_SCREEN && write(1, out[i].buf, out[i].len) != out[i].len) {
            /* nothing useful to do about a full or closed stdout */
        }
        out[i].len = 0;
    }
}

int con_pending(void)
{
    int i;

    for (i = 0; i < CON_MAX; i++)
        if (out[i].len)
            return 1;
    return 0;
}

void con_put(int con, int c)
{
    if (con < 0 || con >= CON_MAX)
        return;
    if (out[con].len == CON_BUF)
        con_flush();
    out[con].buf[out[con].len++] = (char)c;
}

/* ------------------------------------------------------------------- input */

int con_get(int con)
{
    unsigned char buf[1];

    if (con != CON_SCREEN) /* the host has one console */
        return -1;
    if (!con_isatty())
        return -1;
    if (read(0, buf, 1) != 1)
        return -1;
    return buf[0];
}

/* ------------------------------------------------------------------- modes */

t_stat con_init(void)
{
    cmdfl = fcntl(0, F_GETFL, 0); /* get old flags  and status */
    /*
     * make sure systems with broken termios (that don't honor
     * VMIN=0 and VTIME=0) actually implement non blocking reads.
     * This will have no negative effect on other systems since
     * this is turned on and off depending on whether simulation
     * is running or not.
     */
    runfl = cmdfl | O_NONBLOCK;
    if (!con_isatty()) /* skip if !tty */
        return SCPE_OK;
    if (tcgetattr(0, &cmdtty) < 0) /* get old flags */
        return SCPE_TTIERR;
    runtty              = cmdtty;
    runtty.c_lflag      = runtty.c_lflag & ~(ECHO | ICANON); /* no echo or edit */
    runtty.c_oflag      = runtty.c_oflag & ~OPOST;           /* no output edit */
    runtty.c_iflag      = runtty.c_iflag & ~ICRNL;           /* no cr conversion */
    runtty.c_iflag      = runtty.c_iflag & ~IGNCR;           /* don't ignore cr */
    runtty.c_iflag      = runtty.c_iflag & ~IXANY;           /* don't restart after stop */
    runtty.c_iflag      = runtty.c_iflag & ~IMAXBEL; /* don't ring bell on input queue full */
    runtty.c_lflag      = runtty.c_lflag & ~PENDIN;  /* don't retype pending input (state) */
    runtty.c_lflag      = runtty.c_lflag | ECHOK;    /* echo NL after line kill */
    runtty.c_cc[VINTR]  = con_stop_char;             /* interrupt */
    runtty.c_cc[VQUIT]  = 0;                         /* no quit */
    runtty.c_cc[VERASE] = 0;
    runtty.c_cc[VKILL]  = 0;
    runtty.c_cc[VEOF]   = 0;
    runtty.c_cc[VEOL]   = 0;
    runtty.c_cc[VSTART] = 0; /* no host sync */
    runtty.c_cc[VSUSP]  = 0;
    runtty.c_cc[VSTOP]  = 0;
#if defined(VREPRINT)
    runtty.c_cc[VREPRINT] = 0; /* no specials */
#endif
#if defined(VDISCARD)
    runtty.c_cc[VDISCARD] = 0;
#endif
#if defined(VWERASE)
    runtty.c_cc[VWERASE] = 0;
#endif
#if defined(VLNEXT)
    runtty.c_cc[VLNEXT] = 0;
#endif
    runtty.c_cc[VMIN]  = 0; /* no waiting */
    runtty.c_cc[VTIME] = 0;
#if defined(VDSUSP)
    runtty.c_cc[VDSUSP] = 0;
#endif
#if defined(VSTATUS)
    runtty.c_cc[VSTATUS] = 0;
#endif
    return SCPE_OK;
}

/* ^E reaches the host as SIGINT, because termios VINTR takes it before a read
 * can see it.  Braam has neither, and recognises con_stop_char in con_get(). */
static void int_handler(int sig)
{
    (void)sig;
    stop_cpu     = TRUE;
    sim_interval = 0;
}

t_stat con_raw(void)
{
    signal(SIGINT, int_handler);
    signal(SIGTERM, int_handler);
    if (!con_isatty()) /* skip if !tty */
        return SCPE_OK;
    (void)fcntl(0, F_SETFL, runfl);     /* non-block mode */
    runtty.c_cc[VINTR] = con_stop_char; /* in case changed */
    if (tcsetattr(0, TCSAFLUSH, &runtty) < 0)
        return SCPE_TTIERR;
    return SCPE_OK;
}

void con_cooked(void)
{
    con_flush();
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    if (!con_isatty()) /* skip if !tty */
        return;
    (void)fcntl(0, F_SETFL, cmdfl); /* block mode */
    (void)tcsetattr(0, TCSAFLUSH, &cmdtty);
}
