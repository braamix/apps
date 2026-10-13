/* UNIX V7 source code: see /COPYRIGHT or www.tuhs.org for details. */

// The description-file grammar.  v7's was gram.y; there is no yacc in this
// tree's build, so the parser is written out here by hand.  The tokens, the
// lexer and every action are gram.y's.  README.md, "The grammar".
//
//     file     :
//              | file comline
//     comline  : START
//              | MACRODEF
//              | START namelist deplist shellist
//              | error
//     namelist : NAME
//              | namelist NAME
//     deplist  :                       -- "Must be a separator"
//              | dlist
//     dlist    : sepchar
//              | dlist NAME
//     sepchar  : COLON
//              | DOUBLECOLON
//     shellist :
//              | shlist
//     shlist   : SHELLINE
//              | shlist SHELLINE

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"

enum {
    NAME = 257,
    SHELLINE,
    START,
    MACRODEF,
    COLON,
    DOUBLECOLON,
    GREATER,
    INCLUDE,  // include, -include or sinclude: incnames names the files
    SHELLDEF, // a != line, which eqsign() left for runset()
    NEED,     // a line that waits on $(shell) or $(wildcard), to be done again
    PIPE,     // `|', before the order-only prerequisites
};

// What an INCLUDE line named, expanded, and whether a missing file is fine.
static char *incnames;
static int incopt;

// yacc's %union.
static union {
    struct shblock *yshblock;
    struct depblock *ydepblock;
    struct nameblock *ynameblock;
} yylval;

static int retsh(char *q);
static int nextlin(int redo);

struct depblock *pp;
static struct shblock *prevshp;

// The targets of a rule line, as many as there are: v7 stopped at NLEFTS.
static struct nameblock **lefts;
static int leftcap;
struct nameblock *leftp;
static int nlefts;

struct lineblock *lp, *lpp;
static struct depblock *prevdep;
static int sepc;

// Whether two lists of prerequisites name the same, in the same order.
static int samedeps(struct depblock *a, struct depblock *b)
{
    for (; a && b; a = a->nxtdepblock, b = b->nxtdepblock)
        if (a->depname != b->depname || a->orderonly != b->orderonly)
            return NO;
    return a == b;
}

// A pattern rule, GNU's: one with the same target and prerequisites goes, and
// this one takes its place -- unless it has no commands, when it only cancels.
static void patdef(char *target, struct depblock *deps, struct shblock *shell)
{
    struct patrule *pr, **prp;

    for (prp = &firstpatrule; (pr = *prp) != NULL; prp = &pr->nxtpatrule)
        if (!unequal(pr->target, target) && samedeps(pr->depp, deps)) {
            if (shell == 0) {
                *prp = pr->nxtpatrule;
                free(pr);
            } else
                pr->shp = shell;
            return;
        }
    if (shell == 0)
        return;
    pr             = ALLOC(patrule);
    pr->target     = target;
    pr->depp       = deps;
    pr->shp        = shell;
    pr->nxtpatrule = NULL;
    *prp           = pr;
}

// comline : START namelist deplist shellist
static void rule(struct depblock *deps, struct shblock *shell)
{
    int i;

    for (i = 0; i < nlefts && mainname == NULL; ++i)
        if (maincand(lefts[i]->namep))
            mainname = lefts[i];

    while (--nlefts >= 0) {
        leftp = lefts[nlefts];
        if (strchr(leftp->namep, '%')) {
            patdef(leftp->namep, deps, shell);
            continue;
        }
        if (leftp->septype == 0)
            leftp->septype = sepc;
        else if (leftp->septype != sepc)
            diag("Inconsistent rules lines for `%s'\n", leftp->namep);
        else if (sepc == ALLDEPS && *(leftp->namep) != '.' && shell != 0) {
            for (lp = leftp->linep; lp->nxtlineblock != 0; lp = lp->nxtlineblock)
                if (lp->shp)
                    diag("Multiple rules lines for `%s'\n", leftp->namep);
        }

        lp               = ALLOC(lineblock);
        lp->nxtlineblock = NULL;
        lp->depp         = deps;
        lp->shp          = shell;
        lp->stem         = NULL;

        if (!unequal(leftp->namep, ".SUFFIXES") && deps == 0)
            leftp->linep = 0;
        else if (leftp->linep == 0)
            leftp->linep = lp;
        else {
            for (lpp = leftp->linep; lpp->nxtlineblock; lpp = lpp->nxtlineblock)
                ;
            if (sepc == ALLDEPS && leftp->namep[0] == '.')
                lpp->shp = 0;
            lpp->nxtlineblock = lp;
        }
    }
}

