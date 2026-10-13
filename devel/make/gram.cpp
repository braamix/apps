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
    INCLUDE, // include, -include or sinclude: incnames names the files
};

// What an INCLUDE line named, expanded, and whether a missing file is fine.
static char incnames[INMAX];
static int incopt;

// yacc's %union.
static union {
    struct shblock *yshblock;
    struct depblock *ydepblock;
    struct nameblock *ynameblock;
} yylval;

static int retsh(char *q);
static int nextlin(void);

struct depblock *pp;
static struct shblock *prevshp;

static struct nameblock *lefts[NLEFTS];
struct nameblock *leftp;
static int nlefts;

struct lineblock *lp, *lpp;
static struct depblock *prevdep;
static int sepc;

// comline : START namelist deplist shellist
static void rule(struct depblock *deps, struct shblock *shell)
{
    int i;

    for (i = 0; i < nlefts && mainname == NULL; ++i)
        if (maincand(lefts[i]->namep))
            mainname = lefts[i];

    while (--nlefts >= 0) {
        leftp = lefts[nlefts];
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
    struct depblock *deps;
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
        lefts[0] = yylval.ynameblock;
        nlefts   = 1;
        while ((tok = yylex()) == NAME) {
            if (nlefts >= NLEFTS) {
                fatal("Too many lefts");
                co_return 1;
            }
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
        sepc    = (tok == COLON ? ALLDEPS : SOMEDEPS);
        prevdep = 0;
        deps    = 0;
        while ((tok = yylex()) == NAME) {
            pp              = ALLOC(depblock);
            pp->nxtdepblock = NULL;
            pp->depname     = yylval.ynameblock;
            if (prevdep == 0)
                deps = pp;
            else
                prevdep->nxtdepblock = pp;
            prevdep = pp;
        }
        if (stopped)
            co_return 1;

        // shellist
        shell = 0;
        while (tok == SHELLINE) {
            if (shell == 0)
                shell = yylval.yshblock;
            else
                prevshp->nxtshblock = yylval.yshblock;
            prevshp = yylval.yshblock;
            tok     = yylex();
        }
        if (stopped)
            co_return 1;

        rule(deps, shell);
    }
}

char *zznextc; // zero if need another line; otherwise points to next char
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

// Scratch at file scope: three INMAX char arrays.
static char word[INMAX];
static char templin[INMAX];
static char yytext[INMAX];

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

int yylex(void)
{
    char *p;
    char *q;

    if (stopped)
        return 0;

    if (zznextc == 0)
        return nextlin();

    while (isspace((unsigned char)*zznextc))
        ++zznextc;

    if (*zznextc == '\0')
        return nextlin();

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

    if (*zznextc == ';')
        return retsh(zznextc);

    p = zznextc;
    q = word;

    while (!(funny[(unsigned char)*p] & TERMINAL)) {
        if (q >= &word[INMAX - 1]) {
            fatal("word too long");
            return 0;
        }
        *q++ = *p++;
    }

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

static int nextlin(void)
{
    static char *yytextl = yytext + INMAX;
    char *text;
    char c;
    char *p, *t;
    const char *bt;
    char lastch, *lastchp;
    int incom;
    int kc;

again:

    incom   = NO;
    zznextc = 0;

    if (fin == NULL) {
        // A builtin line is copied rather than parsed in place: eqsign() and the
        // substitution below write into it, and a literal is not writable.
        if ((bt = *linesptr++) == 0)
            return 0;
        ++yylineno;
        for (p = yytext; p < yytextl && (*p = *bt++) != '\0'; ++p)
            ;
        if (p >= yytextl) {
            fatal("line too long");
            return 0;
        }
        text = yytext;
    }

    else {
        text = yytext;
        for (p = yytext; p < yytextl; *p++ = kc)
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
                if (p == yytext || p[-1] != '\\') {
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
                return 0;
            }

        fatal("line too long");
        return 0;
    }

endloop:

    if ((c = text[0]) == '\t')
        return retsh(text);

    if (isalpha((unsigned char)c) || isdigit((unsigned char)c) || c == ' ' || c == '.' ||
        c == '_') {
        for (p = text + 1; *p != '\0';)
            if (*p == ':' && !assigncolon(p))
                break;
            else if (*p++ == '=') {
                eqsign(text);
                return MACRODEF;
            }
    }

    // include: the rest of the line, expanded, names the files, which
    // yyparse() reads (README.md, "Includes").
    if ((p = isinclude(text, &incopt)) != NULL) {
        subst(p, incnames, incnames + INMAX - 1);
        return stopped ? 0 : INCLUDE;
    }

    // substitute for macros on dependency line up to the semicolon if any
    for (t = text; *t != '\0' && *t != ';'; ++t)
        ;

    lastchp = t;
    lastch  = *t;
    *t      = '\0';

    subst(text, templin, templin + INMAX - 1);
    if (stopped)
        return 0;

    if (lastch) {
        for (t = templin; *t; ++t)
            ;
        if (t >= &templin[INMAX - 1]) {
            fatal("macro expansion too long");
            return 0;
        }
        *t = lastch;
        while (t < &templin[INMAX - 1] && (*++t = *++lastchp))
            ;
        *t = '\0';
    }

    p = templin;
    t = text;
    while ((*t++ = *p++))
        ;

    for (p = zznextc = text; *p; ++p)
        if (*p != ' ' && *p != '\t')
            return START;
    goto again;
}
