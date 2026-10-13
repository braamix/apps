/* UNIX V7 source code: see /COPYRIGHT or www.tuhs.org for details. */
/* Changes: Copyright (c) 1999 Robert Nordier. All rights reserved. */

// UNIX DEPENDENT PROCEDURES

#include <stdlib.h>
#include <string.h>

#include "defs.h"

// DEFAULT RULES.  POSIX's, for the toolchain Braam has: cc and as.  v7's .out
// rules are POSIX's single-suffix ones, and its yacc, lex, f77, Ratfor and EFL
// rules name nothing that is here.  README.md.
// clang-format off
const char *builtin[] = {
    ".SUFFIXES : .o .c .s .a",
    "CC=cc",
    "AS=as",
    "CFLAGS=",
    "LDFLAGS=",

    ".c :",
    "\t$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<",

    ".s :",
    "\t$(CC) $(LDFLAGS) -o $@ $<",

    ".c.o :",
    "\t$(CC) $(CFLAGS) -c $<",

    ".s.o :",
    "\t$(AS) -o $@ $<",

    0
};
// clang-format on

static Task<TIMETYPE> lookarch(char *filename);
static int amatch(char *s, char *p);
static int umatch(char *s, char *p);

Task<TIMETYPE> exists(char *filename)
{
    char *s;

    for (s = filename; *s != '\0' && *s != '('; ++s)
        ;

    if (*s == '(')
        co_return co_await lookarch(filename);

    Result<FileInfo> r = co_await stat_of(Str(filename, strlen(filename)));
    if (r.is_err())
        co_return 0;
    else
        co_return (TIMETYPE) r.value().mtime;
}

Task<TIMETYPE> prestime(void)
{
    Result<Clock> r = co_await clock_now();

    co_return r.is_ok() ? (TIMETYPE)r.value().epoch_ms : 0;
}

// Names matching pat.  A listing here is taken whole in one call and does not
// change after it, so where v7 rewound an open directory to see what commands
// had made since, this reads it again.  README.md, "Directories".
Task<struct depblock *> srchdir(char *pat, int mkchain, struct depblock *nextdbl)
{
    DIR *dirf;
    char *dirname, *dirpref, *endir, *filepat, *p, *fullname;
    struct dirent *dp;
    struct nameblock *q;
    struct depblock *thisdbl;
    struct opendir *od;
    struct pattern *patp;

    thisdbl = 0;

    if (mkchain == NO)
        for (patp = firstpat; patp; patp = patp->nxtpattern)
            if (!unequal(pat, patp->patval))
                co_return 0;

    patp             = ALLOC(pattern);
    patp->nxtpattern = firstpat;
    firstpat         = patp;
    patp->patval     = copys(pat);

    endir = 0;

    for (p = pat; *p != '\0'; ++p)
        if (*p == '/')
            endir = p;

    // The names are as long as they are: v7's buffers were 100 bytes.
    if (endir == 0) {
        dirname = (char *)".";
        dirpref = copys("");
        filepat = pat;
    } else {
        dirname = pat;
        *endir  = '\0';
        dirpref = concat(dirname, "/", (char *)ckalloc(strlen(dirname) + 2));
        filepat = endir + 1;
    }

    for (od = firstod; od; od = od->nxtopendir)
        if (!unequal(dirname, od->dirn))
            break;

    if (od == NULL) {
        od             = ALLOC(opendir);
        od->nxtopendir = firstod;
        firstod        = od;
        od->dirn       = copys(dirname);
    } else if (od->dirfc != NULL)
        b_closedir(od->dirfc); // read it again rather than rewind it
    dirf = od->dirfc = co_await b_opendir(dirname);

    if (dirf == NULL) {
        diag("Directory %s: ", dirname);
        fatal("Cannot open");
        if (endir != 0)
            *endir = '/';
        free(dirpref);
        co_return 0;
    }

    while ((dp = b_readdir(dirf)) != NULL) {
        if (!amatch(dp->d_name, filepat))
            continue;
        fullname =
            concat(dirpref, dp->d_name, (char *)ckalloc(strlen(dirpref) + strlen(dp->d_name) + 1));
        if ((q = srchname(fullname)) == 0)
            q = makename(fullname);
        free(fullname);
        if (stopped)
            break;
        if (mkchain) {
            thisdbl              = ALLOC(depblock);
            thisdbl->nxtdepblock = nextdbl;
            thisdbl->depname     = q;
            nextdbl              = thisdbl;
        }
    }

    if (endir != 0)
        *endir = '/';
    free(dirpref);

    co_return thisdbl;
}

// stolen from glob through find.  v7 recursed once per matched character;
// iterating instead keeps the depth to the number of `*'s in the pattern.
static int amatch(char *s, char *p)
{
    int cc, scc, k;
    int c, lc;

    for (;;) {
        scc = (unsigned char)*s;
        lc  = 077777;
        switch (c = (unsigned char)*p) {
        case '[':
            k = 0;
            while ((cc = (unsigned char)*++p)) {
                switch (cc) {
                case ']':
                    if (k)
                        goto advance;
                    else
                        return 0;

                case '-':
                    k |= (lc <= scc) & (scc <= (cc = (unsigned char)p[1]));
                }
                if (scc == (lc = cc))
                    k++;
            }
            return 0;

        case '?':
            if (scc)
                goto advance;
            return 0;

        case '*':
            return umatch(s, ++p);

        case 0:
            return !scc;
        }

        if (c != scc)
            return 0;
    advance:
        ++s;
        ++p;
    }
}

static int umatch(char *s, char *p)
{
    if (*p == 0)
        return 1;
    while (*s)
        if (amatch(s++, p))
            return 1;
    return 0;
}

