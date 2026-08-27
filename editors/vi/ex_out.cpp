/* Copyright (c) 1980 Regents of the University of California */
/* @(#)ex_put.c	6.6 11/8/80 */

/*
 * Terminal output. This is the half of ex_put.c that formats; the half that
 * drove a terminal is gone.
 *
 * Upstream's pipeline was five deep. putchar() indirected through Putchar to
 * normchar() or listchar(), which formatted a character and handed it to
 * outchar(), which indirected through Outchar to termchar() in command mode or
 * vputchar() in visual. termchar() collected a line in a 64-byte buffer;
 * flush1() then read that buffer back, turning every space, tab, backspace and
 * carriage return into a change of destcol and destline, and fgoto() worked
 * out the cheapest sequence of terminal capabilities to get the cursor from
 * where it was to where it should be, padded for the line's speed.
 *
 * None of the last three exist here. In visual, Outchar is vputchar and writes
 * a cell; in command mode there is no cursor to move, because standard output
 * is an ordinary byte stream that may be a pipe or a file. So termchar() emits
 * what it is given and keeps destcol itself, which is all anything downstream
 * of it ever wanted.
 *
 * The other change is that nothing here blocks. Upstream's flusho() wrote to
 * file descriptor 1 from wherever it was called, and it is called from plain
 * functions six levels down which cannot await a syscall. So output collects
 * in one growable buffer and exflush() -- the only coroutine in this file --
 * drains it. It is awaited where the editor stops to read, which is where a
 * terminal has to be up to date anyway, and inside the print loop, so that
 * listing a large file does not have to hold all of it at once.
 */
#include "ex.h"
#include "ex_screen.h"
#include "ex_vis.h"
#include "kernel/alloc.h"

/*
 * Actual pointers to the routines that print characters and lines.
 * During open/visual, outchar and putchar will be set to
 * routines in the file ex_vput.c (vputchar, vinschar, etc.).
 */
OutcharFn Outchar = termchar;
OutcharFn Putchar = normchar;
PlineFn Pline     = normline;

OutcharFn setlist(exbool t)
{
    OutcharFn P;

    listf   = t;
    P       = Putchar;
    Putchar = t ? listchar : normchar;
    return (P);
}

PlineFn setnumb(exbool t)
{
    PlineFn P;

    numberf = t;
    P       = Pline;
    Pline   = t ? numbline : normline;
    return (P);
}

/*
 * Format c for list mode; leave things in common
 * with normal print mode to be done by normchar.
 */
int listchar(int c)
{
    c &= (TRIM | QUOTE);
    switch (c) {
    case '\t':
    case '\b':
        outchar('^');
        c = ctlof(c);
        break;

    case '\n':
        break;

    case '\n' | QUOTE:
        outchar('$');
        break;

    default:
        if (c & QUOTE)
            break;
        if (c < ' ' && c != '\n' || c == DELETE)
            outchar('^'), c = ctlof(c);
        break;
    }
    return (normchar(c));
}

/*
 * Format c for printing.
 *
 * Upstream also coped with terminals that had no lower case and Hazeltines
 * that had no tilde; neither is reachable, so neither is here.
 */
int normchar(int c)
{
    c &= (TRIM | QUOTE);
    if (c & QUOTE)
        switch (c) {
        case ' ' | QUOTE:
        case '\b' | QUOTE:
            break;

        case QUOTE:
            return (0);

        default:
            c &= TRIM;
        }
    else if (c < ' ' && c != '\b' && c != '\n' && c != '\t' || c == DELETE)
        putchar('^'), c = ctlof(c);
    outchar(c);
    return (0);
}

/*
 * Print a line with a number.
 */
int numbline(int i)
{
    if (shudclob)
        slobber(' ');
    printf("%6d  ", i);
    return (normline(0));
}

/*
 * Normal line output, no numbering.
 */
int normline(int i)
{
    char *cp;

    (void)i;
    if (shudclob)
        slobber(linebuf[0]);
    for (cp = linebuf; *cp;)
        putchar(*cp++);
    if (!inopen)
        putchar('\n' | QUOTE);
    return (0);
}

/*
 * Given c at the beginning of a line, determine whether
 * the printing of the line will erase or otherwise obliterate
 * the prompt which was printed before.  If it won't, do it now.
 */
