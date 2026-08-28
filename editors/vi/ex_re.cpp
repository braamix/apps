/* Copyright (c) 1980 Regents of the University of California */
#include "ex.h"
#include "ex_re.h"
#include "ex_screen.h"
#include "ex_vis.h"

#include "kernel/text.h"

/*
 * Global, substitute and regular expressions.
 * Very similar to ed, with some re extensions and
 * confirmed substitute.
 */
Task<void> global(exbool k)
{
    char *gp;
    int c;
    line *a1;
    char globuf[GBSIZE], *Cwas;
    int lines     = lineDOL();
    int oinglobal = inglobal;
    char *oglobp  = globp;

    Cwas = Command;
    /*
     * States of inglobal:
     *  0: ordinary - not in a global command.
     *  1: text coming from some buffer, not tty.
     *  2: like 1, but the source of the buffer is a global command.
     * Hence you're only in a global command if inglobal==2. This
     * strange sounding convention is historically derived from
     * everybody simulating a global command.
     */
    if (inglobal == 2)
        COTHROW(error("Global within global@not allowed"));
    markDOT();
    setall();
    nonzero();
    COCHK;
    if (skipend())
        COTHROW(error("Global needs re|Missing regular expression for global"));
    c = getchar();
    ignore(compile(c, 1));
    COCHK;
    savere(scanre);
    gp = globuf;
    while ((c = getchar()) != '\n') {
        switch (c) {
        case EOF:
            c = '\n';
            goto brkwh;

        case '\\':
            c = getchar();
            switch (c) {
            case '\\':
                ungetchar(c);
                break;

            case '\n':
                break;

            default:
                *gp++ = '\\';
                break;
            }
            break;
        }
        *gp++ = c;
        if (gp >= &globuf[GBSIZE - 2])
            COTHROW(error("Global command too long"));
    }
brkwh:
    ungetchar(c);
out:
    newline();
    COCHK;
    *gp++ = c;
    *gp++ = 0;
    saveall();
    inglobal = 2;
    for (a1 = one; a1 <= dol; a1++) {
        *a1 &= ~01;
        if (a1 >= addr1 && a1 <= addr2 && execute(0, a1) == k)
            *a1 |= 01;
    }
#ifdef notdef
    /*
     * This code is commented out for now.  The problem is that we don't
     * fix up the undo area the way we should.  Basically, I think what has
     * to be done is to copy the undo area down (since we shrunk everything)
     * and move the various pointers into it down too.  I will do this later
     * when I have time. (Mark, 10-20-80)
     */
    /*
     * Special case: g/.../d (avoid n^2 algorithm)
     */
    if (globuf[0] == 'd' && globuf[1] == '\n' && globuf[2] == '\0') {
        gdelete();
        co_return;
    }
#endif
    if (inopen)
        inopen = -1;
    /*
     * Now for each marked line, set dot there and do the commands.
     * Note the n^2 behavior here for lots of lines matching.
     * This is really needed: in some cases you could exdelete lines,
     * causing a marked line to be moved before a1 and missed if
     * we didn't restart at zero each time.
     */
    for (a1 = one; a1 <= dol; a1++) {
        if (*a1 & 01) {
            *a1 &= ~01;
            dot   = a1;
            globp = globuf;
            co_await commands(1, 1);
            a1 = zero;
        }
    }
    globp    = oglobp;
    inglobal = oinglobal;
    endline  = 1;
    Command  = Cwas;
    netchHAD(lines);
    setlastchar(EOF);
    if (inopen) {
        ungetchar(EOF);
        inopen = 1;
    }
}

/*
 * gdelete: exdelete inside a global command. Handles the
 * special case g/r.e./d. All lines to be deleted have
 * already been marked. Squeeze the remaining lines together.
 * Note that other cases such as g/r.e./p, g/r.e./s/r.e.2/rhs/,
 * and g/r.e./.,/r.e.2/d are not treated specially.  There is no
 * good reason for this except the question: where to you draw the line?
 */
