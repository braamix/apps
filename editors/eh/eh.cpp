//
// Edit Here
//
// Copyright 2024, 2025 by Anthony C Howe.  All rights reserved.
//
#include <ctype.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <wchar.h>
#include <wctype.h>

#include "globals.h"
#include "proc/io.h"
#include "proc/rt.h"

#ifdef NDEBUG
#define assert(e) ((void)0)
#else
#define assert(e) ((e) ? (void)0 : __builtin_trap())
#endif

#ifndef BUF
#define BUF (64 * 1024)
#endif

#define CTRL_B '\002'
#define CTRL_C '\003'
#define CTRL_F '\006'
#define CTRL_G '\007'
#define CTRL_L '\014'
#define CTRL_R '\022'
#define CTRL_U '\025'
#define CTRL_V '\026'
#define CTRL_W '\027'
#define CTRL_X '\030'
#define CTRL_Z '\032'
#define ESC    '\033'

// Start of undefined and yet unused Unicode values.
// They are not part of an ctype set yet.
// https://symbl.cc/en/unicode-table/
#define BEYOND_HERE_BE_ 0x33480

#define CHANGED      '*'
#define NOCHANGE     ' '
#define MARKS        27
#define MAX_COLS     999
#define TABWIDTH     8
#define TABSTOP(col) (TABWIDTH - ((col) bitand (TABWIDTH - 1)))

#define TOP_LINE 1
#define ROWS     (LINES - TOP_LINE)

#define ALL_CMDS 99

#ifdef NDEBUG
#define OFF_DEC(var) (var - 1) // See bol()
#else                          // NDEBUG
#define OFF_DEC(var) (var - (0 < var))
#endif // NDEBUG

#define MATCHES     10
#define MOTION_CMDS 34
#define MOST_CMDS   68

static int show_all;
static char chg = NOCHANGE;
static int cur_row, cur_col, count, ere_dollar_only, ere_carat_only, search_wrapped, replace_all;
static char *filename, *yank_text, *replace;
static char *buf, *gap, *egap, *ebuf;
static const char ins[] = "INS", cmd[] = "   ", one[] = "ONE", *mode = cmd;
static off_t here, page, epage, match_length, yank_here, yank_length, marks[MARKS], marker = -1,
                                                                                    search_start;
static regex_t ere;

static Task<off_t> getcmd(int);

// Unwind the native stack every so often. A co_await is a call and not a tail
// call here, so getcmd's `do ... while (1 < count--)` -- four awaits a turn for
// a `dd` -- would grow it until 999999dd trapped.
#define EH_YIELD 16

static Task<void> cmd_yield(void)
{
    static int turns;
    if (++turns < EH_YIELD) {
        co_return;
    }
    turns = 0;
    if (Task<Result<void>> t = sleep_for(0)) {
        (void)co_await t;
    }
}

//
// The original prog.c for the IOCCC conformed to the 1536 bytes of
// source size rule.  Not yet sure if this version will still conform,
// though it should for more current versions of the size rule (4096,
// 2503).
//
// 0 <= off_t <= file size	eg. here (the cursor)
//
// buf <= char * <= ebuf	eg. gap (start of the hole)

//
// The following assertions must be maintained.
//
// o  buf <= gap <= egap <= ebuf
// 	If gap == egap then the buffer is full.
//
// o  point = ptr(here) and point < gap or egap <= point
//
// o  page <= here < epage
//
// o  0 <= here <= pos(ebuf) <= BUF
//
//
// Memory representation of the file:
//
// 	low	buf  -->+----------+
// 			|  front   |
// 			| of file  |
// 		gap  -->+----------+<-- character not in file
// 			|   hole   |
// 		egap -->+----------+<-- character in file
// 			|   back   |
// 			| of file  |
// 	high	ebuf -->+----------+<-- character not in file
//
//
// point & gap
//
// The Point is the current cursor position while the Gap is the
// position where the last edit operation took place. The Gap is
// meant to be the cursor but to avoid shuffling characters while
// the cursor moves it is easier to just move a pointer and when
// something serious has to be done then you move the Gap to the
// Point.

//
// Translate a buffer offset into a cursor offset,
// where the gap size has to be factored out.
off_t pos(const char *s)
{
    assert(buf <= s and s <= ebuf);
    // Factor out the implied jumps.
    // 	return s-buf - (s < egap ? 0 : egap-gap);
    return s - buf - (s >= egap) * (egap - gap);
}

//
// Translate a cursor offset into a buffer offset,
// where the gap size has to be factored in.
char *ptr(const off_t cur)
{
    assert(0 <= cur and cur <= pos(ebuf));
    // Factor out the implied jumps.
    // 	return buf+cur + (buf+cur < gap ? 0 : egap-gap);
    return buf + cur + (buf + cur >= gap) * (egap - gap);
}

// Alternative to mblen() that assumes ch is a the start byte of
// UTF-8 multibyte character.  Returns the multibyte length; if
// ch is a continuation byte, it returns 1, so a scanner will
// move across a partial multibyte character to the start of the
// next sequence.
//
// There is nothing wrong with this source code.
// Kono sōsukōdo ni wa nani mo mondai wa arimasen.
// このソースコードには何も問題はありません
//
// https://en.wikipedia.org/wiki/UTF-8
int mblength(const int ch)
{
    //	return (1+(ch > 193)+(ch > 223)+(ch > 239)) * (ch < 128 or (193 < ch and ch < 245));
    assert(0 <= ch and ch < 256);
    return 1 + (ch > 193) + (ch > 223) + (ch > 239);
}

off_t nextch(off_t cur)
{
    const off_t eof = pos(ebuf);
    // Advance to next UTF-8 start byte.  Do not read past eof.
    if (cur < eof)
        while (++cur < eof and (192 bitand *ptr(cur)) == 128) {
            ;
        }
    return cur;
}

off_t prevch(off_t cur)
{
    // Find UTF-8 start byte skipping continuation bytes.
    while (0 < cur and (192 bitand *ptr(--cur)) == 128) {
        ;
    }
    assert(0 <= cur);
    return cur;
}

__attribute__((noinline)) void movegap(const off_t cur)
{
    assert(0 <= cur and cur <= pos(ebuf));
#ifdef FAST_MOVE
    ssize_t len = gap - buf - cur;
    if (len < 0) {
        // Shift data down, moving gap up to cursor.
        (void)memcpy(gap, egap, -len);
        egap -= len;
        gap -= len;
    } else {
        // Shift data up, moving gap down to cursor.
        gap -= len;
        egap -= len;
        (void)memmove(egap, gap, len);
    }
#else  // FAST_MOVE
    char *p = ptr(cur);
    while (p < gap) {
        *--egap = *--gap;
    }
    while (egap < p) {
        *gap++ = *egap++;
    }
#endif // FAST_MOVE
    assert(buf <= gap and gap <= egap and egap <= ebuf);
}

__attribute__((noinline)) void growgap(const size_t min)
{
    char *xbuf;
    assert(buf <= gap and gap <= egap and egap <= ebuf);
    // The gap is used for field input and should be at least as
    // large as the screen width.
    if ((size_t)(egap - gap) < min) {
        // Remember current gap location.
        off_t xhere      = pos(egap);
        ptrdiff_t buflen = ebuf - buf;
        // Append the new space to the end of the gap.
        movegap(pos(ebuf));
        off_t gap_off = pos(gap);
        if (NULL == (xbuf = (char *)realloc(buf, buflen + BUF))) {
            // Bugger.  Don't exit, allow user to write file.
            (void)beep();
            return;
        }
        // Ensure space for a sentinel byte.
        egap = ebuf = xbuf + buflen + BUF - 1;
        gap         = xbuf + gap_off;
        // Asssign a sentinel byte.
        *ebuf = '\0';
        buf   = xbuf;
        // Restore gap's previous location.
        movegap(xhere);
    }
}

typedef enum { UNDO_DEL, UNDO_INS, UNDO_DEL_A, UNDO_INS_B } UndoOp;

struct ubuf {
    struct ubuf *next;
    UndoOp op;
    bool paired;
    off_t off;
    size_t size;
    char buf[];
};

struct ubuf *undo_list, *redo_list;

// See `ex` Mark which describes when the previous-mark (` or ') is set.
// https://pubs.opengroup.org/onlinepubs/9799919799/utilities/ex.html#tag_20_40_13_25
#define SET_PREVIOUS_MARK(x) marks[0] = (x)

void adjmarks(const off_t n)
{
    off_t p = pos(egap);
    for (int i = 0; 0 not_eq n and i < MARKS; i++) {
        if (p < marks[i]) {
            marks[i] += n;
        }
    }
}

