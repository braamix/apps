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

// Find a suffix rule that makes p.  Returns the source file's name block, sets
// *rulep to the rule's and *prefixp to $*, which the caller keeps; or NULL if
// there is none.  v7's buffers were 100 bytes and its names were not looked
// at; these are the names' own lengths.
static Task<struct nameblock *> implicit(struct nameblock *p, char **prefixp,
                                         struct nameblock **rulep)
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
                if ((p1 = srchcat(p1namep, ".a")) && (p2 = srchcat(prefix, p1namep))) {
                    *rulep   = p1;
                    *prefixp = prefix;
                    co_return p2;
                }
            }
        goto none;
    }

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
                        if ((p1 = srchcat(p1namep, pnamep)) && (p2 = srchcat(prefix, p1namep))) {
                            *rulep   = p1;
                            *prefixp = prefix;
                            co_return p2;
                        }
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
                *rulep   = p1;
                *prefixp = prefix;
                co_return p2;
            }
        }
none:
    free(prefix);
    co_return NULL;
}

// Run the commands that bring p up to date and return its new time.
static Task<TIMETYPE> makeit(struct nameblock *p, TIMETYPE tdep, struct shblock *explcom,
                             struct shblock *implcom, struct chain *qchain, struct chain *allq,
                             int *errstatp)
{
    struct nameblock *p1;
    struct lineblock *lp2;
    TIMETYPE ptime;

    ptime = (tdep > 0 ? tdep : co_await prestime());
    setat(p->namep);
    setvar("?", mkqlist(qchain));
    setvar("^", mkqlist(allq));
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
    struct nameblock *p1, *p2;
    struct shblock *implcom, *explcom;
    struct lineblock *lp;
    struct lineblock *lp2;
    char *prefix, *stem;
    struct chain *qchain, *allq, *lineq;
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
    implcom = 0;
    explcom = 0;
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
        td    = 0;
        lineq = NULL;
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
            if (dbgflag)
                mprintf("TIME(%s)=%lld\n", q->depname->namep, td1);
            if (td1 > td)
                td = td1;
            if (ptime < td1)
                qchain = appendq(qchain, q->depname->namep);
            allq  = appendu(allq, q->depname->namep);
            lineq = appendu(lineq, q->depname->namep);
        }
        if (p->septype == SOMEDEPS) {
            if (lp->shp != 0)
                if (ptime < td || (ptime == 0 && td == 0) || lp->depp == 0) {
                    okdel1 = okdel;
                    okdel  = NO;
                    setat(p->namep);
                    setvar("?", mkqlist(qchain));
                    setvar("^", mkqlist(lineq));
                    setvar("<", lineq ? lineq->datap : (char *)"");
                    qchain = NULL;
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
                else
                    explcom = lp->shp;
            }

            if (td > tdep)
                tdep = td;
        }
    }

    // Look for implicit dependents, using suffix rules
    p1     = NULL;
    p2     = NULL;
    prefix = NULL;
    // Only a target with no commands of its own, as POSIX has it: v7 looked
    // for every target, and gave one with commands a prerequisite besides.
    if (!phony && explcom == 0 && p->septype != SOMEDEPS)
        p2 = co_await implicit(p, &prefix, &p1);
    // $< in an explicit rule is its first prerequisite, and $* the target
    // less a suffix .SUFFIXES knows, as GNU make has them; an inference rule
    // sets its own below.  v7 left both as the last inference rule set them.
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
    if (p2 != NULL) {
        errstat += co_await doname(p2, reclevel + 1, &td);
        if (stopped)
            co_return 1;
        if (ptime < td)
            qchain = appendq(qchain, p2->namep);
        if (dbgflag)
            mprintf("TIME(%s)=%lld\n", p2->namep, td);
        if (td > tdep)
            tdep = td;
        setvar("*", prefix);
        setvar("<", p2->namep);
        allq = prependu(allq, p2->namep); // the inferred prerequisite first, as GNU has it
        for (lp2 = p1->linep; lp2; lp2 = lp2->nxtlineblock)
            if ((implcom = lp2->shp))
                break;
    }
    if (stopped)
        co_return 1;

    if (errstat == 0 && (ptime < tdep || (ptime == 0 && tdep == 0)))
        ptime = co_await makeit(p, tdep, explcom, implcom, qchain, allq, &errstat);

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

// The commands, each printed and run.  -n prints and runs none, so nothing
// waits: docom1() is entered only when a command is to be run.
Task<int> docom(struct shblock *q)
{
    char *s, *at;
    int ign, nopr, sil, plus;
    // The expanded line, as long as it is: v7's was OUTMAX bytes.  Kept at file
    // scope until the next, since only one docom() is ever live.
    static char *string;

    ++ndocoms;

    at  = varptr("@")->varval;
    sil = silflag || isnamed(".SILENT", at);

    // -t and -q run none of the commands but the + ones, as -n does; -t then
    // touches the target.
    for (; q; q = q->nxtshblock) {
        free(string);
        string = substa(q->shbp);
        if (stopped)
            co_return YES;

        // A + line, or one naming $(MAKE), runs under -n too: that is how
        // a recursive make passes -n down rather than being skipped.
        ign  = ignerr || isnamed(".IGNORE", at);
        nopr = NO;
        plus = strstr(q->shbp, "$(MAKE)") || strstr(q->shbp, "${MAKE}");
        for (s = string; *s == '-' || *s == '@' || *s == '+'; ++s)
            if (*s == '-')
                ign = YES;
            else if (*s == '@')
                nopr = YES;
            else
                plus = YES;

        if (s[0] == '\0' || ((touchflag || questflag) && !plus))
            continue;
        // -n writes what would run, silent or not, as POSIX and GNU have it.
        if (noexflag || (!sil && !nopr))
            mprintf("%s%s\n", (noexflag ? "" : prompt), s);
        if (noexflag && !plus)
            continue;

        if (co_await docom1(s, ign) && !ign) {
            if (keepgoing)
                co_return YES;
            else
                fatal((char *)NULL);
        }
        if (stopped)
            co_return YES;
    }

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
