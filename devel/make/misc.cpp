/* UNIX V7 source code: see /COPYRIGHT or www.tuhs.org for details. */

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"

static struct nameblock *hashtab[HASHSIZE];
static int nhashed = 0;

// simple linear hash.  hash function is sum of characters mod hash table size.
static int hashloc(const char *s)
{
    int i;
    int hashval;
    const char *t;

    hashval = 0;

    for (t = s; *t != '\0'; ++t)
        hashval += *t;

    hashval %= HASHSIZE;

    for (i = hashval; hashtab[i] != 0 && unequal(s, hashtab[i]->namep); i = (i + 1) % HASHSIZE)
        ;

    return i;
}

struct nameblock *srchname(const char *s)
{
    return hashtab[hashloc(s)];
}

static int hasslash(const char *s)
{
    for (; *s; ++s)
        if (*s == '/')
            return YES;
    return NO;
}

// Whether a target may be the one made by default.  v7 asked this of every
// name it met, a prerequisite of .SILENT as much as a target; POSIX asks it of
// targets, in gram.cpp's rule().
int maincand(const char *s)
{
    return s[0] != '.' || hasslash(s);
}

// NULL once the table is full, and make has stopped.
struct nameblock *makename(const char *s)
{
    struct nameblock *p;

    if (nhashed++ > HASHSIZE - 3) {
        fatal("Hash table overflow");
        return NULL;
    }

    p               = ALLOC(nameblock);
    p->nxtnameblock = firstname;
    p->namep        = copys(s);
    p->linep        = 0;
    p->done         = 0;
    p->septype      = 0;
    p->modtime      = 0;

    firstname = p;

    hashtab[hashloc(s)] = p;

    return p;
}

// make a fresh copy of the string s
char *copys(const char *s)
{
    char *t, *t0;

    t = t0 = (char *)ckalloc(strlen(s) + 1);
    while ((*t++ = *s++))
        ;
    return t0;
}

// c = concatenation of a and b
char *concat(const char *a, const char *b, char *c)
{
    char *t;
    t = c;

    while ((*t = *a++))
        t++;
    while ((*t++ = *b++))
        ;
    return c;
}

// is b the suffix of a?  if so, set p = prefix
int suffix(char *a, char *b, char *p)
{
    char *a0, *b0;
    a0 = a;
    b0 = b;

    while (*a++)
        ;
    while (*b++)
        ;

    if ((a - a0) < (b - b0))
        return 0;

    while (b > b0)
        if (*--a != *--b)
            return 0;

    while (a0 < a)
        *p++ = *a0++;
    *p = '\0';

    return 1;
}

// Out of memory cannot unwind: every caller would have to test the pointer, so
// the process traps.  README.md.
void *ckalloc(int n)
{
    void *p;

    if ((p = calloc(1, n)))
        return p;

    __builtin_trap();
}

// Append n bytes of s at *bp, short of bend.  NO when they do not fit.
static int putn(char **bp, char *bend, const char *s, size_t n)
{
    if ((size_t)(bend - *bp) < n) {
        fatal("macro expansion too long");
        return NO;
    }
    memcpy(*bp, s, n);
    *bp += n;
    return YES;
}

// A macro's value as it reads in use: a := one is already expanded.
static char *valueof(struct varblock *vbp, char *b, char *bend)
{
    if (vbp->simple) {
        putn(&b, bend, vbp->varval, strlen(vbp->varval));
        *b = '\0';
        return b;
    }
    return subst(vbp->varval, b, bend);
}

// $(name:from=to), POSIX's substitution reference: in each word of the value
// ending with `from', that end becomes `to'.  With a % in `from' it is
// POSIX.1-2024's pattern form, $(name:%.c=%.o), the % in `to' being what the
// one in `from' matched.  The words come out one space apart.
static char *substref(struct varblock *vbp, char *spec, char *b, char *bend)
{
    size_t n = bend - b + 1;
    char *sp, *val, *from, *to, *w, *e, *fpct, *tpct;
    size_t fl, wl, pl, sl;
    int first = YES;

    sp  = (char *)ckalloc(OUTMAX);
    val = (char *)ckalloc(n);
    subst(spec, sp, sp + OUTMAX - 1);
    valueof(vbp, val, val + n - 1);
    if (stopped)
        goto out;
    if ((to = strchr(sp, '=')) == NULL) {
        b = subst(val, b, bend); // no `=': the value as it is
        goto out;
    }
    from  = sp;
    *to++ = '\0';
    fl    = strlen(from);
    fpct  = strchr(from, '%');
    tpct  = strchr(to, '%');

    for (w = val;; w = e) {
        while (*w == ' ' || *w == '\t' || *w == '\n')
            ++w;
        if (*w == '\0')
            break;
        for (e = w; *e && *e != ' ' && *e != '\t' && *e != '\n'; ++e)
            ;
        wl = e - w;
        if (!first && !putn(&b, bend, " ", 1))
            break;
        first = NO;
        if (fpct) {
            pl = fpct - from;
            sl = fl - pl - 1;
            if (wl >= pl + sl && !strncmp(w, from, pl) && !strncmp(e - sl, fpct + 1, sl)) {
                if (!tpct) {
                    if (!putn(&b, bend, to, strlen(to)))
                        break;
                } else if (!putn(&b, bend, to, tpct - to) ||
                           !putn(&b, bend, w + pl, wl - pl - sl) ||
                           !putn(&b, bend, tpct + 1, strlen(tpct + 1)))
                    break;
                continue;
            }
        } else if (wl >= fl && !strncmp(e - fl, from, fl)) {
            if (!putn(&b, bend, w, wl - fl) || !putn(&b, bend, to, strlen(to)))
                break;
            continue;
        }
        if (!putn(&b, bend, w, wl))
            break;
    }
    *b = '\0';
out:
    free(sp);
    free(val);
    return b;
}

