/* Copyright (c) 1980 Regents of the University of California */
#include "ex.h"
#include "ex_buf.h"
#include "ex_re.h"
#include "ex_screen.h"
#include "ex_vis.h"

#include "kernel/alloc.h"

/*
 * Entry points to open and visual from command mode processor.
 * The open/visual code breaks down roughly as follows:
 *
 * ex_v.c	entry points, checking of terminal characteristics
 *
 * ex_vadj.c	logical screen control, use of intelligent operations
 *		insert/exdelete line and coordination with screen image;
 *		updating of screen after changes.
 *
 * ex_vget.c	input of single keys and reading of input lines
 *		from the echo area, handling of \ escapes on input for
 *		uppercase only terminals, handling of memory for repeated
 *		commands and small saved texts from inserts and partline
 *		deletes, notification of multi line changes in the echo
 *		area.
 *
 * ex_vmain.c	main command decoding, some command processing.
 *
 * ex_voperate.c   decoding of operator/operand sequences and
 *		contextual scans, implementation of word motions.
 *
 * ex_vops.c	major operator interfaces, undos, motions, deletes,
 *		changes, opening new lines, shifts, replacements and yanks
 *		coordinating logical and physical changes.
 *
 * ex_vops2.c	subroutines for operator interfaces in ex_vops.c,
 *		insert mode, read input line processing at lowest level.
 *
 * ex_vops3.c	structured motion definitions of ( ) { } and [ ] operators,
 *		indent for lisp routines, () and {} balancing.
 *
 * ex_vput.c	output routines, clearing, physical mapping of logical cursor
 *		positioning, cursor motions, handling of insert character
 *		and exdelete character functions of intelligent and unintelligent
 *		terminals, visual mode tracing routines (for debugging),
 *		control of screen image and its updating.
 *
 * ex_vwind.c	window level control of display, forward and backward rolls,
 *		absolute motions, contextual displays, line depth determination
 */

void ovbeg(void)
{
    if (!value(OPEN))
        THROW(error("Can't use open/visual unless open option is set"));
    if (inopen)
        THROW(error("Recursive open/visual not allowed"));
    Vlines = lineDOL();
    fixzero();
    setdot();
    pastwh();
    dot = addr2;
}

Task<void> ovend(void)
{
    splitw++;
    vgoto(WECHO, 0);
    vclreol();
    vgoto(WECHO, 0);
    holdcm = 0;
    splitw = 0;
    co_await vflush();
    co_await vscreen_give();
    setoutt();
    undvis();
    COLUMNS = OCOLUMNS;
    inopen  = 0;
    flusho();
    netchHAD(Vlines);
}

/*
 * Enter visual mode
 */
Task<void> vop(void)
{
    int c, size;

    ovbeg();
    COCHK;
    bastate = VISUAL;
    c       = 0;
    if (any(peekchar(), "+-^."))
        c = getchar();
    pastwh();
    size = isdigit(peekchar()) ? getnum() : -1;
    newline();
    COCHK;

    /* Before the claim, so a failure throws with the screen still the shell's. */
    if (atube == 0)
        atube = (char *)heap_alloc(TUBESIZE + LBSIZE);
    if (atube == 0)
        COTHROW(error("Out of memory@- no room for the screen image"));

    /* Taken here, given back in ovend(): command mode wants neither. */
    if ((co_await vscreen_take()).is_err())
        COTHROW(error("Visual needs a terminal"));

    /*
     * The grid is the screen, and it is only sized once the claim is in. So
     * the window is settled here rather than beside the count, which is where
     * upstream settled it -- there LINES was known before visual was entered.
     */
    setsize((int)vscreen->grid().rows, (int)vscreen->grid().cols);
    vsetsiz(size >= 0 ? size : value(WINDOW));
    setwind();
    vok(atube);
    if (!inglobal)
        savevis();
    Outchar = vputchar;
    vmoving = 0;
    if (initev == 0) {
        vcontext(dot, c);
        vnline(NOSTR);
    }
    co_await vmain();
    Command = (char *)"visual";
    co_await ovend();
}

