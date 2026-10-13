// GNU make's text functions, $(name args): plain code, each writing its result
// at b as subst() does, so that a full buffer is substa()'s to grow.  Not
// v7's; README.md, "Beyond V7".

#include <stdlib.h>
#include <string.h>

#include "defs.h"

enum {
    F_SUBST = 1,
    F_PATSUBST,
    F_STRIP,
    F_FINDSTRING,
    F_FILTER,
    F_FILTEROUT,
    F_SORT,
    F_WORD,
    F_WORDLIST,
    F_WORDS,
    F_FIRSTWORD,
    F_LASTWORD,
    F_DIR,
    F_NOTDIR,
    F_SUFFIX,
    F_BASENAME,
    F_ADDSUFFIX,
    F_ADDPREFIX,
    F_JOIN,
    F_IF,
    F_OR,
    F_AND,
    F_FOREACH,
};

// Each function, the fewest arguments it takes and the most, the last taking
// any commas past it.
static const struct func {
    const char *name;
    int min, max;
} funcs[] = {
    { "subst", 3, 3 },     { "patsubst", 3, 3 },  { "strip", 1, 1 },     { "findstring", 2, 2 },
    { "filter", 2, 2 },    { "filter-out", 2, 2 }, { "sort", 1, 1 },     { "word", 2, 2 },
    { "wordlist", 3, 3 },  { "words", 1, 1 },     { "firstword", 1, 1 }, { "lastword", 1, 1 },
    { "dir", 1, 1 },       { "notdir", 1, 1 },    { "suffix", 1, 1 },    { "basename", 1, 1 },
    { "addsuffix", 2, 2 }, { "addprefix", 2, 2 }, { "join", 2, 2 },      { "if", 2, 3 },
    { "or", 1, 1000 },     { "and", 1, 1000 },    { "foreach", 3, 3 },
};
#define NFUNCS (int)(sizeof(funcs) / sizeof(funcs[0]))

int isfunc(const char *name)
{
    int i;

    for (i = 0; i < NFUNCS; ++i)
        if (!unequal(name, funcs[i].name))
            return i + 1;
    return 0;
}

static int isblank_(int c)
{
    return c == ' ' || c == '\t' || c == '\n';
}

// The next word at *pp: its start, its length in *lp, or NULL at the end.
static char *nextword(char **pp, size_t *lp)
{
    char *w, *e;

    for (w = *pp; isblank_(*w); ++w)
        ;
    if (*w == '\0')
        return NULL;
    for (e = w; *e && !isblank_(*e); ++e)
        ;
    *lp = e - w;
    *pp = e;
    return w;
}

// A word put after a blank, unless it is the first.
static int putword(char **bp, char *bend, int *firstp, const char *w, size_t n)
{
    if (!*firstp && !putn(bp, bend, " ", 1))
        return NO;
    *firstp = NO;
    return putn(bp, bend, w, n);
}

// Whether word w matches pattern p, which has at most one %: the stem in
// *stemp and *stemlp when it does.
static int pmatch(const char *p, const char *w, size_t wl, const char **stemp, size_t *stemlp)
{
    const char *pct = strchr(p, '%');
    size_t pl       = strlen(p), pre, suf;

    if (pct == NULL)
        return pl == wl && !strncmp(p, w, wl);
    pre = pct - p;
    suf = pl - pre - 1;
    if (wl < pre + suf || strncmp(w, p, pre) || strncmp(w + wl - suf, pct + 1, suf))
        return NO;
    if (stemp) {
        *stemp  = w + pre;
        *stemlp = wl - pre - suf;
    }
    return YES;
}

// A word through a pattern and its replacement, as patsubst has it.
static int patword(const char *pat, const char *rep, const char *w, size_t wl, char **bp,
                   char *bend, int *firstp)
{
    const char *stem, *rpct;
    size_t sl;

    if (!pmatch(pat, w, wl, &stem, &sl))
        return putword(bp, bend, firstp, w, wl);
    if (strchr(pat, '%') == NULL || (rpct = strchr(rep, '%')) == NULL)
        return putword(bp, bend, firstp, rep, strlen(rep));
    return putword(bp, bend, firstp, rep, rpct - rep) && putn(bp, bend, stem, sl) &&
           putn(bp, bend, rpct + 1, strlen(rpct + 1));
}

