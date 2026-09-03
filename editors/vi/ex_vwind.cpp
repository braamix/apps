/* Copyright (c) 1980 Regents of the University of California */
#include "ex.h"
#include "ex_buf.h"
#include "ex_screen.h"
#include "ex_vis.h"

/*
 * Routines to adjust the window, showing specified lines
 * in certain positions on the screen, and scrolling in both
 * directions.  Code here is very dependent on mode (open versus visual).
 */

/*
 * Move in a nonlocal way to line addr.
 * If it isn't on screen put it in specified context.
 * New position for cursor is curs.
 * Like most routines here, we vsave().
 */
void vmoveto(line *addr, char *curs, int context)
{
    markit(addr);
    vsave();
    vjumpto(addr, curs, context);
}

/*
 * Vjumpto is like vmoveto, but doesn't mark previous
 * context or save linebuf as current line.
 */
void vjumpto(line *addr, char *curs, int context)
{
    noteit(0);
    if (context != 0)
        vcontext(addr, context);
    else
        vshow(addr, NOLINE);
    noteit(1);
    vnline(curs);
}

/*
 * Go up or down cnt (negative is up) to new position curs.
 */
void vupdown(int cnt, char *curs)
{
    if (cnt > 0)
        vdown(cnt, 0, 0);
    else if (cnt < 0)
        vup(-cnt, 0, 0);
    if (vcnt == 0)
        vrepaint(curs);
    else
        vnline(curs);
}

/*
 * Go up cnt lines, afterwards preferring to be ind
 * logical lines from the top of the screen.
 * If scroll, then we MUST use a scroll.
 * Otherwise clear and redraw if motion is far.
 */
void vup(int cnt, int ind, exbool scroll)
{
    int i, tot;

    if (dot == one) {
        obeep();
        return;
    }
    vsave();
    i = lineDOT() - 1;
    if (cnt > i) {
        ind -= cnt - i;
        if (ind < 0)
            ind = 0;
        cnt = i;
    }
    if (!scroll && cnt <= vcline) {
        vshow(dot - cnt, NOLINE);
        return;
    }
    cnt -= vcline, dot -= vcline, vcline = 0;
    if (hold & HOLDWIG)
        goto contxt;
    if (state == VISUAL && !AL && !SR && cnt <= WTOP - ZERO && vfit(dot - cnt, cnt) <= WTOP - ZERO)
        goto okr;
    tot = WECHO - ZERO;
    if (state != VISUAL || (!AL && !SR) ||
        (!scroll && (cnt > tot || vfit(dot - cnt, cnt) > tot / 3 + 1))) {
        if (ind > basWLINES / 2)
            ind = basWLINES / 3;
    contxt:
        vcontext(dot + ind - cnt, '.');
        return;
    }
okr:
    vrollR(cnt);
    if (scroll) {
        vcline += ind, dot += ind;
        if (vcline >= vcnt)
            dot -= vcline - vcnt + 1, vcline = vcnt - 1;
        getDOT();
    }
}

/*
 * Like vup, but scrolling down.
 */
void vdown(int cnt, int ind, exbool scroll)
{
    int i, tot;

    if (dot == dol) {
        obeep();
        return;
    }
    vsave();
    i = dol - dot;
    if (cnt > i) {
        ind -= cnt - i;
        if (ind < 0)
            ind = 0;
        cnt = i;
    }
    i = vcnt - vcline - 1;
    if (!scroll && cnt <= i) {
        vshow(dot + cnt, NOLINE);
        return;
    }
    cnt -= i, dot += i, vcline += i;
    if (hold & HOLDWIG)
        goto dcontxt;
    if (!scroll) {
        tot = WECHO - ZERO;
        if (state != VISUAL || cnt - tot > 0 || vfit(dot, cnt) > tot / 3 + 1) {
        dcontxt:
            vcontext(dot + cnt, '.');
            return;
        }
    }
    if (cnt > 0)
        vroll(cnt);
    if (state == VISUAL && scroll) {
        vcline -= ind, dot -= ind;
        if (vcline < 0)
            dot -= vcline, vcline = 0;
        getDOT();
    }
}

/*
 * Show line addr in context where on the screen.
 * Work here is in determining new top line implied by
 * this placement of line addr, since we always draw from the top.
 */
void vcontext(line *addr, int where)
{
    line *top;

    getline(*addr);
    if (state != VISUAL)
        top = addr;
    else
        switch (where) {
        case '^':
            addr = vback(addr, basWLINES - vdepth());
            getline(*addr);
            /* fall into ... */

        case '-':
            top = vback(addr, basWLINES - vdepth());
            getline(*addr);
            break;

        case '.':
            top = vback(addr, basWLINES / 2 - vdepth());
            getline(*addr);
            break;

        default:
            top = addr;
            break;
        }
    if (state == ONEOPEN && LINE(0) == WBOT)
        vup1();
    vcnt = vcline = 0;
    vclean();
    if (state == CRTOPEN)
        vup1();
    vshow(addr, top);
}

