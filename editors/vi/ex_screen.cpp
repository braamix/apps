/*
 * The screen, and the keyboard.
 *
 * This replaces ex_tty.c, which read a terminal's description out of the
 * termcap database, and the half of ex_put.c that drove one. Braam has no
 * terminal type and no escape sequences: the screen is an array of cells that
 * the renderer reads straight out of linear memory (Concept.md §2.3), so a
 * capability is a constant in ex_screen.h and cursor addressing is indexing.
 *
 * What is here is the seam. vi keeps an exact image of the screen in vtube --
 * it always did, so that it could work out the fewest bytes to send -- and
 * that image is the back buffer now: vflush() copies the cells that differ
 * into the Grid, which keeps the damage, and one syscall sends the frame.
 */
#include "ex.h"
#include "ex_screen.h"
#include "ex_vis.h"

#include "kernel/alloc.h"

ProcScreen *vscreen;

/*
 * The screen and the keyboard are claims: one holder each, kept by the kernel
 * on the process's record. Command mode wants neither -- it writes a byte
 * stream that may be a pipe -- so they are taken when visual starts, given
 * back when it ends, and handed over and back around every shell escape.
 *
 * ~ProcScreen does not release them, because a destructor cannot await a
 * syscall. ~Proc is what guarantees the shell gets its screen back, and it has
 * to be: a killed program runs no destructor of its own.
 */
Task<Result<void>> vscreen_take(void)
{
    if (vscreen == 0) {
        vscreen = heap_new<ProcScreen>();
        if (vscreen == 0)
            co_return Err(Error::NoMemory);
    }
    CO_TRY_VOID(co_await vscreen->take_keys());
    CO_TRY_VOID(co_await vscreen->take_screen());
    vscreen->grid().cursor_on = true;
    co_return {};
}

Task<void> vscreen_give(void)
{
    if (vscreen == 0)
        co_return;
    if (Task<Result<Geometry>> t = screen_claim(false))
        co_await t;
    if (Task<Result<Geometry>> t = keys_claim(false))
        co_await t;
}

/*
 * The frame.
 *
 * Everything in ex_vput.cpp has been writing characters into vtube, and none
 * of it has reached the screen. This is where it does. A cell is written only
 * where it differs, so the damage rectangle the Grid accumulates is the union
 * of what actually changed and the blit sends that and no more -- which is the
 * same economy ex_put.c's cursor-cost model was after, arrived at from the
 * other end.
 *
 * vtube holds bytes and a Cell holds a codepoint and an attribute. A zero in
 * vtube means "never written" and QUOTE means "part of an expanded tab"; both
 * draw as a blank, which is what vputchar() already intends by them.
 */
Task<Result<void>> vflush(void)
{
    Grid *g;
    int y, x;

    if (vscreen == 0)
        co_return {};
    g = &vscreen->grid();
    for (y = 0; y <= WECHO && y < (int)g->rows; y++) {
        char *tp = vtube[y];
        char *ap = vatube0 && tp ? vatube0 + (tp - vtube0) : 0;

        if (tp == 0)
            continue;
        for (x = 0; x < WCOLS && x < (int)g->cols; x++) {
            int c       = tp[x] & TRIM;
            char32_t ch = c ? (char32_t)c : U' ';
            u8 at       = ap ? (u8)ap[x] : 0;
            Cell *cl    = g->at((u32)x, (u32)y);

            if (cl == 0 || (cl->ch == ch && cl->attrs == at))
                continue;
            cl->ch    = ch;
            cl->attrs = at;
            cl->fg    = COLOR_WHITE;
            cl->bg    = COLOR_BLACK;
            g->touch((u32)x, (u32)y, 1, 1);
        }
    }
    g->cursor_x  = destcol >= 0 && destcol < (int)g->cols ? (u32)destcol : 0;
    g->cursor_y  = destline >= 0 && destline < (int)g->rows ? (u32)destline : 0;
    g->cursor_on = true;
    co_return co_await vscreen->flush();
}

/*
 * The geometry: the tail of setterm() in ex_tty.c. Upstream read LINES and
 * COLUMNS out of termcap and picked the window from the line's speed; there is
 * no line, so this is always the fast case -- the whole screen but the status
 * row, or -w.
 *
 * window and scroll are options. Only one still equal to its default follows
 * the screen; the default moves with it either way.
 */
void setsize(int rows, int cols)
{
    int w;

    if (rows <= 0 || cols <= 0)
        return;
    LINES   = (short)(rows < TUBELINES ? rows : TUBELINES);
    COLUMNS = (short)(cols < TUBECOLS ? cols : TUBECOLS);

    w = defwind ? defwind : LINES - 1;
    if (w >= LINES)
        w = LINES - 1;
    if (w < 1)
        w = 1;
    if (value(WINDOW) == options[WINDOW].odefault)
        value(WINDOW) = (short)w;
    options[WINDOW].odefault = (short)w;

    w = (value(WINDOW) + 1) / 2;
    if (value(SCROLL) == options[SCROLL].odefault)
        value(SCROLL) = (short)w;
    options[SCROLL].odefault = (short)w;
}