/*
 * Hack to allow entry to visual with
 * empty buffer since routines internally
 * demand at least one line.
 */
void fixzero(void)
{
    if (dol == zero) {
        exbool ochng = chng;

        vdoappend("");
        if (!ochng)
            sync();
        addr1 = addr2 = one;
    } else if (addr2 == zero)
        addr2 = one;
}

/*
 * Save lines before visual between unddol and truedol.
 * Accomplish this by throwing away current [unddol,truedol]
 * and then saving all the lines in the buffer and moving
 * unddol back to dol.  Don't do this if in a global.
 *
 * If you do
 *	g/xxx/vi.
 * and then do a
 *	:e xxxx
 * at some point, and then quit from the visual and undo
 * you get the old file back.  Somewhat weird.
 */
void savevis(void)
{
    if (inglobal)
        return;
    truedol = unddol;
    saveall();
    unddol  = dol;
    undkind = UNDNONE;
}

/*
 * Restore a sensible state after a visual/open, moving the saved
 * stuff back to [unddol,dol], and killing the partial line kill indicators.
 */
void undvis(void)
{
    squish();
    pkill[0] = pkill[1] = 0;
    unddol              = truedol;
    unddel              = zero;
    undap1              = one;
    undap2              = dol + 1;
    undkind             = UNDALL;
    if (undadot <= zero || undadot > dol)
        undadot = zero + 1;
}

/*
 * Set the window parameters based on the base state bastate
 * and the available buffer space.
 */
void setwind(void)
{
    WCOLS = COLUMNS;
    switch (bastate) {
    case ONEOPEN:
        if (AM)
            WCOLS--;
        /* fall into ... */

    case HARDOPEN:
        basWTOP = WTOP = WBOT = WECHO = 0;
        ZERO                          = 0;
        holdcm++;
        break;

    case CRTOPEN:
        basWTOP = LINES - 2;
        /* fall into */

    case VISUAL:
        ZERO = LINES - TUBESIZE / WCOLS;
        if (ZERO < 0)
            ZERO = 0;
        if (ZERO > basWTOP)
            THROW(error("Screen too large for internal buffer"));
        WTOP  = basWTOP;
        WBOT  = LINES - 2;
        WECHO = LINES - 1;
        break;
    }
    state     = bastate;
    basWLINES = WLINES = WBOT - WTOP + 1;
}

/*
 * Can we hack an open/visual on this terminal?
 * If so, then divide the screen buffer up into lines,
 * and initialize a bunch of state variables before we start.
 */
void vok(char *atube)
{
    int i;

    if (WCOLS == 1000)
        THROW(serror("Don't know enough about your terminal to use %s", Command));
    if (WCOLS > TUBECOLS)
        THROW(error("Terminal too wide"));
    if (WLINES >= TUBELINES || WCOLS * (WECHO - ZERO + 1) > TUBESIZE)
        THROW(error("Screen too large"));

    vtube0 = atube;
    vclrbyte(atube, WCOLS * (WECHO - ZERO + 1));
    for (i = 0; i < ZERO; i++)
        vtube[i] = (char *)0;
    for (; i <= WECHO; i++)
        vtube[i] = atube, atube += WCOLS;
    for (; i < TUBELINES; i++)
        vtube[i] = (char *)0;
    vutmp    = atube;
    vundkind = VNONE;
    vUNDdot  = 0;
    OCOLUMNS = COLUMNS;
    inopen   = 1;
    vmoving  = 0;
    splitw   = 0;
    doomed   = 0;
    holdupd  = 0;
    Peekkey  = 0;
    vcnt = vcline = 0;
    if (vSCROLL == 0)
        vSCROLL = (value(WINDOW) + 1) / 2; /* round up so dft=6,11 */
}

/*
 * Set the size of the screen to size lines, to take effect the
 * next time the screen is redrawn.
 */
void vsetsiz(int size)
{
    int b;

    if (bastate != VISUAL)
        return;
    b = LINES - 1 - size;
    if (b >= LINES - 1)
        b = LINES - 2;
    if (b < 0)
        b = 0;
    basWTOP   = b;
    basWLINES = WBOT - b + 1;
}
