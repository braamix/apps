/* UNIX V7 source code: see /COPYRIGHT or www.tuhs.org for details. */

//
// command make to update programs.
// Flags: 'd'  print out debugging comments
//        'p'  print out a version of the input graph
//        's'  silent mode--don't print out commands
//        'f'  the next argument is the name of the description file;
//             "makefile" is the default
//        'i'  ignore error codes from the shell
//        'S'  stop after any command fails (normally do parallel work)
//        'n'  don't issue, just print, commands
//        't'  touch (update time of) files but don't issue command
//        'q'  don't do anything, but check if object is up to date;
//             returns exit code 0 if up to date, -1 if not
//
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "proc/rt.h"

struct nameblock *mainname;
struct nameblock *firstname;
struct lineblock *sufflist;
struct varblock *firstvar;
struct pattern *firstpat;
struct opendir *firstod;

int childpid = 0;

int dbgflag    = NO;
int prtrflag   = NO;
int silflag    = NO;
int noexflag   = NO;
int keepgoing  = NO;
int noruleflag = NO;
int touchflag  = NO;
int questflag  = NO;
int ndocoms    = NO;
int ignerr     = NO; // default is to stop on error
int okdel      = YES;
int inarglist;
const char *prompt = "";
char junkname[20];
char funny[256];

const char **linesptr = builtin;

static int firstrd = 0;

static int rdd1(char *buf, int len);

// argv, copied out of the Args views once: main() blanks the words it has used.
#define NARGS 200
static char *avbuf[NARGS + 1];

static Task<int> make_main(int argc, char *argv[]);

Task<i32> proc_main(Args args)
{
    int argc = (int)args.size();
    int i, st;

    if (argc > NARGS)
        argc = NARGS;
    for (i = 0; i < argc; i++) {
        avbuf[i] = (char *)ckalloc(args[i].size() + 1);
        memcpy(avbuf[i], args[i].data(), args[i].size());
    }
    avbuf[argc] = 0;

    co_await sig_catch(SIG_INT);

    st = co_await make_main(argc, avbuf);
    if (stopped)
        st = stopped & 0xff;

    co_await flushout();
    co_return st;
}

static Task<int> make_main(int argc, char *argv[])
{
    struct nameblock *p;
    int i, j;
    int descset, nfargs, errs = 0;
    TIMETYPE tjunk;
    char c;
    const char *s;
    static char onechar[2] = "X";

    descset = 0;

    funny['\0'] = (META | TERMINAL);
    for (s = "=|^();&<>*?[]:$`'\"\\\n"; *s; ++s)
        funny[(unsigned char)*s] |= META;
    for (s = "\n\t :;&>|"; *s; ++s)
        funny[(unsigned char)*s] |= TERMINAL;

    inarglist = 1;
    for (i = 1; i < argc; ++i)
        if (argv[i] != 0 && argv[i][0] != '-' && eqsign(argv[i]))
            argv[i] = 0;

    setvar("$", (char *)"$");
    inarglist = 0;

    for (i = 1; i < argc; ++i)
        if (argv[i] != 0 && argv[i][0] == '-') {
            for (j = 1; (c = argv[i][j]) != '\0'; ++j)
                switch (c) {
                case 'd':
                    dbgflag = YES;
                    break;

                case 'p':
                    prtrflag = YES;
                    break;

                case 's':
                    silflag = YES;
                    break;

                case 'i':
                    ignerr = YES;
                    break;

                case 'S':
                    keepgoing = NO;
                    break;

                case 'k':
                    keepgoing = YES;
                    break;

                case 'n':
                    noexflag = YES;
                    break;

                case 'r':
                    noruleflag = YES;
                    break;

                case 't':
                    touchflag = YES;
                    break;

                case 'q':
                    questflag = YES;
                    break;

                case 'f':
                    if (i >= argc - 1) {
                        fatal("No description argument after -f flag");
                        co_return 1;
                    }
                    if (co_await rddescf(argv[i + 1]))
                        fatal1("Cannot open %s", argv[i + 1]);
                    if (stopped)
                        co_return 1;
                    argv[i + 1] = 0;
                    ++descset;
                    break;

                default:
                    onechar[0] = c;
                    fatal1("Unknown flag argument %s", onechar);
                    co_return 1;
                }

            argv[i] = 0;
        }

    if (!descset)
        if (co_await rddescf("makefile"))
            co_await rddescf("Makefile");
    if (stopped)
        co_return 1;

    if (prtrflag)
        printdesc(NO);

    // With no prerequisites these name every target; with some, only those
    // (docom()).  v7 took any mention as the first.
    if ((p = srchname(".IGNORE")) && !hasdeps(p))
        ++ignerr;
    if ((p = srchname(".SILENT")) && !hasdeps(p))
        silflag = 1;
    if ((p = srchname(".SUFFIXES")))
        sufflist = p->linep;

    nfargs = 0;

    for (i = 1; i < argc; ++i)
        if ((s = argv[i]) != 0) {
            if ((p = srchname(s)) == 0) {
                p = makename(s);
            }
            if (stopped)
                co_return 1;
            ++nfargs;
            errs += co_await doname(p, 0, &tjunk);
            if (stopped)
                co_return 1;
            if (dbgflag)
                printdesc(YES);
        }

    // If no file arguments have been encountered, make the first
    // name encountered that doesn't start with a dot
    if (nfargs == 0) {
        if (mainname == 0)
            fatal("No arguments or description file");
        else {
            errs += co_await doname(mainname, 0, &tjunk);
            if (stopped)
                co_return 1;
            if (dbgflag)
                printdesc(YES);
        }
    }

    // -k went on past a target it could not make, and POSIX says so in the
    // status, where v7 exited 0.
    co_return errs ? 2 : 0;
}