void undo_free(struct ubuf *obj)
{
    struct ubuf *next;
    for (; obj not_eq NULL; obj = next) {
        next = obj->next;
        free(obj);
    }
}

void undo_save(const UndoOp op, const off_t off, const char *const loc, const size_t size)
{
    struct ubuf *obj;
    undo_free(redo_list);
    redo_list = NULL;
    // Append new undo.  We cannot skip saving a zero length
    // string, because of paired undo objects we need both.
    if ((obj = (struct ubuf *)realloc(NULL, sizeof(*obj) + size)) not_eq NULL) {
        obj->op     = (UndoOp)(op bitand 1);
        obj->paired = UNDO_INS < op;
        obj->off    = off;
        obj->size   = size;
        (void)memcpy(obj->buf, loc, size);
        obj->next = undo_list;
        undo_list = obj;
    }
}

void undo_redo(const UndoOp op, const struct ubuf *const obj)
{
    movegap(obj->off);
    if (op == UNDO_INS) {
        // Insert.
        growgap(obj->size);
        egap -= obj->size;
        (void)memcpy(egap, obj->buf, obj->size);
        adjmarks(obj->size);
    } else {
        // Delete.
        //
        // The cast is the port's: size_t is 32 bits and off_t 64, so
        // `-obj->size` zero-extends to 4294967286 instead of -10. Same
        // width on LP64, which is why upstream needs no cast.
        adjmarks(-(off_t)obj->size);
        egap += obj->size;
    }
    here  = pos(egap);
    epage = here + 1;
}

void undo_move(struct ubuf **from, struct ubuf **to)
{
    struct ubuf *tmp = *from;
    *from            = (*from)->next;
    tmp->next        = *to;
    *to              = tmp;
}

void undo(void)
{
    if (undo_list not_eq NULL) {
        undo_redo((UndoOp)(not undo_list->op), undo_list);
        if (undo_list->next not_eq NULL and undo_list->paired) {
            undo_move(&undo_list, &redo_list);
            undo_redo((UndoOp)(not undo_list->op), undo_list);
        }
        undo_move(&undo_list, &redo_list);
        if (undo_list == NULL) {
            chg = NOCHANGE;
        }
    }
}

void redo(void)
{
    if (redo_list not_eq NULL) {
        undo_redo(redo_list->op, redo_list);
        if (redo_list->next not_eq NULL and redo_list->paired) {
            undo_move(&redo_list, &undo_list);
            undo_redo(redo_list->op, redo_list);
        }
        undo_move(&redo_list, &undo_list);
        if (redo_list == NULL) {
            chg = CHANGED;
        }
    }
}

// Upstream's CTRL_Z loop raised SIGTSTP. There is no job control to stop for,
// so this is now only the name every call site knows getch() by.
Task<int> getsigch(void)
{
    co_return co_await getch();
}

// Return the physical line containing a buffer offset.
unsigned long line_number(off_t cur)
{
    unsigned long line = 1;
    while (0 < cur) {
        if (*ptr(--cur) == '\n') {
            line++;
        }
    }
    return line;
}

void ungetstr(const char *str)
{
    ssize_t n = strlen(str);
    assert(n < COLS and COLS <= egap - gap);
    while (0 < n and 0 == ungetch(str[--n])) {
        ;
    }
}

// display()'s printability test. The kit's wcwidth() is Markus Kuhn's, which
// answers 1 for a surrogate or a noncharacter; NetBSD's answers -1, and
// upstream's goldens draw those as the invalid-byte '~'. The width itself is
// still wcwidth()'s.
int printable(const char32_t wc)
{
    if (0x10FFFF < wc or (0xD800 <= wc and wc <= 0xDFFF)) {
        return 0;
    }
    if ((0xFDD0 <= wc and wc <= 0xFDEF) or (wc bitand 0xFFFE) == 0xFFFE) {
        return 0;
    }
    return 0 < wcwidth(wc) or iswspace(wc);
}

int charwidth(const char *s, int col)
{
    char32_t wc;
    mbstate_t mbs = {}; // Do NOT track state.
    ssize_t mbl   = mbrtowc((wchar_t *)&wc, s, 4, &mbs);
    // Invalid MB sequence or MB continuation byte, count
    // width one ASCII character (see display() '~' place
    // holder), or a control count one highlighted ASCII
    // character, otherwise TAB offset or character width.
    // Upstream ended `: col`, the width wcwidth() reported. The grid is one
    // Cell per rune, so a wide character occupies one column here and a 2
    // would drift the cursor along a CJK line.
    return mbl <= 0 or (127 < *s and *s < 194) ? 1
           : (wc == '\t' and not show_all)     ? TABSTOP(col)
                                               : 1;
}

//
// Return the physical BOL or BOF containing cur.
off_t bol(const off_t off)
{
    // Catch any bol(-1), which used to happen. 2026-09-02
    // Use OFF_DEC(x) for known safe instances.
    assert(0 <= off);
    // Clip out of bounds index.  There are some calls that do
    // not handle bounds checking, expecting `bol()` to handle.
    // Typically these are instance of moving to the end of
    // the previous line, which might mean going off the BOF,
    // eg. `bol(here-1)` where `here` is already at BOF.  Ideally
    // this should be `bol(here-(0 < here))`, but these extra
    // checks be handled in `bol()` allowing the simpler version
    // and reduce code size.  (I knew I did this for a clear
    // reason).
    //
    // Of MORE concern is when an loop index or some other position
    // calculation went out of bounds.  Was it intentional, in
    // which case should have a comment saying so, or not in which
    // case can it be fixed.
    off_t cur = off * (0 < off);
    while (0 < cur and *ptr(--cur) not_eq '\n') {
        ;
    }
    // End of previous physical line or BOF.
    assert(*ptr(cur) == '\n' or cur == 0);
    // Add one if stopped at newline of previous line
    // AND we started above zero.
    //
    // BOL to BOL should not move
    //
    // |⸺⸺⸺⸺⸺⸺␊⸺⸺⸺⸺⸺⸺␊⸺⸺⸺⸺⸺
    // 　　　　　　　 ba
    //
    // MOL to BOL
    //
    // |⸺⸺⸺⸺⸺⸺␊⸺⸺⸺⸺⸺⸺␊⸺⸺⸺⸺⸺
    // 　　　　　　　 b 　　　a
    //
    // EOL to BOL
    //
    // |⸺⸺⸺⸺⸺⸺␊⸺⸺⸺⸺⸺⸺␊⸺⸺⸺⸺⸺
    // 　　　 　　　　b　　　　　 a
    //
    // MOL to BOF
    //
    // |⸺⸺⸺⸺⸺⸺␊⸺⸺⸺⸺⸺⸺␊⸺⸺⸺⸺⸺
    //  b　　 a
    //
    // MOL to BOL with an empty first line (CB-29)
    //
    // |␊⸺⸺⸺⸺⸺␊⸺⸺⸺⸺⸺⸺␊⸺⸺⸺⸺⸺
    //    b　　 a
    //
    // EOL to BOF with an empty first line
    //
    // |␊⸺⸺⸺⸺⸺␊⸺⸺⸺⸺⸺⸺␊⸺⸺⸺⸺⸺
    //  ba
    return cur + (0 < off and *ptr(cur) == '\n');
}

//
// Return offset of column position, newline (EOL), or EOF; otherwise
// if maxcol is way larger than the terminal width, eg. 999 (assumes
// that you're not using an IMAX theatre screen for your terminal),
// just EOL or EOF.
__attribute__((noinline)) off_t col_or_eol(off_t cur, int col, const int maxcol)
{
    char *p;
    while (col < maxcol and (p = ptr(cur)) < ebuf and *p not_eq '\n') {
        col += charwidth(p, col);
        cur = nextch(cur);
    }
    assert(0 <= cur and cur <= pos(ebuf));
    return cur;
}

//
// Return offset to start of logical line containing offset.
off_t row_start(off_t cur, const off_t offset)
{
    char *p;
    int col    = 0;
    off_t mark = cur;
    assert(/* 0 <= cur and cur <= offset and*/ offset <= pos(ebuf));
    // Clip, the way bol() does and for the same reason: an offset past the end
    // never terminates the walk below, because nextch() stops advancing at EOF
    // while ptr() stays inside the buffer. Upstream asserted it instead, and
    // the assert is compiled out of a release build.
    const off_t eof = pos(ebuf);
    const off_t lim = offset < eof ? offset : eof;
    while (cur < lim and (p = ptr(cur = nextch(cur))) < ebuf) {
        col += charwidth(p, col);
        if (COLS <= col) {
            mark = cur;
            col  = 0;
        }
    }
    assert(0 <= mark and mark <= cur);
    return mark;
}

