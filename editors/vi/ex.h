/* Copyright (c) 1980 Regents of the University of California */
/* sccs id:	@(#)ex.h	6.1 10/18/80  */
/*
 * Ex version 3 (see exact version in ex_cmds.c, search for /Version/)
 *
 * Mark Horton, UC Berkeley
 * Bill Joy, UC Berkeley
 * November 1979
 *
 * This file contains most of the declarations common to a large number
 * of routines.  The file ex_vis.h contains declarations
 * which are used only inside the screen editor.
 * The file ex_tune.h contains parameters which can be diddled per installation.
 *
 * The declarations relating to the argument list, regular expressions,
 * the temporary file data structure used by the editor
 * and the data describing terminals are each fairly substantial and
 * are kept in the files ex_{argv,re,temp,tty}.h which
 * we #include separately.
 *
 * If you are going to dig into ex, you should look at the outline of the
 * distribution of the code into files at the beginning of ex.c and ex_v.c.
 * Code which is similar to that of ed is lightly or undocumented in spots
 * (e.g. the regular expression code).  Newer code (e.g. open and visual)
 * is much more carefully documented, and still rough in spots.
 *
 * ---
 *
 * Ported to Braam. Three things changed here and are worth knowing before
 * reading any of the .cpp files:
 *
 *  - Every global was a C tentative definition. C++ has no common symbols, so
 *    they are EXTERN, and ex_data.cpp defines them by defining EX_DEFINE.
 *  - typedef short bool is now `exbool`, since bool is a keyword. It stays a
 *    short: `inopen` takes the value -1.
 *  - setjmp is gone. error() records rather than unwinds; see ex_err.h.
 */
#pragma once

#include "braam.h"
#include "proc/io.h"
#include "proc/rt.h"

/* One definition of every global, in ex_data.cpp; a declaration everywhere. */
#ifdef EX_DEFINE
#define EXTERN
#else
#define EXTERN extern
#endif

typedef int line;
typedef short exbool;

#include "ex_err.h"
#include "ex_tune.h"
#include "ex_vars.h"

/*
 * Options in the editor are referred to usually by "value(name)" where
 * name is all uppercase, i.e. "value(PROMPT)".  This is actually a macro
 * which expands to a fixed field in a static structure and so generates
 * very little code.
 */
struct option {
    char *oname;
    char *oabbrev;
    short otype;    /* Types -- see below */
    short odefault; /* Default value */
    short ovalue;   /* Current value */
    char *osvalue;
};

#define ONOFF   0
#define NUMERIC 1
#define STRING  2 /* SHELL or DIRECTORY */
#define OTERM   3

/*
 * Result::value() cannot be spelled anywhere below, because value(a) is a
 * macro for an option's current setting and would eat it. Defined here, above
 * the macro, so that the one place needing it can reach through.
 */
template <class T>
inline T &res_of(Result<T> &r)
{
    return r.value();
}

#define value(a)  options[a].ovalue
#define svalue(a) options[a].osvalue

extern struct option options[NOPTS + 1];

#define BUFSIZ 1024
#ifndef NULL
#define NULL 0
#endif
#define EOF (-1)

/*
 * Character constants and bits
 *
 * The editor uses the QUOTE bit as a flag to pass on with characters
 * e.g. to the putchar routine.  The editor never uses a simple char variable.
 * Only arrays of and pointers to characters are used and parameters and
 * registers are never declared character.
 */
/*
 * A codepoint, and the flag above it: upstream had seven bits of character
 * and bit 7 for the flag, which is the bit UTF-8 needs.
 */
#define QUOTE 0x00200000
#define TRIM  0x001fffff

/*
 * The two bytes UTF-8 can never produce, for the sentinels that have to live
 * in a char buffer beside text: an embedded NUL, kept out of the string
 * terminator, and OVERBUF (ex_vis.h) for a buffer that overflowed.
 */
#define NULMARK 0376
/*
 * Note the quotes, which upstream did not need. A pre-ANSI preprocessor
 * substituted a macro parameter inside a character constant, so `CTRL(v)` in
 * 1980 expanded to ('v' & 037); C++ leaves 'c' alone, and every CTRL() in the
 * tree would quietly be ('c' & 037), which is 3. The call sites carry the
 * quotes now and this takes what it is given.
 */