/*
 * Get a clean line.  If we are in a hard open
 * we may be able to reuse the line we are on
 * if it is blank.  This is a real win.
 */
void vclean(void)
{
    if (state != VISUAL && state != CRTOPEN) {
        destcol = 0;
        if (!ateopr())
            vup1();
        vcnt = 0;
    }
}

/*
 * Show line addr with the specified top line on the screen.
 * Top may be 0; in this case have vcontext compute the top
 * (and call us recursively).  Eventually, we clear the screen
 * (or its open mode equivalent) and redraw.
 */
void vshow(line *addr, line *top)
{
    int cnt       = addr - dot;
    int i         = vcline + cnt;
    short oldhold = hold;

    if (state != HARDOPEN && state != ONEOPEN && i >= 0 && i < vcnt) {
        dot = addr;
        getDOT();
        vcline = i;
        return;
    }
    if (state != VISUAL) {
        dot = addr;
        vopen(dot, WBOT);
        return;
    }
    if (top == 0) {
        vcontext(addr, '.');
        return;
    }
    dot     = top;
    oldhold = hold;
    hold |= HOLDAT;
    vclear();
    vreset(0);
    vredraw(WTOP);
    /* error if vcline >= vcnt ! */
    vcline = addr - top;
    dot    = addr;
    getDOT();
    hold = oldhold;
    vsync(LASTLINE);
}

/*
 * reset the state.
 * If inecho then leave us at the beginning of the echo
 * area;  we are called this way in the middle of a :e escape
 * from visual, e.g.
 */
void vreset(exbool inecho)
{
    vcnt = vcline = 0;
    WTOP          = basWTOP;
    WLINES        = basWLINES;
    if (inecho)
        splitw = 1, vgoto(WECHO, 0);
}

/*
 * Starting from which line preceding tp uses almost (but not more
 * than) cnt physical lines?
 */
line *vback(line *tp, int cnt)
{
    int d;

    if (cnt > 0)
        for (; tp > one; tp--) {
            getline(tp[-1]);
            d = vdepth();
            if (d > cnt)
                break;
            cnt -= d;
        }
    return (tp);
}

/*
 * How much scrolling will it take to roll cnt lines starting at tp?
 */
int vfit(line *tp, int cnt)
{
    int j;

    j = 0;
    while (cnt > 0) {
        cnt--;
        getline(tp[cnt]);
        j += vdepth();
    }
    if (tp > dot)
        j -= WBOT - LASTLINE;
    return (j);
}

/*
 * Roll cnt lines onto the screen.
 */
void vroll(int cnt)
{
    short oldhold = hold;

    if (state != VISUAL)
        hold |= HOLDAT | HOLDROL;
    if (WBOT == WECHO) {
        vcnt = 0;
        if (state == ONEOPEN)
            vup1();
    }
    for (; cnt > 0 && Peekkey != ATTN; cnt--) {
        dot++, vcline++;
        vopen(dot, LASTLINE);
        vscrap();
    }
    hold = oldhold;
    if (state == HARDOPEN)
        sethard();
    vsyncCL();
}

/*
 * Roll backwards (scroll up).
 */
void vrollR(int cnt)
{
    short oldhold = hold;

    if (WBOT == WECHO)
        vcnt = 0;
    heldech = 0;
    hold |= HOLDAT | HOLDECH;
    for (; cnt > 0 && Peekkey != ATTN; cnt--) {
        dot--;
        vopen(dot, WTOP);
        vscrap();
    }
    hold = oldhold;
    if (heldech)
        vclrech(0);
    vsync(LINE(vcnt - 1));
}

/*
 * Go into cooked mode (allow interrupts) during
 * a scroll if we are at less than 1200 baud and not
 * a 'vi' command, of if we are in a 'vi' command and the
 * scroll is more than 2 full screens.
 *
 * BUG:		An interrupt during a scroll in this way
 *		dumps to command mode.
 */
int vcookit(int cnt)
{
    return (cnt > 1 && cnt > LINES * 2);
}

/*
 * Determine displayed depth of current line.
 */
int vdepth(void)
{
    int d;

    d = (column(NOSTR) + WCOLS - 1 + (Putchar == listchar) + IN) / WCOLS;
    return (d == 0 ? 1 : d);
}

/*
 * Move onto a new line, with cursor at position curs.
 */
void vnline(char *curs)
{
    if (curs)
        wcursor = curs;
    else if (vmoving)
        wcursor = vfindcol(vmovcol);
    else
        wcursor = vskipwh(linebuf);
    cursor = linebuf;
    vmove();
}
