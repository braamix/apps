/* Copyright (c) 1980 Regents of the University of California */
/* sccs id:	@(#)ex_vis.h	6.1 10/18/80  */
/*
 * Ex version 3
 * Mark Horton, UCB
 * Bill Joy UCB
 *
 * Open and visual mode definitions.
 *
 * There are actually 4 major states in open/visual modes.  These
 * are visual, crt open (where the cursor can move about the screen and
 * the screen can scroll and be erased), one line open (on dumb glass-crt's
 * like the adm3), and hardcopy open (for everything else).
 *
 * The basic state is given by bastate, and the current state by state,
 * since we can be in pseudo-hardcopy mode if we are on an adm3 and the
 * line is longer than 80.
 *
 * Only VISUAL is reachable here: open mode existed for terminals that could
 * not address a cursor, and a cell grid always can. The other three states
 * and the code that tests for them are left in place rather than excised —
 * `state != VISUAL` is asked in some forty places, and dead branches are
 * cheaper than forty edits.
 */
#pragma once

EXTERN short bastate;
EXTERN short state;

#define VISUAL   0
#define CRTOPEN  1
#define ONEOPEN  2
#define HARDOPEN 3

/*
 * The screen in visual and crtopen is of varying size; the basic
 * window has top basWTOP and basWLINES lines are thereby implied.
 * The current window (which may have grown from the basic size)
 * has top WTOP and WLINES lines.  The top line of the window is WTOP,
 * and the bottom line WBOT.  The line WECHO is used for messages,
 * search strings and the like.  If WBOT==WECHO then we are in ONEOPEN
 * or HARDOPEN and there is no way back to the line we were on if we
 * go to WECHO (i.e. we will have to scroll before we go there, and
 * we can't get back).  There are WCOLS columns per line.
 * If WBOT!=WECHO then WECHO will be the last line on the screen
 * and WBOT is the line before it.
 */
EXTERN short basWTOP;
EXTERN short basWLINES;
EXTERN short WTOP;
EXTERN short WBOT; /* upstream kept WBOT and WECHO in ex_tty.h */
EXTERN short WLINES;
EXTERN short WCOLS;
EXTERN short WECHO;

/*
 * When we are dealing with the echo area we consider the window
 * to be "split" and set the variable splitw.  Otherwise, moving
 * off the bottom of the screen into WECHO causes a screen rollup.
 */
EXTERN exbool splitw;

/*
 * Information about each line currently on the screen includes
 * the y coordinate associated with the line, the printing depth
 * of the line (0 indicates unknown), and a mask which indicates
 * whether the line is "unclean", i.e. whether we should check
 * to make sure the line is displayed correctly at the next
 * appropriate juncture.
 */
struct vlinfo {
    char vliny;   /* Y coordinate */
    char vdepth;  /* Depth of displayed line */
    short vflags; /* Is line potentially dirty ? */
};

EXTERN struct vlinfo vlinfo[TUBELINES + 2];

#define DEPTH(c) (vlinfo[c].vdepth)
#define LINE(c)  (vlinfo[c].vliny)
#define FLAGS(c) (vlinfo[c].vflags)

#define VDIRT 1

#define vlcopy(i, j) i = j

/*
 * The current line on the screen is represented by vcline.
 * There are vcnt lines on the screen, the last being "vcnt - 1".
 * Vcline is intimately tied to the current value of dot,
 * and when command mode is used as a subroutine fancy footwork occurs.
 */
EXTERN short vcline;
EXTERN short vcnt;

/*
 * To allow many optimizations on output, an exact image of the terminal
 * screen is maintained in the space addressed by vtube0.  The vtube
 * array indexes this space as lines, and is shuffled on scrolls, insert+delete
 * lines and the like rather than (more expensively) shuffling the screen
 * data itself.  It is also rearranged during insert mode across line
 * boundaries to make incore work easier.
 *
 * This is now the back buffer proper: ex_screen.cpp's vflush() copies it into
 * the Grid, which keeps the damage, and one syscall sends the frame. vtube
 * cannot simply *be* the Grid, because the shuffling above moves row pointers
 * and a Cell grid is contiguous.
 */
EXTERN char *vtube[TUBELINES];
EXTERN char *vtube0;

/* Standout, alongside vtube: one attrs byte per cell. */
EXTERN char *vatube0;
EXTERN exbool vstandout;

/*
 * The current cursor position within the current line is kept in
 * cursor.  The current line is kept in linebuf.  During insertions
 * we use the auxiliary array genbuf as scratch area.
 * The cursor wcursor and wdot are used in operations within/spanning
 * lines to mark the other end of the affected area, or the target
 * for a motion.
 */
EXTERN char *cursor;
EXTERN char *wcursor;
EXTERN line *wdot;

