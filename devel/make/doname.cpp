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
            if (!ok && havevpath()) {
                char *path;

                ok = co_await vfind(name, &path) > 0;
                free(path);
            }
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

static Task<int> doname1(struct nameblock *p, int reclevel, TIMETYPE *tval)
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
    // Not here: through VPATH and vpath, as GNU looks.
    if (ptime == 0 && !phony && !stopped && havevpath())
        ptime = co_await vfind(p->namep, &p->path);
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
            if (q->depname->done > 0) {
                td = q->depname->modtime;
                errstat += q->depname->done == 3;
            } else
                errstat += co_await doname(q->depname, reclevel + 1, &td);
            if (stopped)
                co_return 1;
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

    if (errstat == 0 && (ptime < tdep || (ptime == 0 && tdep == 0))) {
        // a target found elsewhere is made here, under its own name, as GNU
        // does without GPATH
        free(p->path);
        p->path  = NULL;
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
    int ign, nopr, sil, plus, *plus0, r, i, n, one;
    struct shblock *q1;

    ++ndocoms;

    at  = varptr("@")->varval;
    sil = silflag || isnamed(".SILENT", at);

    // The commands, every one expanded before the first is run, as GNU does,
    // and each as long as it is: v7's were OUTMAX bytes.  A $(shell) or
    // $(wildcard) is performed and the line expanded again, and an $(eval) is
    // read before the next line is expanded.
    for (n = 0, q1 = q; q1; q1 = q1->nxtshblock)
        ++n;
    coms  = (char **)ckalloc((n + 1) * sizeof(char *));
    plus0 = (int *)ckalloc((n + 1) * sizeof(int));
    for (i = 0, q1 = q; q1 && !stopped; q1 = q1->nxtshblock, ++i) {
        while ((coms[i] = substa(q1->shbp)), substneed && !stopped) {
            free(coms[i]);
            co_await perform();
        }
        // A + line, or one naming $(MAKE), runs under -n too: that is how
        // a recursive make passes -n down rather than being skipped.
        plus0[i] = strstr(q1->shbp, "$(MAKE)") || strstr(q1->shbp, "${MAKE}");
        if (evalpend && !stopped)
            co_await evalnow();
    }
    if (stopped) {
        freecoms(coms, plus0);
        co_return YES;
    }

    // GNU's .ONESHELL: the lines are one script, its first line's @, - and +
    // for the whole, and the others' taken off.
    if ((one = srchname(".ONESHELL") != NULL) && coms[0])
        oneshell(coms);

    // -t and -q run none of the commands but the + ones, as -n does; -t then
    // touches the target.
    for (i = 0; coms[i]; ++i) {
        // A value with lines in it, as a define has, is a command per line,
        // each with its own prefixes.  GNU's canned recipes.
        for (line = coms[i]; line != NULL; line = next) {
            for (next = line;
                 *next && (one || *next != '\n' || (next > line && next[-1] == '\\'));
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

            shellonly = one;
            r         = co_await docom1(s, ign);
            shellonly = NO;
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