// copy string a into b, substituting for arguments.  bend is the last byte b may
// write; v7 had no bound here at all and no bound on vname either.
char *subst(char *a, char *b, char *bend)
{
    static int depth = 0;
    char *s, *spec, *st;
    char vname[100];
    struct varblock *vbp;
    char closer;
    int nest;

    if (++depth > MAXSUBST)
        fatal("infinitely recursive macro?");
    if (a != 0)
        while (*a && !stopped) {
            if (b >= bend) {
                fatal("macro expansion too long");
                break;
            }
            if (*a != '$')
                *b++ = *a++;
            else if (*++a == '\0' || *a == '$')
                *b++ = *a++;
            else {
                s    = vname;
                spec = NULL;
                if (*a == '(' || *a == '{') {
                    closer = (*a == '(' ? ')' : '}');
                    ++a;
                    while (*a == ' ')
                        ++a;
                    while (*a != ' ' && *a != ':' && *a != closer && *a != '\0') {
                        if (s >= &vname[sizeof(vname) - 1]) {
                            fatal("macro name too long");
                            break;
                        }
                        *s++ = *a++;
                    }
                    if (*a == ':') {
                        // the rest, to the closer that matches: $(A:.c=$(O))
                        for (st = ++a, nest = 0; *a && (*a != closer || nest > 0); ++a)
                            if (*a == '(' || *a == '{')
                                ++nest;
                            else if (*a == ')' || *a == '}')
                                --nest;
                        spec = (char *)ckalloc(a - st + 1);
                        memcpy(spec, st, a - st);
                    }
                    while (*a != closer && *a != '\0')
                        ++a;
                    if (*a == closer)
                        ++a;
                } else
                    *s++ = *a++;

                *s = '\0';
                if ((vbp = varptr(vname))->varval != 0) {
                    b         = spec ? substref(vbp, spec, b, bend) : valueof(vbp, b, bend);
                    vbp->used = YES;
                }
                free(spec);
            }
        }

    *b = '\0';
    --depth;
    return b;
}

// YES if it was set: a macro from the command line is not reset.
int setvar(const char *v, char *s)
{
    struct varblock *p;

    p = varptr(v);
    if (p->noreset == 0) {
        p->varval  = s;
        p->simple  = NO;
        p->noreset = inarglist;
        if (p->used && unequal(v, "@") && unequal(v, "*") && unequal(v, "<") && unequal(v, "?"))
            diag("Warning: %s changed after being used\n", v);
        return YES;
    }
    return NO;
}

// The assignment operators past v7's `=': `:=' and `::=' expand the value
// now and once, `?=' sets only what is not set, `+=' appends after a space
// -- expanded first when the macro is a := one, as GNU and POSIX have it.
static void assign(char *v, char *s, int op)
{
    struct varblock *p = varptr(v);
    char *buf, *old;
    int simple = p->simple, used;

    if (op == '?' && p->varval != 0)
        return;
    if (op == '+' && p->varval != 0) {
        old = p->varval;
        buf = (char *)ckalloc(strlen(old) + 1 + OUTMAX);
        strcpy(buf, old);
        strcat(buf, " ");
        if (p->simple)
            subst(s, buf + strlen(buf), buf + strlen(old) + OUTMAX - 1);
        else
            strcat(buf, s);
        used    = p->used; // appending after a use is what += is for: no warning
        p->used = NO;
        if (setvar(v, copys(buf)))
            p->simple = simple;
        p->used = used;
        free(buf);
        return;
    }
    if (op == ':') {
        buf = (char *)ckalloc(OUTMAX);
        subst(s, buf, buf + OUTMAX - 1);
        if (setvar(v, copys(buf)))
            p->simple = YES;
        free(buf);
        return;
    }
    setvar(v, copys(s));
}

// Whether s, at a colon, is the colon of := or ::=.
int assigncolon(const char *s)
{
    return s[1] == '=' || (s[1] == ':' && s[2] == '=');
}

