// make(1) -- the shared declarations.  README.md has the account of the port.

#ifndef MAKE_DEFS_H
#define MAKE_DEFS_H

#include <stdio.h>

#include "compat/cio.h"

#define SHELLCOM "/bin/sh"

// Milliseconds since the epoch, as stat_of() and clock_now() answer.  A long is
// 32 bits here.
typedef long long TIMETYPE;

#define NO  0
#define YES 1

#define unequal strcmp

#define HASHSIZE 509  // the name table's first size; it doubles as it fills
#define NLEFTS   40   // the targets of a rule line, at first; they grow
#define INMAX    1500 // the input line's first size; it grows

// An expansion past this is taken for a runaway rather than a command.
#define MAXEXPAND (64 * 1024 * 1024)

// include may nest this deep, which is far past any use and short of a file
// that includes itself running make out of memory.
#define MAXINCL 100

// subst() recurses once per nested macro reference.
#define MAXSUBST 100

#define ALLDEPS  1
#define SOMEDEPS 2

#define META     01
#define TERMINAL 02

// Indexed by a char, and a char is unsigned here: 256 entries, not v7's 128.
extern char funny[256];

#define ALLOC(x) (struct x *)ckalloc(sizeof(struct x))

// A target-specific or pattern-specific macro, GNU's target: name op value.
struct tvar {
    struct tvar *next;
    char *name;
    char *val;
    int op; // as assign() takes it; 'S' for a value expanded when read
    int over, exp;
};

struct patvar {
    struct patvar *next;
    char *pat;
    struct tvar *tv;
};

struct nameblock {
    struct nameblock *nxtnameblock;
    char *namep;
    struct lineblock *linep;
    int done; // v7 packed done and septype into three bits each
    int septype;
    TIMETYPE modtime;
    char *path; // where VPATH or vpath found it, if not here
    struct tvar *tvars;
};

struct lineblock {
    struct lineblock *nxtlineblock;
    struct depblock *depp;
    struct shblock *shp;
    char *stem; // a static pattern's $*
};

// A pattern rule, GNU's %.o: %.c, in the order read.
struct patrule {
    struct patrule *nxtpatrule;
    char *target;
    struct depblock *depp;
    struct shblock *shp;
};

struct depblock {
    struct depblock *nxtdepblock;
    struct nameblock *depname;
    int orderonly; // after a `|': made first, but its time is not looked at
};

struct shblock {
    struct shblock *nxtshblock;
    char *shbp;
};

struct varblock {
    struct varblock *nxtvarblock;
    char *varname;
    char *varval;
    int noreset;
    int used;
    int simple;   // set by := and ::=, and not expanded again
    int exported; // 1 by export, the environment or the command line; -1 by unexport
    int fromenv;  // the value is still the environment's, as it was
    int origin;   // where it was set, for $(origin)
};

// Where a macro was set, as $(origin) names it.
enum { O_UNDEF, O_DEFAULT, O_ENV, O_ENVOVR, O_FILE, O_CMD, O_OVERRIDE, O_AUTO };
extern int setorigin; // what setvar() is setting from now

struct pattern {
    struct pattern *nxtpattern;
    char *patval;
};

// One directory searched, with the listing it was last read as.
struct opendir {
    struct opendir *nxtopendir;
    DIR *dirfc;
    char *dirn;
};

struct chain {
    struct chain *nextp;
    char *datap;
};

// main.cpp
extern struct nameblock *mainname;
extern struct nameblock *firstname;
extern struct lineblock *sufflist;
extern struct varblock *firstvar;
extern struct pattern *firstpat;
extern struct opendir *firstod;
extern struct patrule *firstpatrule;
extern struct patvar *firstpatvar;

extern int childpid; // the child await() is waiting for; v7 called it waitpid
extern int dbgflag;
extern int prtrflag;
extern int silflag;
extern int noexflag;
extern int keepgoing;
extern int noruleflag;
extern int touchflag;
extern int questflag;
extern int ndocoms;
extern int ignerr;
extern int okdel;
extern int envflag;
extern int inarglist;
extern const char *makename0;
extern const char *prompt;
extern char junkname[20];
extern const char **linesptr;