void slobber(int c)
{
    shudclob = 0;
    switch (c) {
    case '\t':
        if (Putchar == listchar)
            return;
        break;

    default:
        return;

    case ' ':
    case 0:
        break;
    }
    flush();
    putch(' ');
    putch('\b');
}

/*
 * The output buffer. Upstream's was BUFSIZ bytes wide and drained itself with
 * a write() whenever it filled; that cannot be done from a plain function
 * here, so it grows instead and exflush() drains it.
 */
static char *obuf;
static int obcap;
static int oblen;

static exbool obgrow(int want)
{
    int n = obcap ? obcap : 4096;
    char *p;

    if (want <= obcap)
        return (1);
    while (n < want)
        n *= 2;
    p = (char *)heap_alloc(n);
    if (p == 0)
        return (0);
    if (obuf) {
        memcpy(p, obuf, oblen);
        heap_free(obuf);
    }
    obuf  = p;
    obcap = n;
    return (1);
}

/* Past this, exflush() is worth awaiting; the print loop asks. */
#define OBSOFT (64 * 1024)

exbool out_pending(void)
{
    return (oblen >= OBSOFT);
}

void draino(void)
{
    oblen = 0;
}

/*
 * The one coroutine here. Everything above collects; this is what reaches the
 * screen, and it is awaited where the editor stops to read.
 */
Task<Result<void>> exflush(void)
{
    if (oblen == 0)
        co_return {};
    if (Task<Result<void>> t = write_all(SYS_STDOUT, Str(obuf, oblen))) {
        Result<void> r = co_await t;
        oblen          = 0;
        if (r.is_err())
            errno = int(r.error());
        co_return r;
    }
    oblen = 0;
    co_return Err(Error::NoMemory);
}

void ex_out_reset(void)
{
    oblen   = 0;
    destcol = destline = outcol = outline = 0;
}

/*
 * Indirect to current definition of putchar.
 */
int putchar(int c)
{
    return ((*Putchar)(c));
}

/*
 * Termchar routine for command mode.
 *
 * Upstream buffered a line here and let flush1() turn it back into cursor
 * motions. Standard output is a byte stream, so the character goes out as it
 * stands; what is kept is destcol, which slobber(), tab() and the visual code
 * read to know where on the line the next character lands.
 */
int termchar(int c)
{
    c &= 0177;
    switch (c) {
    case '\n':
        destline++;
        destcol = 0;
        break;

    case '\r':
        destcol = 0;
        break;

    case '\b':
        if (destcol)
            destcol--;
        break;

    case '\t':
        destcol += value(TABSTOP) - destcol % value(TABSTOP);
        break;

    default:
        if (c >= ' ' && c != DELETE)
            destcol++;
        break;
    }
    outcol  = destcol;
    outline = destline;
    putch(c);
    return (0);
}

/*
 * flush, flush1 and flush2 were the three stages of getting a line onto a
 * terminal. There is one stage now and it has already happened; what is left
 * is the name, because it is called from about eighty places.
 */
void flush(void)
{
}

void flush1(void)
{
}

void flush2(void)
{
}

void flusho(void)
{
}

void putnl(void)
{
    putchar('\n');
}

void putS(char *cp)
{
    if (cp == NULL)
        return;
    while (*cp)
        putch(*cp++);
}

int putch(int c)
{
    /*
     * Poisoned while an error is pending. This is one of the dozen leaf
     * routines that make an unguarded continuation inert rather than
     * wrong: a command that carried on after a failed address would
     * otherwise print half of something. error_end() raises the flag only
     * after the message has been written, which is why the ordering there
     * is called load-bearing.
     */
    if (ex_thrown)
        return (0);
    if (oblen == obcap && !obgrow(oblen + 1))
        return (0);
    obuf[oblen++] = c & 0177;
    return (0);
}

/*
 * Miscellaneous routines related to output.
 */

/*
 * Upstream padded a capability out over the line at the current speed. Two of
 * them still mean something, and what they mean is an attribute on a cell
 * rather than a sequence of bytes; the rest name things a cell grid does by
 * being indexed, and arrive here only from branches that are now dead.
 */
void putpad(char *cp)
{
    if (cp == CAP_SO)
        standout(1);
    else if (cp == CAP_SE)
        standout(0);
}

