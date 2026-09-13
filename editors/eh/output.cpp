// The screen. See globals.h.

#include "globals.h"

#include <wchar.h> // mbtowc

#include "kernel/alloc.h"

int LINES = 24;
int COLS  = 80;

namespace {

ProcScreen *scr;

bool full_blit = true;

// The last cursor cell damaged, so a move that changes nothing else still
// carries the blit's header out.
u32 lastcx = ~0u, lastcy = ~0u;

// The one cell writer. No colour: white on black, attrs as given.
void put_at(int y, int x, char32_t ch, u8 attr)
{
    Grid &g = eh_grid();
    Cell *c = g.at((u32)x, (u32)y);

    if (!c)
        return;
    if (ch == 0)
        ch = ' ';
    if (!full_blit && c->ch == ch && c->fg == (u8)COLOR_WHITE && c->bg == (u8)COLOR_BLACK &&
        c->attrs == attr)
        return;
    c->ch    = ch;
    c->fg    = (u8)COLOR_WHITE;
    c->bg    = (u8)COLOR_BLACK;
    c->attrs = attr;
    g.touch((u32)x, (u32)y, 1, 1);
}

} // namespace

ProcScreen &eh_screen()
{
    return *scr;
}

Grid &eh_grid()
{
    return scr->grid();
}

void eh_full_blit()
{
    full_blit = true;
}

void eh_resized()
{
    Grid &g = eh_grid();

    LINES = (int)g.rows;
    COLS  = (int)g.cols;
    // eh_flush ships the cursor whatever it is, and a touch off the grid is
    // dropped -- so a shrink would send a cursor that is no longer there.
    if (g.cursor_x >= g.cols)
        g.cursor_x = g.cols - 1;
    if (g.cursor_y >= g.rows)
        g.cursor_y = g.rows - 1;
    full_blit = true;
    lastcx = lastcy = ~0u;
}

Task<Result<void>> eh_open()
{
    if (!scr) {
        scr = heap_new<ProcScreen>();
        if (!scr)
            co_return Err(Error::NoMemory);
    }
    CO_TRY_VOID(co_await scr->take_keys());
    CO_TRY_VOID(co_await scr->take_screen());
    eh_resized();
    co_return Result<void>();
}

// The cursor rides in the blit's header, and a blit with no damage in it is
// not sent at all -- so damaging the cell under it is what carries a bare
// cursor move across. Every h/j/k/l depends on this.
Task<Result<void>> eh_flush()
{
    Grid &g = eh_grid();

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

// --------------------------------------------------------------- painting

void eh_erase()
{
    Grid &g = eh_grid();

    for (u32 y = 0; y < g.rows; y++)
        for (u32 x = 0; x < g.cols; x++)
            put_at((int)y, (int)x, ' ', 0);
}

void eh_fill(int y, int x, u8 attr)
{
    for (; x < COLS; x++)
        put_at(y, x, ' ', attr);
}

int eh_put(int y, int x, const char *s, int n, u8 attr)
{
    while (n > 0) {
        wchar_t wc;
        int len = mbtowc(&wc, s, (size_t)n);
        u8 a    = attr;

        if (len < 1) {
            wc = U'~';
            a |= ATTR_REVERSE;
            len = 1;
        }
        s += len;
        n -= len;

        if (wc == '\n' || wc == '\r')
            continue;
        if (wc == '\t') {
            // Curses blanked to the next eight-column stop; clip it, or the
            // last stop on the row never arrives and the walk never ends.
            int stop = (x + 8) & ~7;
            if (stop > COLS)
                stop = COLS;
            while (x < stop)
                put_at(y, x++, ' ', a);
        } else {
            put_at(y, x++, (char32_t)wc, a);
        }
        if (x >= COLS)
            x = COLS - 1;
    }
    return x;
}

void eh_put_rune(int y, int x, char32_t ch, u8 attr)
{
    put_at(y, x, ch, attr);
}

void eh_cursor(int y, int x)
{
    Grid &g = eh_grid();

    if (y < 0 || x < 0 || y >= LINES || x >= COLS)
        return;
    g.cursor_y = (u32)y;
    g.cursor_x = (u32)x;
}

void beep()
{
}