void gdelete(void)
{
    line *a1, *a2, *a3;

    a3 = dol;
    /* find first marked line. can skip all before it */
    for (a1 = zero; (*a1 & 01) == 0; a1++)
        if (a1 >= a3)
            return;
    /* copy down unmarked lines, compacting as we go. */
    for (a2 = a1 + 1; a2 <= a3;) {
        if (*a2 & 01) {
            a2++;     /* line is marked, skip it */
            dot = a1; /* dot left after line deletion */
        } else
            *a1++ = *a2++; /* unmarked, copy it */
    }
    dol = a1 - 1;
    if (dot > dol)
        dot = dol;
    change();
}

exbool cflag;
int scount, slines, stotal;

Task<int> substitute(int c)
{
    line *addr;
    int n;
    int gsubf, hopcount = 0;

    gsubf = co_await compsub(c);
    /* No compiled re past here: execute() would run off an empty expbuf. */
    COCHKV(0);
    if (FIXUNDO)
        save12(), undkind = UNDCHANGE;
    stotal = 0;
    slines = 0;
    for (addr = addr1; addr <= addr2; addr++) {
        scount = 0;
        if (co_await dosubcon(0, addr) == 0) {
            COCHKV(0);
            continue;
        }
        if (gsubf) {
            /*
             * The loop can happen from s/\</&/g
             * but we don't want to break other, reasonable cases.
             */
            while (*loc2) {
                if (++hopcount > sizeof linebuf)
                    COTHROWV(0, error("substitution loop"));
                if (co_await dosubcon(1, addr) == 0)
                    break;
            }
            COCHKV(0);
        }
        if (scount) {
            stotal += scount;
            slines++;
            putmark(addr);
            n = co_await append(getsub, addr);
            COCHKV(0);
            addr += n;
            addr2 += n;
        }
    }
    if (stotal == 0 && !inglobal && !cflag)
        COTHROWV(0, error("Fail|Substitute pattern match failed"));
    snote(stotal, slines);
    co_return (stotal);
}

Task<int> compsub(int ch)
{
    int seof, c, uselastre;
    static int gsubf;

    if (!value(EDCOMPATIBLE))
        gsubf = cflag = 0;
    uselastre = 0;
    switch (ch) {
    case 's':
        ignore(skipwh());
        seof = getchar();
        if (endcmd(seof) || any(seof, "gcr")) {
            ungetchar(seof);
            goto redo;
        }
        if (isalpha(seof) || isdigit(seof))
            COTHROWV(0, error("Substitute needs re|Missing regular expression for substitute"));
        seof = compile(seof, 1);
        COCHKV(0);
        uselastre = 1;
        comprhs(seof);
        COCHKV(0);
        gsubf = 0;
        cflag = 0;
        break;

    case '~':
        uselastre = 1;
        /* fall into ... */
    case '&':
    redo:
        if (re.Expbuf[0] == 0)
            COTHROWV(0, error("No previous re|No previous regular expression"));
        if (subre.Expbuf[0] == 0)
            COTHROWV(0, error("No previous substitute re|No previous substitute to repeat"));
        break;
    }
    for (;;) {
        c = getchar();
        switch (c) {
        case 'g':
            gsubf = !gsubf;
            continue;

        case 'c':
            cflag = !cflag;
            continue;

        case 'r':
            uselastre = 1;
            continue;

        default:
            ungetchar(c);
            setcount();
            newline();
            COCHKV(0);
            if (uselastre)
                savere(subre);
            else
                resre(subre);
            co_return (gsubf);
        }
    }
}

/* getchar(), a whole codepoint at a time: a pattern is UTF-8 like a line. */
static int getrune(void)
{
    char b[4];
    char32_t r;
    int c = getchar();
    int n, i;

    if (c == EOF || c < 0200)
        return (c);
    n = runelen(c);
    if (n == 1)
        return (c);
    b[0] = (char)c;
    for (i = 1; i < n; i++) {
        int d = getchar();

        if (d == EOF) {
            ungetchar(d);
            return (c);
        }
        b[i] = (char)d;
    }
    return (utf8_decode(Str(b, (usize)n), 0, r) ? (int)r : c);
}

