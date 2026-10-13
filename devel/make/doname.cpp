/* UNIX V7 source code: see /COPYRIGHT or www.tuhs.org for details. */

#include <stdlib.h>
#include <string.h>

#include "defs.h"

//  BASIC PROCEDURE.  RECURSIVE.
//
// p->done = 0   don't know what to do yet
// p->done = 1   file in process of being updated
// p->done = 2   file already exists in current state
// p->done = 3   file make failed

static Task<int> docom1(char *comstring, int nohalt);

// $@ for a target, and $% beside it: for lib(member) they are lib and member,
// as POSIX has them, and for anything else the name and nothing.
static void setat(const char *name)
{
    char *lib, *mem;

    if (ismember(name, &lib, &mem)) {
        setvar("@", lib);
        setvar("%", mem);
    } else {
        setvar("@", (char *)name);
        setvar("%", (char *)"");
    }
}

// Two names run together, in a buffer of their own.
static char *concata(const char *a, const char *b)
{
    return concat(a, b, (char *)ckalloc(strlen(a) + strlen(b) + 1));
}

// Whether the names a and b run together name something, freeing the name.
static struct nameblock *srchcat(const char *a, const char *b)
{
    char *s             = concata(a, b);
    struct nameblock *p = srchname(s);

    free(s);
    return p;
}

// Lists the directory for prefix*, so that a source nobody named is known.
static Task<void> srchpre(const char *prefix)
{
    char *pat = concata(prefix, "*");

    co_await srchdir(pat, NO, (struct depblock *)NULL);
    free(pat);
}

// What an inference rule gives a target: its prerequisites, the first being
// $<, the stem that is $*, and the commands.
struct implied {
    struct depblock *deps;
    char *stem;
    struct shblock *com;
};

// The name, or a new one.
static struct nameblock *named(const char *s)
{
    struct nameblock *p = srchname(s);

    return p ? p : makename(s);
}

// One prerequisite as an inference rule names it, at the head of the list it
// is building.
static void impdep(struct implied *im, struct nameblock *np, int orderonly)
{
    struct depblock *d = ALLOC(depblock), **dp;

    d->depname   = np;
    d->orderonly = orderonly;
    for (dp = &im->deps; *dp; dp = &(*dp)->nxtdepblock)
        ;
    *dp = d;
}

// GNU's pattern rules, tried before the suffix rules.  Of those whose target
// matches p and whose prerequisites, the stem put in, are files or are named
// in the makefile, the one with the shortest stem, the first of those.  A
// target pattern with no slash is matched against the file part of the name,
// and the directory goes back on the stem and on each prerequisite.
static Task<int> patimplicit(struct nameblock *p, struct implied *im)
{
    struct patrule *pr, *best = NULL;
    struct depblock *q;
    char *slash, *stem, *beststem = NULL, *name;
    int dirlen, bestdir = 0, ok;

    slash = strrchr(p->namep, '/');
    for (pr = firstpatrule; pr && !stopped; pr = pr->nxtpatrule) {
        dirlen = slash && !strchr(pr->target, '/') ? slash + 1 - p->namep : 0;
        if ((stem = patstem(pr->target, p->namep + dirlen)) == NULL)
            continue;
        if (beststem && strlen(stem) >= strlen(beststem)) {
            free(stem);
            continue;
        }
        for (ok = YES, q = pr->depp; q && ok && !stopped; q = q->nxtdepblock) {
            name = patput(q->depname->namep, stem);
            if (dirlen) {
                char *t = (char *)ckalloc(dirlen + strlen(name) + 1);

                memcpy(t, p->namep, dirlen);
                strcpy(t + dirlen, name);
                free(name);
                name = t;
            }
            ok = srchname(name) != 0 || co_await exists(name) > 0;
            free(name);
        }
        if (ok && !stopped) {
            free(beststem);
            best     = pr;
            beststem = stem;
            bestdir  = dirlen;
        } else
            free(stem);
    }
    if (best == NULL || stopped) {
        free(beststem);
        co_return NO;
    }
    for (q = best->depp; q; q = q->nxtdepblock) {
        name = patput(q->depname->namep, beststem);
        stem = (char *)ckalloc(bestdir + strlen(name) + 1);
        memcpy(stem, p->namep, bestdir);
        strcpy(stem + bestdir, name);
        impdep(im, named(stem), q->orderonly);
        free(name);
        free(stem);
    }
    im->stem = (char *)ckalloc(bestdir + strlen(beststem) + 1);
    memcpy(im->stem, p->namep, bestdir);
    strcpy(im->stem + bestdir, beststem);
    free(beststem);
    im->com = best->shp;
    co_return YES;
}