// ^C, caught while a command ran.  v7's was a signal handler; here await() is
// where the signal arrives.
Task<void> intrupt()
{
    char *p;

    if (okdel && !noexflag && !touchflag && (p = varptr("@")->varval) && co_await exists(p) > 0 &&
        !isprecious(p)) {
        diag("\n***  %s removed.", p);
        co_await b_unlink(p);
    }

    if (junkname[0])
        co_await b_unlink(junkname);
    diag("\n");
    stop(2);
}

int isprecious(char *p)
{
    return isnamed(".PRECIOUS", p);
}

// Whether special target `special' names p among its prerequisites.
int isnamed(const char *special, const char *p)
{
    struct lineblock *lp;
    struct depblock *dp;
    struct nameblock *np;

    if (p && (np = srchname(special)))
        for (lp = np->linep; lp; lp = lp->nxtlineblock)
            for (dp = lp->depp; dp; dp = dp->nxtdepblock)
                if (dp->depname && !unequal(p, dp->depname->namep))
                    return YES;

    return NO;
}

int hasdeps(struct nameblock *np)
{
    struct lineblock *lp;

    for (lp = np->linep; lp; lp = lp->nxtlineblock)
        if (lp->depp)
            return YES;
    return NO;
}

// The whole of a stream, in a buffer that grows.  NULL if it cannot be read.
static Task<char *> rdall(FILE *k, int *lenp)
{
    char *buf = NULL, *nb;
    int len = 0, cap = 0;
    size_t n;

    for (;;) {
        if (cap - len < 4096) {
            cap = cap ? cap * 2 : 8192;
            if ((nb = (char *)realloc(buf, cap)) == NULL) {
                free(buf);
                co_return NULL;
            }
            buf = nb;
        }
        n = co_await b_fread(buf + len, 1, cap - len, k);
        if (n == 0)
            break;
        len += n;
    }
    if (b_ferror(k)) {
        free(buf);
        co_return NULL;
    }
    *lenp = len;
    co_return buf;
}

// Read and parse a description file.  It is read whole first, so the parser
// under it is plain code and never waits.
Task<int> rddescf(const char *descfile)
{
    FILE *k;
    char *buf;
    int len = 0, r;

    if (!firstrd++) {
        if (!noruleflag)
            rdd1(NULL, 0);
    }
    if (stopped)
        co_return 1;
    if (!unequal(descfile, "-")) {
        if ((buf = co_await rdall(stdin, &len)) == NULL)
            co_return 1;
        r = rdd1(buf, len);
        free(buf);
        co_return r;
    }

    if ((k = co_await b_fopen(descfile, "r")) != NULL) {
        buf = co_await rdall(k, &len);
        co_await b_fclose(k);
        if (buf == NULL)
            co_return 1;
        r = rdd1(buf, len);
        free(buf);
        co_return r;
    }

    co_return 1;
}

static int rdd1(char *buf, int len)
{
    fin      = buf;
    finend   = buf + len;
    yylineno = 0;
    zznextc  = 0;

    if (yyparse())
        fatal("Description file error");

    fin = NULL;
    return 0;
}

void printdesc(int prntflag)
{
    struct nameblock *p;
    struct depblock *dp;
    struct varblock *vp;
    struct opendir *od;
    struct shblock *sp;
    struct lineblock *lp;

    // A directory here is a listing and not a descriptor, so there is no number
    // to print beside it.
    if (prntflag) {
        mprintf("Open directories:\n");
        for (od = firstod; od; od = od->nxtopendir)
            mprintf("\t%s\n", od->dirn);
    }

    if (firstvar != 0)
        mprintf("Macros:\n");
    for (vp = firstvar; vp; vp = vp->nxtvarblock)
        mprintf("\t%s = %s\n", vp->varname, vp->varval);

    for (p = firstname; p; p = p->nxtnameblock) {
        mprintf("\n\n%s", p->namep);
        if (p->linep != 0)
            mprintf(":");
        if (prntflag)
            mprintf("  done=%d", p->done);
        if (p == mainname)
            mprintf("  (MAIN NAME)");
        for (lp = p->linep; lp; lp = lp->nxtlineblock) {
            if ((dp = lp->depp)) {
                mprintf("\n depends on:");
                for (; dp; dp = dp->nxtdepblock)
                    if (dp->depname != 0)
                        mprintf(" %s ", dp->depname->namep);
            }

            if ((sp = lp->shp)) {
                mprintf("\n commands:\n");
                for (; sp != 0; sp = sp->nxtshblock)
                    mprintf("\t%s\n", sp->shbp);
            }
        }
    }
    mprintf("\n");
}