// dlist : the names after a separator, those after a `|' order-only.  The
// token after them is returned.
static int dlist(struct depblock **depsp)
{
    int tok, oo = NO;

    prevdep = 0;
    *depsp  = 0;
    while ((tok = yylex()) == NAME || (tok == PIPE && !oo)) {
        if (tok == PIPE) { // GNU's order-only prerequisites follow
            oo = YES;
            continue;
        }
        pp              = ALLOC(depblock);
        pp->nxtdepblock = NULL;
        pp->depname     = yylval.ynameblock;
        pp->orderonly   = oo;
        if (prevdep == 0)
            *depsp = pp;
        else
            prevdep->nxtdepblock = pp;
        prevdep = pp;
    }
    return tok;
}

// GNU's static pattern, targets: %.o: %.c -- a rule for each target, its
// prerequisites the patterns with its stem for the %, and the stem its $*.
static void staticrule(struct depblock *tpat, struct depblock *ppats, struct shblock *shell)
{
    struct nameblock **targets, *np;
    struct depblock *deps, *last, *d, *q;
    char *stem, *name;
    int n, i;

    if (tpat == 0 || tpat->nxtdepblock != 0 || !strchr(tpat->depname->namep, '%')) {
        yyerror("target pattern contains no `%'");
        return;
    }
    n       = nlefts;
    targets = (struct nameblock **)ckalloc(n * sizeof(*targets));
    memcpy(targets, lefts, n * sizeof(*targets));
    for (i = 0; i < n && !stopped; ++i) {
        if ((stem = patstem(tpat->depname->namep, targets[i]->namep)) == NULL) {
            fatal1("target `%s' doesn't match the target pattern", targets[i]->namep);
            break;
        }
        for (deps = last = 0, q = ppats; q; q = q->nxtdepblock) {
            name = patput(q->depname->namep, stem);
            if ((np = srchname(name)) == 0)
                np = makename(name);
            free(name);
            d              = ALLOC(depblock);
            d->depname     = np;
            d->orderonly   = q->orderonly;
            d->nxtdepblock = 0;
            if (last)
                last->nxtdepblock = d;
            else
                deps = d;
            last = d;
        }
        lefts[0] = targets[i];
        nlefts   = 1;
        rule(deps, shell);
        lp->stem = stem;
    }
    free(targets);
}