#define CTRL(c) ((c) & 037)
#define NL      CTRL('j')
#define CR      CTRL('m')
#define DELETE  0177 /* See also ATTN, QUIT in ex_tune.h */
#define ESCAPE  033

/*
 * Miscellaneous random variables used in more than one place
 */
EXTERN exbool aiflag;   /* Append/change/insert with autoindent */
EXTERN exbool anymarks; /* We have used '[a-z] */
EXTERN int chng;        /* Warn "No write" */
EXTERN char *Command;
EXTERN short defwind;             /* -w# change default window size */
EXTERN exbool edited;             /* Current file is [Edited] */
EXTERN line *endcore;             /* Last available core location */
EXTERN exbool endline;            /* Last cmd mode command ended with \n */
EXTERN line *fendcore;            /* First address in line pointer space */
EXTERN char file[FNSIZE];         /* Working file name */
EXTERN char genbuf[LBSIZE];       /* Working buffer when manipulating linebuf */
EXTERN exbool hush;               /* Command line option - was given, hush up! */
EXTERN char *globp;               /* (Untyped) input string to command mode */
EXTERN exbool holdcm;             /* Don't cursor address */
EXTERN exbool inappend;           /* in ex command append mode */
EXTERN exbool inglobal;           /* Inside g//... or v//... */
EXTERN char *initev;              /* Initial : escape for visual */
EXTERN short inopen;              /* Inside open or visual */
EXTERN char *input;               /* Current position in cmd line input buffer */
EXTERN exbool intty;              /* Input is a tty */
EXTERN short io;                  /* General i/o unit (auto-closed on error!) */
extern short lastc;               /* Last character ret'd from cmd input; ex_get.cpp */
EXTERN exbool laste;              /* Last command was an "e" (or "rec") */
EXTERN char lastmac;              /* Last macro called for ** */
EXTERN char lasttag[TAGSIZE];     /* Last argument to a tag command */
EXTERN char *linebp;              /* Used in substituting in \n */
EXTERN char linebuf[LBSIZE];      /* The primary line buffer */
EXTERN exbool listf;              /* Command should run in list mode */
EXTERN char *loc1;                /* Where re began to match (in linebuf) */
EXTERN char *loc2;                /* First char after re match (") */
EXTERN line names['z' - 'a' + 2]; /* Mark registers a-z,' */
EXTERN int otchng;                /* Backup tchng to find changes in macros */
EXTERN int notecnt;               /* Count for notify (to visual from cmd) */
EXTERN exbool numberf;            /* Command should run in number mode */
EXTERN short oprompt;             /* Saved during source */
EXTERN short peekc;               /* Peek ahead character (cmd mode input) */
EXTERN char *pkill[2];            /* Trim for put with ragged (LISP) delete */
EXTERN int pid;                   /* Process id of child */
EXTERN exbool ruptible;           /* Interruptible is normal state */
EXTERN exbool seenprompt;         /* 1 if have gotten user input */
EXTERN exbool shudclob;           /* Have a prompt to clobber (e.g. on ^D) */
EXTERN int status;                /* Status returned from wait() */
EXTERN int tchng;                 /* If nonzero, then [Modified] */
EXTERN exbool vcatch;             /* Want to catch an error (open/visual) */
EXTERN exbool writing;            /* 1 if in middle of a file write */
EXTERN int xchng;                 /* Suppresses multiple "No writes" in !cmd */
EXTERN int errno;                 /* the last syscall's Error, as an int */
EXTERN short ex_pendclose;        /* a descriptor an error left open */

/*
 * Upstream defined these in ex_cmds.c *and* in ex_cmds2.c, which C merged into
 * one common symbol and C++ rejects outright. ex_cmds.cpp keeps them.
 */
extern exbool pflag, nflag;
extern int poffset;

/*
 * Macros
 */
#define CP(a, b) (ignore(strcpy(a, b)))
/*
 * FIXUNDO: do we want to mung undo vars?
 * Usually yes unless in a macro or global.
 */
#define FIXUNDO (inopen >= 0 && (inopen || !inglobal))
#define ckaw()                        \
    {                                 \
        if (chng && value(AUTOWRITE)) \
            co_await wop(0);          \
    }
