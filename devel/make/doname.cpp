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

// Find a suffix rule that makes p.  Returns the source file's name block and
// sets *rulep to the rule's, or NULL if there is none; prefix and sourcename are
// the caller's buffers, 100 bytes each.  `single' allows a single-suffix rule.
static Task<struct nameblock *> implicit(struct nameblock *p, char *prefix, char *sourcename,
                                         struct nameblock **rulep, int single)
{
    struct lineblock *lp, *lp1;
    struct depblock *suffp, *suffp1;
    struct nameblock *p1, *p2;
    char *pnamep, *p1namep;
    char temp[100], concsuff[20];

    // The buffers are 100 bytes, and v7 did not look: a longer name has no
    // inference rule rather than an overrun.
    if (strlen(p->namep) + sizeof(concsuff) >= sizeof(temp))
        co_return NULL;

    for (lp = sufflist; lp; lp = lp->nxtlineblock)
        for (suffp = lp->depp; suffp; suffp = suffp->nxtdepblock) {
            pnamep = suffp->depname->namep;
            if (suffix(p->namep, pnamep, prefix)) {
                co_await srchdir(concat(prefix, "*", temp), NO, (struct depblock *)NULL);
                if (stopped)
                    co_return NULL;
                for (lp1 = sufflist; lp1; lp1 = lp1->nxtlineblock)
                    for (suffp1 = lp1->depp; suffp1; suffp1 = suffp1->nxtdepblock) {
                        p1namep = suffp1->depname->namep;
                        if ((p1 = srchname(concat(p1namep, pnamep, concsuff))) &&
                            (p2 = srchname(concat(prefix, p1namep, sourcename)))) {
                            *rulep = p1;
                            co_return p2;
                        }
                    }
            }
        }

    // POSIX's single-suffix rules: `.c:' makes a target from the target's name
    // with .c after it, as hello from hello.c.  Only a suffix with a rule of
    // its own counts; .SUFFIXES names the rest.  Only for a target with no
    // commands of its own, as POSIX has it.
    if (!single)
        co_return NULL;
    for (lp = sufflist; lp; lp = lp->nxtlineblock)
        for (suffp = lp->depp; suffp; suffp = suffp->nxtdepblock) {
            p1 = suffp->depname;
            if (p1->linep == 0 || strlen(p->namep) + strlen(p1->namep) >= sizeof(temp))
                continue;
            concat(p->namep, "*", temp);
            co_await srchdir(temp, NO, (struct depblock *)NULL);
            if (stopped)
                co_return NULL;
            if ((p2 = srchname(concat(p->namep, p1->namep, sourcename)))) {
                strcpy(prefix, p->namep);
                *rulep = p1;
                co_return p2;
            }
        }
    co_return NULL;
}

// Run the commands that bring p up to date and return its new time.
static Task<TIMETYPE> makeit(struct nameblock *p, TIMETYPE tdep, struct shblock *explcom,
                             struct shblock *implcom, struct chain *qchain, int *errstatp)
{
    struct nameblock *p1;
    struct lineblock *lp2;
    TIMETYPE ptime;

    ptime = (tdep > 0 ? tdep : co_await prestime());
    setvar("@", p->namep);
    setvar("?", mkqlist(qchain));
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
    if (noexflag || (ptime = co_await exists(p->namep)) == 0)
        ptime = co_await prestime();
    co_return ptime;
}

// prefix and sourcename, which v7 kept on the stack: here they would hold a
// coroutine frame past its 512 bytes.
struct names {
    char sourcename[100], prefix[100];
};

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
    struct names *nb;
    struct chain *qchain;

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
    ptime   = co_await exists(p->namep);
    ptime1  = 0;
    didwork = NO;
    p->done = 1; // avoid infinite loops
    if (stopped)
        co_return 1;

    qchain = NULL;

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
        td = 0;
        for (q = lp->depp; q; q = q->nxtdepblock) {
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
        }
        if (p->septype == SOMEDEPS) {
            if (lp->shp != 0)
                if (ptime < td || (ptime == 0 && td == 0) || lp->depp == 0) {
                    okdel1 = okdel;
                    okdel  = NO;
                    setvar("@", p->namep);
                    setvar("?", mkqlist(qchain));
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
    p1 = NULL;
    nb = ALLOC(names);
    if ((p2 = co_await implicit(p, nb->prefix, nb->sourcename, &p1,
                                explcom == 0 && p->septype != SOMEDEPS)) != NULL) {
        errstat += co_await doname(p2, reclevel + 1, &td);
        if (stopped)
            co_return 1;
        if (ptime < td)
            qchain = appendq(qchain, p2->namep);
        if (dbgflag)
            mprintf("TIME(%s)=%lld\n", p2->namep, td);
        if (td > tdep)
            tdep = td;
        setvar("*", nb->prefix);
        setvar("<", copys(nb->sourcename));
        for (lp2 = p1->linep; lp2; lp2 = lp2->nxtlineblock)
            if ((implcom = lp2->shp))
                break;
    } else
        free(nb); // kept otherwise: $* points into it
    if (stopped)
        co_return 1;

    if (errstat == 0 && (ptime < tdep || (ptime == 0 && tdep == 0)))
        ptime = co_await makeit(p, tdep, explcom, implcom, qchain, &errstat);

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
    // At file scope, not in a frame: OUTMAX would be past a coroutine's 512
    // bytes, and only one docom() is ever live.
    static char string[OUTMAX];

    ++ndocoms;
    if (questflag)
        co_return NO;

    at  = varptr("@")->varval;
    sil = silflag || isnamed(".SILENT", at);
    if (touchflag) {
        s = at;
        if (!sil)
            mprintf("touch(%s)\n", s);
        if (!noexflag)
            co_await touch(YES, s);
    }

    else
        for (; q; q = q->nxtshblock) {
            subst(q->shbp, string, string + OUTMAX - 1);
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

            if (s[0] == '\0')
                continue;
            if (!sil && (!nopr || noexflag))
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