// 0 at the end of the file, 1 when the description is in error.  Every error
// is yyerror()'s, which stops make, so `error' never recovers: as in v7, the
// first is the last.
//
// A coroutine for `include' alone, which reads a file.  The lexer under it is
// plain: an await per token that did not park would cost native stack per
// token, where a file read parks every time.
Task<int> yyparse(void)
{
    int tok;
    struct depblock *deps, *tpats;
    struct shblock *shell;

    tok = yylex();
    for (;;) {
        if (stopped)
            co_return 1;
        switch (tok) {
        case 0:
            co_return 0;

        case MACRODEF:
            tok = yylex();
            continue;

        case INCLUDE:
            co_await includef(incnames, incopt);
            tok = yylex();
            continue;

        case SHELLDEF:
            co_await runset();
            tok = yylex();
            continue;

        case NEED:
            co_await perform();
            tok = yyrelex();
            continue;

        case START:
            tok = yylex();
            if (tok != NAME)
                continue; // comline : START
            break;

        default:
            yyerror("syntax error");
            co_return 1;
        }

        // namelist
        if (lefts == NULL)
            lefts = (struct nameblock **)ckalloc((leftcap = NLEFTS) * sizeof(*lefts));
        lefts[0] = yylval.ynameblock;
        nlefts   = 1;
        while ((tok = yylex()) == NAME) {
            if (nlefts >= leftcap)
                lefts = (struct nameblock **)growbuf(lefts, &leftcap, sizeof(*lefts));
            lefts[nlefts++] = yylval.ynameblock;
        }
        if (stopped)
            co_return 1;

        // deplist : the empty one is yacc's default reduction, so any token
        // that is not a separator takes it.
        if (tok != COLON && tok != DOUBLECOLON) {
            char junk[10];

            snprintf(junk, sizeof(junk), "%d", yylineno);
            fatal1("Must be a separator on rules line %s", junk);
            co_return 1;
        }
        sepc  = (tok == COLON ? ALLDEPS : SOMEDEPS);
        tok   = dlist(&deps);
        tpats = 0;
        if (tok == COLON && sepc == ALLDEPS) { // a static pattern
            tpats = deps;
            tok   = dlist(&deps);
        }
        if (stopped)
            co_return 1;

        // shellist
        shell = 0;
        while (tok == SHELLINE || tok == NEED) {
            // a conditional among the commands that waits
            if (tok == NEED) {
                co_await perform();
                tok = yyrelex();
                continue;
            }
            if (shell == 0)
                shell = yylval.yshblock;
            else
                prevshp->nxtshblock = yylval.yshblock;
            prevshp = yylval.yshblock;
            tok     = yylex();
        }
        if (stopped)
            co_return 1;

        if (tpats)
            staticrule(tpats, deps, shell);
        else
            rule(deps, shell);
    }
}

char *zznextc; // zero if need another line; otherwise points to next char
int parsing;   // a makefile is being read, so a line number means something
int yylineno;
char *fin;
char *finend;

// The input stack: a file `include' named, above the one that named it, which
// is put back where it was when the included one ends.
struct inbuf {
    struct inbuf *prev;
    char *own; // the included file's buffer
    char *fin, *finend;
    int lineno;
};
static struct inbuf *instack;
static int indepth;

// buf is the file's whole text, ending in a newline, and is the stack's now.
int pushin(char *buf, int len)
{
    struct inbuf *s;

    if (indepth >= MAXINCL) {
        fatal("include nested too deeply");
        free(buf);
        return NO;
    }
    s         = ALLOC(inbuf);
    s->prev   = instack;
    s->own    = buf;
    s->fin    = fin;
    s->finend = finend;
    s->lineno = yylineno;
    instack   = s;
    fin       = buf;
    finend    = buf + len;
    yylineno  = 0;
    ++indepth;
    return YES;
}

// The line as read, a word of it, and the line expanded, each as long as it
// is: v7's were INMAX bytes, and `line too long' stopped make.
static char *yytext;
static int yycap;
static char *word;
static int wcap;
static char *expline;

// getc(fin), from the description file read whole.
static int getfin(void)
{
    struct inbuf *s;

    while (fin >= finend && (s = instack) != NULL) {
        fin      = s->fin;
        finend   = s->finend;
        yylineno = s->lineno;
        instack  = s->prev;
        free(s->own);
        free(s);
        --indepth;
    }
    return fin < finend ? (unsigned char)*fin++ : EOF;
}

// include, -include and sinclude, at the start of a line and followed by a
// blank: what comes after the word, or NULL.
static char *isinclude(char *t, int *optp)
{
    static const char *const words[] = { "include", "-include", "sinclude" };
    size_t n;
    int i;

    while (*t == ' ')
        ++t;
    for (i = 0; i < 3; ++i) {
        n = strlen(words[i]);
        if (!strncmp(t, words[i], n) && (t[n] == ' ' || t[n] == '\t' || t[n] == '\0')) {
            *optp = i > 0;
            return t + n;
        }
    }
    return NULL;
}