/*
 * A resize. The geometry rides on every terminal reply, so ProcScreen has
 * reshaped the grid already by the time this is called; what is left is vi's
 * own idea of the screen, which is vtube -- row pointers into one block, cut
 * at the width, which vok() is what cuts.
 *
 * Nothing here reads linebuf, cursor or dot, which is what makes it safe from
 * inside insert mode: the insertion's state is a pointer into linebuf, and the
 * screen does not own that.
 */
void vresize(void)
{
    u32 rows, cols;

    if (vscreen == 0)
        return;
    rows = vscreen->grid().rows;
    cols = vscreen->grid().cols;
    if (rows == 0 || cols == 0)
        return;
    setsize((int)rows, (int)cols);
    if (!inopen)
        return;
    /*
     * The line being edited lives in linebuf, and the redraw below reads the
     * buffer; without this it would repaint the line as it was before the
     * current change.
     */
    vsave();
    WCOLS = COLUMNS;
    vsetsiz(value(WINDOW));
    setwind();
    vok(atube);
    vclear();
    vcnt = 0;
    vredraw(WTOP);
    vfixcurs();
}

/*
 * {code, mods} onto the byte codes vi's decoder expects.
 *
 * There are no control characters on this keyboard (Concept.md §3.5): ^C is
 * 'c' with the control modifier set, and this is where the two become one
 * again. The arrows answer ^P, ^N, ^H and space rather than k, j, h and l,
 * because those four work inside insert mode too and hjkl would type
 * themselves.
 */
int key_byte(Key k)
{
    if (k.mods & MOD_CTRL) {
        /*
         * ^C reaches a program in front of the console as SIG_INT, and
         * arrives here as an ordinary key when nothing is in front. vi
         * answers it the same either way: abandon the command.
         */
        if (k.code == 'c')
            return (ATTN);
        if (k.code >= 'a' && k.code <= 'z')
            return ((int)k.code - 'a' + 1);
        if (k.code >= 'A' && k.code <= 'Z')
            return ((int)k.code - 'A' + 1);
        if (k.code == '[')
            return (ESCAPE);
        if (k.code == '\\')
            return (QUIT);
        if (k.code == ']')
            return (CTRL(']'));
        if (k.code == '^')
            return (CTRL('^'));
        if (k.code == '?')
            return (DELETE);
        return (0);
    }
    switch (k.code) {
    case KEY_ESCAPE:
        return (ESCAPE);
    case KEY_ENTER:
        return ('\r');
    case KEY_TAB:
        return ('\t');
    case KEY_BACKSPACE:
        return ('\b');
    case KEY_DELETE:
        return ('x'); /* vi has no forward delete */
    case KEY_UP:
        return (CTRL('p'));
    case KEY_DOWN:
        return (CTRL('n'));
    case KEY_LEFT:
        return ('\b');
    case KEY_RIGHT:
        return (' ');
    case KEY_HOME:
        return ('^');
    case KEY_END:
        return ('$');
    case KEY_PAGE_UP:
        return (CTRL('b'));
    case KEY_PAGE_DOWN:
        return (CTRL('f'));
    }
    /*
     * ex is a byte editor: TRIM is 0177 throughout and a line is a char
     * array, so a codepoint above ASCII has nowhere to go.
     */
    return (k.code < 0x80 ? (int)k.code : 0);
}

/*
 * Around a shell escape. The order is the shell's own (sh/job.cpp): the
 * keyboard goes back *before* anything is spawned, because a full-screen
 * program claims it in its very first step and a child racing us for it would
 * lose.
 */
Task<void> vspawn_begin(void)
{
    if (vscreen == 0 || !inopen)
        co_return;
    co_await vflush();
    co_await vscreen_give();
}

Task<void> vspawn_end(exbool repaint)
{
    if (vscreen == 0 || !inopen)
        co_return;
    if (Task<Result<void>> t = vscreen_take())
        co_await t;
    /*
     * A filter is halfway through its range when it gets here -- the old
     * lines are gone and the new ones are not read yet -- so it repaints
     * itself afterwards and asks not to be repainted now.
     */
    if (repaint) {
        vresize();
        co_await vflush();
    } else {
        /*
         * The alternate screen comes back blank, and vtube still holds what
         * was on it; the diff in vflush() would find nothing to send. Clearing
         * vtube is what makes the caller's repaint reach the screen.
         */
        vclear();
        vcnt = 0;
    }
}
