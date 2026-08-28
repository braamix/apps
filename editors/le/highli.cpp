/*
 * Copyright (c) 1993-2019 by Alexander V. Lukyanov (lav@yars.free.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "config.h"

#include "lesys.h"
#include "kernel/alloc.h"
#include "edit.h"
#include "lefile.h"
#include "epath.h"
#include "leio.h"
#include "highli.h"
#include "screen.h"
#include "search.h"
#ifdef HAVE_ALLOCA_H
#endif

#ifdef HAVE_SYS_TIMES_H
#endif

int hl_option = 1;
int hl_active = 0;

int hl_lines = 20; // maximum height of highlighted constructs

syntax_hl *syntax_hl::chain;
char *syntax_hl::selector;

syntax_hl::syntax_hl(int color, int mask)
{
    rexp = 0;
    next = sub  = 0;
    this->color = color;
    this->mask  = mask;
    memset(&rexp_c, 0, sizeof(rexp_c));
    memset(&regs, 0, sizeof(regs));
}

syntax_hl::~syntax_hl()
{
    if (rexp) {
        free(rexp);
        regfree(&rexp_c);
    }
    free_chain(sub);
}

void syntax_hl::free_chain(syntax_hl *chain)
{
    for (syntax_hl *r = chain; r; r = chain) {
        chain = r->next;
        heap_delete(r);
    }
}

const char *syntax_hl::set_rexp(const char *nr, bool ignore_case)
{
    if (rexp) {
        free(rexp);
        regfree(&rexp_c);
        memset(&rexp_c, 0, sizeof(rexp_c));
        rexp = 0;
        if (regs.start)
            free(regs.start);
        if (regs.end)
            free(regs.end);
        memset(&regs, 0, sizeof(regs));
    }
    rexp = strdup(nr);
    if (rexp == 0)
        return 0;
    if (ignore_case) {
        map_to_lower_init();
        rexp_c.translate = (RE_TRANSLATE_TYPE)malloc(256);
        memcpy(rexp_c.translate, map_to_lower, 256);
    }
    re_syntax_options = RE_SYNTAX_EMACS | RE_FRUGAL | RE_NO_POSIX_BACKTRACKING | RE_NO_BK_VBAR |
                        RE_NO_BK_PARENS | RE_CONTEXT_INDEP_ANCHORS;
    const char *err   = re_compile_pattern(rexp, strlen(rexp), &rexp_c);
    if (err)
        return err;
    rexp_c.fastmap = (char *)malloc(256);
    re_compile_fastmap(&rexp_c);
    return 0;
}

void c_string_interpret(char *s)
{
    while (*s) {
        if (*s == '\\') {
            switch (s[1]) {
            case ('\0'):
                return;
            case ('\\'):
                break;
            case ('n'):
                *s = '\n';
                break;
            case ('r'):
                *s = '\r';
                break;
            case ('t'):
                *s = '\t';
                break;
#if 0 // \b is word bound in regex (one could type \\b, but it's not convenient)
	 case('b'):
	    *s='\b';
	    break;
#endif
            default:
                s++;
                continue;
            }
            s++;
            memmove(s, s + 1, strlen(s));
        } else
            s++;
    }
}

extern Task<void> fskip(FILE *);

Task<char *> read_regex(FILE *f)
{
    char str[1024];
    char *accum = 0;
    int cont    = 1;
    while (cont) {
        String ln;
        bool got = (co_await f->getline(ln, true)).value_or(false);
        char *s  = str;
        if (got) {
            unsigned n = ln.size() < sizeof(str) - 1 ? ln.size() : sizeof(str) - 1;
            memcpy(str, ln.data(), n);
            str[n] = 0;
        } else
            s = 0;
        if (!s) {
            if (accum)
                break;
            co_return 0;
        }
        cont    = 0;
        int len = strlen(s);
        if (s[len - 1] == '\n') {
            len--;
            if (s[len - 1] == '\r')
                len--;
            if (s[len - 1] == '\\') {
                len--;
                cont = 1;
                for (;;) {
                    int ch = co_await le_getc(f);
                    if (ch == EOF || ch == '\n') {
                        cont = 0;
                        break;
                    }
                    if (ch != ' ' && ch != '\t') {
                        le_ungetc(ch, f);
                        break;
                    }
                }
            }
        } else {
            cont = 1;
        }
        s[len] = 0;
        if (!accum) {
            accum = strdup(str);
            if (!accum)
                co_return 0;
        } else {
            s = (char *)realloc(accum, strlen(accum) + len + 1);
            if (!s) {
                free(accum);
                co_return 0;
            }
            accum = s;
            strcat(accum, str);
        }
    }
    c_string_interpret(accum);
    co_return accum;
}

static Task<FILE *> open_syntax_d(const char *name)
{
    if (name[0] != '/') {
        const char *base_dir = "syntax.d";
        static char fn[LE_PATHMAX];
        unsigned nbytes = sizeof(fn);
        snprintf(fn, nbytes, "%s/.le/%s/%s", HOME, base_dir, name);
        if (co_await le_access(fn, R_OK) == -1)
            snprintf(fn, nbytes, "%s/%s/%s", datadir, base_dir, name);
        name = fn;
    }
    co_return co_await le_fopen(name, false);
}

/* The include guard: a syntax file may pull in another, and one that pulls in
   itself would not end. Upstream kept a set<string>; a fixed table of names is
   the whole of what it did, and it keeps <set> and <string> out of the
   binary. */