// The word w at t, past blanks, followed by a blank or the end: what follows
// it and its blanks, or NULL.
static char *isword(char *t, const char *w)
{
    size_t n = strlen(w);

    while (*t == ' ' || *t == '\t')
        ++t;
    if (strncmp(t, w, n) || (t[n] != ' ' && t[n] != '\t' && t[n] != '\0'))
        return NULL;
    for (t += n; *t == ' ' || *t == '\t'; ++t)
        ;
    return t;
}

// The conditionals open, innermost last: whether lines are read in each, and
// whether a branch of it was taken -- or none can be, as its outer one is not.
struct cond {
    int ok;
    int seen;
};
static struct cond *conds;
static int ncond, condcap;

// ifeq's two strings, (a,b) or "a" "b" or 'a' 'b', each expanded: 1 if they are
// the same, 0 if not, -1 if t is neither.  As GNU has it, the blanks before
// a comma are not the first string's and those after it not the second's.
static int condeq(char *t)
{
    char *a1, *e1, *a2, *e2, *x1, *x2;
    int nest, r;

    if (*t == '(') {
        for (a1 = e1 = t + 1, nest = 0; *e1 && (*e1 != ',' || nest > 0); ++e1)
            if (*e1 == '(' || *e1 == '{')
                ++nest;
            else if (*e1 == ')' || *e1 == '}')
                --nest;
        if (*e1 != ',')
            return -1;
        for (a2 = e1 + 1; *a2 == ' ' || *a2 == '\t'; ++a2)
            ;
        for (e2 = a2, nest = 0; *e2 && (*e2 != ')' || nest > 0); ++e2)
            if (*e2 == '(' || *e2 == '{')
                ++nest;
            else if (*e2 == ')' || *e2 == '}')
                --nest;
        if (*e2 != ')')
            return -1;
        t = e2 + 1;
        while (e1 > a1 && (e1[-1] == ' ' || e1[-1] == '\t'))
            --e1;
    } else if (*t == '"' || *t == '\'') {
        a1 = t + 1;
        if ((e1 = strchr(a1, *t)) == NULL)
            return -1;
        for (t = e1 + 1; *t == ' ' || *t == '\t'; ++t)
            ;
        if (*t != '"' && *t != '\'')
            return -1;
        a2 = t + 1;
        if ((e2 = strchr(a2, *t)) == NULL)
            return -1;
        t = e2 + 1;
    } else
        return -1;
    while (*t == ' ' || *t == '\t')
        ++t;
    if (*t != '\0')
        return -1;
    *e1 = *e2 = '\0';
    x1      = substa(a1);
    x2      = substa(a2);
    r       = !unequal(x1, x2);
    free(x1);
    free(x2);
    return r;
}

// ifdef's test: whether the macro t names has a value that is not empty.  The
// name is expanded, the value is not.
static int conddef(char *t)
{
    char *n, *e;
    struct varblock *vp;
    int r;

    n = substa(t);
    for (e = n + strlen(n); e > n && (e[-1] == ' ' || e[-1] == '\t'); --e)
        ;
    *e = '\0';
    r  = (vp = findvar(n)) != NULL && vp->varval != NULL && *vp->varval != '\0';
    free(n);
    return r;
}

// The test after ifeq, ifneq, ifdef or ifndef: 1 or 0, or -1 if it is none.
// *isp says whether t was one; with eval NO that is all it does.
static int condtest(char *t, int *isp, int eval)
{
    static const char *const words[] = { "ifeq", "ifneq", "ifdef", "ifndef" };
    char *a = NULL;
    int i, r;

    for (i = 0; i < 4 && (a = isword(t, words[i])) == NULL; ++i)
        ;
    *isp = i < 4;
    if (i == 4 || !eval)
        return 0;
    if (i < 2)
        return (r = condeq(a)) < 0 ? r : r == (i == 0);
    return conddef(a) == (i == 2);
}