/*
 * Reverse video, for the message line and the file-name echo. In command mode
 * standard output is a byte stream with no attributes, so this is visual's
 * alone; vputchar() reads it as it writes each cell.
 */
void standout(exbool on)
{
    if (inopen)
        vstandout = on;
}

/*
 * Set output through normal command mode routine.
 */
void setoutt(void)
{
    Outchar = termchar;
}

/*
 * Printf (temporarily) in list mode.
 */
/*VARARGS2*/
void lprintf(char *cp, char *dp)
{
    OutcharFn P;

    P = setlist(1);
    printf(cp, dp);
    Putchar = P;
}

/*
 * Newline + flush.
 */
void putNFL(void)
{
    putnl();
    flush();
}

/*
 * pstart and pstop turned the terminal's newline mapping off and on, so that
 * a long listing could be sent with linefeeds instead of cursor motions. There
 * is no terminal driver and no mapping to turn off.
 */
void pstart(void)
{
}

void pstop(void)
{
    if (inopen)
        return;
    draino();
}

/*
 * An input line arrived.
 * Calculate new (approximate) screen line position.
 * Approximate because kill character echoes newline with
 * no feedback and also because of long input lines.
 */
void noteinp(void)
{
    outline++;
    if (outline > LINES - 1)
        outline = LINES - 1;
    destline = outline;
    destcol = outcol = 0;
}

void tab(int col)
{
    while (destcol < col)
        putchar(' ');
}

/*
 * Print newline, or blank if in open/visual
 */
void noonl(void)
{
    putchar(Outchar != termchar ? ' ' : '\n');
}

/*
 * ex's own printf, which prints through putchar.
 *
 * Upstream stole the v6 library's and munged it, to keep stdio out of the
 * editor -- "Ex means to avoid stdio like the plague". Here there is no stdio
 * to avoid; this is the same small conversion set the call sites use, %d %o
 * %s %c and %%, with a width and a zero flag.
 */
static void putnum(long v, int base, int width, exbool zero, exbool sgn)
{
    char digits[24];
    int n      = 0;
    exbool neg = 0;
    unsigned long u;

    if (sgn && v < 0)
        neg = 1, u = (unsigned long)-v;
    else
        u = (unsigned long)v;
    do {
        digits[n++] = "0123456789abcdef"[u % base];
        u /= base;
    } while (u);
    if (neg)
        digits[n++] = '-';
    while (n < width)
        digits[n++] = zero ? '0' : ' ';
    while (n > 0)
        putchar(digits[--n]);
}

void printf(const char *fmt, ...)
{
    __builtin_va_list ap;
    const char *cp;
    char *sp;
    int width;
    exbool zero, lng;

    __builtin_va_start(ap, fmt);
    for (cp = fmt; *cp; cp++) {
        if (*cp != '%') {
            putchar(*cp);
            continue;
        }
        cp++;
        zero = 0;
        if (*cp == '0')
            zero = 1, cp++;
        width = 0;
        while (isdigit(*cp))
            width = width * 10 + (*cp++ - '0');
        lng = 0;
        while (*cp == 'l')
            lng = 1, cp++;
        switch (*cp) {
        case 'd':
            putnum(lng ? __builtin_va_arg(ap, long) : (long)__builtin_va_arg(ap, int), 10, width,
                   zero, 1);
            break;

        case 'u':
            putnum(lng ? (long)__builtin_va_arg(ap, unsigned long)
                       : (long)__builtin_va_arg(ap, unsigned int),
                   10, width, zero, 0);
            break;

        case 'o':
            putnum((long)__builtin_va_arg(ap, unsigned int), 8, width, zero, 0);
            break;

        case 'x':
            putnum((long)__builtin_va_arg(ap, unsigned int), 16, width, zero, 0);
            break;

        case 'c':
            putchar(__builtin_va_arg(ap, int));
            break;

        case 's':
            sp = __builtin_va_arg(ap, char *);
            if (sp == 0)
                sp = (char *)"";
            while (*sp)
                putchar(*sp++);
            break;

        case '%':
            putchar('%');
            break;

        case 0:
            cp--;
            break;

        default:
            putchar(*cp);
            break;
        }
    }
    __builtin_va_end(ap);
}