//
// Return the previous logical BOL or BOF.
off_t prevline(const off_t cur)
{
    off_t s = bol(cur);          // Current physical line.
    off_t t = row_start(s, cur); // Current logical line.
    if (s < t) {
        // Within current physical line, find previous logical line.
        return row_start(s, t - 1);
    }
    // Previous physical line, find last logical line.
    return row_start(bol(OFF_DEC(s)), s - 1);
}

//
// Return the next logical EOL or EOF.
off_t nextline(const off_t cur)
{
    return nextch(col_or_eol(cur, cur_col, COLS - 1));
}

__attribute__((noinline)) void display(void)
{
    char *p;
    int i, j;
    off_t from, to;
    const off_t eof = pos(ebuf);
    // Find the top of the display page.
    if (here < page) {
        // Scroll up one logical line or goto physical line.
        page = row_start(bol(here), here);
    } else if (epage <= here and here < nextline(epage)) {
        // Scroll down one logical line.
        page = nextline(page);
    } else if (epage <= here) {
        // Find top of page from here.  Avoid an unnecessary
        // screen redraw when the EOF marker is displayed (mid-
        // screen) by remembering where the previous page frame
        // started.
        epage = page;
        for (page = here, i = ROWS - (here == eof); 0 < --i and epage < page;) {
            page = prevline(page);
        }
        // When find the top line of the page over shoots the
        // previous frame, restore the previous frame.  This
        // avoid an undesired scroll back of one line.
        if (page < epage) {
            page = epage;
        }
    } // Else still within page bounds, update cursor.
    eh_erase();
    {
        // off_t is 64 bits here and long is 32, so %ld would read four
        // bytes of an eight-byte vararg and shift everything after it.
        char st[256];
        int n = snprintf(st, sizeof(st), "%s %ldB L%lu %c %s", filename, (long)eof,
                         line_number(here), chg, mode);
        if (n < 0) {
            n = 0;
        } else if (n > (int)sizeof(st) - 1) {
            n = (int)sizeof(st) - 1;
        }
        // The status line and its tail are reverse to the right edge.
        eh_fill(0, eh_put(0, 0, st, n, ATTR_REVERSE), ATTR_REVERSE);
    }
    if (marker < 0) {
        from = to = marker;
    } else if (here < marker) {
        from = here;
        to   = marker;
    } else {
        from = marker;
        to   = here;
    }
    for (i = TOP_LINE, j = 0, epage = page; i < LINES;) {
        if (here == epage) {
            cur_row = i;
            cur_col = j;
        }
        if (ebuf <= (p = ptr(epage))) {
            break;
        }
        bool is_ctrl = iscntrl(*p) and (show_all or (*p not_eq '\t' and *p not_eq '\n'));
        // Curses' standout() was sticky and standend() cleared it each turn;
        // every cell carries its own attribute now.
        u8 at = ((from <= epage and epage < to) or is_ctrl) ? ATTR_REVERSE : 0;
        // A multibyte character never straddles the gap,
        // assumes the gap moves by character, not by byte.
        // See also nextch() and prevch().
        int mbl;
        if (is_ctrl) {
            // Display control characters as a single byte
            // highlighted upper case letter (instead of two
            // byte ^X).
            eh_put_rune(i, j, (char32_t)(*p + '@'), at);
            mbl = 1;
        } else {
            char32_t wc;
            mbstate_t mbs = {}; // Do NOT track state.
            mbl           = mbrtowc((wchar_t *)&wc, p, 4, &mbs);
            if (0 < mbl and printable(wc)) {
                // The writer takes UTF-8 and expands a TAB, which is what
                // upstream used the addnstr() family for.
                (void)eh_put(i, j, p, mbl, at);
            } else {
                // Place holder for non-printable, invalid MB
                // sequence or MB continuation byte; each byte
                // of the invalid sequence becomes inverse '~'.
                eh_put_rune(i, j, U'~', ATTR_REVERSE);
                // Force byte size.
                mbl = 1;
            }
        }
        epage += mbl;
        // Handle tab expansion ourselves.  Historical
        // Curses addch() would advance to the next
        // tabstop (a multiple of 8, eg. 0, 8, 16, ...).
        // See SUS Curses Issue 7 section 3.4.3.
        j += charwidth(p, j);
        if (*p == '\n' or COLS <= j) {
            j = 0;
            i++;
        }
    }
    assert(page <= here and here <= epage);
    if (i++ < LINES) {
        (void)eh_put(i, 0, "^D", 2, ATTR_REVERSE);
    }
    eh_cursor(cur_row, cur_col);
}

void redraw(void)
{
    eh_erase();
    count = 0;
}

void left(void)
{
    here = prevch(here);
}

void right(void)
{
    here = nextch(here);
}

//
// Move up one logical line.
void up(void)
{
    here = col_or_eol(prevline(here), 0, cur_col);
}

//
// Move down one logical line.
void down(void)
{
    here = col_or_eol(nextline(here), 0, cur_col);
}

// Insert mode command to perform a single command mode operation,
// not bound to an insert mode control key.
Task<void> gold(void)
{
    mode = one;
    display();
    (void)co_await getcmd(MOST_CMDS);
    (void)ungetch('i');
}

// Down one physical line.  The cursor moves to column 1.
// Physical line column is not tracked.
void lnplus(void)
{
    here = col_or_eol(here, 0, MAX_COLS);
    here += *ptr(here) == '\n';
}

// Up one physical line.  The cursor moves to column 1.
// Physical line column is not tracked.
void lnminus(void)
{
    here = bol(here);
    here = bol(OFF_DEC(here));
}

//
// Beginning of physical line.
void lnbegin(void)
{
    here    = bol(here);
    cur_col = 0;
}

//
// End of physical line.
void lnend(void)
{
    here = col_or_eol(here, 0, MAX_COLS);
}

static const char brackets[] = "()[]{}<>";

void pairs(void)
{
    int ch, level = 0;
    off_t mark  = here;
    ptrdiff_t b = strchr(brackets, *ptr(mark)) - brackets;
    if (b < 0) {
        return;
    }
    if ((b bitand 1) == 1) {
        // Find previous.
        for (mark--; 0 < mark; mark--) {
            ch = *ptr(mark);
            if (ch == brackets[b]) {
                level++;
            } else if (ch == brackets[b - 1]) {
                if (level == 0) {
                    here = mark;
                    break;
                }
                level--;
            }
        }
    } else {
        // Find next.
        const off_t eof = pos(ebuf);
        for (mark++; mark < eof; mark++) {
            ch = *ptr(mark);
            if (ch == brackets[b]) {
                level++;
            } else if (ch == brackets[b + 1]) {
                if (level == 0) {
                    here = mark;
                    break;
                }
                level--;
            }
        }
    }
}

//
// Goto column of physical line.
void column(void)
{
    here  = col_or_eol(bol(here), 0, count - 1);
    count = 0;
}

void pgtop(void)
{
    SET_PREVIOUS_MARK(here);
    here = page;
}

void pgbottom(void)
{
    SET_PREVIOUS_MARK(here);
    here = row_start(bol(OFF_DEC(epage)), epage - 1);
}

//
// Page down logical lines.
void pgdown(void)
{
    int i;
    // Maintain cursor row within the next page frame.
    for (here = epage, i = TOP_LINE; i < cur_row; i++) {
        here = nextline(here);
    }
    // Maintain cursor column on logical line, if possible.
    here = col_or_eol(here, 0, cur_col);
    // Page down advances to the next page or remains as-is because
    // of a short page at EOF, ie. using short.txt J should not redraw
    // the screen, just move the cursor to EOF.
    page = here < pos(ebuf) ? epage : page;
    // Maintain cursor row within the page by adjusting the frame.
    for (epage = here; i < LINES; i++) {
        epage = nextline(epage);
    }
}

//
// Page up logical lines.
void pgup(void)
{
    // Page up N logical lines.
    for (int i = ROWS; 0 < i--;) {
        here = prevline(here);
    }
    // Maintain cursor row within the page by adjusting the frame.
    for (page = here; TOP_LINE < cur_row--;) {
        page = prevline(page);
    }
    // Maintain cursor column on logical line.
    here = col_or_eol(here, 0, cur_col);
}

char32_t c32at(const off_t cur)
{
    char *s       = ptr(cur);
    mbstate_t mbs = {};           // Do NOT track state.
    char32_t wc   = (char32_t)*s; // Assume ASCI or invalid MB value.
    (void)mbrtowc((wchar_t *)&wc, s, 4, &mbs);
    return wc;
}