/*
 * Undo information is saved in a LBSIZE buffer at "vutmp" for changes
 * within the current line, or as for command mode for multi-line changes
 * or changes on lines no longer the current line.
 * The change kind "VCAPU" is used immediately after a U undo to prevent
 * two successive U undo's from destroying the previous state.
 */
#define VNONE    0
#define VCHNG    1
#define VMANY    2
#define VCAPU    3
#define VMCHNG   4
#define VMANYINS 5

EXTERN short vundkind; /* Which kind of undo - from above */
EXTERN char *vutmp;    /* Prev line image when "VCHNG" */

/*
 * State information for undoing of macros.  The basic idea is that
 * if the macro does only 1 change or even none, we don't treat it
 * specially.  If it does 2 or more changes we want to be able to
 * undo it as a unit.  We remember how many changes have been made
 * within the current macro.  (Remember macros can be nested.)
 */
#define VC_NOTINMAC   0 /* Not in a macro */
#define VC_NOCHANGE   1 /* In a macro, no changes so far */
#define VC_ONECHANGE  2 /* In a macro, one change so far */
#define VC_MANYCHANGE 3 /* In a macro, at least 2 changes so far */

EXTERN short vch_mac; /* Change state - one of the above */

/*
 * For U undo's the line is grabbed by "vmove" after it first appears
 * on that line.  The "vUNDdot" which specifies which line has been
 * saved is selectively cleared when changes involving other lines
 * are made, i.e. after a 'J' join.  This is because a 'JU' would
 * lose completely the text of the line just joined on.
 */
/*
 * The bounds of what a change touched within the line, for the U undo.
 * Upstream declared these in ex_vops.c, where C's common symbols merged the
 * declaration with the definition.
 */
extern char *vUD1, *vUD2, *vUA1, *vUA2; /* defined in ex_vops.cpp */

EXTERN char *vUNDcurs; /* Cursor just before 'U' */
EXTERN line *vUNDdot;  /* The line address of line saved in vUNDsav */
EXTERN line vUNDsav;   /* Grabbed initial "*dot" */

#define killU() vUNDdot = NOLINE

/*
 * There are a number of cases where special behaviour is needed
 * from deeply nested routines.  This is accomplished by setting
 * the bits of hold, which acts to change the state of the general
 * visual editing behaviour in specific ways.
 *
 * HOLDAT prevents the clreol (clear to end of line) routines from
 * putting out @'s or ~'s on empty lines.
 *
 * HOLDDOL prevents the reopen routine from putting a '$' at the
 * end of a reopened line in list mode (for hardcopy mode, e.g.).
 *
 * HOLDROL prevents spurious blank lines when scrolling in hardcopy
 * open mode.
 *
 * HOLDQIK prevents the fake insert mode during repeated commands.
 *
 * HOLDPUPD prevents updating of the physical screen image when
 * mucking around while in insert mode.
 *
 * HOLDECH prevents clearing of the echo area while rolling the screen
 * backwards (e.g.) in deference to the clearing of the area at the
 * end of the scroll (1 time instead of n times).  The fact that this
 * is actually needed is recorded in heldech, which says that a clear
 * of the echo area was actually held off.
 */
EXTERN short hold;
EXTERN short holdupd; /* Hold off update when echo line is too long */

#define HOLDAT   1
#define HOLDDOL  2
#define HOLDROL  4
#define HOLDQIK  8
#define HOLDPUPD 16
#define HOLDECH  32
#define HOLDWIG  64

/*
 * Miscellaneous variables
 */
EXTERN short CDCNT;      /* Count of ^D's in insert on this line */
EXTERN char DEL[VBSIZE]; /* Last deleted text */
EXTERN exbool HADUP;     /* This insert line started with ^ then ^D */
EXTERN exbool HADZERO;   /* This insert line started with 0 then ^D */
EXTERN char INS[VBSIZE]; /* Last inserted text */
EXTERN int Vlines;       /* Number of file lines "before" vi command */
EXTERN int Xcnt;         /* External variable holding last cmd's count */
EXTERN exbool Xhadcnt;   /* Last command had explicit count? */
EXTERN short ZERO;
EXTERN short dir;                  /* Direction for search (+1 or -1) */
EXTERN short doomed;               /* Disply chars right of cursor to be killed */
EXTERN exbool gobblebl;            /* Wrapmargin space generated nl, eat a space */
EXTERN exbool hadcnt;              /* (Almost) internal to vmain() */
EXTERN exbool heldech;             /* We owe a clear of echo area */
EXTERN exbool insmode;             /* Are in character insert mode */
EXTERN char lastcmd[5];            /* Chars in last command */
EXTERN int lastcnt;                /* Count for last command */
EXTERN char *lastcp;               /* Save current command here to repeat */
EXTERN exbool lasthad;             /* Last command had a count? */
EXTERN short lastvgk;              /* Previous input key, if not from keyboard */
EXTERN short lastreg;              /* Register with last command */
EXTERN char *ncols['z' - 'a' + 2]; /* Cursor positions of marks */
EXTERN char *notenam;              /* Name to be noted with change count */
EXTERN char *notesgn;              /* Change count from last command */
EXTERN char op;                    /* Operation of current command */
EXTERN short Peekkey;              /* Peek ahead key */
EXTERN exbool rubble;              /* Line is filthy (in hardcopy open), redraw! */
EXTERN int vSCROLL;                /* Number lines to scroll on ^D/^U */
EXTERN char *vglobp;               /* Untyped input (e.g. repeat insert text) */
EXTERN char vmacbuf[VBSIZE];       /* Text of visual macro, hence nonnestable */
EXTERN char *vmacp;                /* Like vglobp but for visual macros */
EXTERN char *vmcurs;               /* Cursor for restore after undo d), e.g. */
EXTERN short vmovcol;              /* Column to try to keep on arrow keys */
EXTERN exbool vmoving;             /* Are trying to keep vmovcol */
EXTERN char vreg;                  /* Register for this command */
EXTERN short wdkind;               /* Liberal/conservative words? */
EXTERN char workcmd[5];            /* Temporary for lastcmd */