void comprhs(int seof)
{
    int *rp, *orp;
    int c;
    int orhsbuf[RHSSIZE];

    rp = rhsbuf;
    for (int i = 0; (orhsbuf[i] = rp[i]) != 0; i++)
        continue;
    for (;;) {
        c = getrune();
        if (c == seof)
            break;
        switch (c) {
        case '\\':
            c = getrune();
            if (c == EOF) {
                ungetchar(c);
                break;
            }
            if (value(MAGIC)) {
                /*
                 * When "magic", \& turns into a plain &,
                 * and all other chars work fine quoted.
                 */
                if (c != '&')
                    c |= QUOTE;
                break;
            }
        magic:
            if (c == '~') {
                for (orp = orhsbuf; *orp; *rp++ = *orp++)
                    if (rp >= &rhsbuf[RHSSIZE - 1])
                        goto toobig;
                continue;
            }
            c |= QUOTE;
            break;

        case '\n':
        case EOF:
            if (!(globp && globp[0])) {
                ungetchar(c);
                goto endrhs;
            }

        case '~':
        case '&':
            if (value(MAGIC))
                goto magic;
            break;
        }
        if (rp >= &rhsbuf[RHSSIZE - 1]) {
        toobig:
            *rp = 0;
            THROW(error("Replacement pattern too long@- limit 256 characters"));
        }
        *rp++ = c;
    }
endrhs:
    *rp++ = 0;
}

Task<int> getsub(void)
{
    char *p;

    if ((p = linebp) == 0)
        co_return (EOF);
    strcLIN(p);
    linebp = 0;
    co_return (0);
}

Task<int> dosubcon(exbool f, line *a)
{
    if (execute(f, a) == 0)
        co_return (0);
    if (co_await confirmed(a)) {
        dosub();
        COCHKV(0);
        scount++;
    }
    co_return (1);
}

Task<exbool> confirmed(line *a)
{
    int c, ch;

    if (cflag == 0)
        co_return (1);
    pofix();
    pline(lineno(a));
    if (inopen)
        putchar('\n' | QUOTE);
    c = column(loc1 - 1);
    ugo(c - 1 + (inopen ? 1 : 0), ' ');
    ugo(column(loc2 - 1) - c, '^');
    flush();
    /*
     * getkey() reads a raw key, which needs a claimed keyboard, and command
     * mode has not claimed one -- upstream reached the terminal directly
     * and so could ask either way. getch(), which upstream left written but
     * never called, is the other half.
     */
    ch = c = inopen ? keycmd(co_await getkey()) : co_await getch();
again:
    if (c == '\r')
        c = '\n';
    if (inopen)
        putchar(c), flush();
    if (c != '\n' && c != EOF) {
        c = inopen ? keycmd(co_await getkey()) : co_await getch();
        goto again;
    }
    noteinp();
    co_return (ch == 'y');
}

/*
 * One character of a substitute's confirmation. Upstream read it from file
 * descriptor 2, which was the terminal even when the commands came from a
 * script; here it comes from the ordinary command input, so a scripted s///c
 * is answered by the script.
 */
Task<int> getch(void)
{
    int c;

    if (need_input())
        if ((co_await ex_readline()).is_err())
            co_return (EOF);
    c = getcd();
    co_return (c == EOF ? EOF : (c & TRIM));
}

void ugo(int cnt, int with)
{
    if (cnt > 0)
        do
            putchar(with);
        while (--cnt > 0);
}

int casecnt;
exbool destuc;