int isword(wint_t ch)
{
    // Include undefined and unused Unicode as word
    // characters for the purpose of movement.
    // https://symbl.cc/en/unicode-table/
    return iswalnum(ch) or ch == '_' or BEYOND_HERE_BE_ <= ch;
}

void wleft(void)
{
    while (0 < here and iswspace(c32at(prevch(here)))) {
        here = prevch(here);
    }
    if (0 < here and isword(c32at(prevch(here)))) {
        // Move backwards to start of previous word.
        while (0 < here and isword(c32at(prevch(here)))) {
            here = prevch(here);
        }
    } else {
        // Move backwards to end of previous word.
        while (0 < here and iswpunct(c32at(prevch(here)))) {
            here = prevch(here);
        }
    }
}

//
// See https://pubs.opengroup.org/onlinepubs/9799919799/utilities/vi.html
// definition of "word" (not "bigword").
void wright(void)
{
    const off_t eof = pos(ebuf);
    if (here < eof and isword(c32at(here))) {
        // Move forwards to end of current word.
        while (here < eof and isword(c32at(here))) {
            here = nextch(here);
        }
        // Skip blanks and newlines.
        // ACH: SUS says end-of-line are word breaks, so in
        // theory word-right treats end-of-line as its own
        // word.  nvi and vim do not.
        while (here < eof and iswblank(c32at(here))) {
            here = nextch(here);
        }
    } else if (here < eof and iswpunct(c32at(here))) {
        // Move forwards to start of next word.
        while (here < eof and iswpunct(c32at(here))) {
            here = nextch(here);
        }
        // Skip blanks and newlines.
        // ACH: SUS standard says begin/end-of-line are word
        // breaks, so in theory word-right treats end-of-line
        // word.  nvi and vim do not.
        while (here < eof and iswblank(c32at(here))) {
            here = nextch(here);
        }
    } else {
        // Skip over all whitespace.
        while (here < eof and iswspace(c32at(here))) {
            here = nextch(here);
        }
    }
}

void lngoto(void)
{
    SET_PREVIOUS_MARK(here);
    // Count physical lines, just as ed, grep, or wc would.
    const off_t eof = pos(ebuf);
    for (here = eof * (count == 0); here < eof and 1 < count; count--) {
        // Next physical line.
        here = col_or_eol(here, 0, MAX_COLS);
        here += here < eof;
    }
    // Set page to eof if beyond page end to force display() to
    // reframe the page with target line at top.
    page = eof;
}

Task<void> insert(void)
{
    int ch, mbl;
    const off_t eof = pos(ebuf);
    movegap(here);
    mode = ins;
    display();
    char *start = gap;
    // Upstream ran with timeout(100) and repainted only the first three
    // keystrokes of a burst, the ERR arm resetting the counter.  There is no
    // non-blocking key read here, so both go and every keystroke repaints --
    // which costs only the cells that changed.
    while ((ch = co_await getsigch()) not_eq CTRL_C and ch not_eq ESC) {
        switch (ch) {
        case KEY_LEFT:
            ungetstr("\033hi");
            continue;
        case KEY_RIGHT:
            ungetstr("\033li");
            continue;
        case KEY_DOWN:
            ungetstr("\033ji");
            continue;
        case KEY_UP:
            ungetstr("\033ki");
            continue;
        case CTRL_B:
            ungetstr("\033Ki");
            continue;
        case CTRL_F:
            ungetstr("\033Ji");
            continue;
        //
        // Reasons for using CTRL+G:
        //
        //  - `CTRL+G` is mnemonic.
        //  - `CTRL+G` is ASCII BEL, which no one really wants to insert into text.
        //  - Seen a CP/M editor (maybe DOS) that had a similar Gold Key `CTRL+G`.
        //  - Reluctant to use other control keys, cause might need them for more
        //    frequent commands, such as word motion, undo/redo, anchor/yank/put.
        //  - Gold Key history : https://en.wikipedia.org/wiki/Gold_key_(DEC)
        case CTRL_G:
            here = pos(egap);
            // Transition to gold() for single command input.
            ungetstr("\033\007");
            continue;
        case '\b':
        case KEY_BACKSPACE:
            // Move to previous (multibyte) character.
            while (eof < pos(ebuf) and (192 bitand *--gap) == 128) {
                ;
            }
            break;
        case CTRL_U:
            // Kill line.
            gap = start;
            break;
        case CTRL_W:
            // Erase previous bigword.
            while (start < gap and isspace(gap[-1])) {
                gap--;
            }
            while (start < gap and !isspace(gap[-1])) {
                gap--;
            }
            break;
        case CTRL_V:
            // Insert literal character. Upstream's nonl()/nl() pair around this
            // read has nothing to say: there is no line discipline here and the
            // key arrives as typed.
            ch = co_await getch();
            // @fallthrough@
        default:
            if (ch < 0 || 255 < ch) {
                // Ignore other KEY_s.
                continue;
            }
            // @fallthrough@
            // Upstream had a `case KEY_BTAB:` here, below the guard
            // above, so a real one reached mblength() with a value its
            // own assert forbids.  input.cpp decodes Shift-Tab to a tab.
            // Input (multibyte) character.
            mbl = mblength(ch);
            // Read the remainder of a multibyte
            // character BEFORE updating the display.
            growgap(mbl);
            do {
                *gap++ = ch;
                epage++;
            } while (0 < --mbl and (ch = co_await getch()) not_eq ERR);
        }
        here = pos(egap);
        chg  = CHANGED;
        display();
    }
    mode      = cmd;
    off_t len = pos(ebuf) - eof;
    if (0 < len) {
        undo_save(UNDO_INS, here - len, gap - len, len);
        adjmarks(len);
    }
    // Not repeatable yet.
    count = 0;
}

// Yank current selection or by motion.
Task<void> yanky(void)
{
    off_t mark = marker;
    if (marker < 0) {
        mark = co_await getcmd(MOTION_CMDS);
    }
    yank_here = here;
    if (mark < yank_here) {
        // Swap.
        mark xor_eq yank_here;
        yank_here xor_eq mark;
        mark xor_eq yank_here;
    }
    assert(yank_here <= mark);
    // SUS 2018 vi(1) `y` yank sets the cursor on the last column
    // of the first character of the yanked region; results in yank
    // forward leaving the cursor as-is, while yank back moves the
    // cursor back.
    //
    // The alternatives are to either always update cursor according
    // to the motion (better reflects GUI editors, select then copy
    // or cut, and better visual feedback) or not update cursor at
    // all (functional but lacks visual feedback ie. did it work).
    free(yank_text);
    movegap(yank_here);
    yank_text = strndup(egap, yank_length = mark - yank_here);
}

// Delete current selection or by motion.
Task<void> deld(void)
{
    co_await yanky();
    marker = -1;
    chg    = CHANGED;
    undo_save(UNDO_DEL, yank_here, yank_text, yank_length);
    egap += yank_length;
    here = pos(egap);
    adjmarks(-yank_length);
}

// Yank current line.
void yankY(void)
{
    marker = -1;
    ungetstr("^yj");
}

// Delete character right (under) the cursor; same as `dl`.
Task<void> delx(void)
{
    (void)ungetch('l');
    co_await deld();
}

// Delete character left the cursor; same as `dh`.
Task<void> delX(void)
{
    (void)ungetch('h');
    co_await deld();
}

// Delete from cursor to end of line, same as `d$`.
Task<void> delD(void)
{
    (void)ungetch('$');
    co_await deld();
}

// Change current selection or by motion.
Task<void> chgc(void)
{
    co_await deld();
    // Convert delete to paired delete-insert.
    undo_list->paired = true;
    co_await insert();
    // Convert insert to paired delete-insert.
    undo_list->paired = true;
}

// Change current selection or from cursor to end of line, same as `c$`.
Task<void> chgC(void)
{
    (void)ungetch('$');
    co_await chgc();
}

void insertI(void)
{
    ungetstr("^i");
}

// Insert right of the cursor.
void append(void)
{
    ungetstr("li");
}

// Insert at the end of the line, same as `$i`.
void appendA(void)
{
    ungetstr("$i");
}

// Open a new empty line below the current cursor line.
void openo(void)
{
    ungetstr("$a\n\033hi");
}

// Open a new empty line above the current cursor line.
void openO(void)
{
    ungetstr("^i\n\033ki");
}