/*
 * A cursor key pressed inside an insertion, for vappend to act on once the
 * line is whole: it ends the insertion, moves, and opens another where it
 * landed, which is what keeps the user inside insert mode.
 */
EXTERN int insmotion;

/* Whether an insertion is open, which is what the mode line says. */
EXTERN exbool inserting;

/*
 * Macros
 */
#define INF       30000
#define LASTLINE  LINE(vcnt)
#define OVERBUF   QUOTE
#define beep      obeep
#define cindent() ((outline - vlinfo[vcline].vliny) * WCOLS + outcol)
/*
 * Upstream padded a capability out over the line at the current baud rate.
 * Upstream emitted a byte here, and padded a capability out over the line at
 * the current speed. The store into vtube beside every one of these is what
 * reaches the screen now -- vtube is the screen image, and ex_screen.cpp blits
 * it -- so all of them are nothing. The names stay because they are written at
 * some forty places, and each one marks where a byte used to go.
 */
#define vputc(c)       ((void)0)
#define vputp(cp, cnt) ((void)0)
#define fgoto()        ((void)0)
#define goim()         ((void)0)
#define endim()        ((void)0)
#define godm()         ((void)0)
#define enddm()        ((void)0)
#define tputs(s, n, f) ((void)0)

/*
 * The type of an operator, and of what vremote() applies to a range.
 *
 * Upstream kept these in an `int (*)()` and called them with whatever
 * arguments suited; K&R matched by position and asked no questions. C++ wants
 * one type, and three of the eight functions involved await -- so the type is
 * the awaiting one, and the five that do not each get a wrapper below rather
 * than being made to await for the table's sake.
 */
typedef Task<void> (*Vopf)(int);

/*
 * lfind() remembers what it was asked for in lf, and the lisp code asks
 * whether that was a plain move or an indent. It is never called through --
 * only compared -- and the two things compared against it have unrelated
 * types, so it is a tag rather than a function pointer.
 */
EXTERN void *lf;

Task<void> op_move(int c);
Task<void> op_beep(int c);
Task<void> op_yank(int c);
Task<void> op_delete(int c);
Task<void> op_shift(int c);
Task<void> op_yankreg(int c);
Task<void> op_put(int c);
Task<void> op_putreg(int c);
Task<void> op_join(int c);
Task<void> op_filter(int c);

/* ---------------------------------------------------------------- ex_v.cpp */
void ovbeg(void);
Task<void> ovend(void);
Task<void> vop(void);
void fixzero(void);
void savevis(void);
void undvis(void);
void setwind(void);
/* The screen image. Upstream kept it on vop's stack, deliberately leaked,
 * standing in for an alloca it could not write; vop is a coroutine, and a
 * frame past 512 bytes costs a whole 64 KiB span. A heap block rather than a
 * global: static data has to fit in the initial memory, and this is 64 KiB.
 * Claimed on the first visual and kept, which is upstream's leak by another
 * name. */
EXTERN char *atube;
void vok(char *atube);
void vsetsiz(int size);

/* ------------------------------------------------------------- ex_vadj.cpp */
void vopen(line *tp, int p);
int vreopen(int p, int lineno, int l);
int vglitchup(int l, int o);
void vinslin(int p, int cnt, int l);
void vopenup(int cnt, exbool could, int l);
void vadjAL(int p, int cnt);
void vrollup(int dl);
void vup1(void);
void vmoveitup(int cnt, exbool doclr);
void vscroll(int cnt);
void vscrap(void);
void vrepaint(char *curs);
void vredraw(int p);
void vdellin(int p, int cnt, int l);
void vadjDL(int p, int cnt);
void vsyncCL(void);
void vsync(int p);
void vsync1(int p);
void vcloseup(int l, int cnt);
void vreplace(int l, int cnt, int newcnt);
void sethard(void);
void vdirty(int base, int i);

