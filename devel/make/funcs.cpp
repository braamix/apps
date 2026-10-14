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
    F_WILDCARD,
    F_SHELL,
    F_VALUE,
    F_ORIGIN,
    F_INFO,
    F_WARNING,
    F_ERROR,
    F_CALL,
    F_EVAL,
    F_ABSPATH,
    F_REALPATH,
    F_FLAVOR,
    F_INTCMP,
    F_LET,
    F_FILE,
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
    { "or", 1, 1000 },     { "and", 1, 1000 },    { "foreach", 3, 3 },   { "wildcard", 1, 1 },
    { "shell", 1, 1 },     { "value", 1, 1 },     { "origin", 1, 1 },    { "info", 1, 1 },
    { "warning", 1, 1 },   { "error", 1, 1 },     { "call", 1, 1000 },   { "eval", 1, 1 },
    { "abspath", 1, 1 },   { "realpath", 1, 1 },  { "flavor", 1, 1 },    { "intcmp", 2, 5 },
    { "let", 3, 3 },       { "file", 1, 2 },
};
#define NFUNCS (int)(sizeof(funcs) / sizeof(funcs[0]))

// $(wildcard) and $(shell) wait, and subst() cannot: each is answered from
// here, and one that is not is recorded and `substneed' set, for the coroutine
// above to perform() and expand again.  Emptied after each command, which may
// have changed what either would say.  README.md.
struct req {
    struct req *next;
    int fn;
    char *arg;
    char *result; // NULL until performed
};
static struct req *reqs;
int substneed = NO;

static void ask(int fn, char *arg, char **bp, char *bend)
{
    struct req *r;

    for (r = reqs; r; r = r->next)
        if (r->fn == fn && !unequal(r->arg, arg))
            break;
    if (r == NULL) { // performed in the order asked: $(file >) then >>
        struct req **rp;

        r      = ALLOC(req);
        r->fn  = fn;
        r->arg = copys(arg);
        for (rp = &reqs; *rp; rp = &(*rp)->next)
            ;
        *rp = r;
    }
    if (r->result == NULL)
        substneed = YES;
    else
        putn(bp, bend, r->result, strlen(r->result));
}

Task<void> perform(void)
{
    struct req *r;

    substneed = NO;
    for (r = reqs; r && !stopped; r = r->next)
        if (r->result == NULL)
            r->result = r->fn == F_SHELL      ? co_await shellout(r->arg)
                        : r->fn == F_REALPATH ? co_await realpaths(r->arg)
                        : r->fn == F_FILE     ? co_await filefn(r->arg)
                                              : co_await wildcard(r->arg);
}

void forget(void)
{
    struct req *r;

    while ((r = reqs) != NULL) {
        reqs = r->next;
        free(r->arg);
        free(r->result);
        free(r);
    }
}

// What $(info) and $(warning) said, held until the expansion they are in is
// done for good: one done again, to grow its buffer or for a $(shell), says
// nothing twice.  substa() calls this at the start and end of the outermost.
struct held {
    struct held *next;
    int fd;
    char *text;
};
static struct held *heldq, **heldtail = &heldq;

static void hold(int fd, const char *text)
{
    struct held *h = ALLOC(held);

    h->fd     = fd;
    h->text   = copys(text);
    *heldtail = h;
    heldtail  = &h->next;
}

// $(eval)'s text, which no parse has taken yet: docom() parses it before the
// commands it was found in run.
char *evalpend;

// The texts of the $(eval)s of an expansion, in order, as one: read at once
// as one more input buffer while a makefile is, held for docom() otherwise.
static void evaltext(const char *t)
{
    size_t n = evalpend ? strlen(evalpend) : 0, m = strlen(t);

    evalpend = (char *)realloc(evalpend, n + m + 2);
    memcpy(evalpend + n, t, m);
    evalpend[n + m]     = '\n';
    evalpend[n + m + 1] = '\0';
}

void heldeffects(int keep)
{
    struct held *h;

    while ((h = heldq) != NULL) {
        heldq = h->next;
        if (keep && h->fd == SYS_STDOUT)
            mprintf("%s\n", h->text);
        else if (keep && h->fd == SYS_STDERR)
            diag("%s\n", h->text);
        else if (keep)
            evaltext(h->text);
        free(h->text);
        free(h);
    }
    heldtail = &heldq;
    if (keep && parsing && evalpend) {
        if (!pushin(evalpend, strlen(evalpend)))
            free(evalpend);
        evalpend = NULL;
    }
}

// A macro bound for $(foreach) or $(call), and what it was before.
struct binding {
    struct varblock *vp;
    char *val;
    int simple, used, fromenv, origin;
};