#define copy(a, b, c)  Copy((char *)a, (char *)b, c)
#define eq(a, b)       ((a) && (b) && strcmp(a, b) == 0)
#define lastchar()     lastc
#define outchar(c)     (*Outchar)(c)
#define pastwh()       (ignore(skipwh()))
#define pline(no)      (*Pline)(no)
#define setlastchar(c) lastc = c
#define ungetchar(c)   peekc = c

/*
 * Environment like memory
 */
EXTERN char altfile[FNSIZE];   /* Alternate file name */
EXTERN char savedfile[FNSIZE]; /* The current file name */
EXTERN char uxb[UXBSIZE + 2];  /* Last !command for !! */

/* These four carry initializers, so ex_data.cpp defines them outright. */
extern char direct[ONMSZ];  /* Temp file goes here */
extern char shell[ONMSZ];   /* Copied to be settable */
extern char ttytype[ONMSZ]; /* A long and pretty name */

/*
 * The editor data structure for accessing the current file consists
 * of an incore array of pointers into the temporary file tfile.
 * Each pointer is 15 bits (the low bit is used by global) and is
 * padded with zeroes to make an index into the temp file where the
 * actual text of the line is stored.
 *
 * To effect undo, copies of affected lines are saved after the last
 * line considered to be in the buffer, between dol and unddol.
 * During an open or visual, which uses the command mode undo between
 * dol and unddol, a copy of the entire, pre-command buffer state
 * is saved between unddol and truedol.
 */
EXTERN line *addr1;   /* First addressed line in a command */
EXTERN line *addr2;   /* Second addressed line */
EXTERN line *dol;     /* Last line in buffer */
EXTERN line *dot;     /* Current line */
EXTERN line *one;     /* First line */
EXTERN line *truedol; /* End of all lines, including saves */
EXTERN line *unddol;  /* End of undo saved lines */
EXTERN line *zero;    /* Points to empty slot before one */

/*
 * Undo information
 *
 * For most commands we save lines changed by salting them away between
 * dol and unddol before they are changed (i.e. we save the descriptors
 * into the temp file tfile which is never garbage collected).  The
 * lines put here go back after unddel, and to complete the undo
 * we delete the lines [undap1,undap2).
 *
 * Undoing a move is much easier and we treat this as a special case.
 * Similarly undoing a "put" is a special case for although there
 * are lines saved between dol and unddol we don't stick these back
 * into the buffer.
 */
EXTERN short undkind;

EXTERN line *unddel;  /* Saved deleted lines go after here */
EXTERN line *undap1;  /* Beginning of new lines */
EXTERN line *undap2;  /* New lines end before undap2 */
EXTERN line *undadot; /* If we saved all lines, dot reverts here */

#define UNDCHANGE 0
#define UNDMOVE   1
#define UNDALL    2
#define UNDNONE   3
#define UNDPUT    4

/*
 * Function type definitions
 */
#define NOSTR  (char *)0
#define NOLINE (line *)0

typedef int (*OutcharFn)(int);
typedef int (*PlineFn)(int);

/* ex_out.cpp defines these three with their initial values. */
extern OutcharFn Outchar;
extern PlineFn Pline;
extern OutcharFn Putchar;

/*
 * C doesn't have a (void) cast, so we have to fake it for lint's sake.
 */
#define ignore(a) a
#define ignorf(a) a

/* ------------------------------------------------------------- ex.cpp */
void init(void);
char *tailpath(char *p);

/* ------------------------------------------------------------- ex_addr.cpp */
void setdot(void);
void setdot1(void);
void setcount(void);
int getnum(void);
void setall(void);
void setnoaddr(void);
line *address(char *incurs);
void setCNL(void);
void setNAEOL(void);

/* ------------------------------------------------------------- ex_cmds.cpp */
Task<int> commands(exbool noprompt, exbool exitoneof);

