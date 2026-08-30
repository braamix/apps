// The curses shim. See curses.h.

#include "curses.h"

#include "braam.h"
#include "kernel/alloc.h"

int LINES = 24;
int COLS  = 80;

// Every call that takes a WINDOW * gets this and every callee ignores it.
WINDOW *stdscr = (WINDOW *)1;

const cchar_t WACS_ULCORNER_c = { A_NORMAL, { 0x250C, 0 } };
const cchar_t WACS_URCORNER_c = { A_NORMAL, { 0x2510, 0 } };
const cchar_t WACS_LLCORNER_c = { A_NORMAL, { 0x2514, 0 } };
const cchar_t WACS_LRCORNER_c = { A_NORMAL, { 0x2518, 0 } };
const cchar_t WACS_HLINE_c    = { A_NORMAL, { 0x2500, 0 } };
const cchar_t WACS_VLINE_c    = { A_NORMAL, { 0x2502, 0 } };
const cchar_t WACS_CKBOARD_c  = { A_NORMAL, { 0x2592, 0 } };
const cchar_t WACS_LTEE_c     = { A_NORMAL, { 0x251C, 0 } };
const cchar_t WACS_RTEE_c     = { A_NORMAL, { 0x2524, 0 } };

namespace {

ProcScreen *scr;

int cy, cx;     // the current position: where the next addch goes
chtype curattr; // what attrset last said
int cursor_vis = 1;
bool full_blit = true;

// What init_pair recorded. -1 is use_default_colors' "the terminal's own",
// which here is white on black.
struct Pair {
    short fg = -1, bg = -1;
} pairs[COLOR_PAIRS];

// One past the highest pair init_pair has seen.
int npairs = 1;

// The last cursor cell damaged, so a move that changes nothing else still
// carries the blit's header out.
u32 lastcx = ~0u, lastcy = ~0u;

void split(chtype a, u8 &fg, u8 &bg, u8 &at)
{
    const Pair &p = pairs[PAIR_NUMBER(a)];

    fg = p.fg < 0 ? (u8)COLOR_WHITE : (u8)p.fg;
    bg = p.bg < 0 ? (u8)COLOR_BLACK : (u8)p.bg;
    at = 0;
    if (a & A_BOLD)
        at |= ATTR_BOLD;
    if (a & A_UNDERLINE)
        at |= ATTR_UNDERLINE;
    if (a & A_REVERSE)
        at |= ATTR_REVERSE;
    // A_DIM has no counterpart; ncurses drops it on a terminal without one too.
}

// The inverse of split(), through split() so the round trip is exact. Pair 0
// answers when none matches, and renders as the white on black it already is.
int pair_of(u8 fg, u8 bg)
{
    for (int i = 1; i < npairs; i++) {
        u8 f, b, a;

        split(COLOR_PAIR(i), f, b, a);
        if (f == fg && b == bg)
            return i;
    }
    return 0;
}

// What ncurses merges in from the window: the pair only if the character has
// none, the other attributes always.
chtype merge_attr(chtype a)
{
    a |= curattr & A_ATTRIBUTES & ~A_COLOR;
    if (!(a & A_COLOR))
        a |= curattr & A_COLOR;
    return a;
}

void put_at(int y, int x, char32_t ch, chtype a)
{
    Grid &g = curses_grid();
    Cell *c = g.at((u32)x, (u32)y);
    u8 fg, bg, at;

    if (!c)
        return;
    split(a, fg, bg, at);
    if (ch == 0)
        ch = ' ';
    if (!full_blit && c->ch == ch && c->fg == fg && c->bg == bg && c->attrs == at)
        return;
    c->ch    = ch;
    c->fg    = fg;
    c->bg    = bg;
    c->attrs = at;
    g.touch((u32)x, (u32)y, 1, 1);
}

// One cell of a wide character; the columns it spills into are left alone,
// which is what upstream's own width accounting already arranges.
void advance(int n)
{
    cx += n;
    if (cx >= COLS)
        cx = COLS - 1;
}

} // namespace