static void bind(struct binding *b, const char *name, char *val)
{
    b->vp      = varptr(name);
    b->val     = b->vp->varval;
    b->simple  = b->vp->simple;
    b->used    = b->vp->used;
    b->fromenv = b->vp->fromenv;
    b->origin  = b->vp->origin;

    b->vp->varval  = val;
    b->vp->simple  = YES;
    b->vp->fromenv = NO;
    b->vp->origin  = O_AUTO;
}

static void unbind(struct binding *b)
{
    b->vp->varval  = b->val;
    b->vp->simple  = b->simple;
    b->vp->used    = b->used;
    b->vp->fromenv = b->fromenv;
    b->vp->origin  = b->origin;
}

static const char *const onames[] = { "undefined", "default", "environment",
                                      "environment override", "file", "command line",
                                      "override", "automatic" };

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

// A signed number, for intcmp: NO if it is none.
static int intval(const char *s, long long *vp)
{
    int neg = NO;

    while (isblank_(*s))
        ++s;
    if (*s == '-' || *s == '+')
        neg = *s++ == '-';
    if (*s < '0' || *s > '9')
        return NO;
    for (*vp = 0; *s >= '0' && *s <= '9'; ++s)
        *vp = *vp * 10 + (*s - '0');
    while (isblank_(*s))
        ++s;
    if (neg)
        *vp = -*vp;
    return *s == '\0';
}