// A suffix rule's commands: those of its first line with any.
static struct shblock *rulecom(struct nameblock *rp)
{
    struct lineblock *lp;

    for (lp = rp->linep; lp; lp = lp->nxtlineblock)
        if (lp->shp)
            return lp->shp;
    return NULL;
}

// Find an inference rule that makes p, and fill *im from it: NO if there is
// none.  The pattern rules first, then the suffix rules.  v7's buffers were
// 100 bytes and its names were not looked at; these are the names' own
// lengths.
static Task<int> implicit(struct nameblock *p, struct implied *im)
{
    struct lineblock *lp, *lp1;
    struct depblock *suffp, *suffp1;
    struct nameblock *p1, *p2;
    char *pnamep, *p1namep, *prefix;

    // lib(member), as POSIX has it: the member less its suffix is $*, and the
    // rule is from a source's suffix to .a -- lib.a(x.o) from x.c by .c.a.
    if (ismember(p->namep, &prefix, &pnamep)) {
        free(prefix);
        prefix = pnamep;
        for (lp = sufflist; lp; lp = lp->nxtlineblock)
            for (suffp = lp->depp; suffp; suffp = suffp->nxtdepblock)
                if (suffix(pnamep, suffp->depname->namep, prefix))
                    goto stemmed;
    stemmed:
        co_await srchpre(prefix);
        if (stopped)
            goto none;
        for (lp = sufflist; lp; lp = lp->nxtlineblock)
            for (suffp = lp->depp; suffp; suffp = suffp->nxtdepblock) {
                p1namep = suffp->depname->namep;
                if ((p1 = srchcat(p1namep, ".a")) && (p2 = srchcat(prefix, p1namep)))
                    goto found;
            }
        goto none;
    }

    if (firstpatrule && co_await patimplicit(p, im))
        co_return YES;
    if (stopped)
        co_return NO;

    prefix = (char *)ckalloc(strlen(p->namep) + 1);
    for (lp = sufflist; lp; lp = lp->nxtlineblock)
        for (suffp = lp->depp; suffp; suffp = suffp->nxtdepblock) {
            pnamep = suffp->depname->namep;
            if (suffix(p->namep, pnamep, prefix)) {
                co_await srchpre(prefix);
                if (stopped)
                    goto none;
                for (lp1 = sufflist; lp1; lp1 = lp1->nxtlineblock)
                    for (suffp1 = lp1->depp; suffp1; suffp1 = suffp1->nxtdepblock) {
                        p1namep = suffp1->depname->namep;
                        if ((p1 = srchcat(p1namep, pnamep)) && (p2 = srchcat(prefix, p1namep)))
                            goto found;
                    }
            }
        }

    // POSIX's single-suffix rules: `.c:' makes a target from the target's name
    // with .c after it, as hello from hello.c.  Only a suffix with a rule of
    // its own counts; .SUFFIXES names the rest.
    for (lp = sufflist; lp; lp = lp->nxtlineblock)
        for (suffp = lp->depp; suffp; suffp = suffp->nxtdepblock) {
            p1 = suffp->depname;
            if (p1->linep == 0)
                continue;
            co_await srchpre(p->namep);
            if (stopped)
                goto none;
            if ((p2 = srchcat(p->namep, p1->namep))) {
                strcpy(prefix, p->namep);
                goto found;
            }
        }
none:
    free(prefix);
    co_return NO;

found:
    impdep(im, p2, NO);
    im->stem = prefix;
    im->com  = rulecom(p1);
    co_return YES;
}