// Find end of word.
void wend(void)
{
    const off_t eof = pos(ebuf);
    while (here < eof and iswspace(c32at(here = nextch(here)))) {
        ;
    }
    if (here < eof and isword(c32at(here))) {
        // Move forwards to end of current word.
        while (here < eof and isword(c32at(here))) {
            here = nextch(here);
        }
    } else {
        // Move forwards to start of next word.
        while (here < eof and iswpunct(c32at(here))) {
            here = nextch(here);
        }
    }
    here = prevch(here);
}

void paraup(void)
{
    int nl = 0;
    while (0 < here and *ptr(here - 1) == '\n') {
        here--;
    }
    while (0 < here and nl < 2) {
        // Count consecutive newlines or reset to zero.
        nl = (nl + 1) * (*ptr(--here) == '\n');
    }
    here += 0 < here;
}

void paradown(void)
{
    int nl          = 0;
    const off_t eof = pos(ebuf);
    while (here < eof and *ptr(++here) == '\n') {
        ;
    }
    while (here < eof and nl < 2) {
        // Count consecutive newlines or reset to zero.
        nl = (nl + 1) * (*ptr(++here) == '\n');
    }
}

void pasteP(void)
{
    if (0 < yank_length) {
        movegap(here);
        growgap(COLS + (count + (count == 0)) * yank_length);
        undo_save(UNDO_INS, here, yank_text, yank_length);
        adjmarks(yank_length);
        chg = CHANGED;
        (void)memcpy(gap, yank_text, yank_length);
        gap += yank_length;
        // SUS 2018 vi(1) `P` paste-before unnamed buffer leaves
        // the cursor on the last column of the last character.
        // Instead keep the cursor on the same character before
        // the paste, which allows for clean repeated pasting,
        // eg. `PP` or `2P` XX_YY => XXpastepaste_YY, where '_'
        // is the cursor.
        here = pos(egap);
        // Force display() to reframe, ie. 1GdGP fails.
        epage = here + 1;
    }
}

void pastep(void)
{
    // SUS 2018 vi(1) `p` paste-after unnamed buffer leaves the
    // cursor on the last column of the last character.  Allows
    // for common transpose combo `xp`, eg. te_h => th_e.  Also
    // `pp` or `2p` eg. 1_23 => 12pastepaste_3, where '_' is the
    // cursor.
    ungetstr("lPh");
}

Task<void> setmark(void)
{
    // ASCII characters ` a..z are the allowed marks.
    int i = co_await getch() - '`';
    if (0 <= i and i < MARKS) {
        marks[i] = here;
    }
}

Task<void> gomark(void)
{
    // ASCII characters ` a..z are the allowed marks.
    int i = co_await getch() - '`';
    if (0 <= i and i < MARKS) {
        off_t j = marks[0];
        SET_PREVIOUS_MARK(here);
        here = 0 < i ? marks[i] : j;
    }
    count = 0;
}

Task<void> lnmark(void)
{
    int ch = co_await getch();
    if (ch == '\'') {
        ch = '`';
    }
    (void)ungetch(ch);
    co_await gomark();
    lnbegin();
}

Task<void> prompt(const char *const msg, const char *str)
{
    // The prompt row is underlined and, unlike the status line it replaces,
    // not reversed.
    size_t len = strlen(msg);
    eh_fill(0, eh_put(0, 0, msg, (int)len, ATTR_UNDERLINE), ATTR_UNDERLINE);
    // Limit the input to a phrase no wider than COLS, not lines.
    if (str == NULL or strchr(str, '\n') not_eq NULL) {
        str = "";
    }
    growgap(COLS);
    // Prime the input with initial input.
    ungetstr(str);
    // Upstream noted that NetBSD's Curses got erase right and kill wrong.
    // input.cpp's reader is ours, so ^U works.
    (void)co_await mvgetnstr(0, (int)len, gap, COLS, ATTR_UNDERLINE);
}

Task<int> filewrite(const char *const fn)
{
    Error err = Error::Io;
    // A Task whose frame did not allocate panics when awaited, so every call
    // here is guarded the way spawn() below is.
    Result<i32> fd = Err(Error::NoMemory);
    if (Task<Result<i32>> t = open_at(Str(fn), SYS_O_WRITE | SYS_O_CREATE | SYS_O_TRUNC)) {
        fd = co_await t;
    }
    if (not fd.is_err()) {
        const off_t eof = pos(ebuf);
        movegap(eof);
        // write_all() retries a short write itself, so upstream's loop is one
        // call and a failure means nothing was written.
        Result<void> w = Err(Error::NoMemory);
        if (Task<Result<void>> t = write_all((u32)fd.value(), Str(buf, (usize)eof))) {
            w = co_await t;
        }
        if (Task<void> t = close_fd((u32)fd.value())) {
            co_await t;
        }
        if (not w.is_err()) {
            co_return 0;
        }
        err = w.error();
    } else {
        err = fd.error();
    }
    // error_name() is prose, where strerror() answers "ENOENT".
    Str e = error_name(err);
    (void)snprintf(gap, COLS - 20, "%.*s", (int)e.size(), e.data());
    mode = gap;
    co_return 1;
}

Task<void> writefile(void)
{
    co_await prompt(">", filename);
    if (*gap not_eq '\0') {
        free(filename);
        filename = strdup(gap);
        if (not co_await filewrite(filename)) {
            chg  = NOCHANGE;
            mode = cmd;
        }
    } else {
        (void)beep();
    }
    count = 0;
}

// Read what is left of fd into the gap, growing it as it fills, and close it.
// The read answers a String rather than filling a buffer, so the bytes are
// copied in; Err(Closed) is the end of the file and not a failure.
static Task<int> gap_fill(u32 fd)
{
    int bad = 0;

    for (;;) {
        usize want = (usize)(egap - gap);
        if (SYS_READ_MAX < want) {
            want = SYS_READ_MAX;
        }
        Result<String> r = Err(Error::NoMemory);
        if (Task<Result<String>> t = read_some(fd, (u32)want)) {
            r = co_await t;
        }
        if (r.is_err()) {
            bad = r.error() not_eq Error::Closed;
            break;
        }
        usize n = r.value().str().size();
        if (n == 0) {
            break;
        }
        (void)memcpy(gap, r.value().str().data(), n);
        gap += n;
        growgap(BUF / 2);
    }
    if (Task<void> t = close_fd(fd)) {
        co_await t;
    }
    co_return bad;
}

Task<int> fileread(const char *const fn)
{
    // No name at all is no file to read: `eh` with no argument, and the `<`
    // prompt answered with nothing.
    if (fn == NULL or *fn == '\0') {
        co_return 0;
    }
    // Callers pass `gap`, and growgap() below reallocs buf out from under it.
    // fn must therefore not be touched after the open -- it is not, and a copy
    // would be a PATH_MAX array in a coroutine frame.
    Result<i32> fd = Err(Error::NoMemory);
    if (Task<Result<i32>> t = open_read(Str(fn))) {
        fd = co_await t;
    }
    if (fd.is_err()) {
        // A name that is not there is a new file, which is upstream's answer to
        // `eh newfile`. Anything else -- a directory, a permission, an I/O
        // error -- is a failure the caller reports.
        co_return fd.error() not_eq Error::NotFound;
    }
    co_return co_await gap_fill((u32)fd.value());
}

Task<void> readfile(void)
{
    movegap(here);
    co_await prompt("<", "");
    const off_t eof = pos(ebuf);
    if (co_await fileread(gap)) {
        // ed(1) ?
        (void)beep();
    } else {
        off_t len = pos(ebuf) - eof;
        undo_save(UNDO_INS, here, gap - len, len);
        here  = pos(egap);
        epage = here + 1;
        chg   = CHANGED;
        adjmarks(len);
    }
    count = 0;
}

Task<void> edit(void)
{
    here  = 0;
    page  = 0;
    epage = 1;
    gap   = buf;
    egap  = ebuf;
    undo_free(undo_list);
    undo_free(redo_list);
    co_await prompt("*", filename);
    free(filename);
    filename = strdup(gap);
    if (co_await fileread(gap)) {
        // ed(1) ?
        (void)beep();
    }
}

//
// [countA]![countB]motion command
//
// Example format one or more lines of text:
//
// ^				Start of line.
// !/^$<newline> fmt -w68		Reformat here to next empty line.
//
// Can also save a text region from `a to here, eg. !`a tee save.txt
// If motion is `!` then read-only from command, eg. !! ls
// Upstream forked and drove the child through two pipes. There is no fork, and
// one task cannot park on both ends of a pipeline, so the region goes out
// through a file and the output comes back through another.
//
// File scope, not locals: two PATH-sized arrays in a coroutine frame would cost
// a whole 64 KiB span.
static char bang_in[32], bang_out[32];

