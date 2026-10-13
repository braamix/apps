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
struct patrule *firstpatrule;

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
int envflag    = NO;
int inarglist;
const char *makename0 = "make"; // argv[0], for $(MAKE)
const char *prompt    = "";
char junkname[20];
char funny[256];

const char **linesptr = builtin;

static int firstrd = 0;

static void readenv(void);

static Task<int> rdd1(char *buf, int len);

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
    if (argc > 0 && avbuf[0][0])
        makename0 = avbuf[0];

    st = co_await make_main(argc, avbuf);
    if (stopped)
        st = stopped & 0xff;

    co_await flushout();
    co_return st;
}

// One option letter, from the command line or MAKEFLAGS.  NO if unknown.
static int setflag(char c)
{
    switch (c) {
    case 'd':
        dbgflag = YES;
        break;

    case 'e':
        envflag = YES;
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

    default:
        return NO;
    }
    return YES;
}

// The macros from the command line and from MAKEFLAGS, as typed, for the
// MAKEFLAGS a child is given.
static char *cmdmacros;

static void addmacro(const char *def)
{
    char *t, *d;
    const char *s;
    size_t n = cmdmacros ? strlen(cmdmacros) : 0;

    t = (char *)ckalloc(n + 2 * strlen(def) + 2);
    if (cmdmacros)
        memcpy(t, cmdmacros, n);
    d = t + n;
    if (n)
        *d++ = ' ';
    for (s = def; *s; *d++ = *s++)
        if (*s == ' ' || *s == '\t' || *s == '\\')
            *d++ = '\\'; // a blank in a value is escaped, as GNU make writes it
    *d = '\0';
    free(cmdmacros);
    cmdmacros = t;
}

// MAKEFLAGS, as a parent make wrote it or a user set it: option letters, with
// or without a dash, and macro definitions, which count as the command line's
// but lose to it.
static void readmakeflags(void)
{
    Str env = proc_env("MAKEFLAGS");
    char *buf, *w, *d, *s;

    if (env.size() == 0)
        return;
    buf = (char *)ckalloc(env.size() + 1);
    memcpy(buf, env.data(), env.size());
    for (s = buf; *s;) {
        while (*s == ' ' || *s == '\t')
            ++s;
        if (*s == '\0')
            break;
        for (w = d = s; *s && *s != ' ' && *s != '\t'; *d++ = *s++)
            if (*s == '\\' && s[1])
                ++s;
        if (*s)
            ++s;
        *d = '\0';
        if (!strcmp(w, "--"))
            continue;
        if (strchr(w, '=')) {
            addmacro(w);
            inarglist = 1;
            if (eqsign(d = copys(w)) && !substneed)
                varptr(d)->exported = 1;
            substneed = NO; // a $(shell) in MAKEFLAGS is not waited for
            inarglist = 0;
        } else
            for (d = w + (*w == '-'); *d; ++d)
                setflag(*d);
    }
    free(buf);
}

// MAKEFLAGS for the children: the letters a child should act on, and the
// command line's macros.
static void writemakeflags(void)
{
    char flags[16], *d = flags, *mf;
    size_t n;

    if (envflag)
        *d++ = 'e';
    if (ignerr && !srchname(".IGNORE"))
        *d++ = 'i';
    if (keepgoing)
        *d++ = 'k';
    if (noexflag)
        *d++ = 'n';
    if (questflag)
        *d++ = 'q';
    if (noruleflag)
        *d++ = 'r';
    if (silflag && !srchname(".SILENT"))
        *d++ = 's';
    if (touchflag)
        *d++ = 't';
    *d = '\0';

    n  = strlen(flags) + (cmdmacros ? strlen(cmdmacros) + 1 : 0);
    mf = (char *)ckalloc(n + 1);
    strcpy(mf, flags);
    if (cmdmacros) {
        if (*flags)
            strcat(mf, " ");
        strcat(mf, cmdmacros);
    }
    setvar("MAKEFLAGS", mf);
    childenv(mf);
}