void dosub(void)
{
    char *lp, *sp;
    int *rp;
    int c;
    char b[4];
    usize k, i;

    lp = linebuf;
    sp = genbuf;
    rp = rhsbuf;
    while (lp < loc1)
        *sp++ = *lp++;
    casecnt = 0;
    while (c = *rp++) {
        if (c & QUOTE)
            switch (c & TRIM) {
            case '&':
                sp = place(sp, loc1, loc2);
                if (sp == 0)
                    goto ovflo;
                continue;

            case 'l':
                casecnt = 1;
                destuc  = 0;
                continue;

            case 'L':
                casecnt = LBSIZE;
                destuc  = 0;
                continue;

            case 'u':
                casecnt = 1;
                destuc  = 1;
                continue;

            case 'U':
                casecnt = LBSIZE;
                destuc  = 1;
                continue;

            case 'E':
            case 'e':
                casecnt = 0;
                continue;
            }
        if ((c & QUOTE) && (c &= TRIM) >= '1' && c < nbra + '1') {
            sp = place(sp, braslist[c - '1'], braelist[c - '1']);
            if (sp == 0)
                goto ovflo;
            continue;
        }
        k = utf8_encode((char32_t)(casecnt ? fixcase(c & TRIM) : (c & TRIM)), b);
        if (sp + k >= &genbuf[LBSIZE])
        ovflo:
            THROW(error("Line overflow@in substitute"));
        for (i = 0; i < k; i++)
            *sp++ = b[i];
    }
    lp   = loc2;
    loc2 = sp + (linebuf - genbuf);
    while (*sp++ = *lp++)
        if (sp >= &genbuf[LBSIZE])
            goto ovflo;
    strcLIN(genbuf);
}

int fixcase(int c)
{
    if (casecnt == 0)
        return (c);
    casecnt--;
    return ((int)(destuc ? rune_upper((char32_t)c) : rune_lower((char32_t)c)));
}

char *place(char *sp, char *l1, char *l2)
{
    while (l1 < l2) {
        char b[4];
        int n, c = runeat(l1, &n);
        usize k;

        l1 += n;
        k = utf8_encode((char32_t)fixcase(c), b);
        if (sp + k >= &genbuf[LBSIZE])
            return (0);
        for (usize i = 0; i < k; i++)
            *sp++ = b[i];
    }
    return (sp);
}

void snote(int total, int lines)
{
    if (!notable(total))
        return;
    printf(mesg("%d subs|%d substitutions"), total);
    if (lines != 1 && lines != total)
        printf(" on %d lines", lines);
    noonl();
    flush();
}