// A conditional line, taken: YES if t was one.  GNU's, with its else-if.  In
// a branch not taken nothing is expanded, and only the nesting counts.
static int condline(char *t)
{
    char *a;
    int is, r, outer;
    struct cond *top;

    outer = ncond == 0 || conds[ncond - 1].ok;
    r     = condtest(t, &is, outer);
    if (substneed)
        return YES;
    if (is) {
        if (outer && r < 0) {
            yyerror("invalid syntax in conditional");
            return YES;
        }
        if (ncond >= condcap)
            conds = (struct cond *)growbuf(conds, &condcap, sizeof(*conds));
        conds[ncond].ok   = outer && r;
        conds[ncond].seen = !outer || r;
        ++ncond;
        return YES;
    }
    if ((a = isword(t, "else")) != NULL) {
        if (ncond == 0) {
            yyerror("extraneous `else'");
            return YES;
        }
        top = &conds[ncond - 1];
        if (*a == '\0') {
            top->ok   = !top->seen;
            top->seen = YES;
            return YES;
        }
        r = condtest(a, &is, !top->seen);
        if (substneed)
            return YES;
        if (!is || r < 0) {
            yyerror("invalid syntax in conditional");
            return YES;
        }
        top->ok = top->seen ? NO : r;
        top->seen |= r;
        return YES;
    }
    if ((a = isword(t, "endif")) != NULL) {
        if (ncond == 0)
            yyerror("extraneous `endif'");
        else
            --ncond;
        return YES;
    }
    return NO;
}

// A define being read: its name, its operator, its body so far, and how deep
// in defines the line is.  Nothing in the body is looked at but define and
// endef.
static int indef;
static char *defname, *defbody;
static int defop, deflen, defcap, defover, defexport, deflines;

static void startdef(char *t, int over, int exp)
{
    char *e, *o;

    for (e = t; *e && *e != ' ' && *e != '\t' && *e != '='; ++e)
        ;
    // the operator, if any: = := ::= :::= += ?= !=
    for (o = e; *o == ' ' || *o == '\t'; ++o)
        ;
    defop = '=';
    if (!strncmp(o, ":::=", 4))
        defop = 'T';
    else if (!strncmp(o, "::=", 3) || !strncmp(o, ":=", 2))
        defop = ':';
    else if (o[0] && strchr("+?!", o[0]) && o[1] == '=')
        defop = o[0];
    *e = '\0';
    free(defname);
    defname = substa(t);
    if (substneed)
        return;
    deflen    = 0;
    deflines  = 0;
    defover   = over;
    defexport = exp;
    indef     = 1;
    if (defbody == NULL)
        defbody = (char *)growbuf(defbody, &defcap, 1);
    defbody[0] = '\0';
}

// A line of a define's body, or its endef: YES when that was the end, and the
// macro is set.
static int defline(char *t)
{
    char *a;
    size_t n;

    if ((a = isword(t, "endef")) != NULL && (*a == '\0' || *a == '#') && t[0] != '\t') {
        if (--indef == 0) {
            overriding = defover;
            assign(defname, defbody, defop);
            overriding = NO;
            if (substneed) { // the endef again, once it can be done
                indef = 1;
                return YES;
            }
            if (defexport)
                varptr(defname)->exported = 1;
            return YES;
        }
    } else if (isword(t, "define") != NULL && t[0] != '\t')
        ++indef;
    n = strlen(t);
    while (deflen + n + 2 > (size_t)defcap)
        defbody = (char *)growbuf(defbody, &defcap, 1);
    if (deflines++ > 0)
        defbody[deflen++] = '\n';
    memcpy(defbody + deflen, t, n + 1);
    deflen += n;
    return NO;
}

// GNU's undefine: each name in t, expanded, has no value, as if never set.  A
// macro from the command line keeps its value, override or not, as GNU's
// does.
static void undefine(char *t)
{
    char *names, *w;
    struct varblock *vp;

    names = substa(t);
    for (w = names; !substneed && (w = strtok(w, " \t")) != NULL; w = NULL)
        if ((vp = findvar(w)) != NULL && !vp->noreset) {
            vp->varval   = 0;
            vp->simple   = NO;
            vp->exported = 0;
            vp->fromenv  = NO;
        }
    free(names);
}