static int cmpstr(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

// A number, for word and wordlist: -1 if it is none.
static long num(const char *s)
{
    long n = 0;

    while (isblank_(*s))
        ++s;
    if (*s < '0' || *s > '9')
        return -1;
    for (; *s >= '0' && *s <= '9'; ++s)
        n = n * 10 + (*s - '0');
    while (isblank_(*s))
        ++s;
    return *s ? -1 : n;
}

// s with its blanks either side gone, in place.
static char *strip(char *s)
{
    char *e;

    while (isblank_(*s))
        ++s;
    for (e = s + strlen(s); e > s && isblank_(e[-1]); --e)
        ;
    *e = '\0';
    return s;
}

// The function fn over args, as written: they are expanded here, and only
// those it uses.
char *callfunc(int fn, char *args, char *b, char *bend)
{
    const struct func *f = &funcs[fn - 1];
    char **av, *x[3] = { 0, 0, 0 }, *p, *w, *s, *t, **v;
    int ac, nest, first = YES, i, n;
    size_t wl, l;
    long k, m;
    struct varblock *vp;

    // The arguments, split at the commas outside parentheses.
    av = (char **)ckalloc((strlen(args) + 2) * sizeof(char *));
    while (isblank_(*args))
        ++args;
    av[0] = args;
    for (ac = 1, p = args, nest = 0; *p; ++p)
        if (*p == '(' || *p == '{')
            ++nest;
        else if (*p == ')' || *p == '}')
            --nest;
        else if (*p == ',' && nest == 0 && ac < f->max) {
            *p       = '\0';
            av[ac++] = p + 1;
        }
    if (ac < f->min) {
        char msg[80];

        snprintf(msg, sizeof(msg), "insufficient number of arguments (%d) to function `%s'", ac,
                 f->name);
        fatal(msg);
        goto out;
    }

    // Those taken whole, expanded now.
    if (fn != F_IF && fn != F_OR && fn != F_AND && fn != F_FOREACH)
        for (i = 0; i < ac && i < 3; ++i)
            x[i] = substa(av[i]);
    if (stopped)
        goto out;

    switch (fn) {
    case F_SUBST:
        if (*x[0] == '\0') {
            putn(&b, bend, x[2], strlen(x[2]));
            putn(&b, bend, x[1], strlen(x[1]));
            break;
        }
        l = strlen(x[0]);
        for (s = x[2]; (t = strstr(s, x[0])) != NULL; s = t + l)
            if (!putn(&b, bend, s, t - s) || !putn(&b, bend, x[1], strlen(x[1])))
                break;
        putn(&b, bend, s, strlen(s));
        break;

    case F_PATSUBST:
        for (p = x[2]; (w = nextword(&p, &wl)) != NULL;)
            if (!patword(x[0], x[1], w, wl, &b, bend, &first))
                break;
        break;

    case F_STRIP:
        for (p = x[0]; (w = nextword(&p, &wl)) != NULL;)
            if (!putword(&b, bend, &first, w, wl))
                break;
        break;

    case F_FINDSTRING:
        if (strstr(x[1], x[0]))
            putn(&b, bend, x[0], strlen(x[0]));
        break;

    case F_FILTER:
    case F_FILTEROUT:
        for (p = x[1]; (w = nextword(&p, &wl)) != NULL;) {
            char *q = x[0], *pw, save;
            size_t pl;
            int hit = NO;

            while (!hit && (pw = nextword(&q, &pl)) != NULL) {
                save   = pw[pl];
                pw[pl] = '\0';
                hit    = pmatch(pw, w, wl, NULL, NULL);
                pw[pl] = save;
            }
            if (hit == (fn == F_FILTER) && !putword(&b, bend, &first, w, wl))
                break;
        }
        break;

    case F_SORT:
        for (n = 0, p = x[0]; nextword(&p, &wl) != NULL; ++n)
            ;
        v = (char **)ckalloc((n + 1) * sizeof(char *));
        for (n = 0, p = x[0]; (w = nextword(&p, &wl)) != NULL; ++n) {
            v[n] = w;
            if (*p)
                *p++ = '\0';
        }
        qsort(v, n, sizeof(char *), cmpstr);
        for (i = 0; i < n; ++i)
            if ((i == 0 || unequal(v[i], v[i - 1])) &&
                !putword(&b, bend, &first, v[i], strlen(v[i])))
                break;
        free(v);
        break;

    case F_WORD:
        if ((k = num(x[0])) <= 0) {
            fatal(k == 0 ? "first argument to `word' function must be greater than 0"
                         : "non-numeric first argument to `word' function");
            break;
        }
        for (p = x[1]; (w = nextword(&p, &wl)) != NULL;)
            if (--k == 0) {
                putn(&b, bend, w, wl);
                break;
            }
        break;

    case F_WORDLIST:
        k = num(x[0]);
        m = num(x[1]);
        if (k <= 0 || m < 0) {
            fatal(k == 0 ? "invalid first argument to `wordlist' function"
                         : "non-numeric argument to `wordlist' function");
            break;
        }
        for (i = 1, p = x[2]; (w = nextword(&p, &wl)) != NULL && i <= m; ++i)
            if (i >= k && !putword(&b, bend, &first, w, wl))
                break;
        break;

    case F_WORDS: {
        char nb[16];

        for (n = 0, p = x[0]; nextword(&p, &wl) != NULL; ++n)
            ;
        snprintf(nb, sizeof(nb), "%d", n);
        putn(&b, bend, nb, strlen(nb));
        break;
    }

    case F_FIRSTWORD:
        p = x[0];
        if ((w = nextword(&p, &wl)) != NULL)
            putn(&b, bend, w, wl);
        break;

    case F_LASTWORD:
        for (s = NULL, l = 0, p = x[0]; (w = nextword(&p, &wl)) != NULL; s = w, l = wl)
            ;
        if (s)
            putn(&b, bend, s, l);
        break;

    case F_DIR:
    case F_NOTDIR:
    case F_SUFFIX:
    case F_BASENAME:
        for (p = x[0]; (w = nextword(&p, &wl)) != NULL;) {
            char *slash = NULL, *dot = NULL;
            int ok = YES;

            for (s = w; s < w + wl; ++s)
                if (*s == '/')
                    slash = s, dot = NULL;
                else if (*s == '.')
                    dot = s;
            if (fn == F_DIR)
                ok = slash ? putword(&b, bend, &first, w, slash + 1 - w)
                           : putword(&b, bend, &first, "./", 2);
            else if (fn == F_NOTDIR)
                ok = slash ? putword(&b, bend, &first, slash + 1, w + wl - slash - 1)
                           : putword(&b, bend, &first, w, wl);
            else if (fn == F_SUFFIX) {
                if (dot)
                    ok = putword(&b, bend, &first, dot, w + wl - dot);
            } else
                ok = putword(&b, bend, &first, w, dot ? (size_t)(dot - w) : wl);
            if (!ok)
                break;
        }
        break;

    case F_ADDSUFFIX:
    case F_ADDPREFIX:
        for (p = x[1]; (w = nextword(&p, &wl)) != NULL;)
            if (fn == F_ADDSUFFIX ? !putword(&b, bend, &first, w, wl) ||
                                        !putn(&b, bend, x[0], strlen(x[0]))
                                  : !putword(&b, bend, &first, x[0], strlen(x[0])) ||
                                        !putn(&b, bend, w, wl))
                break;
        break;

    case F_JOIN: {
        char *p2 = x[1], *w2;
        size_t wl2;

        for (p = x[0];;) {
            w  = nextword(&p, &wl);
            w2 = nextword(&p2, &wl2);
            if (w == NULL && w2 == NULL)
                break;
            if (!putword(&b, bend, &first, w ? w : "", w ? wl : 0) ||
                (w2 && !putn(&b, bend, w2, wl2)))
                break;
        }
        break;
    }

    case F_IF:
        s = substa(av[0]);
        i = *strip(s) != '\0';
        free(s);
        if (i)
            b = subst(av[1], b, bend);
        else if (ac > 2)
            b = subst(av[2], b, bend);
        break;

    case F_OR:
    case F_AND:
        for (i = 0, s = NULL; i < ac && !stopped; ++i) {
            free(s);
            s = substa(av[i]);
            if ((*strip(s) != '\0') == (fn == F_OR))
                break;
        }
        if (s && (fn == F_OR ? i < ac : i == ac))
            putn(&b, bend, strip(s), strlen(strip(s)));
        free(s);
        break;

    case F_FOREACH: {
        char *nameb, *name, *list, *oval;
        int osimple, oused, ofromenv;

        nameb = substa(av[0]);
        name  = strip(nameb);
        list = substa(av[1]);
        vp   = varptr(name);
        oval = vp->varval, osimple = vp->simple, oused = vp->used, ofromenv = vp->fromenv;
        for (p = list; !stopped && !substfull && (w = nextword(&p, &wl)) != NULL;) {
            if (*p)
                *p++ = '\0';
            if (!first && !putn(&b, bend, " ", 1))
                break;
            first       = NO;
            vp->varval  = w;
            vp->simple  = YES;
            vp->fromenv = NO;
            b           = subst(av[2], b, bend);
        }
        vp->varval = oval, vp->simple = osimple, vp->used = oused, vp->fromenv = ofromenv;
        free(list);
        free(nameb);
        break;
    }
    }
    *b = '\0';
out:
    for (i = 0; i < 3; ++i)
        free(x[i]);
    free(av);
    return b;
}