// Run `gap` under the shell with its input and output on the two temp files,
// and answer its exit status.  stderr joins stdout, as upstream's third dup2
// did, which is what puts `!!WOOT`'s "not found" into the buffer.
static Task<int> bang_spawn(void)
{
    const char *shell = getenv("SHELL");
    Str words[3];
    Args v;
    ChildIo cio;
    int ex = 127;

    if (shell == NULL or *shell == '\0') {
        // Upstream dereferenced this unchecked.
        shell = "/bin/sh";
    }
    Result<i32> fdin = Err(Error::NoMemory);
    if (Task<Result<i32>> t = open_read(Str(bang_in))) {
        fdin = co_await t;
    }
    if (fdin.is_err()) {
        co_return 127;
    }
    Result<i32> fdout = Err(Error::NoMemory);
    if (Task<Result<i32>> t = open_at(Str(bang_out), SYS_O_WRITE | SYS_O_CREATE | SYS_O_TRUNC)) {
        fdout = co_await t;
    }
    if (fdout.is_err()) {
        if (Task<void> t = close_fd((u32)fdin.value())) {
            co_await t;
        }
        co_return 127;
    }
    Result<u32> fderr = Err(Error::NoMemory);
    if (Task<Result<u32>> t = dup_fd((u32)fdout.value())) {
        fderr = co_await t;
    }
    if (fderr.is_err()) {
        if (Task<void> t = close_fd((u32)fdin.value())) {
            co_await t;
        }
        if (Task<void> t = close_fd((u32)fdout.value())) {
            co_await t;
        }
        co_return 127;
    }

    words[0] = Str(shell, strlen(shell));
    words[1] = Str("-c", 2);
    words[2] = Str(gap, strlen(gap));
    v.v      = Span<const Str>(words, 3);
    // ChildIo moves a descriptor out of this process's table, so these three
    // must not be closed here.
    cio.in  = (u32)fdin.value();
    cio.out = (u32)fdout.value();
    cio.err = fderr.value();

    Result<u32> pid = Err(Error::NoMemory);
    if (Task<Result<u32>> t = spawn(v, cio)) {
        pid = co_await t;
    }
    if (pid.is_err()) {
        co_return 127;
    }
    // The console goes to the child so that ^C reaches it.  The claims stay
    // ours: all three of its streams are files, so it can neither read the
    // keyboard nor write the screen.
    if (Task<Result<void>> t = set_fg(pid.value())) {
        (void)co_await t;
    }
    Result<Exited> w = Err(Error::NoMemory);
    if (Task<Result<Exited>> t = wait_child(pid.value())) {
        w = co_await t;
    }
    if (Task<Result<void>> t = set_fg(0)) {
        (void)co_await t;
    }
    if (not w.is_err()) {
        ex = w.value().status;
    }
    co_return ex;
}

// [countA]![countB]motion command
//
// Example format one or more lines of text:
//
// ^				Start of line.
// !/^$<newline> fmt -w68		Reformat here to next empty line.
//
// Can also save a text region from `a to here, eg. !`a tee save.txt
// If motion is `!` then read-only from command, eg. !! ls
Task<void> bang(void)
{
    int ex = 74;
    co_await deld();
    co_await prompt("!", "");

    unsigned tag = proc_random();
    (void)snprintf(bang_in, sizeof(bang_in), "/tmp/eh-%08x-i", tag);
    (void)snprintf(bang_out, sizeof(bang_out), "/tmp/eh-%08x-o", tag);

    // The region, or nothing at all for `!!`, is the child's standard input.
    Result<i32> fd = Err(Error::NoMemory);
    if (Task<Result<i32>> t = open_at(Str(bang_in), SYS_O_WRITE | SYS_O_CREATE | SYS_O_TRUNC)) {
        fd = co_await t;
    }
    if (not fd.is_err()) {
        if (yank_text not_eq NULL and 0 < yank_length) {
            if (Task<Result<void>> t =
                    write_all((u32)fd.value(), Str(yank_text, (usize)yank_length))) {
                (void)co_await t;
            }
        }
        if (Task<void> t = close_fd((u32)fd.value())) {
            co_await t;
        }

        if (yank_text not_eq NULL) {
            ex = co_await bang_spawn();

            // The child has exited and the source is a file, so the read
            // answers Closed at the end -- upstream needed O_NONBLOCK here
            // because it raced waitpid against a pipe.
            const off_t eof = pos(ebuf);
            Result<i32> out = Err(Error::NoMemory);
            if (Task<Result<i32>> t = open_read(Str(bang_out))) {
                out = co_await t;
            }
            if (not out.is_err()) {
                (void)co_await gap_fill((u32)out.value());
            }
            // Convert delete to paired delete-insert.
            undo_list->paired = true;
            off_t len         = pos(ebuf) - eof;
            if (0 < len) {
                chg = CHANGED;
            }
            undo_save(UNDO_INS_B, here, gap - len, len);
            adjmarks(len - undo_list->next->size);
            // Position after the last character read.
            here = pos(egap);
            // Force page reframe in case region goes
            // off the bottom of the screen.
            epage = 1;
        }
    }
    if (Task<Result<void>> t = remove_path(Str(bang_in), false)) {
        (void)co_await t;
    }
    if (Task<Result<void>> t = remove_path(Str(bang_out), false)) {
        (void)co_await t;
    }

    if (ex not_eq 0) {
        (void)beep();
    }
}

void altx(void)
{
    int i;
    char *p;
    char32_t wc;
    mbstate_t mbs = {}; // Do NOT track state.
    movegap(here);
    assert(gap < egap);
    // Scan backwards at most 5 hex digits.
    for (i = 0, *gap = '\0'; i < 6 and buf < gap and isxdigit(gap[-1]); gap--, i++) {
        ;
    }
    wc = (char32_t)strtoul(gap, &p, 16);
    if (gap == p) {
        // Erase UTF-8 character, insert code point.
        gap -= here - prevch(here);
        i = mbrtowc((wchar_t *)&wc, gap, 4, &mbs);
        if (0 < i) {
            gap += snprintf(gap, 9, "%06X", wc);
        }
    } else if (wc <= 0x10FFFF) {
        // Erase code point, insert printable UTF-8 character.
        gap += wcrtomb(gap, (wchar_t)wc, &mbs);
    } else {
        // Restore state, nothing converted.
        gap += i;
        beep();
    }
    here  = pos(egap);
    epage = here + 1;
}

int cescape(const int ch)
{
    for (const char *s = "a\ab\bf\fn\nr\rt\tv\ve\033?\177"; *s not_eq '\0'; s += 2) {
        if (ch == *s) {
            return s[1];
        }
    }
    return ch;
}

void version(void)
{
    mode = BUILT " " VERSION;
}

Task<void> quit(void)
{
    if (chg == CHANGED) {
        co_await prompt("Discard changes y/[n]? ", "");
        if (*gap not_eq 'y') {
            co_return;
        }
    }
    mode = NULL;
}

void list(void)
{
    show_all = !show_all;
}

void nil(void)
{
    // Do nothing.
}

void flipcase(void)
{
    char32_t wc;
    movegap(here);
    mbstate_t mbs = {}; // Do NOT track state.
    ssize_t mbl   = mbrtowc((wchar_t *)&wc, egap, 4, &mbs);
    if (iswalpha(wc)) {
        undo_save(UNDO_DEL_A, here, egap, mbl);
        wc = iswlower(wc) ? towupper(wc) : towlower(wc);
        undo_save(UNDO_INS_B, here, egap, wcrtomb(egap, (wchar_t)wc, &mbs));
        chg = CHANGED;
    }
    right();
}

void scrollup(const off_t here, size_t n)
{
    for (page = here; 0 < n--;) {
        page = prevline(page);
    }
}

