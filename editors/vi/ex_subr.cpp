/* Copyright (c) 1980 Regents of the University of California */
#include "ex.h"
#include "ex_buf.h"
#include "ex_re.h"
#include "ex_screen.h"
#include "ex_vis.h"

#include "kernel/text.h"

/*
 * UTF-8, one codepoint at a time.
 *
 * A line stays a char array of UTF-8 bytes -- the SDK's own TextBuf keeps text
 * that way -- and only the stepping knows about codepoints. A malformed
 * sequence decodes to U+FFFD and advances one byte, so bad input is visible
 * and never swallows the rest of the line.
 */
int runeat(char *cp, int *len)
{
    char32_t r;
    usize used;
    int n = 0;

    while (n < 4 && cp[n])
        n++;
    if (n == 0) {
        *len = 0;
        return (0);
    }
    used = utf8_decode(Str(cp, (usize)n), 0, r);
    *len = used ? (int)used : 1;
    return ((int)r);
}

/* The codepoint at cp, for a caller with no use for its length. */
int runeof(char *cp)
{
    int n;

    return (runeat(cp, &n));
}

/* Bytes the sequence this lead byte opens should have; 1 if it opens none. */
int runelen(int lead)
{
    unsigned char b = (unsigned char)lead;

    if (b < 0200)
        return (1);
    if ((b & 0340) == 0300)
        return (2);
    if ((b & 0360) == 0340)
        return (3);
    if ((b & 0370) == 0360)
        return (4);
    return (1);
}

char *nextchar(char *cp)
{
    int n;

    if (*cp == 0)
        return (cp);
    ignore(runeat(cp, &n));
    return (cp + n);
}

/*
 * Blank, and word character, over the whole range.
 *
 * Above ASCII there are no tables to consult, so the rule is by exclusion:
 * anything that is not a blank and not punctuation is a letter. Four ranges
 * cover what text actually contains -- Latin-1 punctuation and symbols,
 * General Punctuation, and the CJK forms -- and Cyrillic, Greek, CJK, Arabic
 * and Hebrew all come out as word characters.
 */
