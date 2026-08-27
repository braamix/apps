/* Copyright (c) 1980 Regents of the University of California */
#include "ex.h"
#include "ex_buf.h"
#include "ex_screen.h"
#include "ex_vis.h"

/*
 * Routines to handle structure.
 * Operations supported are:
 *	( ) { } [ ]
 *
 * These cover:		LISP		TEXT
 *	( )		s-exprs		sentences
 *	{ }		list at same	paragraphs
 *	[ ]		defuns		sections
 *
 * { and } for C used to attempt to do something with matching {}'s, but
 * I couldn't find definitions which worked intuitively very well, so I
 * scrapped this.
 *
 * The code here is very hard to understand.
 */
line *llimit;

exbool wasend;

/*
 * Find over structure, repeated count times.
 * Don't go past line limit.  F is the operation to
 * be performed eventually.  If pastatom then the user said {}
 * rather than (), implying past atoms in a list (or a paragraph
 * rather than a sentence.
 */
int lfind(exbool pastatom, int cnt, Vopf f, line *limit)
{
    int c;
    int rc = 0;
    char save[LBSIZE];

    /*
     * Initialize, saving the current line buffer state
     * and computing the limit; a 0 argument means
     * directional end of file.
     */
    wasend = 0;
    lf     = (void *)f;
    strcpy(save, linebuf);
    if (limit == 0)
        limit = dir < 0 ? one : dol;
    llimit  = limit;
    wdot    = dot;
    wcursor = cursor;

    if (pastatom >= 2) {
        while (cnt > 0 && word(f, cnt))
            cnt--;
        if (pastatom == 3)
            eend(f);
        if (dot == wdot) {
            wdot = 0;
            if (cursor == wcursor)
                rc = -1;
        }
    } else {
        char *icurs;
        line *idot;

        if (linebuf[0] == 0) {
            do
                if (!lnext())
                    goto ret;
            while (linebuf[0] == 0);
            if (dir > 0) {
                wdot--;
                linebuf[0] = 0;
                wcursor    = linebuf;
                /*
                 * If looking for sentence, next line
                 * starts one.
                 */
                if (!pastatom) {
                    icurs = wcursor;
                    idot  = wdot;
                    goto begin;
                }
            }
        }
        icurs = wcursor;
        idot  = wdot;

        /*
         * Advance so as to not find same thing again.
         */
        if (dir > 0) {
            if (!lnext()) {
                rc = -1;
                goto ret;
            }
        } else
            ignore(lskipa1(""));

        /*
         * Count times find end of sentence/paragraph.
         */
    begin:
        for (;;) {
            while (!endsent(pastatom))
                if (!lnext())
                    goto ret;
            if (!pastatom || wcursor == linebuf && endPS())
                if (--cnt <= 0)
                    break;
            if (linebuf[0] == 0) {
                do
                    if (!lnext())
                        goto ret;
                while (linebuf[0] == 0);
            } else if (!lnext())
                goto ret;
        }

        /*
         * If going backwards, and didn't hit the end of the buffer,
         * then reverse direction.
         */
        if (dir < 0 && (wdot != llimit || wcursor != linebuf)) {
            dir    = 1;
            llimit = dot;
            /*
             * Empty line needs special treatement.
             * If moved to it from other than begining of next line,
             * then a sentence starts on next line.
             */
            if (linebuf[0] == 0 && !pastatom && (wdot != dot - 1 || cursor != linebuf)) {
                lnext();
                goto ret;
            }
        }

        /*
         * If we are not at a section/paragraph division,
         * advance to next.
         */
        if (wcursor == icurs && wdot == idot || wcursor != linebuf || !endPS())
            ignore(lskipa1(""));
    }
ret:
    strcLIN(save);
    return (rc);
}

/*
 * Is this the end of a sentence?
 */
exbool endsent(exbool pastatom)
{
    char *cp = wcursor;
    int c, d;

    /*
     * If this is the beginning of a line, then
     * check for the end of a paragraph or section.
     */
    if (cp == linebuf)
        return (endPS());

    /*
     * Sentences end with . ! ? not at the beginning
     * of the line, and must be either at the end of the line,
     * or followed by 2 spaces.  Any number of intervening ) ] ' "
     * characters are allowed.
     */
    if (!any(c = *cp, ".!?"))
        goto tryps;
    do
        if ((d = *++cp) == 0)
            return (1);
    while (any(d, ")]'"));
    if (*cp == 0 || *cp++ == ' ' && *cp == ' ')
        return (1);
tryps:
    if (cp[1] == 0)
        return (endPS());
    return (0);
}

/*
 * End of paragraphs/sections are respective
 * macros as well as blank lines and form feeds.
 */
exbool endPS(void)
{
    return (linebuf[0] == 0 || isa(svalue(PARAGRAPHS)) || isa(svalue(SECTIONS)));
}

