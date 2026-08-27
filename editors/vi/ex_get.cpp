/* Copyright (c) 1980 Regents of the University of California */
#include "ex.h"
#include "ex_vis.h"
#include "ex_screen.h"

/*
 * Input routines for command mode.
 * Since we translate the end of reads into the implied ^D's
 * we have different flavors of routines which do/don't return such.
 */
static exbool junkbs;
short lastc = '\n';

void ignchar(void)
{
    ignore(getchar());
}

int getchar(void)
{
    int c;

    do
        c = getcd();
    while (!globp && c == CTRL('d'));
    return (c);
}

int getcd(void)
{
    int c;

again:
    c = getach();
    if (c == EOF)
        return (c);
    c &= TRIM;
    if (!inopen)
        if (!globp && c == CTRL('d'))
            setlastchar('\n');
        else if (junk(c)) {
            checkjunk(c);
            goto again;
        }
    return (c);
}

int peekchar(void)
{
    if (peekc == 0)
        peekc = getchar();
    return (peekc);
}

int peekcd(void)
{
    if (peekc == 0)
        peekc = getcd();
    return (peekc);
}

int getach(void)
{
    int c;
    static char incurs[128];

    c = peekc;
    if (c != 0) {
        peekc = 0;
        return (c);
    }
    if (globp) {
        if (*globp)
            return (*globp++);
        globp = 0;
        return (lastc = EOF);
    }
top:
    if (input) {
        if (c = *input++) {
            if (c &= TRIM)
                return (lastc = c);
            goto top;
        }
        input = 0;
    }
    /*
     * Upstream read here, and that is the one thing this cannot do: a read
     * is a syscall, a syscall must be awaited, and this is called from the
     * bottom of the address parser. ex_readline() below fills the buffer
     * instead, from the three places that can await -- the top of the
     * command loop, gettty(), and a substitute's confirmation.
     *
     * Running dry is therefore end of input, and that is safe because a
     * whole line is always buffered before any of it is parsed: a command
     * stops at its newline and never asks for the one after it.
     */
    return (lastc = EOF);
}

/*
 * A line of command input. Loops until it holds a newline, because everything
 * below getach() assumes a complete line and answers EOF rather than waiting.
 *
 * On a terminal the kernel hands over a line at a time, so one read usually
 * does. Off one, this reads a byte at a time, which is what upstream did and
 * for the same reason: what is not read stays readable, so a child spawned by
 * :! or :r ! sees the rest of the script rather than finding it eaten.
 */
/*
 * Is the command input buffer empty?
 *
 * A drained buffer still points at its own terminating NUL: getach() is what
 * turns that into a null pointer, and it does so only when it next runs. So
 * "input == 0" is not the question -- asking it was worth one evening.
 */
exbool need_input(void)
{
    return (peekc == 0 && globp == 0 && (input == 0 || *input == 0));
}

Task<Result<void>> ex_readline(void)
{
    static char inbuf[LBSIZE + 4];
    int n        = 0;
    exbool sawnl = 0;

    while (n < (int)sizeof inbuf - 4) {
        Result<String> r = Err(Error::NoMemory);
        if (Task<Result<String>> t = read_some(SYS_STDIN, intty ? (u32)(sizeof inbuf - 4 - n) : 1u))
            r = co_await t;
        if (r.is_err()) {
            if (r.error() == Error::Closed)
                break;
            errno = int(r.error());
            co_return Err(r.error());
        }
        Str got = res_of(r).str();
        if (got.empty())
            break;
        for (usize i = 0; i < got.size() && n < (int)sizeof inbuf - 4; i++) {
            inbuf[n++] = got.data()[i];
            if (got.data()[i] == '\n')
                sawnl = 1;
        }
        if (sawnl)
            break;
    }

    /*
     * Upstream's own coding of the buffer: a short read with no newline is
     * a ^D, an embedded NUL becomes QUOTE so that the string stays one
     * string, and a completed line moves the notional cursor down.
     */
    if (n == 0 || inbuf[n - 1] != '\n')
        inbuf[n++] = CTRL('d');
    if (inbuf[n - 1] == '\n')
        noteinp();
    inbuf[n] = 0;
    for (n--; n >= 0; n--)
        if (inbuf[n] == 0)
            inbuf[n] = QUOTE;
    input = inbuf;
    co_return {};
}