enum { SYNTAX_FILES_MAX = 64 };

static char files_loaded[SYNTAX_FILES_MAX][64];
static int files_loaded_n;

/* False when `fn` was already taken, as set::insert's second was. */
static bool remember_file(const char *fn)
{
    for (int i = 0; i < files_loaded_n; i++)
        if (!strcmp(files_loaded[i], fn))
            return false;
    if (files_loaded_n >= SYNTAX_FILES_MAX || strlen(fn) >= sizeof(files_loaded[0]))
        return false;
    strcpy(files_loaded[files_loaded_n++], fn);
    return true;
}
static bool hl_section_match;
static Task<void> ReadSyntaxFile(const char *fn, FILE *f, syntax_hl **chain)
{
    if (!remember_file(fn))
        co_return;

    int ch;
    char str[1024];
    char *s;
    unsigned len;
    int res;
    int color, mask;
    String tok, fld; // what the scanners fill
    const char *bn = le_basename(FileName);
    char *rx;

    for (;;) {
        ch = co_await le_getc(f);
        switch (ch) {
        case (EOF):
            goto end;
        case ('/'):
            if (hl_section_match)
                goto end;
            {
                String ln;
                if (!(co_await f->getline(ln, true)).value_or(false))
                    goto end;
                unsigned n = ln.size() < sizeof(str) - 1 ? ln.size() : sizeof(str) - 1;
                memcpy(str, ln.data(), n);
                str[n] = 0;
                s      = str;
            }
            len = strlen(s);
            if (s[len - 1] == '\n')
                len--;
            if (s[len - 1] == '\r')
                len--;
            s[len]              = 0;
            syntax_hl::selector = strdup(s);
            s                   = strtok(str, "|");
            while (s) {
                if (s[0] == '/') {
                    // it is a regex for file contents
                    if (strlen(s) + (s - str) < len)
                        s[strlen(s)] = '|'; // undo strtok

                    s++;

                    if (!buffer)
                        break;

                    static re_pattern_buffer rexp;
                    re_syntax_options = RE_SYNTAX_EMACS | RE_NO_BK_VBAR | RE_NO_BK_PARENS |
                                        RE_CONTEXT_INDEP_ANCHORS;
                    if (!re_compile_pattern(s, strlen(s), &rexp)) {
                        int s1   = ptr1;
                        int s2   = BufferSize - ptr2;
                        char *p1 = s1 ? buffer : 0;
                        char *p2 = s2 ? buffer + ptr2 : 0;
                        if (p2 && !p1) {
                            p1 = p2;
                            s1 = s2;
                            p2 = 0;
                            s2 = 0;
                        }
                        int pos = -1;
                        if (p1)
                            pos = re_search_2(&rexp, p1, s1, p2, s2, 0, 1024, NULL, 1024);
                        if (pos != -1) {
                            hl_section_match = true;
                            break;
                        }
                    }
                    break;
                }
                if (fnmatch(s, bn, 0) == 0) {
                    hl_section_match = true;
                    break;
                }
                s = strtok(0, "|");
            }
            if (!hl_section_match) {
                free(syntax_hl::selector);
                syntax_hl::selector = 0;
            }
            break;
        case ('c'): {
            if (!hl_section_match) {
                co_await fskip(f);
                continue;
            }
            bool ignore_case = false;
            int c            = co_await le_getc(f);
            if (c == 'i')
                ignore_case = true;
            else
                le_ungetc(c, f);
            {
                Result<i64> c1 = co_await f->scan_i64();
                res            = c1.is_ok() ? 1 : 0;
                color          = c1.value_or(0);
                if (res == 1 && (co_await f->scan_lit(',')).value_or(false)) {
                    Result<i64> m1 = co_await f->scan_i64(0);
                    if (m1.is_ok()) {
                        mask = (int)m1.value();
                        if ((co_await f->scan_lit('=')).value_or(false))
                            res = 2;
                    }
                }
            }
            if (res == 1) {
                mask = 1;
                if (co_await le_getc(f) != '=') {
                    co_await fskip(f);
                    continue;
                }
            } else if (res != 2) {
                co_await fskip(f);
                continue;
            } else {
                mask <<= 1;
            }
            rx = co_await read_regex(f);
            if (!rx)
                goto end;
            syntax_hl *hl   = heap_new<syntax_hl>(color, mask);
            const char *err = hl->set_rexp(rx, ignore_case);
            free(rx);
            rx = 0;
            if (err) {
                ErrMsg(err);
                heap_delete(hl);
                goto end;
            }
            *chain    = hl; // add to chain
            chain     = &hl->next;
            hl_active = 1; // have at least one element
            break;
        }
        case ('h'): {
            Result<i64> n = co_await f->scan_i64();
            if (n.is_ok())
                hl_lines = (int)n.value();
        }
            if (hl_lines < 1)
                hl_lines = 1;
            co_await fskip(f);
            break;
        case ('i'):
            if (!hl_section_match) {
                co_await fskip(f);
                continue;
            }
            /*fallthrought*/
        case ('I'):
            if ((co_await f->scan_lit('=')).value_or(false) &&
                (co_await f->scan_token(tok, 255)).value_or(false) &&
                (snprintf(str, sizeof(str), "%.*s", (int)tok.size(), tok.data()), true)) {
                FILE *i_f = co_await open_syntax_d(str);
                if (i_f) {
                    co_await ReadSyntaxFile(str, i_f, chain);
                    while (*chain) // skip the newly added nodes
                        chain = &chain[0]->next;
                }
            }
            co_await fskip(f);
            break;
        case ('s'): {
            if (!hl_section_match) {
                co_await fskip(f);
                continue;
            }
            ch               = co_await le_getc(f);
            bool ignore_case = false;
            if (ch == 'i') {
                ignore_case = true;
                ch          = co_await le_getc(f);
            }
            if (ch != '(') {
                co_await fskip(f);
                break;
            }
            if ((co_await f->scan_until(fld, ")\n=", 255)).value_or(false) &&
                (snprintf(str, sizeof(str), "%.*s", (int)fld.size(), fld.data()), true)) {
                co_await f->scan_lit(')');
                {
                    Result<i64> m2 = co_await f->scan_i64(0);
                    res            = 0;
                    if (m2.is_ok()) {
                        mask = (int)m2.value();
                        if ((co_await f->scan_lit('=')).value_or(false))
                            res = 1;
                    }
                }
                if (res != 1) {
                    mask = 1;
                    if (co_await le_getc(f) != '=') {
                        co_await fskip(f);
                        continue;
                    }
                } else {
                    mask <<= 1;
                }
                rx = co_await read_regex(f);
                if (!rx)
                    goto end;

                syntax_hl *hl   = heap_new<syntax_hl>(-1, mask);
                const char *err = hl->set_rexp(rx, ignore_case);
                free(rx);
                rx = 0;
                if (err) {
                    ErrMsg(err);
                    heap_delete(hl);
                    goto end;
                }
                *chain    = hl; // add to chain
                chain     = &hl->next;
                hl_active = 1; // have at least one element

                FILE *i_f = co_await open_syntax_d(str);
                if (i_f) {
                    co_await ReadSyntaxFile(str, i_f, &hl->sub);
                }
            } else {
                co_await fskip(f);
            }
            break;
        }
        default:
            co_await fskip(f);
        case ('\n'):
            break;
        }
    }
end:
    co_await le_fclose(f);
}

