/*
 * The terminal. This replaces ex_tty.h.
 *
 * Upstream summarised a terminal as forty-odd termcap capabilities and spent
 * ex_put.c working out, per motion, which sequence of them was cheapest at the
 * line's baud rate. Braam has no escape sequences and no terminal type: the
 * screen is an array of cells and cursor addressing is indexing (Concept.md
 * §2.3). So there is nothing to look up and nothing to optimise.
 *
 * The capability variables stay, as constants describing an ideal CRT, because
 * every `if (AL)` and `if (state == VISUAL)` in ex_vadj.cpp and ex_vput.cpp
 * still has to choose the path it always chose. Only the emission behind them
 * is gone. Two of the constants are load-bearing rather than decorative:
 *
 *   IM/EI/IC null puts vinschar() down its "just put the character out"
 *   branch, so the terminal insert-mode simulation is unreachable rather than
 *   deleted; and OS false with EO false keeps every overstrike path out.
 */
#pragma once

#include "kernel/key.h"
#include "proc/screen.h"

/*
 * String capabilities. Real objects rather than literals, so that putpad() can
 * tell one from another by address -- SO and SE are the only two that still
 * mean anything, and they set an attribute on the cell rather than emit.
 * A null one means "the terminal cannot do this", which is how IM/EI/IC keep
 * vinschar() out of its terminal-insert half.
 */
extern char CAP_YES[];
extern char CAP_SO[];
extern char CAP_SE[];

#define AL CAP_YES     /* P* Add new blank line */
#define BC CAP_YES     /*    Back cursor */
#define BT CAP_YES     /* P  Back tab */
#define CD CAP_YES     /* P* Clear to end of display */
#define CE CAP_YES     /* P  Clear to end of line */
#define CL CAP_YES     /* P* Clear screen */
#define CM CAP_YES     /* P  Cursor motion */
#define DC CAP_YES     /* P* Delete character */
#define DL CAP_YES     /* P* Delete line sequence */
#define DM ((char *)0) /*    Delete mode (enter) */
#define DO CAP_YES     /*    Down line sequence */
#define ED ((char *)0) /*    End delete mode */
#define EI ((char *)0) /*    End insert mode */
#define HO CAP_YES     /*    Home cursor */
#define IC ((char *)0) /* P  Insert character */
#define IM ((char *)0) /*    Insert mode */
#define IP ((char *)0) /* P* Insert pad after char ins'd */
#define KE ((char *)0) /*    Keypad don't xmit */
#define KS ((char *)0) /*    Keypad start xmitting */
#define LL CAP_YES     /*    Quick to last line, column 0 */
#define ND CAP_YES     /*    Non-destructive space */
#define SE CAP_SE      /*    Standout end */
#define SF CAP_YES     /* P  Scroll forwards */
#define SO CAP_SO      /*    Stand out begin */
#define SR CAP_YES     /* P  Scroll backwards */
#define TA CAP_YES     /* P  Tab */
#define TE ((char *)0) /*    Terminal end sequence */
#define TI ((char *)0) /*    Terminal initial sequence */
#define UP CAP_YES     /*    Upline */
#define VB ((char *)0) /*    Visible bell */
#define VE ((char *)0) /*    Visual end sequence */
#define VS ((char *)0) /*    Visual start sequence */

/* Boolean capabilities. */
#define AM 0 /* Automatic margins */
#define BS 1 /* Backspace works */
#define CA 1 /* Cursor addressible */
#define DA 0 /* Display may be retained above */
#define DB 0 /* Display may be retained below */
#define EO 0 /* Can erase overstrikes with ' ' */
#define GT 0 /* Gtty indicates tabs */
#define HC 0 /* Hard copy terminal */
#define HZ 0 /* Hazeltine ~ braindamage */
#define IN 0 /* Insert-null blessing */
#define MI 0 /* can move in insert mode */
#define NC 0 /* No Cr */
#define NS 0 /* No scroll */
#define OS 0 /* Overstrike works */
#define UL 0 /* Underlining works even though !os */
#define XB 0 /* Beehive */
#define XN 0 /* A newline gets eaten after wrap */
#define XT 0 /* Tabs are destructive */

#define NONL      0 /* Terminal can't hack linefeeds doing a CR */
#define UPPERCASE 0 /* Ick! */

/* Defined with their defaults in ex_data.cpp. */
extern short LINES; /* Number of lines on screen */
extern short COLUMNS;

EXTERN short OCOLUMNS; /* Save COLUMNS for a hack in open mode */

EXTERN short outcol; /* Where the cursor is */
EXTERN short outline;

EXTERN short destcol; /* Where the cursor should be */
EXTERN short destline;

/*
 * Macros. Upstream's map, immacs and abbrevs tables; the arrow entries are
 * built from Braam's named keys rather than from termcap strings.
 */
#define MAXNOMACS   128  /* max number of macros of each kind */
#define MAXCHARMACS 2048 /* max # of chars total in macros */

struct maps {
    char *cap;   /* pressing button that sends this.. */
    char *mapto; /* .. maps to this string */
    char *descr; /* legible description of key */
};

EXTERN struct maps arrows[MAXNOMACS];  /* macro defs - 1st 5 built in */
EXTERN struct maps immacs[MAXNOMACS];  /* for while in insert mode */
EXTERN struct maps abbrevs[MAXNOMACS]; /* for word abbreviations */
EXTERN char mapspace[MAXCHARMACS];
EXTERN char *msnext;    /* next free location in mapspace */
EXTERN int maphopcnt;   /* check for infinite mapping loops */
EXTERN exbool anyabbrs; /* true if abbr or unabbr has been done */

/* The screen, while open or visual holds it. Null in ex mode. */
extern ProcScreen *vscreen;

/*
 * Upstream padded a capability out over the line at the current speed.
 * Nothing is emitted now, so this acts on the two that still mean something
 * and ignores the rest.
 */
void putpad(char *cap);
void standout(exbool on);

int key_byte(Key k);
int keycmd(int c);
Task<Result<void>> vscreen_take(void);
Task<void> vscreen_give(void);