// $?, $^, $+ and $| for commands about to run.
static void setlists(struct chain *qchain, struct chain *allq, struct chain *plusq,
                     struct chain *ooq)
{
    setvar("?", mkqlist(qchain));
    setvar("^", mkqlist(allq));
    setvar("+", mkqlist(plusq));
    setvar("|", mkqlist(ooq));
}

static Task<TIMETYPE> makeit(struct nameblock *p, TIMETYPE tdep, TIMETYPE ptime0,
                             struct shblock *explcom, struct shblock *implcom,
                             struct chain **lists, int *errstatp)
{
    struct nameblock *p1;
    struct lineblock *lp2;
    TIMETYPE ptime;

    ptime = (tdep > 0 ? tdep : co_await prestime());
    setat(p->namep);
    setlists(lists[0], lists[1], lists[2], lists[3]);
    attime = ptime0;
    if (explcom)
        *errstatp += co_await docom(explcom);
    else if (implcom)
        *errstatp += co_await docom(implcom);
    else if (p->septype == 0) {
        if ((p1 = srchname(".DEFAULT"))) {
            setvar("<", p->namep);
            for (lp2 = p1->linep; lp2; lp2 = lp2->nxtlineblock)
                if ((implcom = lp2->shp)) {
                    *errstatp += co_await docom(implcom);
                    break;
                }
        } else if (keepgoing) {
            mprintf("Don't know how to make %s\n", p->namep);
            ++*errstatp;
        } else
            fatal1(" Don't know how to make %s", p->namep);
    }
    if (stopped)
        co_return 0;

    setvar("@", (char *)NULL);
    if (noexflag || isnamed(".PHONY", p->namep) || (ptime = co_await exists(p->namep)) == 0)
        ptime = co_await prestime();
    co_return ptime;
}