// look for arguments with equal signs but not colons -- but for those of :=
// and ::=.  The character before the = may make it +=, ?= or :=.
int eqsign(char *a)
{
    char *s, *t, *e;
    int op;

    while (*a == ' ')
        ++a;
    for (s = a; *s != '\0' && *s != '='; ++s)
        if (*s == ':' && !assigncolon(s))
            return NO;
    if (*s != '=')
        return NO;

    op = '=';
    e  = s;
    if (e > a && (e[-1] == '+' || e[-1] == '?' || e[-1] == ':')) {
        op = *--e;
        if (op == ':' && e > a && e[-1] == ':')
            --e;
    }
    for (t = a; t < e && *t != ' ' && *t != '\t'; ++t)
        ;
    *t = '\0';

    for (++s; *s == ' ' || *s == '\t'; ++s)
        ;
    assign(a, s, op);
    return YES;
}

struct varblock *varptr(const char *v)
{
    struct varblock *vp;

    for (vp = firstvar; vp; vp = vp->nxtvarblock)
        if (!unequal(v, vp->varname))
            return vp;

    vp              = ALLOC(varblock);
    vp->nxtvarblock = firstvar;
    firstvar        = vp;
    vp->varname     = copys(v);
    vp->varval      = 0;
    return vp;
}

// v7 passed sprintf()'s value straight to fatal(); C11's returns an int.  And a
// name is as long as it likes, so this is bounded.
void fatal1(const char *s, const char *t)
{
    char buf[100];

    snprintf(buf, sizeof(buf), s, t);
    fatal(buf);
}

int stopped = 0;

void stop(int status)
{
    if (!stopped)
        stopped = status | 0x100; // nonzero even for a status of 0
}

// Said once, by proc_main(), after everything held before it.
void fatal(const char *s)
{
    if (stopped)
        return;
    if (s)
        diag("Make: %s.  Stop.\n", s);
    else
        diag("\nStop.\n");
    stop(2); // POSIX: >1 is an error, 1 being -q's "not up to date"
}

void yyerror(const char *s)
{
    char buf[50];

    snprintf(buf, sizeof(buf), "line %d: %s", yylineno, s);
    fatal(buf);
}

struct chain *appendq(struct chain *head, char *tail)
{
    struct chain *p, *q;

    p        = ALLOC(chain);
    p->datap = tail;

    if (head) {
        for (q = head; q->nextp; q = q->nextp)
            ;
        q->nextp = p;
        return head;
    } else
        return p;
}

char *mkqlist(struct chain *p)
{
    char *qbufp, *s;
    static char qbuf[QBUFMAX];

    if (p == NULL) {
        qbuf[0] = '\0';
        return qbuf; // v7 returned nothing at all here
    }

    qbufp = qbuf;

    for (; p; p = p->nextp) {
        s = p->datap;
        if (qbufp + strlen(s) > &qbuf[QBUFMAX - 3]) {
            diag("$? list too long\n");
            break;
        }
        while (*s)
            *qbufp++ = *s++;
        *qbufp++ = ' ';
    }
    // v7 wrote *--qbufp unconditionally, forming a pointer below the array when
    // the first name already overflowed.
    if (qbufp > qbuf)
        --qbufp;
    *qbufp = '\0';
    return qbuf;
}

// The held output, stdout and stderr in the order they were said: one buffer of
// runs, each a descriptor byte, a length and the bytes.  It grows rather than
// truncates, and only flushout() writes it -- a plain function cannot, and an
// await that never suspends costs native stack (README.md, "Output").
static char *obuf;
static int olen, ocap, orun; // orun: where the open run's header is, or -1

static void vput(int fd, const char *fmt, va_list ap)
{
    va_list aq;
    int n, cap;
    char *p;

    va_copy(aq, ap);
    n = vsnprintf(NULL, 0, fmt, aq);
    va_end(aq);
    if (n <= 0)
        return;
    if (olen + n + 5 + 1 > ocap) {
        cap = ocap ? ocap : 4096;
        while (olen + n + 5 + 1 > cap)
            cap *= 2;
        if ((p = (char *)realloc(obuf, cap)) == NULL)
            return;
        obuf = p;
        ocap = cap;
    }
    if (olen == 0 || obuf[orun] != fd) {
        orun       = olen;
        obuf[olen] = (char)fd;
        memset(obuf + olen + 1, 0, 4);
        olen += 5;
    }
    vsnprintf(obuf + olen, n + 1, fmt, ap);
    olen += n;
    int run = 0;
    memcpy(&run, obuf + orun + 1, 4);
    run += n;
    memcpy(obuf + orun + 1, &run, 4);
}

void mprintf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vput(SYS_STDOUT, fmt, ap);
    va_end(ap);
}

void diag(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vput(SYS_STDERR, fmt, ap);
    va_end(ap);
}

int outheld(void)
{
    return olen;
}

Task<void> flushout(void)
{
    int at, run;

    for (at = 0; at < olen; at += 5 + run) {
        memcpy(&run, obuf + at + 1, 4);
        co_await write_all((u32)obuf[at], Str(obuf + at + 5, run));
    }
    olen = 0;
}