ProcScreen &curses_screen()
{
    return *scr;
}

Grid &curses_grid()
{
    return scr->grid();
}

void curses_full_blit()
{
    full_blit = true;
}

void curses_resized()
{
    LINES = (int)curses_grid().rows;
    COLS  = (int)curses_grid().cols;
    // curses_flush ships cx/cy whatever they are, and a touch off the grid is
    // dropped -- so a shrink would send a cursor that is no longer there.
    if (cx >= COLS)
        cx = COLS - 1;
    if (cy >= LINES)
        cy = LINES - 1;
    full_blit = true;
    lastcx = lastcy = ~0u;
}

Task<Result<void>> curses_open()
{
    if (!scr) {
        scr = heap_new<ProcScreen>();
        if (!scr)
            co_return Err(Error::NoMemory);
    }
    CO_TRY_VOID(co_await scr->take_keys());
    CO_TRY_VOID(co_await scr->take_screen());
    curses_resized();
    co_return Result<void>();
}

// The cursor rides in the blit's header, and a blit with no damage in it is
// not sent at all -- so damaging the cell under it is what carries a bare
// cursor move across.
Task<Result<void>> curses_flush()
{
    Grid &g = curses_grid();

    g.cursor_x  = (u32)cx;
    g.cursor_y  = (u32)cy;
    g.cursor_on = cursor_vis != 0;
    if (g.cursor_x != lastcx || g.cursor_y != lastcy) {
        g.touch(g.cursor_x, g.cursor_y, 1, 1);
        lastcx = g.cursor_x;
        lastcy = g.cursor_y;
    }
    if (full_blit)
        g.touch(0, 0, g.cols, g.rows);

    /* The damage is taken before the blit is awaited, so a frame the kernel
       refuses -- a resize landing under it -- is gone. Ask for it again. */
    Result<void> r = co_await scr->flush();
    full_blit      = r.is_err();
    co_return r;
}

int endwin()
{
    return OK;
}

// ---------------------------------------------------------------- colour

int start_color()
{
    return OK;
}

int use_default_colors()
{
    return OK;
}

bool has_colors()
{
    return true;
}

int init_pair(short pair, short fg, short bg)
{
    if (pair < 0 || pair >= COLOR_PAIRS)
        return ERR;
    pairs[pair].fg = fg;
    pairs[pair].bg = bg;
    if (pair >= npairs)
        npairs = pair + 1;
    return OK;
}

int pair_content(short pair, short *fg, short *bg)
{
    if (pair < 0 || pair >= COLOR_PAIRS)
        return ERR;
    *fg = pairs[pair].fg;
    *bg = pairs[pair].bg;
    return OK;
}

// ----------------------------------------------------------------- modes

int cbreak()
{
    return OK;
}
int noecho()
{
    return OK;
}
int nonl()
{
    return OK;
}
int raw()
{
    return OK;
}
int meta(WINDOW *, bool)
{
    return OK;
}
int intrflush(WINDOW *, bool)
{
    return OK;
}
int keypad(WINDOW *, bool)
{
    return OK;
}
int leaveok(WINDOW *, bool)
{
    return OK;
}
int clearok(WINDOW *, bool)
{
    curses_full_blit();
    return OK;
}

int curs_set(int visibility)
{
    int was    = cursor_vis;
    cursor_vis = visibility;
    return was;
}

int reset_prog_mode()
{
    curses_full_blit();
    return OK;
}

// -------------------------------------------------------------- position

int move(int y, int x)
{
    if (y < 0 || x < 0 || y >= LINES || x >= COLS)
        return ERR;
    cy = y;
    cx = x;
    return OK;
}

int getcury(WINDOW *)
{
    return cy;
}

int getcurx(WINDOW *)
{
    return cx;
}

int attrset(chtype a)
{
    curattr = a;
    return OK;
}

int bkgdset(chtype a)
{
    curattr = a;
    return OK;
}

// --------------------------------------------------------------- writing

int clear()
{
    Grid &g = curses_grid();

    for (u32 y = 0; y < g.rows; y++)
        for (u32 x = 0; x < g.cols; x++)
            put_at((int)y, (int)x, ' ', curattr);
    cy = cx = 0;
    return OK;
}