/*
 * Input routine for insert/append/change in command mode.
 * Most work here is in handling autoindent.
 */
static short lastin;

Task<int> gettty(void)
{
    int c      = 0;
    char *cp   = genbuf;
    char hadup = 0;
    int offset = Pline == numbline ? 8 : 0;
    int ch;

    if (need_input())
        if ((co_await ex_readline()).is_err())
            co_return (EOF);
    if (intty && !inglobal) {
        if (offset) {
            holdcm = 1;
            printf("  %4d  ", lineDOT() + 1);
            flush();
            holdcm = 0;
        }
        if (value(AUTOINDENT) ^ aiflag) {
            holdcm = 1;
            tab(lastin + offset);
            while ((c = getcd()) == CTRL('d')) {
                if (lastin == 0) {
                    holdcm = 0;
                    co_return (EOF);
                }
                lastin = backtab(lastin);
                tab(lastin + offset);
            }
            switch (c) {
            case '^':
            case '0':
                ch = getcd();
                if (ch == CTRL('d')) {
                    if (c == '0')
                        lastin = 0;
                    if (!OS) {
                        putchar('\b' | QUOTE);
                        putchar(' ' | QUOTE);
                        putchar('\b' | QUOTE);
                    }
                    tab(offset);
                    hadup = 1;
                    c     = getchar();
                } else
                    ungetchar(ch);
                break;

            case '.':
                if (peekchar() == '\n') {
                    ignchar();
                    noteinp();
                    holdcm = 0;
                    co_return (EOF);
                }
                break;

            case '\n':
                hadup = 1;
                break;
            }
        }
        flush();
        holdcm = 0;
    }
    if (c == 0)
        c = getchar();
    while (c != EOF && c != '\n') {
        if (cp > &genbuf[LBSIZE - 2])
            COTHROWV(0, error("Input line too long"));
        *cp++ = c;
        c     = getchar();
    }
    if (c == EOF) {
        if (inglobal)
            ungetchar(EOF);
        co_return (EOF);
    }
    *cp = 0;
    cp  = linebuf;
    if ((value(AUTOINDENT) ^ aiflag) && hadup == 0 && intty && !inglobal) {
        lastin = c = smunch(lastin, genbuf);
        for (c = lastin; c >= value(TABSTOP); c -= value(TABSTOP))
            *cp++ = '\t';
        for (; c > 0; c--)
            *cp++ = ' ';
    }
    CP(cp, genbuf);
    if (linebuf[0] == '.' && linebuf[1] == 0)
        co_return (EOF);
    co_return (0);
}

/*
 * Crunch the indent.
 * Hard thing here is that in command mode some of the indent
 * is only implicit, so we must seed the column counter.
 * This should really be done differently so as to use the whitecnt routine
 * and also to hack indenting for LISP.
 */
int smunch(int col, char *ocp)
{
    char *cp;

    cp = ocp;
    for (;;)
        switch (*cp++) {
        case ' ':
            col++;
            continue;

        case '\t':
            col += value(TABSTOP) - (col % value(TABSTOP));
            continue;

        default:
            cp--;
            CP(ocp, cp);
            return (col);
        }
}

char *cntrlhm = "^H discarded\n";

void checkjunk(int c)
{
    if (junkbs == 0 && c == '\b') {
        putS(cntrlhm);
        junkbs = 1;
    }
}

void setin(line *addr)
{
    if (addr == zero)
        lastin = 0;
    else
        getline(*addr), lastin = smunch(0, linebuf);
}