// Each name in t, expanded, marked to be exported (1) or not (-1).
static void markexp(char *t, int how)
{
    char *names, *w, *e;

    names = substa(t);
    for (w = names; !stopped; w = e) {
        while (*w == ' ' || *w == '\t')
            ++w;
        if (*w == '\0')
            break;
        for (e = w; *e && *e != ' ' && *e != '\t'; ++e)
            ;
        if (*e)
            *e++ = '\0';
        if (!substneed)
            varptr(w)->exported = how;
    }
    free(names);
}

// define, override, export and unexport.  t when the line is none of them,
// NULL when it was taken whole, and another pointer when it was a macro
// definition, as nextlin()'s MACRODEF.
static char *directive(char *t)
{
    char *a, *n;
    int over = NO, how;

    if ((a = isword(t, "vpath")) != NULL && !strchr(a, '=')) {
        vpathdef(a);
        return NULL;
    }
    if ((a = isword(t, "override")) != NULL) {
        over = YES;
        t    = a;
    }
    if ((a = isword(t, "define")) != NULL) {
        startdef(a, over, NO);
        return NULL;
    }
    if ((a = isword(t, "undefine")) != NULL && !strchr(a, '=')) {
        undefine(a);
        return NULL;
    }
    how = 0;
    if ((a = isword(t, "export")) != NULL)
        how = 1;
    else if ((a = isword(t, "unexport")) != NULL)
        how = -1;
    if (how != 0) {
        if (how == 1 && !over && (n = isword(a, "define")) != NULL) {
            startdef(n, NO, YES);
            return NULL;
        }
        if (*a == '\0') {
            exportall = how == 1;
            return NULL;
        }
        overriding = over;
        n          = a;
        if (how == 1 && eqsign(n)) {
            overriding = NO;
            while (*n == ' ')
                ++n;
            if (!substneed)
                varptr(n)->exported = 1;
            return a;
        }
        overriding = NO;
        markexp(a, how);
        return NULL;
    }
    if (over) {
        overriding = YES;
        if (!eqsign(t)) {
            overriding = NO;
            yyerror("override without an assignment");
            return NULL;
        }
        overriding = NO;
        return t;
    }
    return t;
}

// The first of `stops' in t outside $(...) and ${...}, or the end.
static char *outside(char *t, const char *stops)
{
    int nest = 0;

    for (; *t; ++t)
        if (*t == '(' || *t == '{')
            ++nest;
        else if (*t == ')' || *t == '}')
            --nest;
        else if (nest == 0 && strchr(stops, *t))
            break;
    return t;
}

// target...: [override] [export] name op value -- GNU's target-specific and
// pattern-specific macros, kept for doname() to put in effect.  NO if t is
// not one, YES when it was taken, NEED when it waits on $(shell).
static int targetvar(char *t)
{
    char *sep, *rest, *a, *eq, *e, *n, *val, *names, *w, *x;
    struct tvar *tv, **tp;
    struct patvar *pv, **pp;
    struct nameblock *np;
    int op, over = NO, exp = NO;

    sep = outside(t, ":;=");
    if (*sep != ':')
        return NO;
    rest = sep + 1 + (sep[1] == ':');
    eq   = outside(rest, ";=");
    if (*eq != '=')
        return NO;
    a = rest;
    if ((x = isword(a, "override")) != NULL)
        over = YES, a = x;
    if ((x = isword(a, "export")) != NULL)
        exp = YES, a = x;
    e  = eq;
    op = '=';
    if (e > a && (e[-1] == '+' || e[-1] == '?' || e[-1] == ':')) {
        op = *--e;
        while (e > a && e[-1] == ':') // ::= and :::= expand when read, as := does
            --e;
    }
    while (*a == ' ' || *a == '\t')
        ++a;
    while (e > a && (e[-1] == ' ' || e[-1] == '\t'))
        --e;
    for (n = a; n < e; ++n)
        if (*n == ' ' || *n == '\t')
            return NO; // a name is one word
    if (e == a)
        return NO;
    for (val = eq + 1; *val == ' ' || *val == '\t'; ++val)
        ;
    *e   = '\0';
    *sep = '\0';
    names = substa(t);
    x     = op == ':' ? substnow(val) : copys(val);
    if (substneed || stopped) {
        free(names);
        free(x);
        return substneed ? NEED : YES;
    }
    for (w = names; (w = strtok(w, " \t")) != NULL; w = NULL) {
        tv       = ALLOC(tvar);
        tv->name = copys(a);
        tv->val  = copys(x);
        tv->op   = op == ':' ? 'S' : op;
        tv->over = over;
        tv->exp  = exp;
        if (strchr(w, '%')) {
            pv      = ALLOC(patvar);
            pv->pat = copys(w);
            pv->tv  = tv;
            for (pp = &firstpatvar; *pp; pp = &(*pp)->next)
                ;
            *pp = pv;
        } else {
            if ((np = srchname(w)) == 0)
                np = makename(w);
            for (tp = &np->tvars; *tp; tp = &(*tp)->next)
                ;
            *tp = tv;
        }
    }
    free(names);
    free(x);
    return YES;
}