Task<int> doname(struct nameblock *p, int reclevel, TIMETYPE *tval)
{
    int errstat;
    int okdel1;
    int didwork;
    TIMETYPE td, td1, tdep, ptime, ptime1;
    struct depblock *q;
    struct depblock *qtemp;
    struct shblock *implcom, *explcom;
    struct lineblock *lp;
    struct implied im;
    char *stem, *explstem;
    struct chain *qchain, *allq, *lineq, *plusq, *lineplus, *ooq, *lists[4];
    int phony;

    if (p == 0) {
        *tval = 0;
        co_return 0;
    }

    if (dbgflag)
        mprintf("doname(%s,%d)\n", p->namep, reclevel);

    if (p->done > 0) {
        *tval = p->modtime;
        co_return p->done == 3;
    }

    errstat = 0;
    tdep    = 0;
    implcom  = 0;
    explcom  = 0;
    explstem = NULL;
    // A .PHONY target is no file: never looked for, and always remade.
    phony   = isnamed(".PHONY", p->namep);
    ptime   = phony ? 0 : co_await exists(p->namep);
    ptime1  = 0;
    didwork = NO;
    p->done = 1; // avoid infinite loops
    if (stopped)
        co_return 1;

    qchain = NULL;
    allq   = NULL; // $^: every prerequisite, once each
    plusq  = NULL; // $+: every one, in order, repeats kept
    ooq    = NULL; // $|: the order-only ones

    // Expand any names that have embedded metacharaters
    for (lp = p->linep; lp; lp = lp->nxtlineblock)
        for (q = lp->depp; q; q = qtemp) {
            qtemp = q->nxtdepblock;
            if (q->depname && strpbrk(q->depname->namep, "*?[")) {
                co_await expand(q);
                if (stopped)
                    co_return 1;
            }
        }

    // make sure all dependents are up to date
    for (lp = p->linep; lp; lp = lp->nxtlineblock) {
        td       = 0;
        lineq    = NULL;
        lineplus = NULL;
        for (q = lp->depp; q; q = q->nxtdepblock) {
            // POSIX's .WAIT orders what is made in parallel, and this make
            // makes one thing at a time: it is no prerequisite at all.
            if (q->depname && !unequal(q->depname->namep, ".WAIT"))
                continue;
            // A name already done, or one expand() emptied, answers without
            // waiting: a co_await is a call, and costs native stack until
            // something suspends.  README.md, "Output".
            if (q->depname == 0)
                td1 = 0;
            else if (q->depname->done > 0) {
                if (dbgflag)
                    mprintf("doname(%s,%d)\n", q->depname->namep, reclevel + 1);
                td1 = q->depname->modtime;
                errstat += q->depname->done == 3;
            } else
                errstat += co_await doname(q->depname, reclevel + 1, &td1);
            if (stopped)
                co_return 1;
            if (q->depname == 0)
                continue; // v7 went on to read q->depname->namep
            if (q->orderonly) {
                ooq = appendu(ooq, q->depname->namep);
                continue;
            }
            if (dbgflag)
                mprintf("TIME(%s)=%lld\n", q->depname->namep, td1);
            if (td1 > td)
                td = td1;
            if (ptime < td1)
                qchain = appendu(qchain, q->depname->namep);
            allq     = appendu(allq, q->depname->namep);
            lineq    = appendu(lineq, q->depname->namep);
            plusq    = appendq(plusq, q->depname->namep);
            lineplus = appendq(lineplus, q->depname->namep);
        }
        if (p->septype == SOMEDEPS) {
            if (lp->shp != 0)
                if (ptime < td || (ptime == 0 && td == 0) || lp->depp == 0) {
                    okdel1 = okdel;
                    okdel  = NO;
                    setat(p->namep);
                    setlists(qchain, lineq, lineplus, ooq);
                    setvar("<", lineq ? lineq->datap : (char *)"");
                    qchain = NULL;
                    attime = ptime;
                    if (!questflag)
                        errstat += co_await docom(lp->shp);
                    if (stopped)
                        co_return 1;
                    setvar("@", (char *)NULL);
                    okdel   = okdel1;
                    ptime1  = co_await prestime();
                    didwork = YES;
                }
        }

        else {
            if (lp->shp != 0) {
                if (explcom)
                    diag("Too many command lines for `%s'\n", p->namep);
                else {
                    explcom  = lp->shp;
                    explstem = lp->stem;
                }
            }

            if (td > tdep)
                tdep = td;
        }
    }

    // Look for implicit dependents, using pattern and suffix rules
    im.deps = NULL;
    im.stem = NULL;
    im.com  = NULL;
    // Only a target with no commands of its own, as POSIX has it: v7 looked
    // for every target, and gave one with commands a prerequisite besides.
    if (!phony && explcom == 0 && p->septype != SOMEDEPS)
        co_await implicit(p, &im);
    if (stopped)
        co_return 1;
    // $< in an explicit rule is its first prerequisite, and $* the target
    // less a suffix .SUFFIXES knows, or a static pattern's stem, as GNU make
    // has them; an inference rule sets its own below.  v7 left both as the
    // last inference rule set them.
    setvar("<", allq ? allq->datap : (char *)"");
    setvar("*", (char *)"");
    stem = (char *)ckalloc(strlen(p->namep) + 1);
    for (lp = sufflist; lp; lp = lp->nxtlineblock)
        for (q = lp->depp; q; q = q->nxtdepblock)
            if (q->depname && suffix(p->namep, q->depname->namep, stem)) {
                setvar("*", stem);
                stem = NULL;
                goto stemmed;
            }
    free(stem);
stemmed:
    if (explstem)
        setvar("*", explstem);
    if (im.deps != NULL) {
        struct chain *impq = NULL, *impplus = NULL;

        for (q = im.deps; q; q = q->nxtdepblock) {
            if (q->depname->done > 0) {
                td = q->depname->modtime;
                errstat += q->depname->done == 3;
            } else
                errstat += co_await doname(q->depname, reclevel + 1, &td);
            if (stopped)
                co_return 1;
            if (q->orderonly) {
                ooq = appendu(ooq, q->depname->namep);
                continue;
            }
            if (ptime < td)
                qchain = appendu(qchain, q->depname->namep);
            if (dbgflag)
                mprintf("TIME(%s)=%lld\n", q->depname->namep, td);
            if (td > tdep)
                tdep = td;
            impq    = appendu(impq, q->depname->namep);
            impplus = appendq(impplus, q->depname->namep);
        }
        setvar("*", im.stem);
        setvar("<", impq ? impq->datap : (char *)"");
        // the inferred prerequisites first, as GNU has them
        allq    = joinu(impq, allq);
        plusq   = joinq(impplus, plusq);
        implcom = im.com;
    }

    if (errstat == 0 && (ptime < tdep || (ptime == 0 && tdep == 0))) {
        lists[0] = qchain, lists[1] = allq, lists[2] = plusq, lists[3] = ooq;
        ptime    = co_await makeit(p, tdep, ptime, explcom, implcom, lists, &errstat);
    }

    else if (errstat != 0 && reclevel == 0)
        mprintf("`%s' not remade because of errors\n", p->namep);

    else if (!questflag && reclevel == 0 && didwork == NO)
        mprintf("`%s' is up to date.\n", p->namep);
    if (stopped)
        co_return 1;

    if (questflag && reclevel == 0) {
        stop(ndocoms > 0 ? 1 : 0); // v7's exit(-1); POSIX's 1
        co_return 1;
    }

    p->done = (errstat ? 3 : 2);
    if (ptime1 > ptime)
        ptime = ptime1;
    p->modtime = ptime;
    *tval      = ptime;
    co_return errstat;
}