/* ------------------------------------------------------------- ex_vget.cpp */
void ungetkey(int c);
Task<int> getkey(void);
Task<int> peekbr(void);
Task<int> getbr(void);
Task<int> getesc(void);
Task<int> peekkey(void);
Task<int> readecho(int c);
void setLAST(void);
void addtext(char *cp);
void setDEL(void);
void setBUF(char *BUF);
void addto(char *buf, char *str);
exbool noteit(exbool must);
void obeep(void);
Task<int> map(int c, struct maps *maps);
void macpush(char *st, int canundo = 0);
Task<int> vgetcnt(void);
Task<int> fastpeekkey(void);

/* ------------------------------------------------------------ ex_vmain.cpp */
Task<void> vmain(void);
Task<void> grabtag(void);
void prepapp(void);
Task<void> vremote(int cnt, Vopf f, int arg);
void vsave(void);
Task<void> vzop(exbool hadcnt, int cnt, int c);

/* ------------------------------------------------------------ ex_voper.cpp */
Task<void> operate(int c, int cnt);
Task<exbool> find(int c);
exbool word(Vopf op, int cnt);
void eend(Vopf op);
exbool wordof(int which, char *wc);
int wordch(char *wc);
exbool edge(void);
exbool margin(void);

/* ------------------------------------------------------------- ex_vops.cpp */
Task<void> vUndo(void);
Task<void> vundo(exbool show);
Task<void> vmacchng(exbool fromvis);
void vnoapp(void);
void vmove(void);
Task<void> vdelete(int c);
Task<void> vchange(int c);
Task<void> voOpen(int c, int cnt);
Task<void> vshftop(int c);
Task<void> vfilter(int c);
Task<int> xdw(void);
void vshift(void);
Task<void> vrep(int cnt);
Task<void> vyankit(int c);
void setpk(void);

/* ------------------------------------------------------------ ex_vops2.cpp */
void bleep(int i, char *cp);
int vdcMID(void);
void takeout(char *BUF);
exbool ateopr(void);
Task<void> vappend(int ch, int cnt, int indent);
Task<exbool> vinsmove(void);
void back1(void);
Task<char *> vgetline(int cnt, char *gcursor, exbool *aescaped, int commch);
void vdoappend(char *lp);
int vmaxrep(int ch, int cnt);
int vinschar(int c);

/* ------------------------------------------------------------ ex_vops3.cpp */
int lfind(exbool pastatom, int cnt, Vopf f, line *limit);
exbool endsent(exbool pastatom);
exbool endPS(void);
int lindent(line *addr);
int lmatchp(line *addr);
void lsmatch(char *cp);
exbool ltosolid(void);
exbool ltosol1(char *parens);
exbool lskipbal(char *parens);
exbool lskipatom(void);
exbool lskipa1(char *parens);
exbool lnext(void);
int lbrack(int c, Vopf f);
exbool isa(char *cp);

/* ------------------------------------------------------------- ex_vput.cpp */
void vclear(void);
void vclrbyte(char *cp, int i);
void vclrlin(int l, line *tp);
void vclreol(void);
void vclrech(exbool didphys);
void fixech(void);
void vcursbef(char *cp);
void vcursat(char *cp);
void vcursaft(char *cp);
void vfixcurs(void);
void vsetcurs(char *nc);
void vigoto(int y, int x);
void vcsync(void);
void vgotoCL(int x);
void vigotoCL(int x);
void vgoto(int y, int x);
void vgotab(void);
void vprepins(void);
void vmaktop(int p, char *cp);
void vneedpos(int cnt);
void vnpins(int dosync);
void vishft(void);
void viin(int c);
void vrigid(void);
void physdc(int stcol, int endcol);
int vputchar(int c);
void tfixnl(void);
int vputch(int c);

/* ------------------------------------------------------------ ex_vwind.cpp */
void vmoveto(line *addr, char *curs, int context);
void vjumpto(line *addr, char *curs, int context);
void vupdown(int cnt, char *curs);
void vup(int cnt, int ind, exbool scroll);
void vdown(int cnt, int ind, exbool scroll);
void vcontext(line *addr, int where);
void vclean(void);
void vshow(line *addr, line *top);
void vreset(exbool inecho);
line *vback(line *tp, int cnt);
int vfit(line *tp, int cnt);
void vroll(int cnt);
void vrollR(int cnt);
int vcookit(int cnt);
int vdepth(void);
void vnline(char *curs);