// A name made absolute against the directory make is in, with ., .. and
// repeated slashes gone, in a buffer of its own: $(abspath).
char *abspath(const char *name)
{
    char *full, *out, *d, *c, *e;
    size_t n;

    n    = (curdir ? strlen(curdir) : 1) + strlen(name) + 3;
    full = (char *)ckalloc(n);
    if (name[0] == '/')
        strcpy(full, name);
    else
        snprintf(full, n, "%s/%s", curdir ? curdir : "", name);
    out = (char *)ckalloc(strlen(full) + 2);
    d   = out;
    for (c = full; *c;) {
        while (*c == '/')
            ++c;
        for (e = c; *e && *e != '/'; ++e)
            ;
        if (e == c)
            break;
        if (e - c == 1 && c[0] == '.')
            ;
        else if (e - c == 2 && c[0] == '.' && c[1] == '.') {
            while (d > out && *--d != '/')
                ;
        } else {
            *d++ = '/';
            memcpy(d, c, e - c);
            d += e - c;
        }
        c = e;
    }
    if (d == out)
        *d++ = '/';
    *d = '\0';
    free(full);
    return out;
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
    int ac, nest, first = YES, i, n, before = substneed, argneed;
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

    // Those taken whole, expanded now. One still waiting on a call is not the
    // argument yet: what acts on it waits too, rather than run or stop on half
    // of it.
    argneed = NO;
    if (fn != F_IF && fn != F_OR && fn != F_AND && fn != F_FOREACH && fn != F_CALL &&
        fn != F_INTCMP && fn != F_LET) {
        substneed = NO;
        for (i = 0; i < ac && i < 3; ++i)
            x[i] = substa(av[i]);
        argneed   = substneed;
        substneed = before || argneed;
    }
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
        char *nameb, *name, *list;
        struct binding bd;

        nameb = substa(av[0]);
        name  = strip(nameb);
        list  = substa(av[1]);
        bind(&bd, name, NULL);
        for (p = list; !stopped && !substfull && (w = nextword(&p, &wl)) != NULL;) {
            if (*p)
                *p++ = '\0';
            if (!first && !putn(&b, bend, " ", 1))
                break;
            first         = NO;
            bd.vp->varval = w;
            b             = subst(av[2], b, bend);
        }
        unbind(&bd);
        free(list);
        free(nameb);
        break;
    }

    case F_VALUE:
        if ((vp = findvar(strip(x[0]))) != NULL && vp->varval)
            putn(&b, bend, vp->varval, strlen(vp->varval));
        break;

    case F_ORIGIN:
        s = strip(x[0]);
        if ((vp = findvar(s)) == NULL || vp->varval == NULL)
            n = O_UNDEF;
        else if (s[0] && strchr("@<*?^%+|", s[0]) && s[1] == '\0')
            n = O_AUTO;
        else
            n = vp->origin ? vp->origin : O_FILE;
        putn(&b, bend, onames[n], strlen(onames[n]));
        break;

    case F_INFO:
        hold(SYS_STDOUT, x[0]);
        break;

    case F_WARNING:
        if (parsing) {
            char *t = (char *)ckalloc(strlen(x[0]) + 32);

            snprintf(t, strlen(x[0]) + 32, "line %d: %s", yylineno, x[0]);
            hold(SYS_STDERR, t);
            free(t);
        } else
            hold(SYS_STDERR, x[0]);
        break;

    case F_ERROR:
        if (!argneed)
            fatal(x[0]);
        break;

    case F_EVAL:
        hold(-1, x[0]);
        break;

    case F_CALL: {
        // $(0) is the name and $(1)... the arguments, bound as $(foreach)
        // binds its name, and the value expanded with them.
        struct binding *bd;
        char *nameb, num[12];

        nameb = substa(av[0]);
        bd    = (struct binding *)ckalloc(ac * sizeof(*bd));
        for (i = 1; i < ac; ++i) {
            snprintf(num, sizeof(num), "%d", i);
            bind(&bd[i], num, substa(av[i]));
        }
        bind(&bd[0], "0", strip(nameb));
        if ((vp = findvar(bd[0].vp->varval)) != NULL && vp->varval)
            b = subst(vp->varval, b, bend);
        for (i = ac; i-- > 0;) {
            if (i > 0)
                free(bd[i].vp->varval);
            unbind(&bd[i]);
        }
        free(bd);
        free(nameb);
        break;
    }

    case F_WILDCARD:
    case F_SHELL:
    case F_REALPATH:
        if (!argneed)
            ask(fn, x[0], &b, bend);
        break;

    case F_ABSPATH:
        for (p = x[0]; (w = nextword(&p, &wl)) != NULL;) {
            char save = w[wl];

            w[wl] = '\0';
            s     = abspath(w);
            w[wl] = save;
            if (!putword(&b, bend, &first, s, strlen(s))) {
                free(s);
                break;
            }
            free(s);
        }
        break;

    case F_FLAVOR:
        s = strip(x[0]);
        vp = findvar(s);
        s  = (char *)(vp == NULL || vp->varval == NULL ? "undefined"
                      : vp->simple || (s[0] && strchr("@<*?^%+|", s[0]) && s[1] == '\0')
                          ? "simple"
                          : "recursive");
        putn(&b, bend, s, strlen(s));
        break;

    case F_INTCMP: {
        // $(intcmp a,b,lt,eq,gt): gt is eq when not given, and with two
        // arguments eq is the number itself.
        long long va, vb;
        int ok;

        s  = substa(av[0]);
        t  = substa(av[1]);
        ok = intval(s, &va) && intval(t, &vb);
        free(s);
        free(t);
        if (!ok) {
            fatal("non-numeric argument to `intcmp' function");
            break;
        }
        if (ac == 2) {
            if (va == vb) {
                char nb[24];

                snprintf(nb, sizeof(nb), "%lld", va);
                putn(&b, bend, nb, strlen(nb));
            }
            break;
        }
        i = va < vb ? 2 : va == vb ? 3 : ac > 4 ? 4 : 3;
        if (i < ac)
            b = subst(av[i], b, bend);
        break;
    }

    case F_LET: {
        // $(let a b,list,text): each name a word of the list, the last the
        // rest of it, bound as $(foreach) binds its name.
        struct binding *bd;
        char *names, *list, *nm, *rest;

        names = substa(av[0]);
        list  = substa(av[1]);
        for (n = 0, p = names; nextword(&p, &wl) != NULL; ++n)
            ;
        bd = (struct binding *)ckalloc((n + 1) * sizeof(*bd));
        for (i = 0, p = names, rest = list; (nm = nextword(&p, &wl)) != NULL; ++i) {
            if (*p)
                *p++ = '\0';
            while (isblank_(*rest))
                ++rest;
            if (i == n - 1) {
                bind(&bd[i], nm, copys(strip(rest)));
                continue;
            }
            w = rest;
            if ((w = nextword(&rest, &wl)) == NULL) {
                bind(&bd[i], nm, copys(""));
                continue;
            }
            s = (char *)ckalloc(wl + 1);
            memcpy(s, w, wl);
            bind(&bd[i], nm, s);
        }
        b = subst(av[2], b, bend);
        for (i = n; i-- > 0;) {
            free(bd[i].vp->varval);
            unbind(&bd[i]);
        }
        free(bd);
        free(names);
        free(list);
        break;
    }

    case F_FILE: {
        // $(file >f,text), >>f and <f: a request, as $(shell) is, the
        // operator, the name and the text in its key.
        char *key, *op = strip(x[0]);
        size_t kl;

        if (strncmp(op, ">", 1) && strncmp(op, "<", 1)) {
            fatal1("invalid file operation: %s", op);
            break;
        }
        kl  = strlen(op) + (ac > 1 ? strlen(x[1]) : 0) + 3;
        key = (char *)ckalloc(kl);
        snprintf(key, kl, "%s%c%s", op, ac > 1 ? '\1' : '\2', ac > 1 ? x[1] : "");
        if (!argneed)
            ask(fn, key, &b, bend);
        free(key);
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