int erase()
{
    return clear();
}

int clrtoeol()
{
    for (int x = cx; x < COLS; x++)
        put_at(cy, x, ' ', curattr);
    return OK;
}

int addch(chtype c)
{
    put_at(cy, cx, (char32_t)(c & A_CHARTEXT), curattr | (c & A_ATTRIBUTES & ~A_ALTCHARSET));
    advance(1);
    return OK;
}

int mvaddch(int y, int x, chtype c)
{
    if (move(y, x) == ERR)
        return ERR;
    return addch(c);
}

int mvaddstr(int y, int x, const char *s)
{
    if (move(y, x) == ERR)
        return ERR;
    // Bytes, not runes: every caller here has already made the string ASCII or
    // has laid it out itself.
    for (; *s; s++) {
        put_at(cy, cx, (unsigned char)*s, curattr);
        advance(1);
    }
    return OK;
}

int addnwstr(const wchar_t *s, int n)
{
    for (int i = 0; (n < 0 || i < n) && s[i]; i++) {
        put_at(cy, cx, (char32_t)s[i], curattr);
        advance(1);
    }
    return OK;
}

int mvaddchnstr(int y, int x, const chtype *s, int n)
{
    for (int i = 0; i < n && x + i < COLS; i++)
        put_at(y, x + i, (char32_t)(s[i] & A_CHARTEXT), s[i] & A_ATTRIBUTES & ~A_ALTCHARSET);
    return OK;
}

// A cchar_t's first codepoint is the cell; the rest are combining marks the
// renderer does not compose, so they are dropped rather than drawn.
int add_wch(const cchar_t *c)
{
    put_at(cy, cx, (char32_t)c->chars[0], merge_attr(c->attr));
    advance(1);
    return OK;
}

int mvadd_wch(int y, int x, const cchar_t *c)
{
    if (move(y, x) == ERR)
        return ERR;
    return add_wch(c);
}

// No merge_attr, unlike add_wch: every caller builds each cell's attribute.
int mvadd_wchnstr(int y, int x, const cchar_t *s, int n)
{
    for (int i = 0; i < n && x + i < COLS; i++)
        put_at(y, x + i, (char32_t)s[i].chars[0], s[i].attr);
    return OK;
}

int mvin_wch(int y, int x, cchar_t *out)
{
    Cell *c = curses_grid().at((u32)x, (u32)y);

    if (!c)
        return ERR;
    for (int i = 0; i < CCHARW_MAX; i++)
        out->chars[i] = 0;
    out->chars[0] = (wchar_t)c->ch;
    // The pair too, so the window stack restores what it saved.
    out->attr = COLOR_PAIR(pair_of(c->fg, c->bg));
    if (c->attrs & ATTR_BOLD)
        out->attr |= A_BOLD;
    if (c->attrs & ATTR_UNDERLINE)
        out->attr |= A_UNDERLINE;
    if (c->attrs & ATTR_REVERSE)
        out->attr |= A_REVERSE;
    return OK;
}

chtype mvinch(int y, int x)
{
    cchar_t c;

    if (mvin_wch(y, x, &c) == ERR)
        return (chtype)' ';
    return (chtype)c.chars[0] | c.attr;
}

// --------------------------------------------------------------- the rest

int refresh()
{
    return OK;
}

int doupdate()
{
    return OK;
}

int beep()
{
    return OK;
}

// ------------------------------------------------------------- pushback

namespace {

enum { UNGET_MAX = 32 };

int unget_buf[UNGET_MAX];
int unget_n;

} // namespace

int ungetch(int key)
{
    if (unget_n >= UNGET_MAX)
        return ERR;
    unget_buf[unget_n++] = key;
    return OK;
}

int curses_unget_pending()
{
    return unget_n;
}

int curses_unget_take()
{
    return unget_n > 0 ? unget_buf[--unget_n] : ERR;
}

int flushinp()
{
    unget_n = 0;
    return OK;
}