//
// Indent the lines *containing* a region or selection inclusive.
//
// a/ MOL to MOL (3 lines indent)
// ⸺⸺｜⸺⸺⸺⸺⸺⸺｜⸺⸺⸺⸺⸺⸺⸺⸺⸺｜⸺⸺⸺⸺
// 　　　　　　a 　　　　　　　　　　　　　　　b
//
// 	⸺⸺｜␉⸺⸺⸺⸺⸺⸺｜␉⸺⸺⸺⸺⸺⸺⸺⸺⸺｜␉⸺⸺⸺⸺
// 　　　　　　　a 　　　　　　　　　　　　　　　　　b
//
// b/ MOL to BOL (2 lines indent)
// ⸺⸺｜⸺⸺⸺⸺⸺⸺｜⸺⸺⸺⸺⸺⸺⸺⸺⸺｜⸺⸺⸺⸺
// 　　　　　　a 　　　　　　　　　　　　　b
//
// 	⸺⸺｜␉⸺⸺⸺⸺⸺⸺｜␉⸺⸺⸺⸺⸺⸺⸺⸺⸺｜⸺⸺⸺⸺
// 　　　　　　　a 　　　　　　　　　　　　　　b
//
// c/ BOL to MOL (3 lines indent)
// ⸺⸺｜⸺⸺⸺⸺⸺⸺｜⸺⸺⸺⸺⸺⸺⸺⸺⸺｜⸺⸺⸺⸺
// 　　　a 　　　　　　　　　　　　　　　　　　b
//
// ⸺⸺｜␉⸺⸺⸺⸺⸺⸺｜␉⸺⸺⸺⸺⸺⸺⸺⸺⸺｜␉⸺⸺⸺⸺
// 　　　　a 　　　　　　　　　　　　　　　　　　　　b
//
// d/ BOL to BOL (2 lines indent)
// ⸺⸺｜⸺⸺⸺⸺⸺⸺｜⸺⸺⸺⸺⸺⸺⸺⸺⸺｜⸺⸺⸺⸺
// 　　　a 　　　　　　　　　　　　　　　　b
//
// ⸺⸺｜␉⸺⸺⸺⸺⸺⸺｜␉⸺⸺⸺⸺⸺⸺⸺⸺⸺｜⸺⸺⸺⸺
// 　　　　a 　　　　　　　　　　　　　　　　　b
//
Task<void> indent(void)
{
    off_t start, stop = marker;
    if (stop < 0) {
        stop = here;
        (void)co_await getcmd(MOTION_CMDS);
    }
    start = here;
    if (stop < start) {
        // Swap.
        stop xor_eq start;
        start xor_eq stop;
        stop xor_eq start;
    }
    assert(start <= stop);
    // We're working with line units.  If start is in middle
    // of line, move to the beginning before editing.
    if (0 < start and *ptr(start - 1) not_eq '\n') {
        start = bol(start);
    }
    // Save region before change (delete).
    movegap(start);
    undo_save(UNDO_DEL_A, start, egap, stop - start);
    // Modify the region from stop down to start.
    off_t tabs = 0;
    while (start < stop) {
        // Previous physical line.
        stop = bol(OFF_DEC(stop));
        // Only indent non-empty lines.  Consider `>2}` repeated;
        // if you indent empty lines subsequent motions will be off.
        if (*ptr(stop) not_eq '\n') {
            movegap(stop);
            // Indent physical line.
            *--egap = '\t';
            // Maintain cursor position accounting for inserted tabs.
            if (stop < here) {
                adjmarks(1);
                here++;
            }
            tabs++;
        }
    }
    // Treat the modified region as "insert" text.
    undo_save(UNDO_INS_B, start, egap, undo_list->size + tabs);
    // Force display() to reframe.
    epage = here + 1;
    chg   = CHANGED;
    count = 0;
}

//
// /^( {,7}\t| {8}//
Task<void> outdent(void)
{
    off_t start, stop = marker;
    if (stop < 0) {
        stop = here;
        (void)co_await getcmd(MOTION_CMDS);
    }
    start = here;
    if (stop < start) {
        // Swap.
        stop xor_eq start;
        start xor_eq stop;
        stop xor_eq start;
    }
    assert(start <= stop);
    // We're working with line units.  If start is in middle
    // of line, move to the beginning before editing.
    if (0 < start and *ptr(start - 1) not_eq '\n') {
        start = bol(start);
    }
    // Save region before change (delete).
    movegap(start);
    undo_save(UNDO_DEL_A, start, egap, stop - start);
    // Modify the region from stop down to start.
    off_t tabs = 0;
    while (start < stop) {
        // Previous physical line.
        stop = bol(OFF_DEC(stop));
        movegap(stop);
        if (isblank(*egap)) {
            int span;
            if (*egap == '\t') {
                span = 1;
            } else {
                span = strspn(egap, " ");
                if (TABWIDTH < span) {
                    span = TABWIDTH;
                }
            }
            egap += span;
            tabs += span;
            // Maintain cursor position accounting for deleting tabs.
            if (stop < here) {
                adjmarks(-span);
                here -= span;
            }
        }
    }
    // Treat the modified region as "insert" text.
    undo_save(UNDO_INS_B, start, egap, undo_list->size - tabs);
    // Force display() to reframe.
    epage = here + 1;
    chg   = CHANGED;
    count = 0;
}

void replace_match(regmatch_t matches[MATCHES], const char *const str)
{
    if (NULL not_eq str) {
        movegap(here);
        char *xgap = gap;
        // CB-1 Have we come full circle?
        if (replace_all and search_wrapped and search_start < here) {
            here -= matches[0].rm_so;
            search_wrapped = 0;
            search_start   = 0;
            match_length   = 0;
            replace_all    = 0;
            return;
        }
        for (const char *s = str; *s not_eq '\0'; s++) {
            growgap(COLS);
            if (*s == '$' and isdigit(s[1])) {
                // Subexpression $0..$9
                int i   = *++s - '0';
                off_t n = matches[i].rm_eo - matches[i].rm_so;
                (void)memcpy(gap, egap + (matches[i].rm_so - matches[0].rm_so), n);
                gap += n;
                continue;
            } else if (*s == '\\') {
                *gap++ = cescape(*++s);
                continue;
            } else if (*s == '/') {
                // End replacement string.
                if (replace_all and *++s == 'a') {
                    // a = replace all (do it again)
                    (void)ungetch('n');
                }
                break;
            }
            *gap++ = *s;
        }
        // Delete the match.
        undo_save(UNDO_DEL_A, here, egap, match_length);
        egap += match_length;
        // Replacement string length.
        undo_save(UNDO_INS_B, here, xgap, gap - xgap);
        adjmarks(match_length - undo_list->next->size);
        // Position at end of replacement.
        here = pos(egap);
        chg  = CHANGED;
        // CB-1 Adjust search_start to maintain origin position
        // by preceding replacements.
        if (search_wrapped and here < search_start) {
            search_start += (gap - xgap) - match_length;
        }
    }
}

// In case we're sitting on a previous match, we need to search starting
// from the end of that match.  Note some special cases:
//
// Pattern /^/
// -----------
//      BOFtext\n\nterminated\nEOF
//         ^     ^ ^           ^
//      BOFtext\n\nnot terminatedEOF
//         ^     ^ ^
//
// Pattern /$/
// -----------
// BOFtext\n\nterminated\nEOF
//             ^ ^           ^ ^
// BOFtext\n\nnot terminatedEOF
//             ^ ^               ^
//
// Pattern /.$/
// ------------
// BOFtext\n\nterminated\nEOF
//            ^             ^
// BOFtext\n\nnot terminatedEOF
//            ^                 ^
//
// Pattern /^$/
// ------------
//      BOFtext\n\nterminated\n\nEOF
//               ^             ^
//      BOFtext\n\nnot terminatedEOF
//          ^               ^
//
// Cursor position:
//  - No match, cursor unchanged.
//  - Match found, cursor at start of match.
//  - Match found and replaced, cursor at end of replacement.
void search_next(void)
{
    const off_t eof = pos(ebuf);
    regmatch_t matches[MATCHES];
    // Move the gap out of the way in case it sits in the middle
    // of a potential match and NUL terminate the buffer.
    movegap(eof);
    SET_PREVIOUS_MARK(here);
    assert(gap < egap);
    *gap = '\0';
    // REG_NOTBOL allows /^/ to advance to start of next line.
    //
    // In all other cases need to offset by one the start of the
    // next search so as to avoid remaining stuck repeatedly
    // matching at the cursor.
    off_t next = here + (replace == NULL ? match_length : 0);
    // From cursor to EOF or after a wrap around from BOF to start point.
    if ((next < eof or next < search_start) and
        0 == regexec(&ere, ptr(next), MATCHES, matches, REG_NOTBOL)) {
        here = next + matches[0].rm_so;
    }
    // Wrap-around search only once.
    else if (not search_wrapped and 0 == regexec(&ere, buf, MATCHES, matches, 0)) {
        here           = matches[0].rm_so;
        search_wrapped = 1;
    }
    // No match after wrap-around.
    else {
        search_wrapped = 0;
        search_start   = 0;
        match_length   = 0;
        return;
    }
    // Next search resumes after this match, see here+match_length above.
    match_length = matches[0].rm_eo - matches[0].rm_so;
    replace_match(matches, replace);
    // CB-1 an empty match, ie. /$/, needs advance on next search.
    match_length += ere_dollar_only;
    // Position match in the centre of the screen.
    scrollup(here, LINES / 2 - TOP_LINE);
}