static Task<int> make_main(int argc, char *argv[])
{
    struct nameblock *p;
    int i, j;
    int nfargs, ndescf = 0, errs = 0;
    TIMETYPE tjunk;
    char c, *d;
    const char *s;
    static char onechar[2] = "X";
    static char *descf[NARGS];

    funny['\0'] = (META | TERMINAL);
    for (s = "=|^();&<>*?[]:$`'\"\\\n"; *s; ++s)
        funny[(unsigned char)*s] |= META;
    for (s = "\n\t :;&>|"; *s; ++s)
        funny[(unsigned char)*s] |= TERMINAL;

    inarglist = 1;
    setorigin = O_CMD;
    for (i = 1; i < argc; ++i)
        if (argv[i] != 0 && argv[i][0] != '-' && strchr(argv[i], '=')) {
            addmacro(argv[i]);
            for (;;) {
                d = copys(argv[i]);
                if (!(j = eqsign(d)) || !substneed)
                    break;
                free(d);
                co_await perform();
            }
            if (j) {
                varptr(d)->exported = 1; // a child has it, as in GNU
                argv[i] = 0;
            }
            if (shellpend)
                co_await runset();
        }

    setorigin = O_DEFAULT;
    setvar("$", (char *)"$");
    inarglist = 0;
    setorigin = O_CMD;
    readmakeflags();
    if (shellpend) {
        inarglist = 1;
        co_await runset();
        inarglist = 0;
    }
    setorigin = O_FILE;

    // Every option before any file is read, as POSIX has it: v7 read a -f
    // file the moment it met it, before the options after it.
    for (i = 1; i < argc; ++i)
        if (argv[i] != 0 && argv[i][0] == '-') {
            for (j = 1; (c = argv[i][j]) != '\0'; ++j)
                if (c == 'f') {
                    if (i >= argc - 1 || argv[i + 1] == 0) {
                        fatal("No description argument after -f flag");
                        co_return 1;
                    }
                    descf[ndescf++] = argv[i + 1];
                    argv[i + 1]     = 0;
                } else if (c == 'C') {
                    // -C dir, or -Cdir: there before any makefile is read, each
                    // -C from where the last left it.  Not POSIX's, but every
                    // make's, and not passed on in MAKEFLAGS.
                    if (argv[i][j + 1] != '\0')
                        s = argv[i] + j + 1;
                    else if (i < argc - 1 && argv[i + 1] != 0) {
                        s           = argv[i + 1];
                        argv[i + 1] = 0;
                    } else {
                        fatal("No directory argument after -C flag");
                        co_return 1;
                    }
                    if (co_await b_chdir(s) != 0) {
                        fatal1("Cannot change to directory %s", s);
                        co_return 1;
                    }
                    break;
                } else if (!setflag(c)) {
                    onechar[0] = c;
                    fatal1("Unknown flag argument %s", onechar);
                    co_return 1;
                }

            argv[i] = 0;
        }

    // GNU's MAKECMDGOALS: the targets named on the command line.
    for (i = 1, d = copys(""); i < argc; ++i)
        if (argv[i] != 0) {
            s = d;
            d = (char *)ckalloc(strlen(s) + strlen(argv[i]) + 2);
            concat(s, *s ? " " : "", d);
            strcat(d, argv[i]);
            free((char *)s);
        }
    setvar("MAKECMDGOALS", d);

    for (i = 0; i < ndescf; ++i) {
        if (co_await rddescf(descf[i]))
            fatal1("Cannot open %s", descf[i]);
        if (stopped)
            co_return 1;
    }
    if (!ndescf)
        if (co_await rddescf("makefile"))
            co_await rddescf("Makefile");
    if (stopped)
        co_return 1;

    // GNU's .DEFAULT_GOAL: a makefile may name the default target, and reads
    // the one chosen when it does not.
    if ((d = substnow((char *)"$(.DEFAULT_GOAL)")) != NULL && *d) {
        for (s = d; *s == ' '; ++s)
            ;
        for (j = 0; s[j] && s[j] != ' '; ++j)
            ;
        d[s - d + j] = '\0';
        if ((p = srchname(s)) == 0)
            p = makename(s);
        mainname = p;
    } else if (mainname)
        setvar(".DEFAULT_GOAL", mainname->namep);
    substneed = NO;

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
    writemakeflags();

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
static int ismember(void);

Task<void> intrupt()
{
    char *p;

    if (okdel && !noexflag && !touchflag && (p = varptr("@")->varval) && co_await exists(p) > 0 &&
        !isprecious(p) && !isnamed(".PHONY", p) && !ismember()) {
        diag("\n***  %s removed.", p);
        co_await b_unlink(p);
    }

    if (junkname[0])
        co_await b_unlink(junkname);
    diag("\n");
    stop(2);
}

// Whether the target being made is a library member: ^C leaves a library
// alone, where v7 would have removed the whole of it as $@.
static int ismember(void)
{
    char *m = varptr("%")->varval;

    return m != NULL && *m != '\0';
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

// The environment, between the built-in rules and the makefile: a makefile's
// macro beats it, and it beats a built-in one; -e reverses the first.  SHELL
// is not taken, as POSIX has it.  MAKE is the name make was run by.
static void readenv(void)
{
    usize i, n = proc_env_count();
    Str e;
    char *def, *v;

    inarglist = envflag;
    setorigin = envflag ? O_ENVOVR : O_ENV;
    for (i = 0; i < n; ++i) {
        e = proc_env_at(i);
        if (e.starts_with("SHELL=") || e.starts_with("MAKEFLAGS="))
            continue;
        def = (char *)ckalloc(e.size() + 1);
        memcpy(def, e.data(), e.size());
        if ((v = strchr(def, '=')) != NULL && v != def) {
            *v++ = '\0';
            if (setvar(def, v))
                varptr(def)->fromenv = YES;
            varptr(def)->exported = 1;
        } else
            free(def);
    }
    inarglist = 0;
    setorigin = O_DEFAULT;
    setvar("MAKE", (char *)makename0);
    setvar("SHELL", (char *)SHELLCOM); // what runs a command, as POSIX has make provide it
    setorigin = O_FILE;
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
            co_await rdd1(NULL, 0);
        readenv();
    }
    if (stopped)
        co_return 1;
    if (!unequal(descfile, "-")) {
        if ((buf = co_await rdall(stdin, &len)) == NULL)
            co_return 1;
        r = co_await rdd1(buf, len);
        free(buf);
        co_return r;
    }

    if ((k = co_await b_fopen(descfile, "r")) != NULL) {
        buf = co_await rdall(k, &len);
        co_await b_fclose(k);
        if (buf == NULL)
            co_return 1;
        r = co_await rdd1(buf, len);
        free(buf);
        co_return r;
    }

    co_return 1;
}

static Task<int> rdd1(char *buf, int len)
{
    fin       = buf;
    finend    = buf + len;
    yylineno  = 0;
    zznextc   = 0;
    parsing   = buf != NULL;
    setorigin = buf ? O_FILE : O_DEFAULT;

    if (co_await yyparse())
        fatal("Description file error");

    fin       = NULL;
    parsing   = NO;
    setorigin = O_FILE;
    co_return 0;
}

// The files an include line names, read whole and put on the parser's input
// stack, last first, so that the first is read first.  A missing one stops
// make unless it was -include or sinclude.
Task<void> includef(char *names, int opt)
{
    char **w, **bufs, *s;
    int *lens, n, i, len;
    FILE *k;

    for (n = 0, s = names; *s;) {
        while (*s == ' ' || *s == '\t')
            ++s;
        if (*s == '\0')
            break;
        ++n;
        while (*s && *s != ' ' && *s != '\t')
            ++s;
    }
    w    = (char **)ckalloc((n + 1) * sizeof(char *));
    bufs = (char **)ckalloc((n + 1) * sizeof(char *));
    lens = (int *)ckalloc((n + 1) * sizeof(int));
    for (i = 0, s = names; i < n; ++i) {
        while (*s == ' ' || *s == '\t')
            ++s;
        w[i] = s;
        while (*s && *s != ' ' && *s != '\t')
            ++s;
        if (*s)
            *s++ = '\0';
    }

    for (i = 0; i < n && !stopped; ++i) {
        bufs[i] = NULL;
        len     = 0;
        if ((k = co_await b_fopen(w[i], "r")) != NULL) {
            bufs[i] = co_await rdall(k, &len);
            co_await b_fclose(k);
        }
        if (bufs[i] == NULL) {
            if (!opt)
                fatal1("Cannot open %s", w[i]);
            continue;
        }
        // A last line with no newline ends where the file does, not on the
        // line after the include.
        if (len == 0 || bufs[i][len - 1] != '\n') {
            bufs[i]        = (char *)realloc(bufs[i], len + 1);
            bufs[i][len++] = '\n';
        }
        lens[i] = len;
    }
    for (i = n; i-- > 0;)
        if (bufs[i] != NULL) {
            if (stopped || !pushin(bufs[i], lens[i]))
                free(bufs[i]);
        }
    free(w);
    free(bufs);
    free(lens);
}

void printdesc(int prntflag)
{
    struct nameblock *p;
    struct depblock *dp;
    struct varblock *vp;
    struct opendir *od;
    struct shblock *sp;
    struct lineblock *lp;
    struct patrule *pr;

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

    for (pr = firstpatrule; pr; pr = pr->nxtpatrule) {
        mprintf("\n\n%s:  (PATTERN)", pr->target);
        if ((dp = pr->depp)) {
            mprintf("\n depends on:");
            for (; dp; dp = dp->nxtdepblock)
                mprintf(" %s%s ", dp->orderonly ? "| " : "", dp->depname->namep);
        }
        mprintf("\n commands:\n");
        for (sp = pr->shp; sp != 0; sp = sp->nxtshblock)
            mprintf("\t%s\n", sp->shbp);
    }
    mprintf("\n");
}