/* ------------------------------------------------------------ ex_cmds2.cpp */
int cmdreg(void);
int endcmd(int ch);
void eol(void);
void error(char *str, int i = 0);
void erewind(void);
void error0(void);
void error_end(exbool had_msg);
exbool excatch(void);
void ex_reset(void);
void fixol(void);
int exclam(void);
void makargs(void);
Task<void> next(void);
void newline(void);
void serror(char *str, char *cp);
void nomore(void);
exbool quickly(void);
void resetflav(void);
void setflav(void);
int skipend(void);
void tailspec(int c);
void tail(char *comm);
void tail2of(char *comm);
void tailprim(char *comm, int i, exbool notinvis);
Task<void> vcontin(exbool ask);
void vnfl(void);

/* ---------------------------------------------------------- ex_cmdsub.cpp */
Task<int> append(Task<int> (*f)(void), line *a);
extern char *locs; /* ex_re.cpp */
void appendnone(void);
void pargs(void);
void exdelete(exbool hold);
void deletenone(void);
void squish(void);
Task<void> join(int c);
Task<void> move(void);
Task<void> move1(int cflag, line *addrt);
Task<int> getcopy(void);
Task<int> getput(void);
Task<void> put(void);
Task<void> pragged(exbool kill);
void shift(int c, int cnt);
Task<void> tagfind(exbool quick);
void yank(void);
Task<void> zop(int hadpr);
Task<void> zop2(int lines, int op);
Task<void> splitit(void);
Task<void> plines(line *adr1, line *adr2, exbool movedot);
void pofix(void);
Task<void> undo(exbool c);
exbool somechange(void);
void mapcmd(int un, int ab);
void addmac(char *src, char *dest, char *dname, struct maps *mp);
Task<void> cmdmac(char c);

/* ------------------------------------------------------------- ex_get.cpp */
void ignchar(void);
int getchar(void);
int getcd(void);
int peekchar(void);
int peekcd(void);
int getach(void);
Task<int> gettty(void);
int smunch(int col, char *ocp);
void checkjunk(int c);
void setin(line *addr);
Task<Result<void>> ex_readline(void);
exbool need_input(void);

/* ------------------------------------------------------------ ex_file.cpp */
/*
 * What ex called by their Unix names. Each is a syscall and every syscall is
 * awaited, so each is a Task; each answers upstream's own convention and sets
 * errno beside it.
 */
struct exstat {
    long st_size;
    exbool st_isdir;
};

Task<int> ex_open(char *path, int mode);
Task<int> ex_creat(char *path);
Task<void> ex_close(int fd);
Task<int> ex_read(int fd, char *buf, int n);
Task<int> ex_write(int fd, char *buf, int n);
Task<long> ex_seek(int fd, long off, int whence);
Task<int> ex_stat(char *path, struct exstat *sb);
Task<int> ex_fstat(int fd, struct exstat *sb);

/* -------------------------------------------------------------- ex_io.cpp */
void filename(int comm);
int getargs(void);
void glob(struct glob *gp);
int gscan(void);
void getone(void);
Task<void> rop(int c);
Task<void> rop2(void);
Task<void> rop3(int c);
exbool samei(struct exstat *sp, char *cp);
Task<void> wop(exbool dofname);
exbool edfile(void);
Task<int> getfile(void);
Task<void> putfile(void);
void wrerror(void);
Task<void> source(char *fil, exbool okfail);
void clrstats(void);
Task<int> iostats(void);

/* ------------------------------------------------------------- ex_out.cpp */
OutcharFn setlist(exbool t);
PlineFn setnumb(exbool t);
int listchar(int c);
int normchar(int c);
int numbline(int i);
int normline(int i);
void slobber(int c);
exbool out_pending(void);
int putchar(int c);
int termchar(int c);
void flush(void);
void flush1(void);
void flush2(void);
void tab(int col);
void setoutt(void);
void noteinp(void);
void draino(void);
void flusho(void);
void putnl(void);
void putS(char *cp);
int putch(int c);
void lprintf(char *cp, char *dp);
/* ex's own printf, which prints through putchar; see the note in ex_out.cpp. */
void printf(const char *fmt, ...);
void putNFL(void);
void pstart(void);
void pstop(void);
void noonl(void);
Task<Result<void>> exflush(void);
void ex_out_reset(void);