exbool rune_space(int c)
{
    if (c < 0200)
        return (isspace(c));
    return (c == 0xa0 || (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 ||
            c == 0x202f || c == 0x205f || c == 0x3000);
}

exbool rune_word(int c)
{
    if (c < 0200)
        return (isalpha(c) || isdigit(c) || c == '_');
    if (rune_space(c))
        return (0);
    if (c >= 0xa0 && c <= 0xbf)
        return (0);
    if (c == 0xd7 || c == 0xf7)
        return (0);
    if (c >= 0x2000 && c <= 0x206f)
        return (0);
    if (c >= 0x3000 && c <= 0x303f)
        return (0);
    return (1);
}

/* One character along linebuf, in either direction; off the front is allowed. */
char *vstep(char *cp, int dir)
{
    if (dir > 0)
        return (nextchar(cp));
    return (cp > linebuf ? prevchar(linebuf, cp) : cp - 1);
}

char *prevchar(char *base, char *cp)
{
    if (cp <= base)
        return (base);
    cp--;
    while (cp > base && ((unsigned char)*cp & 0300) == 0200)
        cp--;
    return (cp);
}

/*
 * Random routines, in alphabetical order.
 */

exbool any(int c, char *s)
{
    int x;

    while (x = *s++)
        if (x == c)
            return (1);
    return (0);
}

int backtab(int i)
{
    int j;

    j = i % value(SHIFTWIDTH);
    if (j == 0)
        j = value(SHIFTWIDTH);
    i -= j;
    if (i < 0)
        i = 0;
    return (i);
}

void change(void)
{
    tchng++;
    chng = tchng;
}

/*
 * Column returns the number of
 * columns occupied by printing the
 * characters through position cp of the
 * current line.
 */
int column(char *cp)
{
    if (cp == 0)
        cp = &linebuf[LBSIZE - 2];
    return (qcolumn(cp, (char *)0));
}

/*
 * Ignore a comment to the end of the line.
 * This routine eats the trailing newline so don't call newline().
 */
void comment(void)
{
    int c;

    do {
        c = getchar();
    } while (c != '\n' && c != EOF);
    if (c == EOF)
        ungetchar(c);
}

void Copy(char *to, char *from, int size)
{
    if (size > 0)
        do
            *to++ = *from++;
        while (--size > 0);
}

void copyw(line *to, line *from, int size)
{
    if (size > 0)
        do
            *to++ = *from++;
        while (--size > 0);
}

void copywR(line *to, line *from, int size)
{
    while (--size >= 0)
        to[size] = from[size];
}

int ctlof(int c)
{
    return (c == DELETE ? '?' : c | ('A' - 1));
}

void dingdong(void)
{
    if (VB)
        putpad(VB);
    else if (value(ERRORBELLS))
        putch('\207');
}

int fixindent(int indent)
{
    int i;
    char *cp;

    i  = whitecnt(genbuf);
    cp = vpastwh(genbuf);
    if (*cp == 0 && i == indent && linebuf[0] == 0) {
        genbuf[0] = 0;
        return (i);
    }
    CP(genindent(i), cp);
    return (i);
}

void filioerr(char *cp)
{
    int oerrno = ex_errno;

    lprintf("\"%s\"", cp);
    ex_errno = oerrno;
    THROW(syserror());
}

char *genindent(int indent)
{
    char *cp;

    for (cp = genbuf; indent >= value(TABSTOP); indent -= value(TABSTOP))
        *cp++ = '\t';
    for (; indent > 0; indent--)
        *cp++ = ' ';
    return (cp);
}

void getDOT(void)
{
    getline(*dot);
}

line *getmark(int c)
{
    line *addr;

    for (addr = one; addr <= dol; addr++)
        if (names[c - 'a'] == (*addr & ~01)) {
            return (addr);
        }
    return (0);
}

int getn(char *cp)
{
    int i = 0;

    while (isdigit(*cp))
        i = i * 10 + *cp++ - '0';
    if (*cp)
        return (0);
    return (i);
}

void ignnEOF(void)
{
    int c = getchar();

    if (c == EOF)
        ungetchar(c);
    else if (c == '"')
        comment();
}

exbool iswhite(int c)
{
    return (c == ' ' || c == '\t');
}

exbool junk(int c)
{
    if (c && !value(BEAUTIFY))
        return (0);
    if (c >= ' ' && c != DELETE)
        return (0);
    switch (c) {
    case '\t':
    case '\n':
    case '\f':
        return (0);

    default:
        return (1);
    }
}

void killed(void)
{
    killcnt(addr2 - addr1 + 1);
}

void killcnt(int cnt)
{
    if (inopen) {
        notecnt = cnt;
        notenam = notesgn = "";
        return;
    }
    if (!notable(cnt))
        return;
    printf("%d lines", cnt);
    if (value(TERSE) == 0) {
        printf(" %c%s", Command[0] | ' ', Command + 1);
        if (Command[strlen(Command) - 1] != 'e')
            putchar('e');
        putchar('d');
    }
    putNFL();
}

int lineno(line *a)
{
    return (a - zero);
}

int lineDOL(void)
{
    return (lineno(dol));
}

int lineDOT(void)
{
    return (lineno(dot));
}

void markDOT(void)
{
    markpr(dot);
}

void markpr(line *which)
{
    if ((inglobal == 0 || inopen) && which <= endcore) {
        names['z' - 'a' + 1] = *which & ~01;
        if (inopen)
            ncols['z' - 'a' + 1] = cursor;
    }
}

int markreg(int c)
{
    if (c == '\'' || c == '`')
        return ('z' + 1);
    if (c >= 'a' && c <= 'z')
        return (c);
    return (0);
}

/*
 * Mesg decodes the terse/verbose strings. Thus
 *	'xxx@yyy' -> 'xxx' if terse, else 'xxx yyy'
 *	'xxx|yyy' -> 'xxx' if terse, else 'yyy'
 * All others map to themselves.
 */
char *mesg(char *str)
{
    char *cp;

    str = strcpy(genbuf, str);
    for (cp = str; *cp; cp++)
        switch (*cp) {
        case '@':
            if (value(TERSE))
                *cp = 0;
            else
                *cp = ' ';
            break;

        case '|':
            if (value(TERSE) == 0)
                return (cp + 1);
            *cp = 0;
            break;
        }
    return (str);
}

/*
 * Clear the echo line for a message. vclreol() raises an error of its own on a
 * botched destline, and error() reports through here rather than unwinding, so
 * a second trip would recurse until the stack ran out. Report it on the line
 * as it stands.
 */
static void msgclreol(void)
{
    static exbool busy;

    if (busy)
        return;
    busy = 1;
    vclreol();
    busy = 0;
}

/*VARARGS2*/
void merror(char *seekpt, int i)
{
    char *cp = linebuf;

    if (seekpt == 0)
        return;
    merror1(seekpt);
    if (*cp == '\n')
        putnl(), cp++;
    if (inopen && CE)
        msgclreol();
    if (SO && SE)
        putpad(SO);
    printf(mesg(cp), i);
    if (SO && SE)
        putpad(SE);
}

/*
 * Off VMUNIX the messages lived in a file that xstr built and this seeked to
 * one; here, as there, they are compiled in and the "seek pointer" is the
 * string itself.
 */
void merror1(char *seekpt)
{
    strcpy(linebuf, seekpt);
}

int morelines(void)
{
    if (endcore + 1024 >= lx_limit)
        return (-1);
    endcore += 1024;
    return (0);
}

void nonzero(void)
{
    if (addr1 == zero) {
        notempty();
        THROW(error("Nonzero address required@on this command"));
    }
}

exbool notable(int i)
{
    return (hush == 0 && !inglobal && i > value(REPORT));
}

void notempty(void)
{
    if (dol == zero)
        THROW(error("No lines@in the buffer"));
}

void netchHAD(int cnt)
{
    netchange(lineDOL() - cnt);
}

void netchange(int i)
{
    char *cp;

    if (i > 0)
        notesgn = cp = "more ";
    else
        notesgn = cp = "fewer ", i = -i;
    if (inopen) {
        notecnt = i;
        notenam = "";
        return;
    }
    if (!notable(i))
        return;
    printf(mesg("%d %slines@in file after %s"), i, cp, Command);
    putNFL();
}

void putmark(line *addr)
{
    putmk1(addr, putline());
}

void putmk1(line *addr, int n)
{
    line *markp;
    int oldglobmk;

    oldglobmk = *addr & 1;
    *addr &= ~1;
    for (markp = (anymarks ? names : &names['z' - 'a' + 1]); markp <= &names['z' - 'a' + 1];
         markp++)
        if (*markp == *addr)
            *markp = n;
    *addr = n | oldglobmk;
}

char *plural(long i)
{
    return (char *)(i == 1 ? "" : "s");
}

int qcount();
short vcntcol;

int qcolumn(char *lim, char *gp)
{
    int x;
    OutcharFn OO;

    OO      = Outchar;
    Outchar = qcount;
    vcntcol = 0;
    if (lim != NULL)
        x = lim[1], lim[1] = 0;
    pline(0);
    if (lim != NULL)
        lim[1] = x;
    if (gp)
        while (*gp) {
            int n, c = runeat(gp, &n);

            putchar(c);
            gp += n;
        }
    Outchar = OO;
    return (vcntcol);
}

int qcount(int c)
{
    if (c == '\t') {
        vcntcol += value(TABSTOP) - vcntcol % value(TABSTOP);
        return (0);
    }
    vcntcol++;
    return (0);
}

void reverse(line *a1, line *a2)
{
    line t;

    for (;;) {
        t = *--a2;
        if (a2 <= a1)
            return;
        *a2   = *a1;
        *a1++ = t;
    }
}

void save(line *a1, line *a2)
{
    int more;

    if (!FIXUNDO)
        return;
    undkind = UNDNONE;
    undadot = dot;
    more    = (a2 - a1 + 1) - (unddol - dol);
    while (more > (endcore - truedol))
        if (morelines() < 0)
            THROW(error("Out of memory@saving lines for undo - try using ed"));
    if (more)
        (*(more > 0 ? copywR : copyw))(unddol + more + 1, unddol + 1, (truedol - unddol));
    unddol += more;
    truedol += more;
    copyw(dol + 1, a1, a2 - a1 + 1);
    undkind = UNDALL;
    unddel  = a1 - 1;
    undap1  = a1;
    undap2  = a2 + 1;
}

void save12(void)
{
    save(addr1, addr2);
}

void saveall(void)
{
    save(one, dol);
}

int span(void)
{
    return (addr2 - addr1 + 1);
}

void sync(void)
{
    chng  = 0;
    tchng = 0;
    xchng = 0;
}

int skipwh(void)
{
    int wh;

    wh = 0;
    while (iswhite(peekchar())) {
        wh++;
        ignchar();
    }
    return (wh);
}

/*VARARGS2*/
void smerror(char *seekpt, char *cp)
{
    if (seekpt == 0)
        return;
    merror1(seekpt);
    if (inopen && CE)
        msgclreol();
    if (SO && SE)
        putpad(SO);
    lprintf(mesg(linebuf), cp);
    if (SO && SE)
        putpad(SE);
}

char *strend(char *cp)
{
    while (*cp)
        cp++;
    return (cp);
}

void strcLIN(char *dp)
{
    CP(linebuf, dp);
}

/*
 * The last system call's complaint. Upstream carried a copy of the errno
 * message list, because perror wrote to standard error and ex wanted the text
 * in a buffer of its own; the kernel names its errors, so this asks.
 */
void syserror(void)
{
    static char buf[48];
    Str m   = error_name(Error(ex_errno));
    usize n = m.size() < sizeof buf - 1 ? m.size() : sizeof buf - 1;

    putchar(' ');
    edited = 0; /* for temp file errors, for example */
    memcpy(buf, m.data(), n);
    buf[n] = 0;
    THROW(error(buf));
}

/*
 * Return the column number that results from being in column col and
 * hitting a tab, where tabs are set every ts columns.  Work right for
 * the case where col > COLUMNS, even if ts does not divide COLUMNS.
 */
int tabcol(int col, int ts)
{
    int offset, result;

    if (col >= COLUMNS) {
        offset = COLUMNS * (col / COLUMNS);
        col -= offset;
    } else
        offset = 0;
    result = col + ts - (col % ts) + offset;
    return (result);
}

char *vfindcol(int i)
{
    char *cp;
    OutcharFn OO = Outchar;

    Outchar = qcount;
    ignore(qcolumn(linebuf - 1, NOSTR));
    for (cp = linebuf; *cp && vcntcol < i;) {
        int n, c = runeat(cp, &n);

        putchar(c);
        cp += n;
    }
    if (cp != linebuf)
        cp = prevchar(linebuf, cp);
    Outchar = OO;
    return (cp);
}

char *vskipwh(char *cp)
{
    while (iswhite(*cp) && cp[1])
        cp++;
    return (cp);
}

char *vpastwh(char *cp)
{
    while (iswhite(*cp))
        cp++;
    return (cp);
}

int whitecnt(char *cp)
{
    int i;

    i = 0;
    for (;;)
        switch (*cp++) {
        case '\t':
            i += value(TABSTOP) - i % value(TABSTOP);
            break;

        case ' ':
            i++;
            break;

        default:
            return (i);
        }
}

void markit(line *addr)
{
    if (addr != dot && addr >= one && addr <= dol)
        markDOT();
}

/*
 * An interrupt occurred.  Drain any output which
 * is still in the output buffering pipeline.
 * Catch interrupts again.  Unless we are in visual
 * reset the output state (out of -nl mode, e.g).
 * Then like a normal error (with the \n before Interrupt
 * suppressed in visual mode).
 *
 * Upstream re-armed the handler here, because a caught signal reverted to the
 * default action on delivery. sig_catch is a standing request, so there is
 * nothing to re-arm; what is left is the draining and the message.
 */
void onintr(void)
{
    draino();
    if (!inopen) {
        pstop();
        setlastchar('\n');
    }
    THROW(error((char *)"\nInterrupt" + inopen));
}

/*
 * If we are interruptible, enable interrupts again.
 * In some critical sections we turn interrupts off,
 * but not very often.
 *
 * Asking for SIG_INT is what stops it killing the process outright: the
 * default action is death, and a full-screen program that has taken the whole
 * screen has to stay killable until it says otherwise.
 */
Task<void> setrupt(void)
{
    if (ruptible)
        if (Task<Result<void>> t = sig_catch(SIG_INT))
            co_await t;
}

/*
 * Upstream's exit() closed the trace file and called _exit. Sys::Exit only
 * records a status here -- a process ends when its root task returns -- so
 * this records what to end with and unwinds by the same route an error takes.
 */
void ex_exit(int i)
{
    ex_status   = i;
    ex_quitting = 1;
    ex_thrown   = 1;
}
