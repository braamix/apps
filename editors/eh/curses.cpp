// The curses shim. See curses.h.

#include "curses.h"

#include "braam.h"
#include "kernel/alloc.h"

int LINES = 24;
int COLS  = 80;

// Every call that takes a WINDOW * gets this and every callee ignores it.
WINDOW *stdscr = (WINDOW *)1;

namespace {

ProcScreen *scr;

int cy, cx;     // the current position: where the next addch goes
chtype curattr; // what attrset last said
bool full_blit = true;

// The last cursor cell damaged, so a move that changes nothing else still
// carries the blit's header out.
u32 lastcx = ~0u, lastcy = ~0u;

void split(chtype a, u8 &fg, u8 &bg, u8 &at)
{
    fg = (u8)COLOR_WHITE;
    bg = (u8)COLOR_BLACK;
    at = 0;
    if (a & A_BOLD)
        at |= ATTR_BOLD;
    if (a & A_UNDERLINE)
        at |= ATTR_UNDERLINE;
    if (a & A_REVERSE)
        at |= ATTR_REVERSE;
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

void advance(int n)
{
    cx += n;
    if (cx >= COLS)
        cx = COLS - 1;
}

// One rune into one cell. TAB blanks to the next eight-column stop and LF is
// nothing, which is what curses drew: display() places every run itself with
// mvaddnstr, so the line breaks are its own.
void put_rune(char32_t ch, chtype a)
{
    if (ch == '\n' || ch == '\r')
        return;
    if (ch == '\t') {
        int stop = (cx + 8) & ~7;
        while (cx < stop) {
            put_at(cy, cx, ' ', a);
            advance(1);
        }
        return;
    }
    put_at(cy, cx, ch, a);
    advance(1);
}

// A byte that starts no sequence draws as a reverse-video ~, which is what
// display() does with one too.
int put_utf8(const char *s, int n)
{
    while (n > 0) {
        wchar_t wc;
        int len = mbtowc(&wc, s, (size_t)n);

        if (len < 1) {
            put_rune(U'~', curattr | A_REVERSE);
            len = 1;
        } else {
            put_rune((char32_t)wc, curattr);
        }
        s += len;
        n -= len;
    }
    return OK;
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
// cursor move across. Every h/j/k/l depends on this.
Task<Result<void>> curses_flush()
{
    Grid &g = curses_grid();

    g.cursor_x  = (u32)cx;
    g.cursor_y  = (u32)cy;
    g.cursor_on = true;
    if (g.cursor_x != lastcx || g.cursor_y != lastcy) {
        g.touch(g.cursor_x, g.cursor_y, 1, 1);
        lastcx = g.cursor_x;
        lastcy = g.cursor_y;
    }
    if (full_blit)
        g.touch(0, 0, g.cols, g.rows);

    // The damage is taken before the blit is awaited, so a frame the kernel
    // refuses -- a resize landing under it -- is gone. Ask for it again.
    Result<void> r = co_await scr->flush();
    full_blit      = r.is_err();
    co_return r;
}

int endwin()
{
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
int echo()
{
    return OK;
}
int nonl()
{
    return OK;
}
int nl()
{
    return OK;
}
int raw()
{
    return OK;
}
int noraw()
{
    return OK;
}
int keypad(WINDOW *, bool)
{
    return OK;
}
int delwin(WINDOW *)
{
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

int attron(chtype a)
{
    curattr |= a & A_ATTRIBUTES;
    return OK;
}

int attroff(chtype a)
{
    curattr &= ~(a & A_ATTRIBUTES);
    return OK;
}

int standout()
{
    return attron(A_STANDOUT);
}

int standend()
{
    return attrset(A_NORMAL);
}

// --------------------------------------------------------------- writing

// The background, not the current attribute: curses erases to the window's
// bkgd, which nothing here ever sets. display() leaves standout on across the
// frame boundary -- erasing with it would paint the whole screen reverse.
int clear()
{
    Grid &g = curses_grid();

    for (u32 y = 0; y < g.rows; y++)
        for (u32 x = 0; x < g.cols; x++)
            put_at((int)y, (int)x, ' ', A_NORMAL);
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
        put_at(cy, x, ' ', A_NORMAL);
    return OK;
}

int addch(chtype c)
{
    put_rune((char32_t)(c & A_CHARTEXT), curattr | (c & A_ATTRIBUTES));
    return OK;
}

int mvaddch(int y, int x, chtype c)
{
    if (move(y, x) == ERR)
        return ERR;
    return addch(c);
}

int addstr(const char *s)
{
    return put_utf8(s, (int)strlen(s));
}

int addnstr(const char *s, int n)
{
    return put_utf8(s, n);
}

int mvaddstr(int y, int x, const char *s)
{
    if (move(y, x) == ERR)
        return ERR;
    return addstr(s);
}

int mvaddnstr(int y, int x, const char *s, int n)
{
    if (move(y, x) == ERR)
        return ERR;
    return addnstr(s, n);
}

int printw(const char *fmt, ...)
{
    char buf[256];
    va_list ap;

    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0)
        return ERR;
    if (n > (int)sizeof(buf) - 1)
        n = (int)sizeof(buf) - 1;
    return addnstr(buf, n);
}

// --------------------------------------------------------------- the rest

int refresh()
{
    return OK;
}

int beep()
{
    return OK;
}

// ------------------------------------------------------------- pushback

namespace {

// A line, not a key: prompt() primes the field with a whole filename through
// ungetstr(), whose own bound is COLS, and getch() pushes the tail of a UTF-8
// sequence on top of that.
enum { UNGET_MAX = 1024 };

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