exbool compile(int eof, exbool oknl)
{
    int c;
    int *ep;
    int *lastep;
    int bracket[NBRA], *bracketp;
    int *rhsp;
    int cclcnt;

    if (isalpha(eof) || isdigit(eof))
        THROWV(0, error("Regular expressions cannot be delimited by letters or digits"));
    ep = expbuf;
    c  = getchar();
    if (eof == '\\')
        switch (c) {
        case '/':
        case '?':
            if (scanre.Expbuf[0] == 0)
                THROWV(0, error("No previous scan re|No previous scanning regular expression"));
            resre(scanre);
            return (c);

        case '&':
            if (subre.Expbuf[0] == 0)
                THROWV(
                    0,
                    error("No previous substitute re|No previous substitute regular expression"));
            resre(subre);
            return (c);

        default:
            THROWV(0, error("Badly formed re|Regular expression \\ must be followed by / or ?"));
        }
    if (c == eof || c == '\n' || c == EOF) {
        if (*ep == 0)
            THROWV(0, error("No previous re|No previous regular expression"));
        if (c == '\n' && oknl == 0)
            THROWV(0, error("Missing closing delimiter@for regular expression"));
        if (c != eof)
            ungetchar(c);
        return (eof);
    }
    bracketp = bracket;
    nbra     = 0;
    circfl   = 0;
    if (c == '^') {
        c = getchar();
        circfl++;
    }
    ungetchar(c);
    for (;;) {
        if (ep >= &expbuf[ESIZE - 2])
        complex:
            THROWV(0, cerror("Re too complex|Regular expression too complicated"));
        c = getrune();
        if (c == eof || c == EOF) {
            if (bracketp != bracket)
                THROWV(0, cerror("Unmatched \\(|More \\('s than \\)'s in regular expression"));
            *ep++ = CEOFC;
            if (c == EOF)
                ungetchar(c);
            return (eof);
        }
        if (value(MAGIC)) {
            if (c != '*' || ep == expbuf)
                lastep = ep;
        } else if (c != '\\' || peekchar() != '*' || ep == expbuf)
            lastep = ep;
        switch (c) {
        case '\\':
            c = getrune();
            switch (c) {
            case '(':
                if (nbra >= NBRA)
                    THROWV(0, cerror("Awash in \\('s!|Too many \\('d subexressions in a regular "
                                     "expression"));
                *bracketp++ = nbra;
                *ep++       = CBRA;
                *ep++       = nbra++;
                continue;

            case ')':
                if (bracketp <= bracket)
                    THROWV(0, cerror("Extra \\)|More \\)'s than \\('s in regular expression"));
                *ep++ = CKET;
                *ep++ = *--bracketp;
                continue;

            case '<':
                *ep++ = CBRC;
                continue;

            case '>':
                *ep++ = CLET;
                continue;
            }
            if (value(MAGIC) == 0)
            magic:
                switch (c) {
                case '.':
                    *ep++ = CDOT;
                    continue;

                case '~':
                    rhsp = rhsbuf;
                    while (*rhsp) {
                        if (*rhsp & QUOTE) {
                            c = *rhsp & TRIM;
                            if (c == '&')
                                THROWV(0,
                                       error("Replacement pattern contains &@- cannot use in re"));
                            if (c >= '1' && c <= '9')
                                THROWV(
                                    0,
                                    error("Replacement pattern contains \\d@- cannot use in re"));
                        }
                        if (ep >= &expbuf[ESIZE - 2])
                            goto complex;
                        *ep++ = CCHR;
                        *ep++ = *rhsp++ & TRIM;
                    }
                    continue;

                case '*':
                    if (ep == expbuf)
                        break;
                    if (*lastep == CBRA || *lastep == CKET)
                        THROWV(0, cerror("Illegal *|Can't * a \\( ... \\) in regular expression"));
                    if (*lastep == CCHR && (lastep[1] & QUOTE))
                        THROWV(0, cerror("Illegal *|Can't * a \\n in regular expression"));
                    *lastep |= STAR;
                    continue;

                case '[':
                    *ep++  = CCL;
                    *ep++  = 0;
                    cclcnt = 1;
                    c      = getrune();
                    if (c == '^') {
                        c      = getrune();
                        ep[-2] = NCCL;
                    }
                    if (c == ']')
                        THROWV(0, cerror("Bad character class|Empty character class '[]' or '[^]' "
                                         "cannot match"));
                    while (c != ']') {
                        if (c == '\\' && any(peekchar(), "]-^\\"))
                            c = getrune() | QUOTE;
                        if (c == '\n' || c == EOF)
                            THROWV(0, cerror("Missing ]"));
                        *ep++ = c;
                        cclcnt++;
                        if (ep >= &expbuf[ESIZE])
                            goto complex;
                        c = getrune();
                    }
                    lastep[1] = cclcnt;
                    continue;
                }
            if (c == EOF) {
                ungetchar(EOF);
                c = '\\';
                goto defchar;
            }
            *ep++ = CCHR;
            if (c == '\n')
                THROWV(
                    0,
                    cerror("No newlines in re's|Can't escape newlines into regular expressions"));
            /*
                                    if (c < '1' || c > NBRA + '1') {
            */
            *ep++ = c;
            continue;
            /*
                                    }
                                    c -= '1';
                                    if (c >= nbra)
            THROWV(0, cerror("Bad \\n|\\n in regular expression with n greater than the number of
            \\('s")); *ep++ = c | QUOTE; continue;
            */

        case '\n':
            if (oknl) {
                ungetchar(c);
                *ep++ = CEOFC;
                return (eof);
            }
            THROWV(0, cerror("Badly formed re|Missing closing delimiter for regular expression"));

        case '$':
            if (peekchar() == eof || peekchar() == EOF || oknl && peekchar() == '\n') {
                *ep++ = CDOL;
                continue;
            }
            goto defchar;

        case '.':
        case '~':
        case '*':
        case '[':
            if (value(MAGIC))
                goto magic;
        defchar:
        default:
            *ep++ = CCHR;
            *ep++ = c;
            continue;
        }
    }
}

void cerror(char *s)
{
    expbuf[0] = 0;
    THROW(error(s));
}

exbool same(int a, int b)
{
    return (a == b || (value(IGNORECASE) && rune_lower((char32_t)a) == rune_lower((char32_t)b)));
}

char *locs;