Task<void> intrupt();
int isprecious(char *p);
int isnamed(const char *special, const char *p);
int hasdeps(struct nameblock *np);
Task<int> rddescf(const char *descfile);
void printdesc(int prntflag);

// gram.cpp
extern char *zznextc;
extern int yylineno;
extern char *fin; // the description file, read whole; NULL while reading builtin[]
extern char *finend;
extern int parsing;
Task<int> yyparse(void);
int pushin(char *buf, int len);
Task<void> includef(char *names, int opt);
int yylex(void);
int yyrelex(void);

// files.cpp
extern const char *builtin[];
Task<TIMETYPE> exists(char *filename);
Task<TIMETYPE> prestime(void);
Task<struct depblock *> srchdir(char *pat, int mkchain, struct depblock *nextdbl);
Task<char *> wildcard(char *pats);
void vpathdef(char *line);
int havevpath(void);
Task<TIMETYPE> vfind(const char *name, char **pathp);
char *dname(struct nameblock *p);

// doname.cpp
Task<int> doname(struct nameblock *p, int reclevel, TIMETYPE *tval);
Task<int> docom(struct shblock *q);
extern TIMETYPE attime;
Task<void> expand(struct depblock *q);

// dosys.cpp
Task<int> dosys(char *comstring, int nohalt);
Task<void> touch(int force, char *name);
void childenv(const char *makeflags);
Args *mkenv(void);
Task<char *> shellout(char *cmd);

// funcs.cpp
extern int substneed;
void heldeffects(int keep);
extern char *evalpend;
Task<void> evalnow(void);
Task<void> perform(void);
void forget(void);
int isfunc(const char *name);
char *callfunc(int fn, char *args, char *b, char *bend);

// misc.cpp
struct nameblock *srchname(const char *s);
struct nameblock *makename(const char *s);
int maincand(const char *s);
char *patstem(const char *pat, const char *name);
char *patput(const char *pat, const char *stem);
char *copys(const char *s);
void *growbuf(void *buf, int *capp, int n);
char *concat(const char *a, const char *b, char *c);
int suffix(char *a, char *b, char *p);
void *ckalloc(int n);
char *subst(char *a, char *b, char *bend); // bend: last writable byte of b
char *substa(char *a);
int putn(char **bp, char *bend, const char *s, size_t n);
char *substnow(char *a);
extern int substfull;
int setvar(const char *v, char *s);
extern int overriding;
extern int exportall;
void assign(char *v, char *s, int op);
int eqsign(char *a);
int assigncolon(const char *s);
extern int shellpend;
extern char *shellname, *shellcmd;
extern int shellover;
Task<void> runset(void);
struct varblock *varptr(const char *v);
struct varblock *findvar(const char *v);
void fatal1(const char *s, const char *t);
void fatal(const char *s);
void yyerror(const char *s);
struct chain *appendq(struct chain *head, char *tail);
struct chain *appendu(struct chain *head, char *tail);
struct chain *prependu(struct chain *head, char *name);
struct chain *prependq(struct chain *head, char *name);
struct chain *joinu(struct chain *a, struct chain *b);
struct chain *joinq(struct chain *a, struct chain *b);
char *mkqlist(struct chain *p);
int ismember(const char *name, char **libp, char **memp);

// exit() traps here, so fatal() and its kind stop rather than exit: they record
// the status, and every caller of something that may stop tests `stopped` and
// returns.  proc_main() says the message and returns the status.  README.md.
extern int stopped;
void stop(int status);

// stdout and stderr, held in the order they were said and written by flushout()
// before a command runs and at exit.  printf and fprintf(stderr, ...) in v7.
void mprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void diag(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
int outheld(void);
Task<void> flushout(void);

#endif // MAKE_DEFS_H