// Look inside archives for notations a(b) and a((b))
//      a(b)    is file member  b  in archive a
//      a((b))  is entry point  b  in object archive a
//
// v7 read PDP-11 archive and object headers.  An archive here is what
// devel/wasm's ar writes, System V's: `!<arch>\n', then members behind 60-byte
// headers of decimal fields, a name past 15 bytes in the `//' member, and the
// entry points in the `/' member -- so a((b)) needs no object read at all.
// README.md, "Archives".

#define SARMAG  8
#define ARHDRSZ 60

struct arhdr {
    char name[256];
    long long date; // seconds
    long long size;
};

// A decimal field, blank-padded.
static long long arnum(const char *f, int n)
{
    long long v = 0;

    for (; n > 0 && *f == ' '; ++f, --n)
        ;
    for (; n > 0 && *f >= '0' && *f <= '9'; ++f, --n)
        v = v * 10 + (*f - '0');
    return v;
}

// The header at `off', its name resolved against the long-name table.  0 at
// the end of the archive or on a header that is not one.
static Task<int> getarhdr(FILE *fi, long long off, const char *longnames, long long nlong,
                          struct arhdr *h)
{
    char raw[ARHDRSZ];
    int i;
    long long at;

    if (co_await b_fseeko(fi, off, SEEK_SET) != 0)
        co_return 0;
    if (co_await b_fread(raw, 1, ARHDRSZ, fi) != ARHDRSZ)
        co_return 0;
    if (raw[58] != '`' || raw[59] != '\n')
        co_return 0;
    h->date = arnum(raw + 16, 12);
    h->size = arnum(raw + 48, 10);

    if (raw[0] == '/' && raw[1] >= '0' && raw[1] <= '9') {
        at = arnum(raw + 1, 15);
        for (i = 0; at + i < nlong && longnames[at + i] != '/' && longnames[at + i] != '\n' &&
                    i < (int)sizeof(h->name) - 1;
             ++i)
            h->name[i] = longnames[at + i];
        h->name[i] = '\0';
    } else {
        // "/" and "//" are themselves; a member name ends at its slash.
        for (i = 0; i < 16 && raw[i] != ' '; ++i)
            h->name[i] = raw[i];
        if (i > 1 && h->name[i - 1] == '/' && !(i == 2 && raw[0] == '/'))
            --i;
        h->name[i] = '\0';
    }
    co_return 1;
}

static Task<TIMETYPE> lookarch(char *filename)
{
    char *p, *q, *qend;
    char member[256];
    char magic[SARMAG];
    int objarch;
    FILE *fi;
    struct arhdr *h;
    char *symtab = NULL, *longnames = NULL;
    long long off, nsym = 0, nlong = 0, symsz = 0, i, at;
    TIMETYPE date;

    for (p = filename; *p != '('; ++p)
        ;
    *p = '\0';
    fi = co_await b_fopen(filename, "r");
    if (fi == NULL) {
        fatal1("cannot open %s", filename);
        *p = '(';
        co_return 0;
    }
    if (co_await b_fread(magic, 1, SARMAG, fi) != SARMAG || memcmp(magic, "!<arch>\n", SARMAG)) {
        fatal1("%s is not an archive", filename);
        *p = '(';
        co_await b_fclose(fi);
        co_return 0;
    }
    *p++ = '(';

    if (*p == '(') {
        objarch = YES;
        ++p;
    } else
        objarch = NO;

    qend = member + sizeof(member) - 1;
    for (q = member; q < qend && *p != '\0' && *p != ')'; *q++ = *p++)
        ;
    *q = '\0';

    date = 0;
    h    = ALLOC(arhdr);

    for (off = SARMAG; co_await getarhdr(fi, off, longnames, nlong, h);
         off += ARHDRSZ + h->size + (h->size & 1)) {
        if (!unequal(h->name, "/")) {
            free(symtab);
            if ((symtab = (char *)malloc(h->size + 1)) == NULL ||
                co_await b_fread(symtab, 1, h->size, fi) != (size_t)h->size)
                break;
            symtab[h->size] = '\0';
            symsz           = h->size;
            continue;
        }
        if (!unequal(h->name, "//")) {
            free(longnames);
            if ((longnames = (char *)malloc(h->size)) == NULL ||
                co_await b_fread(longnames, 1, h->size, fi) != (size_t)h->size)
                break;
            nlong = h->size;
            continue;
        }

        if (!objarch && !unequal(h->name, member)) {
            date = h->date;
            break;
        }
        if (objarch)
            break; // the symbol table is first or there is none
    }

    // An entry point: the `/' member is a big-endian count, that many member
    // offsets, then that many names, each NUL-terminated.
    if (objarch && symtab != NULL && symsz >= 4) {
        unsigned char *u = (unsigned char *)symtab;

        nsym = ((long long)u[0] << 24) | (u[1] << 16) | (u[2] << 8) | u[3];
        if (4 + 4 * nsym <= symsz) {
            q = symtab + 4 + 4 * nsym;
            for (i = 0; i < nsym && q < symtab + symsz; ++i, q += strlen(q) + 1)
                if (!unequal(q, member)) {
                    u  = (unsigned char *)symtab + 4 + 4 * i;
                    at = ((long long)u[0] << 24) | (u[1] << 16) | (u[2] << 8) | u[3];
                    if (co_await getarhdr(fi, at, longnames, nlong, h))
                        date = h->date;
                    break;
                }
        }
    }

    free(symtab);
    free(longnames);
    free(h);
    co_await b_fclose(fi);
    // A member's date is whole seconds and a file's is milliseconds: the end of
    // its second, so that a member is not older than the file it was made from.
    co_return date ? date * 1000 + 999 : 0;
}