int yylex(void)
{
    char *p;
    char *q;

    if (stopped)
        return 0;

    if (zznextc == 0)
        return nextlin(NO);

    while (isspace((unsigned char)*zznextc))
        ++zznextc;

    if (*zznextc == '\0')
        return nextlin(NO);

    if (*zznextc == ':') {
        if (*++zznextc == ':') {
            ++zznextc;
            return DOUBLECOLON;
        } else
            return COLON;
    }

    if (*zznextc == '>') {
        ++zznextc;
        return GREATER;
    }

    if (*zznextc == '|') {
        ++zznextc;
        return PIPE;
    }

    if (*zznextc == ';')
        return retsh(zznextc);

    p = zznextc;
    while ((int)strlen(p) >= wcap)
        word = (char *)growbuf(word, &wcap, 1);
    q = word;

    while (!(funny[(unsigned char)*p] & TERMINAL))
        *q++ = *p++;

    if (p != zznextc) {
        *q = '\0';
        if ((yylval.ynameblock = srchname(word)) == 0)
            yylval.ynameblock = makename(word);
        if (stopped)
            return 0;
        zznextc = p;
        return NAME;
    }

    else {
        diag("Bad character %c (octal %o), line %d", *zznextc, (unsigned char)*zznextc, yylineno);
        fatal((char *)NULL);
    }
    return 0;
}

static int retsh(char *q)
{
    char *p;
    struct shblock *sp;

    for (p = q + 1; *p == ' ' || *p == '\t'; ++p)
        ;

    sp              = ALLOC(shblock);
    sp->nxtshblock  = NULL;
    sp->shbp        = copys(p); // v7 kept the pointer when reading builtin[]
    yylval.yshblock = sp;
    zznextc         = 0;
    return SHELLINE;
}

// The last line, as it was read, for nextlin() to do again: one that waited
// on $(shell) or $(wildcard) had nothing of it done.  README.md.
static char *linecopy;
static int linecap;

int yyrelex(void)
{
    return nextlin(YES);
}