exbool execute(exbool gf, line *addr)
{
    char *p1;
    int *p2;
    int c;

    if (gf) {
        if (circfl)
            return (0);
        locs = p1 = loc2;
    } else {
        if (addr == zero)
            return (0);
        p1 = linebuf;
        getline(*addr);
        locs = 0;
    }
    p2 = expbuf;
    if (circfl) {
        loc1 = p1;
        return (advance(p1, p2));
    }
    /* fast check for first character */
    if (*p2 == CCHR) {
        c = p2[1];
        do {
            int n;

            if (!same(c, runeat(p1, &n)))
                continue;
            if (advance(p1, p2)) {
                loc1 = p1;
                return (1);
            }
        } while (*p1 && (p1 = nextchar(p1)));
        return (0);
    }
    /* regular algorithm */
    do {
        if (advance(p1, p2)) {
            loc1 = p1;
            return (1);
        }
    } while (*p1 && (p1 = nextchar(p1)));
    return (0);
}

#define uletter(c) rune_word(c)

exbool advance(char *lp, int *ep)
{
    char *curlp;
    char *sp, *sp1;
    int c, n;

    for (;;)
        switch (*ep++) {
        case CCHR:
            /* useless
                            if (*ep & QUOTE) {
                                    c = *ep++ & TRIM;
                                    sp = braslist[c];
                                    sp1 = braelist[c];
                                    while (sp < sp1) {
                                            if (!same(*sp, *lp))
                                                    return (0);
                                            sp++, lp++;
                                    }
                                    continue;
                            }
            */
            if (!same(*ep, runeat(lp, &n)))
                return (0);
            ep++, lp += n;
            continue;

        case CDOT:
            if (*lp) {
                ignore(runeat(lp, &n));
                lp += n;
                continue;
            }
            return (0);

        case CDOL:
            if (*lp == 0)
                continue;
            return (0);

        case CEOFC:
            loc2 = lp;
            return (1);

        case CCL:
            c = runeat(lp, &n), lp += n;
            if (cclass(ep, c, 1)) {
                ep += *ep;
                continue;
            }
            return (0);

        case NCCL:
            c = runeat(lp, &n), lp += n;
            if (cclass(ep, c, 0)) {
                ep += *ep;
                continue;
            }
            return (0);

        case CBRA:
            braslist[*ep++] = lp;
            continue;

        case CKET:
            braelist[*ep++] = lp;
            continue;

        case CDOT | STAR:
            curlp = lp;
            while (*lp) {
                ignore(runeat(lp, &n));
                lp += n;
            }
            lp++; /* the overshoot star: gives back */
            goto star;

        case CCHR | STAR:
            curlp = lp;
            while (same(*ep, runeat(lp, &n)))
                lp += n;
            lp++;
            ep++;
            goto star;

        case CCL | STAR:
        case NCCL | STAR:
            curlp = lp;
            do {
                c = runeat(lp, &n), lp += n ? n : 1;
            } while (cclass(ep, c, ep[-1] == (CCL | STAR)));
            ep += *ep;
            goto star;
        star:
            do {
                lp = lp > curlp ? prevchar(curlp, lp) : lp - 1;
                if (lp == locs)
                    break;
                if (advance(lp, ep))
                    return (1);
            } while (lp > curlp);
            return (0);

        case CBRC:
            if (lp == linebuf) /* upstream wrote expbuf, and meant this */
                continue;
            {
                int prev = runeat(prevchar(linebuf, lp), &n);

                if (uletter(runeat(lp, &n)) && !uletter(prev))
                    continue;
            }
            return (0);

        case CLET:
            if (!uletter(runeat(lp, &n)))
                continue;
            return (0);

        default:
            THROWV(0, error("Re internal error"));
        }
}

exbool cclass(int *set, int c, exbool af)
{
    int n;

    if (c == 0)
        return (0);
    if (value(IGNORECASE))
        c = (int)rune_lower((char32_t)c);
    n = *set++;
    while (--n)
        if (n > 2 && (set[1] & TRIM) == '-' && !(set[1] & QUOTE)) {
            if (c >= (set[0] & TRIM) && c <= (set[2] & TRIM))
                return (af);
            set += 3;
            n -= 2;
        } else if ((*set++ & TRIM) == c)
            return (af);
    return (!af);
}
