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
static Task<void> startjob(struct nameblock *p, struct shblock *com, TIMETYPE ptime0);

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
// matches pname and whose prerequisites, the stem put in, are files, or are
// named in the makefile, or can be made by another pattern rule, the one with
// the shortest stem, the first of those.  A target pattern with no slash is
// matched against the file part of the name, and the directory goes back on
// the stem and on each prerequisite.  With im NULL it only asks whether one
// applies, for a chain: a rule is used once in a chain, ten deep at most.
static Task<int> patimplicit(const char *pname, struct implied *im, int depth)
{
    struct patrule *pr, *best = NULL;
    struct depblock *q;
    struct nameblock *np;
    char *slash, *stem, *beststem = NULL, *name, *path;
    int dirlen, bestdir = 0, ok;

    slash = strrchr(pname, '/');
    for (pr = firstpatrule; pr && !stopped; pr = pr->nxtpatrule) {
        if (pr->busy)
            continue;
        dirlen = slash && !strchr(pr->target, '/') ? slash + 1 - pname : 0;
        if ((stem = patstem(pr->target, pname + dirlen)) == NULL)
            continue;
        if (beststem && strlen(stem) >= strlen(beststem)) {
            free(stem);
            continue;
        }
        pr->busy = YES;
        for (ok = YES, q = pr->depp; q && ok && !stopped; q = q->nxtdepblock) {
            name = patput(q->depname->namep, stem);
            if (dirlen) {
                char *t = (char *)ckalloc(dirlen + strlen(name) + 1);

                memcpy(t, pname, dirlen);
                strcpy(t + dirlen, name);
                free(name);
                name = t;
            }
            ok = srchname(name) != 0 || co_await exists(name) > 0;
            if (!ok && havevpath()) {
                ok = co_await vfind(name, &path) > 0;
                free(path);
            }
            if (!ok && depth < 10) // made through another rule: a chain
                ok = co_await patimplicit(name, NULL, depth + 1);
            free(name);
        }
        pr->busy = NO;
        if (ok && !stopped) {
            free(beststem);
            best     = pr;
            beststem = stem;
            bestdir  = dirlen;
            if (im == NULL)
                break;
        } else
            free(stem);
    }
    if (best == NULL || stopped || im == NULL) {
        free(beststem);
        co_return best != NULL && !stopped;
    }
    for (q = best->depp; q; q = q->nxtdepblock) {
        name = patput(q->depname->namep, beststem);
        stem = (char *)ckalloc(bestdir + strlen(name) + 1);
        memcpy(stem, pname, bestdir);
        strcpy(stem + bestdir, name);
        // Neither named nor there: a link in a chain, which is intermediate.
        if ((np = srchname(stem)) == 0) {
            np = makename(stem);
            if (co_await exists(stem) == 0 && !(havevpath() && co_await vfind(stem, &np->path)))
                np->intermediate = YES;
        }
        impdep(im, np, q->orderonly);
        free(name);
        free(stem);
    }
    im->stem = (char *)ckalloc(bestdir + strlen(beststem) + 1);
    memcpy(im->stem, pname, bestdir);
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

// The names a and b run together, if VPATH or vpath finds them.
static Task<struct nameblock *> vnamed(const char *a, const char *b)
{
    char *s = concata(a, b), *path;
    struct nameblock *p = NULL;

    if (co_await vfind(s, &path) > 0) {
        free(path);
        p = makename(s);
    }
    free(s);
    co_return p;
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

    if (firstpatrule && co_await patimplicit(p->namep, im, 0))
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
                        if ((p1 = srchcat(p1namep, pnamep)) &&
                            ((p2 = srchcat(prefix, p1namep)) ||
                             (havevpath() && (p2 = co_await vnamed(prefix, p1namep)))))
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
    if (explcom || implcom)
        p->made = YES; // an intermediate made is removed at the end
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
        } else if (remaking) // a makefile with no rule is what it is
            ++*errstatp;
        else if (keepgoing) {
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

// A macro as it was before a target's own value was put in effect.
struct saved {
    struct saved *next;
    struct varblock *vp;
    char *val;
    int simple, used, fromenv, origin, exported, noreset;
};
static struct saved *savedvars;

// One target-specific or pattern-specific macro put in effect.
static void apply1(struct tvar *t)
{
    struct varblock *vp = varptr(t->name);
    struct saved *sv    = ALLOC(saved);

    sv->vp       = vp;
    sv->val      = vp->varval;
    sv->simple   = vp->simple;
    sv->used     = vp->used;
    sv->fromenv  = vp->fromenv;
    sv->origin   = vp->origin;
    sv->exported = vp->exported;
    sv->noreset  = vp->noreset;
    sv->next     = savedvars;
    savedvars    = sv;

    vp->used   = NO; // no "changed after being used": this is what it is for
    overriding = t->over;
    if (t->op == 'S') {
        if (setvar(t->name, copys(t->val)))
            vp->simple = YES;
    } else
        assign(t->name, t->val, t->op);
    overriding = NO;
    substneed  = NO; // a += that waits on $(shell) adds nothing
    if (t->exp)
        vp->exported = 1;
    vp->used = sv->used;
}

// p's own macros, after those of the patterns it matches, in the order read.
static void pushvars(struct nameblock *p)
{
    struct patvar *pv;
    struct tvar *t;
    char *stem;

    for (pv = firstpatvar; pv; pv = pv->next)
        if ((stem = patstem(pv->pat, p->namep)) != NULL) {
            free(stem);
            apply1(pv->tv);
        }
    for (t = p->tvars; t; t = t->next)
        apply1(t);
}

static void popvars(struct saved *mark)
{
    struct saved *sv;

    while ((sv = savedvars) != mark) {
        savedvars        = sv->next;
        sv->vp->varval   = sv->val;
        sv->vp->simple   = sv->simple;
        sv->vp->used     = sv->used;
        sv->vp->fromenv  = sv->fromenv;
        sv->vp->origin   = sv->origin;
        sv->vp->exported = sv->exported;
        sv->vp->noreset  = sv->noreset;
        free(sv);
    }
}

static Task<int> doname1(struct nameblock *p, int reclevel, TIMETYPE *tval);

// A target's own macros are in effect while it is made, and so for what it
// needs made too, as GNU has them.
Task<int> doname(struct nameblock *p, int reclevel, TIMETYPE *tval)
{
    struct saved *mark = savedvars;
    int r;

    if (p == 0 || p->done > 0 || (p->tvars == NULL && firstpatvar == NULL))
        co_return co_await doname1(p, reclevel, tval);
    pushvars(p);
    r = co_await doname1(p, reclevel, tval);
    popvars(mark);
    co_return r;
}

// GNU's -B: every target with commands is out of date.
int alwaysmake = NO;

// Set while remakemfs() brings the makefiles up to date: no word about one
// with nothing to do or no rule.
int remaking;

// The time of the target whose prerequisite is being made, for one that is
// intermediate.
static TIMETYPE parenttime;

// A prerequisite that is an intermediate passed over, made after all.  *pendp
// under -j while it is.
static Task<int> unskip(struct depblock *q, int reclevel, int *pendp)
{
    TIMETYPE t;
    int e;

    if (q->depname == 0 || !q->depname->skipped)
        co_return 0;
    q->depname->skipped = NO;
    q->depname->done    = 0;
    parenttime          = 0; // no target newer to pass it over for
    e                   = co_await doname(q->depname, reclevel + 1, &t);
    if (q->depname->done == 0 || q->depname->done == 4)
        *pendp = YES;
    co_return e;
}

static Task<int> doname1(struct nameblock *p, int reclevel, TIMETYPE *tval)
{
    TIMETYPE myparent = parenttime;
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
    int phony, pend = NO, remake;

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

    // GNU's -o: there, as old as can be, and nothing it needs looked at.
    if (p->oldfile) {
        p->done    = 2;
        p->modtime = 1;
        *tval      = 1;
        co_return 0;
    }

    errstat = 0;
    tdep    = 0;
    implcom  = 0;
    explcom  = 0;
    explstem = NULL;
    // A .PHONY target is no file: never looked for, and always remade.
    phony   = isnamed(".PHONY", p->namep);
    ptime   = phony ? 0 : co_await exists(p->namep);
    // Not here: through VPATH and vpath, as GNU looks.
    if (ptime == 0 && !phony && !stopped && havevpath())
        ptime = co_await vfind(p->namep, &p->path);
    if (p->newfile && !stopped) // GNU's -W: as if just changed
        ptime = co_await prestime();
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

    // Under -j, a :: target's prerequisites are all made before any of its
    // rules is run, since a rule run is not run again.
    if (jobmode && p->septype == SOMEDEPS) {
        for (lp = p->linep; lp; lp = lp->nxtlineblock)
            for (q = lp->depp; q && !stopped; q = q->nxtdepblock)
                if (q->depname && q->depname->done == 0 && unequal(q->depname->namep, ".WAIT")) {
                    parenttime = ptime;
                    errstat += co_await doname(q->depname, reclevel + 1, &td1);
                }
        for (lp = p->linep; lp; lp = lp->nxtlineblock)
            for (q = lp->depp; q; q = q->nxtdepblock)
                if (q->depname && (q->depname->done == 0 || q->depname->done == 4) &&
                    unequal(q->depname->namep, ".WAIT"))
                    pend = YES;
        if (pend || stopped) {
            p->done = 0;
            *tval   = 0;
            co_return stopped ? 1 : errstat;
        }
    }

    // make sure all dependents are up to date
    for (lp = p->linep; lp; lp = lp->nxtlineblock) {
        td       = 0;
        lineq    = NULL;
        lineplus = NULL;
        for (q = lp->depp; q; q = q->nxtdepblock) {
            // POSIX's .WAIT: under -j, nothing after it is begun while
            // anything before it is being made.  It is no prerequisite.
            if (q->depname && !unequal(q->depname->namep, ".WAIT")) {
                if (pend)
                    break;
                continue;
            }
            // A name already done, or one expand() emptied, answers without
            // waiting: a co_await is a call, and costs native stack until
            // something suspends.  README.md, "Output".
            if (q->depname == 0)
                td1 = 0;
            else if (q->depname->done == 4) { // running, under -j
                pend = YES;
                continue;
            } else if (q->depname->done > 0) {
                if (dbgflag)
                    mprintf("doname(%s,%d)\n", q->depname->namep, reclevel + 1);
                td1 = q->depname->modtime;
                errstat += q->depname->done == 3;
            } else {
                parenttime = ptime;
                errstat += co_await doname(q->depname, reclevel + 1, &td1);
            }
            if (stopped)
                co_return 1;
            if (q->depname == 0)
                continue; // v7 went on to read q->depname->namep
            if (q->depname->done == 0 || q->depname->done == 4) { // -j: not yet
                pend = YES;
                continue;
            }
            if (q->orderonly) {
                ooq = appendu(ooq, dname(q->depname));
                continue;
            }
            if (dbgflag)
                mprintf("TIME(%s)=%lld\n", q->depname->namep, td1);
            if (td1 > td)
                td = td1;
            if (ptime < td1)
                qchain = appendu(qchain, dname(q->depname));
            allq     = appendu(allq, dname(q->depname));
            lineq    = appendu(lineq, dname(q->depname));
            plusq    = appendq(plusq, dname(q->depname));
            lineplus = appendq(lineplus, dname(q->depname));
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
    if (!phony && explcom == 0 && p->septype != SOMEDEPS && co_await implicit(p, &im))
        implcom = im.com; // a pattern rule may have no prerequisites
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
            if (q->depname->done == 4) {
                pend = YES;
                continue;
            } else if (q->depname->done > 0) {
                td = q->depname->modtime;
                errstat += q->depname->done == 3;
            } else {
                parenttime = ptime;
                errstat += co_await doname(q->depname, reclevel + 1, &td);
            }
            if (stopped)
                co_return 1;
            if (q->depname->done == 0 || q->depname->done == 4) {
                pend = YES;
                continue;
            }
            if (q->orderonly) {
                ooq = appendu(ooq, dname(q->depname));
                continue;
            }
            if (ptime < td)
                qchain = appendu(qchain, dname(q->depname));
            if (dbgflag)
                mprintf("TIME(%s)=%lld\n", q->depname->namep, td);
            if (td > tdep)
                tdep = td;
            impq    = appendu(impq, dname(q->depname));
            impplus = appendq(impplus, dname(q->depname));
        }
        setvar("<", impq ? impq->datap : (char *)"");
        // the inferred prerequisites first, as GNU has them
        allq  = joinu(impq, allq);
        plusq = joinq(impplus, plusq);
    }
    if (im.stem)
        setvar("*", im.stem);

    // Under -j, something it needs is being made: it is looked at again on a
    // later walk.
    if (pend) {
        p->done = 0;
        *tval   = 0;
        co_return errstat;
    }

    // A missing intermediate is not made for a target newer than everything
    // it is made from, as GNU has it.
    if (p->intermediate && ptime == 0 && errstat == 0 && myparent > 0 && tdep > 0 &&
        myparent >= tdep) {
        p->skipped = YES;
        p->done    = 2;
        p->modtime = tdep;
        *tval      = tdep;
        co_return 0;
    }

    remake = errstat == 0 && (ptime < tdep || (ptime == 0 && tdep == 0) ||
                              (alwaysmake && !remaking && !p->newfile && (explcom || implcom)));

    // An intermediate passed over is wanted after all, its target being
    // remade for some other reason: made now, as GNU does.
    if (remake) {
        char *lt = varptr("<")->varval, *st = varptr("*")->varval;

        for (lp = p->linep; lp; lp = lp->nxtlineblock)
            for (q = lp->depp; q; q = q->nxtdepblock)
                errstat += co_await unskip(q, reclevel, &pend);
        for (q = im.deps; q; q = q->nxtdepblock)
            errstat += co_await unskip(q, reclevel, &pend);
        if (stopped)
            co_return 1;
        setvar("<", lt); // as this target has them, not the intermediate
        setvar("*", st);
        if (pend) {
            p->done = 0;
            *tval   = 0;
            co_return errstat;
        }
        remake = errstat == 0;
    }

    if (remake) {
        // a target found elsewhere is made here, under its own name, as GNU
        // does without GPATH
        free(p->path);
        p->path = NULL;
        // Under -j, the commands are a job, begun when there is a slot.
        if (jobmode && (explcom || implcom)) {
            if (njobs >= jobslots || jobfail) {
                p->done = 0;
                *tval   = 0;
                co_return 0;
            }
            setat(p->namep);
            setlists(alwaysmake ? allq : qchain, allq, plusq, ooq);
            p->made = YES;
            co_await startjob(p, explcom ? explcom : implcom, ptime);
            setvar("@", (char *)NULL);
            *tval = p->modtime;
            co_return p->done == 3;
        }
        if (alwaysmake && !remaking) // -B: every prerequisite is newer
            qchain = allq;
        lists[0] = qchain, lists[1] = allq, lists[2] = plusq, lists[3] = ooq;
        ptime    = co_await makeit(p, tdep, ptime, explcom, implcom, lists, &errstat);
    }

    else if (errstat != 0 && reclevel == 0 && !remaking)
        mprintf("`%s' not remade because of errors\n", p->namep);

    else if (!questflag && reclevel == 0 && didwork == NO && !remaking)
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

// The lines of coms as one script, in coms[0].
static void oneshell(char **coms)
{
    char *script, *d, *s;
    size_t n = 1;
    int i, first = YES;

    for (i = 0; coms[i]; ++i)
        n += strlen(coms[i]) + 1;
    d = script = (char *)ckalloc(n);
    for (i = 0; coms[i]; ++i) {
        for (s = coms[i]; *s; first = NO) {
            if (!first) {
                *d++ = '\n';
                while (*s == '@' || *s == '-' || *s == '+' || *s == ' ' || *s == '\t')
                    ++s;
            }
            while (*s && *s != '\n')
                *d++ = *s++;
            if (*s)
                ++s;
        }
        free(coms[i]);
        coms[i] = NULL;
    }
    *d      = '\0';
    coms[0] = script;
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
static Task<void> delonerr(char *at, int member)
{
    TIMETYPE t;

    if (at == NULL || !srchname(".DELETE_ON_ERROR") || isprecious(at) || isnamed(".PHONY", at))
        co_return;
    if (member) // a library is not removed for a member
        co_return;
    if ((t = co_await exists(at)) > 0 && t != attime) {
        diag("***  %s removed.\n", at);
        co_await b_unlink(at);
    }
}

// A rule's commands, expanded, and which is the next to run.
struct comstate {
    char **coms;
    int *plus0; // each names $(MAKE)
    int i;      // the command being run
    char *next; // the rest of its lines, or NULL
    int one;    // .ONESHELL
    int sil;
    char *at;
};

// The commands for $@, every one expanded before the first is run, as GNU
// does, and each as long as it is: v7's were OUTMAX bytes.  A $(shell) or
// $(wildcard) is performed and the line expanded again, and an $(eval) is
// read before the next line is expanded.  NO if make stopped.
static Task<int> prepcoms(struct shblock *q, struct comstate *cs)
{
    struct shblock *q1;
    int i, n;

    cs->at  = varptr("@")->varval;
    cs->sil = silflag || isnamed(".SILENT", cs->at);
    for (n = 0, q1 = q; q1; q1 = q1->nxtshblock)
        ++n;
    cs->coms  = (char **)ckalloc((n + 1) * sizeof(char *));
    cs->plus0 = (int *)ckalloc((n + 1) * sizeof(int));
    for (i = 0, q1 = q; q1 && !stopped; q1 = q1->nxtshblock, ++i) {
        while ((cs->coms[i] = substa(q1->shbp)), substneed && !stopped) {
            free(cs->coms[i]);
            co_await perform();
        }
        // A + line, or one naming $(MAKE), runs under -n too: that is how
        // a recursive make passes -n down rather than being skipped.
        cs->plus0[i] = strstr(q1->shbp, "$(MAKE)") || strstr(q1->shbp, "${MAKE}");
        if (evalpend && !stopped)
            co_await evalnow();
    }
    // GNU's .ONESHELL: the lines are one script, its first line's @, - and +
    // for the whole, and the others' taken off.
    if ((cs->one = srchname(".ONESHELL") != NULL) && cs->coms[0])
        oneshell(cs->coms);
    cs->i    = 0;
    cs->next = cs->coms[0];
    co_return !stopped;
}

// The next command to run, said as it is run; NULL after the last.  -n says
// them all and runs only the + ones; -t and -q run none but the + ones.
static char *nextcom(struct comstate *cs, int *ignp)
{
    char *line, *s, *e;
    int nopr, plus;

    for (;;) {
        if (cs->next == NULL) {
            if (cs->coms[cs->i] == NULL || cs->coms[++cs->i] == NULL)
                return NULL;
            cs->next = cs->coms[cs->i];
        }
        // A value with lines in it, as a define has, is a command per line,
        // each with its own prefixes.  GNU's canned recipes.
        line = cs->next;
        for (e = line; *e && (cs->one || *e != '\n' || (e > line && e[-1] == '\\')); ++e)
            ;
        if (*e) {
            *e++     = '\0';
            cs->next = e;
        } else
            cs->next = NULL;

        *ignp = ignerr || isnamed(".IGNORE", cs->at);
        nopr  = NO;
        plus  = cs->plus0[cs->i];
        for (s = line; *s == '-' || *s == '@' || *s == '+' || *s == ' ' || *s == '\t'; ++s)
            if (*s == '-')
                *ignp = YES;
            else if (*s == '@')
                nopr = YES;
            else if (*s == '+')
                plus = YES;

        if (s[0] == '\0' || ((touchflag || questflag) && !plus))
            continue;
        // -n writes what would run, silent or not, as POSIX and GNU have it.
        if (noexflag || (!cs->sil && !nopr))
            mprintf("%s%s\n", (noexflag ? "" : prompt), s);
        if (noexflag && !plus)
            continue;
        return s;
    }
}

// The commands, each printed and run.  -n prints and runs none, so nothing
// waits: docom1() is entered only when a command is to be run.
Task<int> docom(struct shblock *q)
{
    struct comstate cs;
    char *s;
    int ign, r;

    ++ndocoms;
    if (!co_await prepcoms(q, &cs)) {
        freecoms(cs.coms, cs.plus0);
        co_return YES;
    }
    while ((s = nextcom(&cs, &ign)) != NULL) {
        shellonly = cs.one;
        r         = co_await docom1(s, ign);
        shellonly = NO;
        forget(); // the command may have changed what $(shell) would say
        if (r && !ign) {
            freecoms(cs.coms, cs.plus0);
            co_await delonerr(cs.at, *varptr("%")->varval != '\0');
            if (keepgoing)
                co_return YES;
            fatal((char *)NULL);
            co_return YES;
        }
        if (stopped) {
            freecoms(cs.coms, cs.plus0);
            co_return YES;
        }
    }
    freecoms(cs.coms, cs.plus0);

    if (touchflag && !questflag) {
        if (!cs.sil)
            mprintf("touch(%s)\n", cs.at);
        if (!noexflag)
            co_await touch(YES, cs.at);
    }
    co_return NO;
}

// A command's status, said as v7's docom1() said it.
static void saystatus(int status, int nohalt)
{
    if (status >> 8)
        mprintf("*** Error code %d", status >> 8);
    else
        mprintf("*** Termination code %d", status);

    if (nohalt)
        mprintf(" (ignored)\n");
    else
        mprintf("\n");
}

// What is left of v7's docom1() once the printing has moved up into docom():
// the command run, and its status said.
static Task<int> docom1(char *comstring, int nohalt)
{
    int status;

    if ((status = co_await dosys(comstring, nohalt)) && !stopped)
        saystatus(status, nohalt);
    co_return status;
}

// -j: what runs at once.  Plan.md, section 3.3.
int jobslots = 1;
int jobmode; // more than one slot, and nothing that runs one at a time
int jobfail; // a command failed without -k: nothing more is started

// A target whose commands are running, one at a time, beside the others'.
struct job {
    struct job *next;
    struct nameblock *p;
    struct comstate cs;
    u32 pid; // the command running, or 0
    int ign; // its failure is ignored
    int failed;
    int member;
    int phony;
    TIMETYPE attime;
    i32 rfd[2];     // -O: the read ends of the command's stdout and stderr, or -1
    char *buf[2];   // what they and make have said, held
    size_t len[2];
};
static struct job *jobs;

// GNU's -O: 0 for none, 'l' for each command's output whole, 't' for each
// target's.
int outsync;

// What a job has held, written: stdout's, then stderr's.
static Task<void> writejob(struct job *j)
{
    int k;

    takeheld(j->buf, j->len);
    for (k = 0; k < 2; ++k)
        if (j->len[k]) {
            co_await write_all(k ? SYS_STDERR : SYS_STDOUT, Str(j->buf[k], j->len[k]));
            j->len[k] = 0;
        }
}
int njobs;

// A job done with: its target's time read, and the target done.
static Task<void> finishjob(struct job *j)
{
    struct job **jp;
    TIMETYPE t = 0;

    if (outsync) // the target's output, whole
        co_await writejob(j);
    for (int k = 0; k < 2; ++k)
        free(j->buf[k]);
    if (!j->phony)
        t = co_await exists(j->p->namep);
    if (t == 0)
        t = co_await prestime();
    j->p->modtime = t;
    j->p->done    = j->failed ? 3 : 2;
    for (jp = &jobs; *jp && *jp != j; jp = &(*jp)->next)
        ;
    if (*jp)
        *jp = j->next;
    --njobs;
    freecoms(j->cs.coms, j->cs.plus0);
    free(j->cs.at);
    free(j);
}

// A command of j ended with status: YES if that is the end of the job.
static Task<int> jobstatus(struct job *j, int status, int ign)
{
    forget(); // the command may have changed what $(shell) would say
    if (status == 0 || stopped)
        co_return stopped;
    saystatus(status, ign);
    if (ign)
        co_return NO;
    attime = j->attime;
    co_await delonerr(j->cs.at, j->member);
    j->failed = YES;
    if (!keepgoing && !jobfail) {
        jobfail = YES;
        if (njobs > 1)
            mprintf("*** Waiting for unfinished jobs\n");
    }
    co_return YES;
}

// Under -O, a command started with its stdout and stderr on two pipes of the
// job's -- two, as a descriptor given to a child is moved into it.  The line
// said for it is the job's too.
static Task<int> startsync(struct job *j, char *s, int ign)
{
    Result<Piped> po = co_await make_pipe(), pe = Err(Error::Io);
    ChildIo io;
    int st;

    takeheld(j->buf, j->len);
    if (po.is_ok())
        pe = co_await make_pipe();
    if (po.is_err() || pe.is_err()) {
        if (po.is_ok()) {
            co_await close_fd(po.value().r);
            co_await close_fd(po.value().w);
        }
        fatal("Cannot make a pipe");
        co_return 1 << 8;
    }
    io.out  = (u32)po.value().w;
    io.err  = (u32)pe.value().w;
    spawnio = &io;
    st      = co_await startsys(s, ign, &j->pid);
    spawnio = NULL;
    if (st == 0 && j->pid != 0) {
        j->rfd[0] = po.value().r;
        j->rfd[1] = pe.value().r;
        co_return 0;
    }
    co_await close_fd(po.value().r);
    co_await close_fd(pe.value().r);
    co_await close_fd(po.value().w);
    co_await close_fd(pe.value().w);
    co_return st;
}

// The job's next command started, or the job finished.
static Task<void> advance(struct job *j)
{
    char *s;
    int ign, st;

    for (;;) {
        if (jobfail || stopped) {
            j->failed = YES; // what is left of it is not run
            break;
        }
        if ((s = nextcom(&j->cs, &ign)) == NULL)
            break;
        shellonly = j->cs.one;
        if (outsync)
            st = co_await startsync(j, s, ign);
        else
            st = co_await startsys(s, ign, &j->pid);
        shellonly = NO;
        if (st == 0 && j->pid != 0) {
            j->ign = ign;
            co_return;
        }
        if (co_await jobstatus(j, st, ign)) // it could not be started
            break;
    }
    co_await finishjob(j);
}

// p's commands as a job, the first started, and p running: done is 4 until it
// ends.  $@ and the rest are set.
static Task<void> startjob(struct nameblock *p, struct shblock *com, TIMETYPE ptime0)
{
    struct job *j = ALLOC(job), **jp;
    char *m       = varptr("%")->varval;
    int ok;

    ++ndocoms;
    j->p      = p;
    j->attime = ptime0;
    j->phony  = isnamed(".PHONY", p->namep);
    j->member = m != NULL && *m != '\0';
    j->rfd[0] = j->rfd[1] = -1;
    for (jp = &jobs; *jp; jp = &(*jp)->next) // in the order begun
        ;
    *jp = j;
    ++njobs;
    p->done  = 4;
    ok       = co_await prepcoms(com, &j->cs);
    j->cs.at = copys(j->cs.at ? j->cs.at : "");
    if (!ok) {
        j->failed = YES;
        co_await finishjob(j);
    } else
        co_await advance(j);
}

// ^C under -j: every command sent it, each reaped, and each target removed.
static Task<void> killjobs(void)
{
    struct job *j;
    Result<Exited> r = Err(Error::Io);
    int left, intr = 0;

    for (j = jobs; j; j = j->next)
        if (j->pid)
            co_await kill_child(j->pid, SIG_INT);
    for (;;) {
        for (left = 0, j = jobs; j; j = j->next)
            left += j->pid != 0;
        if (left == 0)
            break;
        r = co_await wait_child();
        if (r.is_err()) {
            if (r.error() == Error::Intr && sig_take(SIG_INT) && !intr++) {
                for (j = jobs; j; j = j->next)
                    if (j->pid)
                        co_await kill_child(j->pid, SIG_KILL);
                continue;
            }
            break;
        }
        for (j = jobs; j; j = j->next)
            if (j->pid == r.value().pid)
                j->pid = 0;
    }
    while ((j = jobs) != NULL) {
        for (int k = 0; k < 2; ++k)
            if (j->rfd[k] >= 0)
                co_await close_fd(j->rfd[k]);
        if (outsync)
            co_await writejob(j);
        co_await rmtarget(j->cs.at, j->member);
        jobs = j->next;
        free(j->buf[0]);
        free(j->buf[1]);
        freecoms(j->cs.coms, j->cs.plus0);
        free(j->cs.at);
        free(j);
    }
    njobs = 0;
    diag("\n");
    stop(2);
}

// A command of j has ended, with status: the job goes on or is done.
static Task<void> ended(struct job *j, int status)
{
    j->pid = 0;
    if (outsync == 'l') // each command's output, whole
        co_await writejob(j);
    if (co_await jobstatus(j, status, j->ign))
        co_await finishjob(j);
    else
        co_await advance(j);
}

// -O's reap: what the running commands write is held, and a command whose
// pipes are both at their end is reaped by pid, which hardly parks.
static Task<void> reapsync(void)
{
    PollFd fds[SYS_POLL_MAX];
    struct job *who[SYS_POLL_MAX], *j;
    int side[SYS_POLL_MAX], n = 0, k;
    Result<usize> pr = Err(Error::Io);
    Result<Exited> r = Err(Error::Io);

    for (j = jobs; j; j = j->next)
        for (k = 0; k < 2; ++k)
            if (j->rfd[k] >= 0 && n < (int)SYS_POLL_MAX) {
                fds[n].fd      = (u32)j->rfd[k];
                fds[n].events  = SYS_POLL_IN;
                fds[n].revents = 0;
                who[n]         = j;
                side[n++]      = k;
            }
    if (n > 0) {
        pr = co_await poll_fds(Span<PollFd>(fds, n));
        if (pr.is_err()) {
            if (pr.error() == Error::Intr && sig_take(SIG_INT))
                co_await killjobs();
            else {
                stop(pr.error() == Error::Cancelled ? 130 : 2);
                jobs  = NULL;
                njobs = 0;
            }
            co_return;
        }
        for (k = 0; k < n; ++k) {
            if (fds[k].revents == 0)
                continue;
            j                = who[k];
            Result<String> c = co_await read_chunk(fds[k].fd);
            if (c.is_ok() && c.value().size() > 0) {
                j->buf[side[k]] = (char *)realloc(j->buf[side[k]],
                                                  j->len[side[k]] + c.value().size() + 1);
                memcpy(j->buf[side[k]] + j->len[side[k]], c.value().data(), c.value().size());
                j->len[side[k]] += c.value().size();
            } else {
                co_await close_fd(fds[k].fd);
                j->rfd[side[k]] = -1;
            }
        }
    }
    // A command with both pipes at their end has ended, or is about to.
    for (;;) {
        for (j = jobs; j && !(j->pid && j->rfd[0] < 0 && j->rfd[1] < 0); j = j->next)
            ;
        if (j == NULL)
            break;
        r = co_await wait_child(j->pid);
        if (r.is_err()) {
            if (r.error() == Error::Intr && sig_take(SIG_INT))
                co_await killjobs();
            else
                stop(2);
            co_return;
        }
        co_await ended(j, (r.value().status & 0xff) << 8);
    }
}

// One command of a job ends, and the job goes on or is done; under ^C, every
// one of them ends.
Task<void> reap(void)
{
    if (outsync) {
        co_await reapsync();
        co_return;
    }
    Result<Exited> r = co_await wait_child();
    struct job *j;

    if (r.is_err()) {
        if (r.error() == Error::Intr && sig_take(SIG_INT)) {
            co_await killjobs();
            co_return;
        }
        if (r.error() == Error::Cancelled)
            stop(130);
        else
            fatal("bad wait code");
        jobs  = NULL; // nothing more will be heard of them
        njobs = 0;
        co_return;
    }
    for (j = jobs; j && j->pid != r.value().pid; j = j->next)
        ;
    if (j == NULL)
        co_return;
    co_await ended(j, (r.value().status & 0xff) << 8);
    co_await flushout();
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