Task<void> InitHighlight()
{
    files_loaded_n = 0;
    free(syntax_hl::selector);
    syntax_hl::selector = 0;
    syntax_hl::free_chain(syntax_hl::chain);
    syntax_hl::chain = 0;

    hl_active = 0;
    if (!hl_option)
        co_return;

    static const char base_fn[] = "syntax";
    static char fn1[LE_PATHMAX], fn2[LE_PATHMAX], fn3[LE_PATHMAX];
    char *fn;

    snprintf(fn1, sizeof(fn1), "%s/%s", datadir, base_fn);
    snprintf(fn2, sizeof(fn2), "%s/.le/%s", HOME, base_fn);
    snprintf(fn3, sizeof(fn3), ".le.%s", base_fn);

    FILE *f = 0;
    if (!f)
        f = co_await le_fopen(fn = fn3, false);
    if (!f)
        f = co_await le_fopen(fn = fn2, false);
    if (!f)
        f = co_await le_fopen(fn = fn1, false);
    if (!f)
        co_return;
    hl_section_match = false;
    co_await ReadSyntaxFile(fn, f, &syntax_hl::chain);
}

class element {
    static element *pool;
    static element *hunk;
    static int hunk_size;

public:
    int begin, end;
    element *next;
    element *sub;
    byte color;

    static element *New();
    static void Free(element *);
    static void FreeChain(element *);
};

element *element::pool;
element *element::hunk;
int element::hunk_size;