static int nextlin(int redo)
{
    char *text;
    char c;
    char *p, *t;
    const char *bt;
    char lastch, *lastchp;
    int incom;
    int kc;

    if (redo) {
        zznextc = 0;
        strcpy(yytext, linecopy);
        goto again1;
    }

again:

    incom   = indef > 0; // a define's body is taken as it is
    zznextc = 0;

    if (fin == NULL) {
        // A builtin line is copied rather than parsed in place: eqsign() and the
        // substitution below write into it, and a literal is not writable.
        if ((bt = *linesptr++) == 0)
            return 0;
        ++yylineno;
        while ((int)strlen(bt) >= yycap)
            yytext = (char *)growbuf(yytext, &yycap, 1);
        strcpy(yytext, bt);
        text = yytext;
    }

    else {
        if (yytext == NULL)
            yytext = (char *)growbuf(yytext, &yycap, 1);
        for (p = yytext;; *p++ = kc) {
            if (p - yytext >= yycap - 1) {
                kc     = p - yytext;
                yytext = (char *)growbuf(yytext, &yycap, 1);
                p      = yytext + kc;
            }
            switch (kc = getfin()) {
            case '\t':
                if (p != yytext)
                    break;
                // fall through: a tab in column 1 opens a command line
            case ';':
                incom = YES;
                break;

            case '#':
                if (!incom)
                    kc = '\0';
                break;

            case '\n':
                ++yylineno;
                if (p == yytext || p[-1] != '\\' || indef) {
                    *p = '\0';
                    goto endloop;
                }
                if (incom) {
                    // POSIX: a command keeps its backslash-newline for the
                    // shell, and the next line loses the tab it starts with.
                    if ((kc = getfin()) != '\t' && kc != EOF)
                        --fin;
                    kc = '\n';
                    break;
                }
                // Elsewhere the backslash-newline and the blanks either side
                // of it are one space.  v7 went on past empty lines too.
                for (--p; p > yytext && (p[-1] == ' ' || p[-1] == '\t'); --p)
                    ;
                while ((kc = getfin()) == '\t' || kc == ' ')
                    ;
                if (kc != EOF)
                    --fin;
                kc = ' ';
                break;

            case EOF:
                // A last line with no newline is still a line; v7 dropped it.
                if (p != yytext) {
                    *p = '\0';
                    goto endloop;
                }
                *p = '\0';
                if (indef)
                    yyerror("missing `endef'");
                else if (ncond)
                    yyerror("missing `endif'");
                return 0;
            }
        }
    }

endloop:
    while ((int)strlen(yytext) >= linecap)
        linecopy = (char *)growbuf(linecopy, &linecap, 1);
    strcpy(linecopy, yytext);
again1:
    text = yytext;

    if (indef) {
        if (defline(text))
            return substneed ? NEED : shellpend ? SHELLDEF : MACRODEF;
        goto again;
    }
    if (text[0] != '\t' && condline(text)) {
        if (stopped)
            return 0;
        if (substneed)
            return NEED;
        goto again;
    }
    if (ncond > 0 && !conds[ncond - 1].ok)
        goto again;

    if ((c = text[0]) == '\t')
        return retsh(text);

    p = directive(text);
    if (stopped)
        return 0;
    if (substneed)
        return NEED;
    if (p == NULL)
        goto again;
    if (p != text)
        return shellpend ? SHELLDEF : MACRODEF;

    if (isalpha((unsigned char)c) || isdigit((unsigned char)c) || c == ' ' || c == '.' ||
        c == '_') {
        for (p = text + 1; *p != '\0';)
            if (*p == ':' && !assigncolon(p))
                break;
            else if (*p++ == '=') {
                eqsign(text);
                return substneed ? NEED : shellpend ? SHELLDEF : MACRODEF;
            }
    }

    // include: the rest of the line, expanded, names the files, which
    // yyparse() reads (README.md, "Includes").
    if ((p = isinclude(text, &incopt)) != NULL) {
        free(incnames);
        incnames = substa(p);
        return stopped ? 0 : substneed ? NEED : INCLUDE;
    }

    switch (targetvar(text)) {
    case YES:
        if (stopped)
            return 0;
        goto again;
    case NEED:
        return NEED;
    }

    // substitute for macros on dependency line up to the semicolon if any
    for (t = text; *t != '\0' && *t != ';'; ++t)
        ;

    lastchp = t;
    lastch  = *t;
    *t      = '\0';

    free(expline);
    expline = substa(text);
    if (stopped)
        return 0;
    if (substneed)
        return NEED;

    // and the rest, from the semicolon on, as it was
    if (lastch) {
        *lastchp = lastch;
        t        = (char *)ckalloc(strlen(expline) + strlen(lastchp) + 1);
        concat(expline, lastchp, t);
        free(expline);
        expline = t;
    }

    for (p = zznextc = expline; *p; ++p)
        if (*p != ' ' && *p != '\t')
            return START;
    goto again;
}
