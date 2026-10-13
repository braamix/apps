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

#define HASHSIZE 509
#define NLEFTS   40
#define INMAX    1500 // longest input line
#define OUTMAX   2500 // longest command line after substitution
#define QBUFMAX  1500 // longest $? list

// subst() recurses once per nested macro reference.
#define MAXSUBST 100

#define ALLDEPS  1
#define SOMEDEPS 2

#define META     01
#define TERMINAL 02

// Indexed by a char, and a char is unsigned here: 256 entries, not v7's 128.
extern char funny[256];

#define ALLOC(x) (struct x *)ckalloc(sizeof(struct x))

struct nameblock {
    struct nameblock *nxtnameblock;
    char *namep;
    struct lineblock *linep;
    int done; // v7 packed done and septype into three bits each
    int septype;
    TIMETYPE modtime;
};

struct lineblock {
    struct lineblock *nxtlineblock;
    struct depblock *depp;
    struct shblock *shp;
};

struct depblock {
    struct depblock *nxtdepblock;
    struct nameblock *depname;
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
    int simple; // set by := and ::=, and not expanded again
};

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
int yyparse(void);
int yylex(void);

// files.cpp
extern const char *builtin[];
Task<TIMETYPE> exists(char *filename);
Task<TIMETYPE> prestime(void);
Task<struct depblock *> srchdir(char *pat, int mkchain, struct depblock *nextdbl);

// doname.cpp
Task<int> doname(struct nameblock *p, int reclevel, TIMETYPE *tval);
Task<int> docom(struct shblock *q);
Task<void> expand(struct depblock *q);

// dosys.cpp
Task<int> dosys(char *comstring, int nohalt);
Task<void> touch(int force, char *name);
void childenv(const char *makeflags);

// misc.cpp
struct nameblock *srchname(const char *s);
struct nameblock *makename(const char *s);
int maincand(const char *s);
char *copys(const char *s);
char *concat(const char *a, const char *b, char *c);
int suffix(char *a, char *b, char *p);
void *ckalloc(int n);
char *subst(char *a, char *b, char *bend); // bend: last writable byte of b
int setvar(const char *v, char *s);
int eqsign(char *a);
int assigncolon(const char *s);
struct varblock *varptr(const char *v);
void fatal1(const char *s, const char *t);
void fatal(const char *s);
void yyerror(const char *s);
struct chain *appendq(struct chain *head, char *tail);
struct chain *appendu(struct chain *head, char *tail);
struct chain *prependu(struct chain *head, char *name);
char *mkqlist(struct chain *p);

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