// Search and replace.
//
// /regex			find only
// /regex/			find only
// /regex//		replace with empty string
// /regex/pattern		replace with pattern
// /regex/pattern/		replace with pattern
// /regex/pattern/flags 	replace with pattern subject to flags
//
// Current flags:
//
// a			replace all, no prompt
Task<void> search(void)
{
    char *s, *t;
    co_await prompt("/", "");
    free(replace);
    replace = NULL;
    // Find end of pattern.
    for (s = t = gap; *t not_eq '\0'; s++, t++) {
        // Check for C escape, but ignore ERE meta.
        if (*t == '\\' and (isalpha(t[1]) or t[1] == '/')) {
            // Escape next character.
            *s = cescape(*++t);
        }
        // Is end of pattern, start of replacement.
        else if (*t == '/') {
            *t++ = '\0';
            // Is there a pattern or empty string to follow?
            if (*t not_eq '\0') {
                replace = strdup(t);
            }
            break;
        }
        // Copy/shift string.
        else {
            *s = *t;
        }
    }
    // CB-1 Small hack to handle `/^/XYZ/a`.
    search_start   = here;
    search_wrapped = 0;
    replace_all    = 1;
    *s             = '\0';
    regfree(&ere);
    if (regcomp(&ere, gap, REG_EXTENDED bitor REG_NEWLINE) not_eq 0) {
        // Something about the pattern is fubar.
        (void)beep();
    } else {
        // Kludge to handle repeated /^/ matching.
        ere_carat_only = gap[0] == '^' and '\0' == gap[1];
        // Kludge to handle repeated /$/ matching.
        ere_dollar_only = gap[0] == '$' and '\0' == gap[1];
        match_length    = 0;
        search_next();
    }
    count = 0;
}

void anchor(void)
{
    marker = marker < 0 ? here : -1;
}

void cleanup(void)
{
    // Most of these are to satisfy Valgrind or sanitisers.  The OS
    // reclaims memory when the program exits, making the need to
    // free() theoretically unnecessary.
    undo_free(undo_list);
    undo_free(redo_list);
    free(yank_text);
    free(filename);
    regfree(&ere);
    free(replace);
    free(buf);
}

// Two slots, exactly one of them filled. Making every handler a Task would put
// a heap frame behind every `l`; only 23 of the 68 block.
struct binding {
    int key;
    void (*func)(void);
    Task<void> (*task)(void);
};

// NOTE A, C, and D are questionable as "good parts", more like muscle
// memory concessions, since they are composites. X and x are supported
// since they are so frequently used, while dh and dl are functional,
// they're less common.

static struct binding cmds[] = { // Motion
                                 { KEY_LEFT, left, nullptr },
                                 { KEY_DOWN, down, nullptr },
                                 { KEY_UP, up, nullptr },
                                 { KEY_RIGHT, right, nullptr },
                                 { KEY_NPAGE, pgdown, nullptr },
                                 { KEY_PPAGE, pgup, nullptr },
                                 { KEY_HOME, lnbegin, nullptr },
                                 { KEY_END, lnend, nullptr },
                                 { 'h', left, nullptr },
                                 { 'j', down, nullptr },
                                 { 'k', up, nullptr },
                                 { 'l', right, nullptr },
                                 { 'b', wleft, nullptr },
                                 { 'e', wend, nullptr },
                                 { 'w', wright, nullptr },
                                 { 'H', pgtop, nullptr },
                                 { 'J', pgdown, nullptr },
                                 { 'K', pgup, nullptr },
                                 { 'L', pgbottom, nullptr },
                                 { '^', lnbegin, nullptr },
                                 { '$', lnend, nullptr },
                                 { '|', column, nullptr },
                                 { 'G', lngoto, nullptr },
                                 { '/', nullptr, search },
                                 { 'n', search_next, nullptr },
                                 { '`', nullptr, gomark },
                                 { '\'', nullptr, lnmark },
                                 { '%', pairs, nullptr },
                                 { CTRL_F, pgdown, nullptr },
                                 { CTRL_B, pgup, nullptr },
                                 { '{', paraup, nullptr },
                                 { '}', paradown, nullptr },
                                 { '+', lnplus, nullptr },
                                 { '-', lnminus, nullptr },

                                 // Edit
                                 { KEY_DC, nullptr, delx },
                                 { KEY_BACKSPACE, nullptr, delX },
                                 { '~', flipcase, nullptr },
                                 { 'i', nullptr, insert },
                                 { 'I', insertI, nullptr },
                                 { 'a', append, nullptr },
                                 { 'A', appendA, nullptr },
                                 { 'x', nullptr, delx },
                                 { 'X', nullptr, delX },
                                 { 'y', nullptr, yanky },
                                 { 'Y', yankY, nullptr },
                                 { 'd', nullptr, deld },
                                 { 'D', nullptr, delD },
                                 { 'c', nullptr, chgc },
                                 { 'C', nullptr, chgC },
                                 { 'o', openo, nullptr },
                                 { 'O', openO, nullptr },
                                 { 'P', pasteP, nullptr },
                                 { 'p', pastep, nullptr },
                                 { 'u', undo, nullptr },
                                 { 'U', redo, nullptr },
                                 { '!', nullptr, bang },
                                 { '<', nullptr, outdent },
                                 { '>', nullptr, indent },
                                 { CTRL_L, list, nullptr },
                                 { CTRL_X, altx, nullptr },

                                 // Other
                                 { '\\', anchor, nullptr },
                                 { 'm', nullptr, setmark },
                                 { 'E', nullptr, edit },
                                 { 'R', nullptr, readfile },
                                 { 'W', nullptr, writefile },
                                 { 'Q', nullptr, quit },
                                 { CTRL_C, nullptr, quit },
                                 { CTRL_R, redraw, nullptr },

                                 { 'V', version, nullptr },
                                 { CTRL_G, nullptr, gold },
                                 { -1, nil, nullptr }
};

static Task<off_t> getcmd(const int m)
{
    int j, ch;
    off_t was_here = here;
    for (j = 0; isdigit(ch = co_await getsigch());) {
        j = j * 10 + ch - '0';
    }
    // 2dw = d2w and 2d3w = d6w
    count = j > 0 and count > 0 ? j * count : 0 < j ? j : count;
    static int this_cmd;
    if (ch == this_cmd and strchr("cdy<>", ch) not_eq NULL) {
        // These commands can operate on line units;
        // eg. cc, dd, yy, <<, >>, 2>>, <3<, 2d3d
        if (ch not_eq 'c' and ch not_eq 'd') {
            // yy, <<, >> maintain cursor position
            SET_PREVIOUS_MARK(here);
            (void)ungetstr("``");
        }
        // Operate on physical line units.
        ch = '+';
        lnbegin();
    }
    for (j = 0; cmds[j].key not_eq -1 and ch not_eq cmds[j].key; j++) {
        ;
    }
    if (j < m) {
        this_cmd = ch;
        was_here = here;
        // Count always defaults to 1.
        do {
            if (cmds[j].func not_eq NULL) {
                (*cmds[j].func)();
            } else {
                co_await (*cmds[j].task)();
            }
            co_await cmd_yield();
        } while (1 < count--);
        this_cmd = 0;
    }
    count = 0;
    co_return was_here;
}

// atexit() traps here; proc_at_exit takes a Task.
static Task<void> at_exit(void)
{
    cleanup();
    co_return;
}

Task<i32> proc_main(Args args)
{
    char name[256];
    Args rest = args.tail();

    // setlocale() is gone: this system is UTF-8 throughout.
    {
        Result<void> r = Err(Error::NoMemory);
        if (Task<Result<void>> t = eh_open()) {
            r = co_await t;
        }
        if (r.is_err()) {
            co_return 1;
        }
    }
    proc_at_exit(at_exit);
    growgap(BUF);

    name[0] = '\0';
    if (0 < rest.size()) {
        Str a   = rest[0];
        usize n = a.size() < sizeof(name) - 1 ? a.size() : sizeof(name) - 1;
        (void)memcpy(name, a.data(), n);
        name[n] = '\0';
    }
    filename = strdup(name);
    if (co_await fileread(filename)) {
        // Good grief Charlie Brown!
        co_return 2;
    }
    // Force display() to frame the initial screen.
    epage = 1;
    while (mode not_eq NULL) {
        // A bare resize answers getch() with ERR and leaves the grid the new
        // shape; reframe against the new COLS before painting.
        if (eh_resize_flag) {
            eh_resize_flag = 0;
            epage          = here + 1;
        }
        display();
        (void)co_await getcmd(ALL_CMDS);
    }
    co_return 0;
}