/* -------------------------------------------------------------- ex_re.cpp */
Task<void> global(exbool k);
void gdelete(void);
Task<int> substitute(int c);
Task<int> compsub(int ch);
void comprhs(int seof);
Task<int> getsub(void);
Task<int> dosubcon(exbool f, line *a);
Task<exbool> confirmed(line *a);
Task<int> getch(void);
void ugo(int cnt, int with);
void dosub(void);
int fixcase(int c);
char *place(char *sp, char *l1, char *l2);
void snote(int total, int lines);
exbool compile(int eof, exbool oknl);
exbool same(int a, int b);
void cerror(char *s);
exbool execute(exbool gf, line *addr = 0);
exbool advance(char *lp, int *ep);
exbool cclass(int *set, int c, exbool af);

/* ------------------------------------------------------------- ex_set.cpp */
void set(void);
exbool setend(void);
void prall(void);
void propts(void);
void propt(struct option *op);

/* ------------------------------------------------------------ ex_subr.cpp */
exbool any(int c, char *s);
int backtab(int i);
void change(void);
int column(char *cp);

/* UTF-8 stepping; see ex_subr.cpp. */
int runeat(char *cp, int *len);
int runelen(int lead);
exbool rune_space(int c);
exbool rune_word(int c);
int runeof(char *cp);
char *nextchar(char *cp);
char *prevchar(char *base, char *cp);
char *vstep(char *cp, int dir);
void comment(void);
/*
 * Note the order. Upstream wrote `Copy(to, from, size)` and then declared
 * `register char *from, *to;` under it, which K&R matches by position and not
 * by name -- so the first argument is the destination.
 */
void Copy(char *to, char *from, int size);
void copyw(line *to, line *from, int size);
void copywR(line *to, line *from, int size);
int ctlof(int c);
void dingdong(void);
int fixindent(int indent);
void filioerr(char *cp);
char *genindent(int indent);
void getDOT(void);
line *getmark(int c);
int getn(char *cp);
void ignnEOF(void);
exbool iswhite(int c);
exbool junk(int c);
void killed(void);
void killcnt(int cnt);
int lineno(line *a);
int lineDOL(void);
int lineDOT(void);
void markDOT(void);
void markpr(line *which);
int markreg(int c);
char *mesg(char *str);
void merror(char *seekpt, int i = 0);
void merror1(char *seekpt);
int morelines(void);
void nonzero(void);
exbool notable(int i);
void notempty(void);
void netchHAD(int cnt);
void netchange(int i);
void putmark(line *addr);
void putmk1(line *addr, int n);
char *plural(long i);
int qcolumn(char *lim, char *gp);
int qcount(int c);
void reverse(line *a1, line *a2);
void save(line *a1, line *a2);
void save12(void);
void saveall(void);
int span(void);
void sync(void);
int skipwh(void);
void smerror(char *seekpt, char *cp);
char *strend(char *cp);
void strcLIN(char *dp);
void syserror(void);
int tabcol(int col, int ts);
char *vfindcol(int i);
char *vskipwh(char *cp);
char *vpastwh(char *cp);
int whitecnt(char *cp);
void Ignore(char *a);
void Ignorf(int (*a)(void));
void markit(line *addr);
void onintr(void);
Task<void> setrupt(void);
void ex_exit(int i);

/* ------------------------------------------------------------- ex_buf.cpp */
void fileinit(void);
void cleanup(exbool all);
void getline(line tl);
line putline(void);
char *getblock(line atl, int iof);
void regio(int b, exbool writing);
line REGblk(void);
struct strreg *mapreg(int c);
void KILLreg(int c);
int shread(void);
Task<void> putreg(int c);
void notpart(int c);
Task<int> getREG(void);
void YANKreg(int c);
void kshift(void);
void YANKline(void);
void rbflush(void);
Task<char *> regbuf(int c, char *buf, int buflen);

Task<void> waitfor(void);

/* ------------------------------------------------------------ ex_unix.cpp */
Task<void> unix0(exbool warn);
Task<void> unixex(char *opt, char *up, int newstdin, int mode);
Task<void> unixwt(exbool c, int p);
Task<void> filter(int mode);

/* ----------------------------------------------------------- ex_screen.cpp */
Task<Result<void>> vflush(void);
Task<void> vspawn_begin(void);
Task<void> vspawn_end(exbool repaint = 1);
void setsize(int rows, int cols);
void vresize(void);
