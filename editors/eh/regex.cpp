// A compact POSIX ERE engine: recursive-descent parser into a node arena, then
// a backtracking matcher over an explicit continuation list.
//
// Leftmost-longest, not leftmost-first: accept() records the end and returns
// false, so every match at a start position is enumerated and the longest kept.
// Captures come from whichever match won.
//
// Offsets are bytes, as eh's are. `.` and a bracket consume a whole UTF-8
// sequence, so a match never leaves `here` mid-character.

#include "regex.h"

#include "braam.h"

namespace {

enum {
    OP_LIT,   // bytes[0..len) verbatim
    OP_ANY,   // .
    OP_SET,   // [...]
    OP_BOL,   // ^
    OP_EOL,   // $
    OP_GROUP, // ( ... )
    OP_CLOSE, // the end of a group, so the capture closes on the way out
    OP_ALT,   // branches chained through `alt`
    OP_REP,   // child repeated min..max, max < 0 for unbounded
};

struct Node {
    int op;
    int next;  // sibling in a sequence, -1 at the end
    int child; // GROUP/REP body, ALT first branch
    int alt;   // ALT next branch
    int min, max;
    int group; // GROUP/CLOSE
    int set;   // SET
    int close; // GROUP: its OP_CLOSE node
    int len;
    unsigned char bytes[4];
};

// 12 POSIX classes, applied to runes past ASCII; ASCII lands in `bits`.
enum {
    CL_ALPHA  = 1,
    CL_DIGIT  = 2,
    CL_ALNUM  = 4,
    CL_UPPER  = 8,
    CL_LOWER  = 16,
    CL_SPACE  = 32,
    CL_BLANK  = 64,
    CL_PUNCT  = 128,
    CL_PRINT  = 256,
    CL_GRAPH  = 512,
    CL_CNTRL  = 1024,
    CL_XDIGIT = 2048,
};

struct Set {
    unsigned char bits[16]; // ASCII membership
    int neg;
    int rfirst, rcount; // into the range arena
    int classes;
};

struct Range {
    unsigned int lo, hi;
};

// --------------------------------------------------------------- the arenas

struct Build {
    regex_t *re;
    const char *p;
    int ngroup;
    int err;
};

Node *nodes(const regex_t *re)
{
    return (Node *)re->prog;
}
Set *sets(const regex_t *re)
{
    return (Set *)re->sets;
}
Range *ranges(const regex_t *re)
{
    return (Range *)re->ranges;
}

int new_node(Build *b, int op)
{
    regex_t *re = b->re;
    Node *n     = (Node *)realloc(re->prog, sizeof(Node) * (size_t)(re->nnodes + 1));

    if (!n) {
        b->err = REG_ESPACE;
        return -1;
    }
    re->prog = n;
    n += re->nnodes;
    memset(n, 0, sizeof(*n));
    n->op    = op;
    n->next  = -1;
    n->child = -1;
    n->alt   = -1;
    n->set   = -1;
    n->close = -1;
    n->max   = -1;
    return re->nnodes++;
}

int new_set(Build *b)
{
    regex_t *re = b->re;
    Set *s      = (Set *)realloc(re->sets, sizeof(Set) * (size_t)(re->nsets + 1));

    if (!s) {
        b->err = REG_ESPACE;
        return -1;
    }
    re->sets = s;
    s += re->nsets;
    memset(s, 0, sizeof(*s));
    s->rfirst = re->nranges;
    return re->nsets++;
}

int add_range(Build *b, unsigned int lo, unsigned int hi)
{
    regex_t *re = b->re;
    Range *r    = (Range *)realloc(re->ranges, sizeof(Range) * (size_t)(re->nranges + 1));

    if (!r) {
        b->err = REG_ESPACE;
        return -1;
    }
    re->ranges = r;
    r += re->nranges;
    r->lo = lo;
    r->hi = hi;
    return re->nranges++;
}

// ------------------------------------------------------------------- UTF-8

// Bytes in the sequence at s, or 1 for anything malformed.
int seqlen(const char *s)
{
    unsigned char c = (unsigned char)*s;

    if (c < 0x80)
        return 1;
    if (c < 0xC2)
        return 1;
    if (c < 0xE0)
        return ((s[1] & 0xC0) == 0x80) ? 2 : 1;
    if (c < 0xF0)
        return ((s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) ? 3 : 1;
    if (c < 0xF5)
        return ((s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) ? 4 : 1;
    return 1;
}

// The rune at s and its length; a malformed byte decodes as itself.
int decode(const char *s, unsigned int *out)
{
    int n           = seqlen(s);
    unsigned char c = (unsigned char)*s;

    if (n == 1) {
        *out = c;
        return 1;
    }
    unsigned int v = c & (unsigned int)(0xFF >> (n + 1));
    for (int i = 1; i < n; i++)
        v = (v << 6) | ((unsigned char)s[i] & 0x3F);
    *out = v;
    return n;
}

// ------------------------------------------------------------------ parser

int parse_alt(Build *b);

void set_bit(Set *s, unsigned int c)
{
    if (c < 128)
        s->bits[c >> 3] |= (unsigned char)(1 << (c & 7));
}

int class_named(const char *name, int len)
{
    struct {
        const char *n;
        int bit;
    } static const T[] = {
        { "alpha", CL_ALPHA }, { "digit", CL_DIGIT }, { "alnum", CL_ALNUM },
        { "upper", CL_UPPER }, { "lower", CL_LOWER }, { "space", CL_SPACE },
        { "blank", CL_BLANK }, { "punct", CL_PUNCT }, { "print", CL_PRINT },
        { "graph", CL_GRAPH }, { "cntrl", CL_CNTRL }, { "xdigit", CL_XDIGIT },
    };

    for (unsigned i = 0; i < sizeof(T) / sizeof(T[0]); i++) {
        if ((int)strlen(T[i].n) == len && memcmp(T[i].n, name, (size_t)len) == 0)
            return T[i].bit;
    }
    return 0;
}

int ascii_in_class(unsigned int c, int bit)
{
    switch (bit) {
    case CL_ALPHA:
        return isalpha((int)c);
    case CL_DIGIT:
        return isdigit((int)c);
    case CL_ALNUM:
        return isalnum((int)c);
    case CL_UPPER:
        return isupper((int)c);
    case CL_LOWER:
        return islower((int)c);
    case CL_SPACE:
        return isspace((int)c);
    case CL_BLANK:
        return c == ' ' || c == '\t';
    case CL_PUNCT:
        return ispunct((int)c);
    case CL_PRINT:
        return isprint((int)c);
    case CL_GRAPH:
        return isgraph((int)c);
    case CL_CNTRL:
        return iscntrl((int)c);
    case CL_XDIGIT:
        return isxdigit((int)c);
    }
    return 0;
}

// [ has been consumed.
int parse_bracket(Build *b)
{
    int si = new_set(b);

    if (si < 0)
        return -1;
    if (*b->p == '^') {
        b->p++;
        sets(b->re)[si].neg = 1;
    }

    int first = 1;
    for (;;) {
        if (*b->p == '\0') {
            b->err = REG_BADPAT;
            return -1;
        }
        if (*b->p == ']' && !first)
            break;
        first = 0;

        // [:class:]
        if (b->p[0] == '[' && b->p[1] == ':') {
            const char *q = b->p + 2;
            while (*q && !(q[0] == ':' && q[1] == ']'))
                q++;
            if (*q == '\0') {
                b->err = REG_BADPAT;
                return -1;
            }
            int bit = class_named(b->p + 2, (int)(q - (b->p + 2)));
            if (!bit) {
                b->err = REG_BADPAT;
                return -1;
            }
            Set *s = &sets(b->re)[si];
            s->classes |= bit;
            for (unsigned int c = 0; c < 128; c++)
                if (ascii_in_class(c, bit))
                    set_bit(s, c);
            b->p = q + 2;
            continue;
        }

        unsigned int lo;
        if (*b->p == '\\' && b->p[1]) {
            b->p++;
            lo = (unsigned char)*b->p++;
        } else {
            b->p += decode(b->p, &lo);
        }

        unsigned int hi = lo;
        if (b->p[0] == '-' && b->p[1] != ']' && b->p[1] != '\0') {
            b->p++;
            if (*b->p == '\\' && b->p[1]) {
                b->p++;
                hi = (unsigned char)*b->p++;
            } else {
                b->p += decode(b->p, &hi);
            }
            if (hi < lo) {
                b->err = REG_BADPAT;
                return -1;
            }
        }

        Set *s = &sets(b->re)[si];
        if (hi < 128) {
            for (unsigned int c = lo; c <= hi; c++)
                set_bit(s, c);
        } else {
            for (unsigned int c = lo; c < 128 && c <= hi; c++)
                set_bit(s, c);
            if (add_range(b, lo < 128 ? 128 : lo, hi) < 0)
                return -1;
            sets(b->re)[si].rcount++;
        }
    }
    b->p++; // ]

    int ni = new_node(b, OP_SET);
    if (ni < 0)
        return -1;
    nodes(b->re)[ni].set = si;
    return ni;
}

// One \x escape as its byte. Upstream's own cescape() has already run over the
// pattern for \t and friends, so these are the ones only a regex sees.
int escape_byte(int c)
{
    switch (c) {
    case 'a':
        return '\a';
    case 'b':
        return '\b';
    case 'f':
        return '\f';
    case 'n':
        return '\n';
    case 'r':
        return '\r';
    case 't':
        return '\t';
    case 'v':
        return '\v';
    }
    return c;
}

int parse_atom(Build *b)
{
    int ni;

    switch (*b->p) {
    case '(': {
        b->p++;
        int g = ++b->ngroup;
        ni    = new_node(b, OP_GROUP);
        if (ni < 0)
            return -1;
        int ci = new_node(b, OP_CLOSE);
        if (ci < 0)
            return -1;
        int inner = parse_alt(b);
        if (b->err)
            return -1;
        if (*b->p != ')') {
            b->err = REG_BADPAT;
            return -1;
        }
        b->p++;
        Node *n     = nodes(b->re);
        n[ci].group = g;
        n[ni].group = g;
        n[ni].child = inner;
        n[ni].close = ci;
        return ni;
    }
    case '[':
        b->p++;
        return parse_bracket(b);
    case '.':
        b->p++;
        return new_node(b, OP_ANY);
    case '^':
        b->p++;
        return new_node(b, OP_BOL);
    case '$':
        b->p++;
        return new_node(b, OP_EOL);
    case ')':
    case '|':
    case '\0':
        b->err = REG_BADPAT;
        return -1;
    case '*':
    case '+':
    case '?':
        // A repeat with nothing to repeat.
        b->err = REG_BADPAT;
        return -1;
    }

    ni = new_node(b, OP_LIT);
    if (ni < 0)
        return -1;
    Node *n = &nodes(b->re)[ni];
    if (*b->p == '\\') {
        b->p++;
        if (*b->p == '\0') {
            b->err = REG_BADPAT;
            return -1;
        }
        n->bytes[0] = (unsigned char)escape_byte((unsigned char)*b->p++);
        n->len      = 1;
    } else {
        int len = seqlen(b->p);
        for (int i = 0; i < len; i++)
            n->bytes[i] = (unsigned char)b->p[i];
        n->len = len;
        b->p += len;
    }
    return ni;
}

int parse_rep(Build *b)
{
    int ai = parse_atom(b);

    if (ai < 0)
        return -1;
    for (;;) {
        int mn, mx;

        if (*b->p == '*') {
            mn = 0;
            mx = -1;
        } else if (*b->p == '+') {
            mn = 1;
            mx = -1;
        } else if (*b->p == '?') {
            mn = 0;
            mx = 1;
        } else if (*b->p == '{' && isdigit((unsigned char)b->p[1])) {
            const char *q = b->p + 1;
            mn            = 0;
            while (isdigit((unsigned char)*q))
                mn = mn * 10 + (*q++ - '0');
            if (*q == ',') {
                q++;
                if (isdigit((unsigned char)*q)) {
                    mx = 0;
                    while (isdigit((unsigned char)*q))
                        mx = mx * 10 + (*q++ - '0');
                } else {
                    mx = -1;
                }
            } else {
                mx = mn;
            }
            if (*q != '}' || (0 <= mx && mx < mn)) {
                b->err = REG_BADPAT;
                return -1;
            }
            b->p = q;
        } else {
            return ai;
        }
        b->p++;

        int ri = new_node(b, OP_REP);
        if (ri < 0)
            return -1;
        Node *n     = nodes(b->re);
        n[ri].child = ai;
        n[ri].min   = mn;
        n[ri].max   = mx;
        n[ai].next  = -1;
        ai          = ri;
    }
}

// A concatenation, returned as the head of a `next` chain.
int parse_cat(Build *b)
{
    int head = -1, tail = -1;

    while (*b->p && *b->p != '|' && *b->p != ')') {
        int ni = parse_rep(b);
        if (ni < 0)
            return -1;
        if (tail < 0)
            head = ni;
        else
            nodes(b->re)[tail].next = ni;
        tail = ni;
    }
    return head;
}

// An empty branch is the empty sequence, which is -1 and would end the branch
// chain. A REP{0,0} matches empty and keeps the chain walkable.
int empty_branch(Build *b)
{
    int ni = new_node(b, OP_REP);

    if (ni < 0)
        return -1;
    Node *n     = nodes(b->re);
    n[ni].child = -1;
    n[ni].min   = 0;
    n[ni].max   = 0;
    return ni;
}

int parse_alt(Build *b)
{
    int first = parse_cat(b);

    if (b->err)
        return -1;
    if (*b->p != '|')
        return first;

    if (first < 0 && (first = empty_branch(b)) < 0)
        return -1;

    int ai = new_node(b, OP_ALT);
    if (ai < 0)
        return -1;
    nodes(b->re)[ai].child = first;

    int tail = first;
    while (*b->p == '|') {
        b->p++;
        int br = parse_cat(b);
        if (b->err)
            return -1;
        if (br < 0 && (br = empty_branch(b)) < 0)
            return -1;
        nodes(b->re)[tail].alt = br;
        tail                   = br;
    }
    return ai;
}

// ------------------------------------------------------------- the matcher

enum { NCAP = 10 };

struct Cont {
    int kind; // 0 sequence, 1 another turn of a repeat
    int node;
    int count;
    const char *from;
    // REP: the captures as they were before this turn. A turn that consumes
    // nothing did not participate, and POSIX says its groups do not count.
    const regmatch_t *save;
    const Cont *up;
};

struct Exec {
    const regex_t *re;
    const char *base;
    int eflags;
    int newline;
    long budget;
    int depth;

    // Where each turn of a simple repeat ended, so the backing off is a walk
    // down this rather than a return down the native stack.
    const char **marks;
    int nmarks, cmarks;

    regmatch_t *cap;
    int ncap;
    const char *best; // longest end seen at this start, or null
    regmatch_t *bestcap;
};

enum { MAX_DEPTH = 2000 };

bool mseq(Exec *e, int ni, const char *s, const Cont *k);

bool mcont(Exec *e, const Cont *k, const char *s);

bool mrep(Exec *e, int ni, const char *s, int count, const Cont *k);

bool in_set(const Exec *e, const Set *st, unsigned int c)
{
    int hit = 0;

    if (c < 128) {
        hit = (st->bits[c >> 3] >> (c & 7)) & 1;
    } else {
        const Range *r = ranges(e->re);
        for (int i = 0; i < st->rcount; i++) {
            if (r[st->rfirst + i].lo <= c && c <= r[st->rfirst + i].hi) {
                hit = 1;
                break;
            }
        }
        if (!hit && st->classes) {
            if ((st->classes & CL_ALPHA) && iswalpha((wint_t)c))
                hit = 1;
            else if ((st->classes & CL_ALNUM) && iswalnum((wint_t)c))
                hit = 1;
            else if ((st->classes & CL_UPPER) && iswupper((wint_t)c))
                hit = 1;
            else if ((st->classes & CL_LOWER) && iswlower((wint_t)c))
                hit = 1;
            else if ((st->classes & CL_SPACE) && iswspace((wint_t)c))
                hit = 1;
            else if ((st->classes & CL_PUNCT) && iswpunct((wint_t)c))
                hit = 1;
            else if ((st->classes & CL_PRINT) && iswprint((wint_t)c))
                hit = 1;
            else if ((st->classes & CL_GRAPH) && iswgraph((wint_t)c))
                hit = 1;
        }
    }
    if (!st->neg)
        return hit != 0;
    // A negated bracket never takes a newline under REG_NEWLINE.
    if (e->newline && c == '\n')
        return false;
    return hit == 0;
}

// The end of the whole pattern: record and refuse, so the search goes on and
// the longest match wins.
bool accept(Exec *e, const char *s)
{
    if (!e->best || e->best < s) {
        e->best = s;
        for (int i = 0; i < e->ncap; i++)
            e->bestcap[i] = e->cap[i];
    }
    return false;
}

bool mcont(Exec *e, const Cont *k, const char *s)
{
    if (!k)
        return accept(e, s);
    if (k->kind == 0)
        return mseq(e, k->node, s, k->up);
    // A turn that consumed nothing would repeat for ever, so stop expanding.
    // It counts only when it is the only turn -- which is what sets the group
    // in (a*)* against a string with no a, and what stops a trailing empty turn
    // overwriting the one that matched.
    if (s == k->from) {
        if (1 < k->count)
            for (int i = 0; i < e->ncap; i++)
                e->cap[i] = k->save[i];
        return mcont(e, k->up, s);
    }
    return mrep(e, k->node, s, k->count, k->up);
}

bool mrep(Exec *e, int ni, const char *s, int count, const Cont *k)
{
    const Node *n = &nodes(e->re)[ni];

    if (n->max < 0 || count < n->max) {
        regmatch_t save[NCAP];
        Cont kk = { 1, ni, count + 1, s, save, k };

        for (int i = 0; i < e->ncap; i++)
            save[i] = e->cap[i];
        if (mseq(e, n->child, s, &kk))
            return true;
    }
    if (count >= n->min)
        return mcont(e, k, s);
    return false;
}

// How many bytes one turn of a body that is a single LIT, ANY or SET takes
// here, or 0 for none.
int body_width(const Exec *e, const Node *c, const char *s)
{
    unsigned int ch;
    int len;

    switch (c->op) {
    case OP_LIT:
        for (int i = 0; i < c->len; i++)
            if ((unsigned char)s[i] != c->bytes[i])
                return 0;
        return c->len;
    case OP_ANY:
        if (*s == '\0')
            return 0;
        len = decode(s, &ch);
        return e->newline && ch == '\n' ? 0 : len;
    case OP_SET:
        if (*s == '\0')
            return 0;
        len = decode(s, &ch);
        return in_set(e, &sets(e->re)[c->set], ch) ? len : 0;
    }
    return 0;
}

bool push_mark(Exec *e, const char *s)
{
    if (e->nmarks == e->cmarks) {
        int want       = e->cmarks ? e->cmarks * 2 : 256;
        const char **m = (const char **)realloc(e->marks, sizeof(*m) * (size_t)want);
        if (!m)
            return false;
        e->marks  = m;
        e->cmarks = want;
    }
    e->marks[e->nmarks++] = s;
    return true;
}

// A body that is one LIT, ANY or SET has no groups and a fixed shape, so the
// turns can be taken in a loop and backed off down a mark stack. That keeps
// `.*` on a long line off the native stack, where one frame per character
// would trap.
bool mrep_simple(Exec *e, int ni, const char *s, const Cont *k)
{
    const Node *n = &nodes(e->re)[ni];
    const Node *c = &nodes(e->re)[n->child];
    int base      = e->nmarks;
    int count     = 0;

    if (!push_mark(e, s))
        return false;
    while (n->max < 0 || count < n->max) {
        int w = body_width(e, c, s);

        if (w == 0 || --e->budget < 0)
            break;
        s += w;
        count++;
        if (!push_mark(e, s))
            break;
    }

    bool ok = false;
    for (int i = count; i >= n->min; i--) {
        if (mcont(e, k, e->marks[base + i])) {
            ok = true;
            break;
        }
    }
    e->nmarks = base;
    return ok;
}

bool rep_is_simple(const Exec *e, const Node *n)
{
    if (n->child < 0)
        return false;
    const Node *c = &nodes(e->re)[n->child];
    if (c->next >= 0)
        return false;
    return c->op == OP_LIT || c->op == OP_ANY || c->op == OP_SET;
}

bool mone(Exec *e, int ni, const char *s, const Cont *k)
{
    const Node *n = &nodes(e->re)[ni];

    if (--e->budget < 0)
        return false;

    switch (n->op) {
    case OP_LIT:
        // Byte at a time, so a literal longer than what is left never reads
        // past the NUL.
        for (int i = 0; i < n->len; i++)
            if ((unsigned char)s[i] != n->bytes[i])
                return false;
        return mcont(e, k, s + n->len);

    case OP_ANY: {
        unsigned int c;
        if (*s == '\0')
            return false;
        int len = decode(s, &c);
        if (e->newline && c == '\n')
            return false;
        return mcont(e, k, s + len);
    }

    case OP_SET: {
        unsigned int c;
        if (*s == '\0')
            return false;
        int len = decode(s, &c);
        if (!in_set(e, &sets(e->re)[n->set], c))
            return false;
        return mcont(e, k, s + len);
    }

    case OP_BOL:
        if (s == e->base)
            return (e->eflags & REG_NOTBOL) ? false : mcont(e, k, s);
        if (e->newline && s[-1] == '\n')
            return mcont(e, k, s);
        return false;

    case OP_EOL:
        if (*s == '\0')
            return (e->eflags & REG_NOTEOL) ? false : mcont(e, k, s);
        if (e->newline && *s == '\n')
            return mcont(e, k, s);
        return false;

    case OP_GROUP: {
        regoff_t was = n->group < e->ncap ? e->cap[n->group].rm_so : -1;
        if (n->group < e->ncap)
            e->cap[n->group].rm_so = s - e->base;
        Cont kk = { 0, n->close, 0, nullptr, nullptr, k };
        if (mseq(e, n->child, s, &kk))
            return true;
        if (n->group < e->ncap)
            e->cap[n->group].rm_so = was;
        return false;
    }

    case OP_CLOSE: {
        regoff_t was = n->group < e->ncap ? e->cap[n->group].rm_eo : -1;
        if (n->group < e->ncap)
            e->cap[n->group].rm_eo = s - e->base;
        if (mcont(e, k, s))
            return true;
        if (n->group < e->ncap)
            e->cap[n->group].rm_eo = was;
        return false;
    }

    case OP_ALT:
        // A branch is a `next` chain of its own; running out of nodes drops it
        // into k, which is the continuation past the whole alternation.
        for (int b = n->child; b >= 0; b = nodes(e->re)[b].alt)
            if (mseq(e, b, s, k))
                return true;
        return false;

    case OP_REP:
        return rep_is_simple(e, n) ? mrep_simple(e, ni, s, k) : mrep(e, ni, s, 0, k);
    }
    return false;
}

bool mseq(Exec *e, int ni, const char *s, const Cont *k)
{
    if (ni < 0)
        return mcont(e, k, s);
    if (MAX_DEPTH <= e->depth)
        return false;
    Cont kk = { 0, nodes(e->re)[ni].next, 0, nullptr, nullptr, k };
    e->depth++;
    bool r = mone(e, ni, s, &kk);
    e->depth--;
    return r;
}

// --------------------------------------------------------------- prefilter

// Both return whether the thing can match empty -- in which case whatever
// follows it also contributes a first byte. *done turns the filter off, for a
// node that can start with anything.
bool first_bytes(regex_t *re, int ni, int *done);

bool first_of_seq(regex_t *re, int ni, int *done)
{
    for (; ni >= 0; ni = nodes(re)[ni].next) {
        if (!first_bytes(re, ni, done))
            return false;
        if (*done)
            return true;
    }
    return true; // ran out: the sequence matches empty
}

void mark(regex_t *re, unsigned int c)
{
    re->first[(c & 0xFF) >> 3] |= (unsigned char)(1 << (c & 7));
}

bool first_bytes(regex_t *re, int ni, int *done)
{
    const Node *n = &nodes(re)[ni];

    switch (n->op) {
    case OP_LIT:
        mark(re, n->bytes[0]);
        return false;
    case OP_ANY:
    case OP_SET:
        *done = 1;
        return false;
    case OP_BOL:
    case OP_EOL:
    case OP_CLOSE:
        return true;
    case OP_GROUP:
        return first_of_seq(re, n->child, done);
    case OP_ALT: {
        bool empty = false;
        for (int b = n->child; b >= 0; b = nodes(re)[b].alt)
            if (first_of_seq(re, b, done))
                empty = true;
        return empty;
    }
    case OP_REP:
        return first_of_seq(re, n->child, done) || n->min == 0;
    }
    *done = 1;
    return false;
}

} // namespace

// ------------------------------------------------------------------ public

int regcomp(regex_t *preg, const char *pattern, int cflags)
{
    Build b;

    memset(preg, 0, sizeof(*preg));
    preg->cflags = cflags;
    preg->start  = -1;

    b.re     = preg;
    b.p      = pattern;
    b.ngroup = 0;
    b.err    = 0;

    preg->start = parse_alt(&b);
    if (!b.err && *b.p != '\0')
        b.err = REG_BADPAT; // a stray ) or |
    if (b.err) {
        regfree(preg);
        return b.err;
    }
    preg->re_nsub = (size_t)b.ngroup;

    // A pattern that can match empty matches everywhere; no filter can help.
    int done = 0;
    if (first_of_seq(preg, preg->start, &done))
        done = 1;
    preg->have_first = !done;

    // ^ at the head of every branch pins the match to a line start.
    if (preg->start >= 0) {
        const Node *n  = &nodes(preg)[preg->start];
        preg->anchored = n->op == OP_BOL;
    }
    return 0;
}

void regfree(regex_t *preg)
{
    free(preg->prog);
    free(preg->sets);
    free(preg->ranges);
    memset(preg, 0, sizeof(*preg));
    preg->start = -1;
}

int regexec(const regex_t *preg, const char *string, size_t nmatch, regmatch_t pmatch[], int eflags)
{
    regmatch_t cap[NCAP], bestcap[NCAP];
    Exec e;

    e.re      = preg;
    e.base    = string;
    e.eflags  = eflags;
    e.newline = (preg->cflags & REG_NEWLINE) != 0;
    e.depth   = 0;
    e.marks   = nullptr;
    e.nmarks  = 0;
    e.cmarks  = 0;
    e.cap     = cap;
    e.bestcap = bestcap;
    e.ncap    = NCAP;
    // One budget for the call, not per start position: a pathological pattern
    // reports no match rather than hanging, and there is no co_await in here to
    // interrupt it with.
    e.budget = 20000000;

    int rc = REG_NOMATCH;

    for (const char *s = string;; s++) {
        // Skip start positions the pattern cannot begin at.
        if (preg->anchored) {
            bool bol = s == string ? !(eflags & REG_NOTBOL) : e.newline && s[-1] == '\n';
            if (!bol) {
                const char *nl = e.newline ? strchr(s, '\n') : nullptr;
                if (!nl)
                    break;
                s = nl; // the s++ below lands on the start of the next line
                continue;
            }
        } else if (preg->have_first) {
            while (*s != '\0' && !((preg->first[((unsigned char)*s) >> 3] >> (*s & 7)) & 1))
                s++;
        }

        for (int i = 0; i < NCAP; i++) {
            cap[i].rm_so = cap[i].rm_eo = -1;
            bestcap[i].rm_so = bestcap[i].rm_eo = -1;
        }
        e.best = nullptr;

        (void)mseq(&e, preg->start, s, nullptr);

        if (e.best) {
            if (nmatch > 0) {
                pmatch[0].rm_so = s - string;
                pmatch[0].rm_eo = e.best - string;
                for (size_t i = 1; i < nmatch; i++)
                    pmatch[i] = i < NCAP ? bestcap[i] : regmatch_t{ -1, -1 };
            }
            rc = 0;
            break;
        }
        if (*s == '\0')
            break;
    }

    free(e.marks);
    return rc;
}