element *element::New()
{
    element *res;
    if (pool) {
        res  = pool;
        pool = pool->next;
        return res;
    }
    if (!hunk || hunk_size == 0)
        hunk = (element *)calloc(hunk_size = 128, sizeof(element));
    res = hunk;
    hunk++;
    hunk_size--;
    return res;
}

void element::Free(element *el)
{
    FreeChain(el->sub);
    el->sub  = 0;
    el->next = pool;
    pool     = el;
}
void element::FreeChain(element *el)
{
    if (!el)
        return;

    element *tmp = el;
    while (tmp->next)
        tmp = tmp->next;
    tmp->next = pool;
    pool      = el;
}

static element *top_els;

#ifdef HAVE_TIMES
static clock_t clock0;
#endif

void syntax_hl::make_els(const char *buf1, int len1, const char *buf2, int len2, int pos0, int ll,
                         syntax_hl *scan, element **elpp0)
{
    element *el;
    element **elpp;

    for (; scan; scan = scan->next) {
        int pos = 0;
        elpp    = elpp0;
        for (;;) {
            if (pos >= ll)
                break;
            pos =
                re_search_2(&scan->rexp_c, buf1, len1, buf2, len2, pos, ll - pos, &scan->regs, ll);
            if (pos == -1) // not found
                break;
            if (pos == -2) // error ?
                break;
            unsigned r;
            unsigned m;
            for (r = 0, m = 1; r < scan->regs.num_regs; r++, m = m << 1) {
                if (scan->regs.start[r] == -1 || scan->regs.start[r] == scan->regs.end[r] ||
                    !(scan->mask & m))
                    continue;

                el        = element::New();
                el->begin = scan->regs.start[r] + pos0;
                el->end   = scan->regs.end[r] + pos0;
                el->color = scan->color;
                el->sub   = 0;

                // insert new element in proper position
                while (*elpp && elpp[0]->begin <= el->begin)
                    elpp = &elpp[0]->next;
                el->next = *elpp;
                *elpp    = el;

                if (scan->sub) {
                    // reduce the buffer so that ^ and $ work properly
                    int sub_ll           = scan->regs.end[r] - scan->regs.start[r];
                    int sub_pos          = scan->regs.start[r];
                    int sub_len1         = len1 > sub_pos ? len1 - sub_pos : 0;
                    const char *sub_buf1 = len1 > sub_pos ? buf1 + sub_pos : 0;
                    int sub_len2         = len1 > sub_pos ? len2 : len2 - (sub_pos - len1);
                    const char *sub_buf2 = len1 > sub_pos ? buf2 : buf2 + (sub_pos - len1);
                    if (sub_len2 < 0) {
                        sub_len2 = 0;
                        sub_buf2 = 0;
                    }
                    if (sub_len1 <= 0 && sub_len2 > 0) {
                        sub_len1 = sub_len2;
                        sub_buf1 = sub_buf2;
                        sub_len2 = 0;
                        sub_buf2 = 0;
                    }
                    if (sub_len1 > sub_ll) {
                        sub_len1 = sub_ll;
                        sub_len2 = 0;
                        sub_buf2 = 0;
                    } else if (sub_len1 + sub_len2 > sub_ll) {
                        sub_len2 = sub_ll - sub_len1;
                    }
                    make_els(sub_buf1, sub_len1, sub_buf2, sub_len2, sub_pos + pos0,
                             sub_len1 + sub_len2, scan->sub, &el->sub);
                }
            }
            pos++;

#ifdef HAVE_TIMES
#ifndef CLOCKS_PER_SEC
#define CLOCKS_PER_SEC CLK_TCK
#endif
            struct tms tms;
            times(&tms);
            clock_t clock1 = tms.tms_utime;
            if (clock1 - clock0 > CLOCKS_PER_SEC / 5)
                break;
#endif
        }
    }
}

static void do_color(element *els, unsigned char *line)
{
    for (element *el = els; el; el = el->next) {
        int end = el->end;
        if (el->sub) {
            do_color(el->sub, line);
        } else {
            for (int i = el->begin; i < end; i++)
                line[i] = el->color;
        }
        // skip overlapping elements
        while (el->next && el->next->begin < end)
            el = el->next;
    }
}

void syntax_hl::attrib_line(const char *buf1, int len1, const char *buf2, int len2,
                            unsigned char *line)
{
    int ll = len1 + len2;
    if (ll == 0)
        return;

    memset(line, '\0', ll);

    // It's too expensive to color such a long text
    if (ll > hl_lines * 1024)
        return;

#ifdef HAVE_TIMES
    struct tms tms;
    times(&tms);
    clock0 = tms.tms_utime;
#endif

    make_els(buf1, len1, buf2, len2, 0, ll, chain, &top_els);
    do_color(top_els, line);

    element::FreeChain(top_els);
    top_els = 0;
}