int lmatchp(line *addr)
{
    int i;
    char *parens, *cp;

    for (cp = cursor; !any(*cp, "({[)}]");)
        if (*cp++ == 0)
            return (0);
    lf     = 0;
    parens = (char *)(any(*cp, "()") ? "()" : any(*cp, "[]") ? "[]" : "{}");
    if (*cp == parens[1]) {
        dir    = -1;
        llimit = one;
    } else {
        dir    = 1;
        llimit = dol;
    }
    if (addr)
        llimit = addr;
    if (splitw)
        llimit = dot;
    wcursor = cp;
    wdot    = dot;
    i       = lskipbal(parens);
    return (i);
}

void lsmatch(char *cp)
{
    char save[LBSIZE];
    char *sp    = save;
    char *scurs = cursor;

    wcursor = cp;
    strcpy(sp, linebuf);
    *wcursor = 0;
    strcpy(cursor, genbuf);
    cursor = strend(linebuf) - 1;
    if (lmatchp(dot - vcline)) {
        int i = insmode;
        int c = outcol;
        int l = outline;

        if (!MI)
            endim();
        vgoto(splitw ? WECHO : LINE(wdot - llimit), column(wcursor) - 1);
        flush();
        vgoto(l, c);
        if (i)
            goim();
    } else {
        strcLIN(sp);
        strcpy(scurs, genbuf);
        if (!lmatchp((line *)0))
            obeep();
    }
    strcLIN(sp);
    wdot    = 0;
    wcursor = 0;
    cursor  = scurs;
}

exbool ltosolid(void)
{
    return (ltosol1((char *)"()"));
}

exbool ltosol1(char *parens)
{
    char *cp;

    if (*parens && !*wcursor && !lnext())
        return (0);
    while (isspace(*wcursor) || (*wcursor == 0 && *parens))
        if (!lnext())
            return (0);
    if (any(*wcursor, parens) || dir > 0)
        return (1);
    for (cp = wcursor; cp > linebuf; cp--)
        if (isspace(cp[-1]) || any(cp[-1], parens))
            break;
    wcursor = cp;
    return (1);
}

exbool lskipbal(char *parens)
{
    int level = dir;
    int c;

    do {
        if (!lnext()) {
            wdot = NOLINE;
            return (0);
        }
        c = *wcursor;
        if (c == parens[1])
            level--;
        else if (c == parens[0])
            level++;
    } while (level);
    return (1);
}

exbool lskipatom(void)
{
    return (lskipa1("()"));
}

exbool lskipa1(char *parens)
{
    int c;

    for (;;) {
        if (dir < 0 && wcursor == linebuf) {
            if (!lnext())
                return (0);
            break;
        }
        c = *wcursor;
        if (c && (isspace(c) || any(c, parens)))
            break;
        if (!lnext())
            return (0);
        if (dir > 0 && wcursor == linebuf)
            break;
    }
    return (ltosol1(parens));
}

exbool lnext(void)
{
    if (dir > 0) {
        if (*wcursor)
            wcursor++;
        if (*wcursor)
            return (1);
        if (wdot >= llimit) {
            if (lf == (void *)op_move && wcursor > linebuf)
                wcursor--;
            return (0);
        }
        wdot++;
        getline(*wdot);
        wcursor = linebuf;
        return (1);
    } else {
        --wcursor;
        if (wcursor >= linebuf)
            return (1);
        if (wdot <= llimit) {
            wcursor = linebuf;
            return (0);
        }
        wdot--;
        getline(*wdot);
        wcursor = linebuf[0] == 0 ? linebuf : strend(linebuf) - 1;
        return (1);
    }
}

int lbrack(int c, Vopf f)
{
    line *addr;

    addr = dot;
    for (;;) {
        addr += dir;
        if (addr < one || addr > dol) {
            addr -= dir;
            break;
        }
        getline(*addr);
        if (linebuf[0] == '{' || isa(svalue(SECTIONS))) {
            if (c == ']' && f != op_move) {
                addr--;
                getline(*addr);
            }
            break;
        }
        if (c == ']' && f != op_move && linebuf[0] == '}')
            break;
    }
    if (addr == dot)
        return (0);
    if (f != op_move)
        wcursor = c == ']' ? strend(linebuf) : linebuf;
    else
        wcursor = 0;
    wdot    = addr;
    vmoving = 0;
    return (1);
}

exbool isa(char *cp)
{
    if (linebuf[0] != '.')
        return (0);
    for (; cp[0] && cp[1]; cp += 2)
        if (linebuf[1] == cp[0]) {
            if (linebuf[2] == cp[1])
                return (1);
            if (linebuf[2] == 0 && cp[1] == ' ')
                return (1);
        }
    return (0);
}