// The commands of a rule, every one expanded before the first is run, as GNU
// does, and NULL after them.  A $(shell) or $(wildcard) is performed and the
// whole expanded again.
static char **expcoms(struct shblock *q0, int **plusp)
{
    struct shblock *q;
    char **v;
    int n, i;

    for (n = 0, q = q0; q; q = q->nxtshblock)
        ++n;
    v      = (char **)ckalloc((n + 1) * sizeof(char *));
    *plusp = (int *)ckalloc((n + 1) * sizeof(int));
    for (i = 0, q = q0; q && !stopped; q = q->nxtshblock, ++i) {
        v[i] = substa(q->shbp);
        // A + line, or one naming $(MAKE), runs under -n too: that is how
        // a recursive make passes -n down rather than being skipped.
        (*plusp)[i] = strstr(q->shbp, "$(MAKE)") || strstr(q->shbp, "${MAKE}");
    }
    return v;
}

static void freecoms(char **v, int *plus)
{
    int i;

    for (i = 0; v[i]; ++i)
        free(v[i]);
    free(v);
    free(plus);
}

// The target's time before its commands ran, for .DELETE_ON_ERROR.
TIMETYPE attime;

// GNU's .DELETE_ON_ERROR: a target its failed command changed is removed, as
// ^C removes it.
static Task<void> delonerr(char *at)
{
    TIMETYPE t;
    char *mem = varptr("%")->varval;

    if (at == NULL || !srchname(".DELETE_ON_ERROR") || isprecious(at) || isnamed(".PHONY", at))
        co_return;
    if (mem && *mem) // a library is not removed for a member
        co_return;
    if ((t = co_await exists(at)) > 0 && t != attime) {
        diag("***  %s removed.\n", at);
        co_await b_unlink(at);
    }
}

// The commands, each printed and run.  -n prints and runs none, so nothing
// waits: docom1() is entered only when a command is to be run.
Task<int> docom(struct shblock *q)
{
    char *s, *at, *line, *next, **coms;
    int ign, nopr, sil, plus, *plus0, r, i;

    ++ndocoms;

    at  = varptr("@")->varval;
    sil = silflag || isnamed(".SILENT", at);

    // Each expanded line as long as it is: v7's were OUTMAX bytes.
    for (;;) {
        coms = expcoms(q, &plus0);
        if (stopped || !substneed)
            break;
        freecoms(coms, plus0);
        co_await perform(); // $(shell) or $(wildcard): run, and expand again
    }
    if (stopped) {
        freecoms(coms, plus0);
        co_return YES;
    }

    // -t and -q run none of the commands but the + ones, as -n does; -t then
    // touches the target.
    for (i = 0; coms[i]; ++i) {
        // A value with lines in it, as a define has, is a command per line,
        // each with its own prefixes.  GNU's canned recipes.
        for (line = coms[i]; line != NULL; line = next) {
            for (next = line; *next && (*next != '\n' || (next > line && next[-1] == '\\'));
                 ++next)
                ;
            if (*next)
                *next++ = '\0';
            else
                next = NULL;

            ign  = ignerr || isnamed(".IGNORE", at);
            nopr = NO;
            plus = plus0[i];
            for (s = line; *s == '-' || *s == '@' || *s == '+' || *s == ' ' || *s == '\t'; ++s)
                if (*s == '-')
                    ign = YES;
                else if (*s == '@')
                    nopr = YES;
                else if (*s == '+')
                    plus = YES;

            if (s[0] == '\0' || ((touchflag || questflag) && !plus))
                continue;
            // -n writes what would run, silent or not, as POSIX and GNU have it.
            if (noexflag || (!sil && !nopr))
                mprintf("%s%s\n", (noexflag ? "" : prompt), s);
            if (noexflag && !plus)
                continue;

            r = co_await docom1(s, ign);
            forget(); // the command may have changed what $(shell) would say
            if (r && !ign) {
                freecoms(coms, plus0);
                co_await delonerr(at);
                if (keepgoing)
                    co_return YES;
                fatal((char *)NULL);
                co_return YES;
            }
            if (stopped) {
                freecoms(coms, plus0);
                co_return YES;
            }
        }
    }
    freecoms(coms, plus0);

    if (touchflag && !questflag) {
        if (!sil)
            mprintf("touch(%s)\n", at);
        if (!noexflag)
            co_await touch(YES, at);
    }
    co_return NO;
}

// What is left of v7's docom1() once the printing has moved up into docom():
// the command run, and its status said.
static Task<int> docom1(char *comstring, int nohalt)
{
    int status;

    if ((status = co_await dosys(comstring, nohalt))) {
        if (stopped)
            co_return status;
        if (status >> 8)
            mprintf("*** Error code %d", status >> 8);
        else
            mprintf("*** Termination code %d", status);

        if (nohalt)
            mprintf(" (ignored)\n");
        else
            mprintf("\n");
    }

    co_return status;
}

// If there are any Shell meta characters in the name,
// expand into a list, after searching directory
Task<void> expand(struct depblock *q)
{
    char *s;
    char *s1;
    struct depblock *p;

    if (q->depname == 0)
        co_return;
    s1 = q->depname->namep;
    for (s = s1;;)
        switch (*s++) {
        case '\0':
            co_return;

        case '*':
        case '?':
        case '[':
            if ((p = co_await srchdir(s1, YES, q->nxtdepblock))) {
                q->nxtdepblock = p;
                q->depname     = 0;
            }
            co_return;
        }
}
