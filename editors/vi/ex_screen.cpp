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

/* Where the cursor was in the frame last sent. */
static u32 vcurx, vcury;

/* Whether the keyboard claim is ours; ProcScreen keeps its own copy private. */
static exbool vkeys;

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
    vkeys = 1;
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
    vkeys = 0;
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
 * A vtube cell and a Cell both hold a codepoint, so this is a copy. A zero in
 * vtube means "never written" and QUOTE means "part of an expanded tab"; both
 * draw as a blank, which is what vputchar() already intends by them.
 *
 * Colour is this port's, since a termcap terminal had none: the echo area is
 * cyan and the ~ of a line past the end of the file is blue. A row is one or
 * the other by where it is and what is on it -- vclrlin() leaves a ~ alone in
 * column zero -- so nothing else has to keep track.
 *
 * So is the mode line. It is written over the echo area rather than into
 * vtube, which is vi's own idea of the screen and would have to be cleared
 * again: the cells under it differ from vtube while it shows, and that is
 * what puts the echo area back when the insertion ends.
 */
static const char MODELINE[] = "-- INSERT --";

Task<Result<void>> vflush(void)
{
    Grid *g;
    int y, x;
    int mode = inserting ? (int)sizeof(MODELINE) - 1 : 0;

    if (vscreen == 0)
        co_return {};
    g = &vscreen->grid();
    for (y = 0; y <= WECHO && y < (int)g->rows; y++) {
        int *tp  = vtube[y];
        char *ap = vatube0 && tp ? vatube0 + (tp - vtube0) : 0;
        u8 fg    = COLOR_WHITE;

        if (tp == 0)
            continue;
        if (y == WECHO)
            fg = COLOR_CYAN;
        else if ((tp[0] & TRIM) == '~' && (WCOLS < 2 || tp[1] == 0))
            fg = COLOR_BLUE;
        for (x = 0; x < WCOLS && x < (int)g->cols; x++) {
            /* The mode line has the echo area to itself while it is up. */
            exbool over = mode && y == WECHO;
            exbool note = over && x < mode;
            int c       = note ? MODELINE[x] : over ? 0 : tp[x] & TRIM;
            char32_t ch = c ? (char32_t)c : U' ';
            u8 at       = over || ap == 0 ? 0 : (u8)ap[x];
            u8 f        = note ? (u8)(COLOR_WHITE | COLOR_BRIGHT) : fg;
            Cell *cl    = g->at((u32)x, (u32)y);

            if (cl == 0 || (cl->ch == ch && cl->attrs == at && cl->fg == f))
                continue;
            cl->ch    = ch;
            cl->attrs = at;
            cl->fg    = f;
            cl->bg    = COLOR_BLACK;
            g->touch((u32)x, (u32)y, 1, 1);
        }
    }
    g->cursor_x  = destcol >= 0 && destcol < (int)g->cols ? (u32)destcol : 0;
    g->cursor_y  = destline >= 0 && destline < (int)g->rows ? (u32)destline : 0;
    g->cursor_on = true;

    /*
     * The cursor rides in the blit's header, and a blit with no damage in it
     * is not sent at all (proc/screen.cpp). A motion changes no cell, so
     * without this h, j, k, l and every other motion would move the cursor in
     * the editor and leave it where it was on the screen. Damaging the cell
     * under it is what carries the header out.
     */
    if (g->cursor_x != vcurx || g->cursor_y != vcury) {
        g->touch(g->cursor_x, g->cursor_y, 1, 1);
        vcurx = g->cursor_x;
        vcury = g->cursor_y;
    }
    co_return co_await vscreen->flush();
}

/*
 * Erase the screen: CL, where the screen is the Grid. The diff in vflush()
 * reaches only the cells vtube covers, so a row past WECHO, a column past
 * WCOLS, or what a shell escape left would otherwise stay.
 */
void vscreen_erase(void)
{
    Grid *g;
    u32 i, n;

    if (vscreen == 0)
        return;
    g = &vscreen->grid();
    if (g->cells == 0)
        return;
    n = g->rows * g->cols;
    for (i = 0; i < n; i++) {
        Cell *cl  = &g->cells[i];
        cl->ch    = U' ';
        cl->attrs = 0;
        cl->fg    = COLOR_WHITE;
        cl->bg    = COLOR_BLACK;
    }
    g->touch(0, 0, g->cols, g->rows);
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
    short peek;

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
     *
     * Not while splitw: the echo line owns linebuf then -- readecho() empties
     * it to compose a : command in genbuf -- and vsave() would take that empty
     * line for an edit and write it over the real one.
     */
    if (!splitw)
        vsave();
    WCOLS = COLUMNS;
    vsetsiz(value(WINDOW));
    setwind();
    /*
     * vok() clears Peekkey, which is right when visual starts and wrong here:
     * a resize and the retake after a shell escape both happen with a key
     * already pushed back.
     */
    peek = Peekkey;
    vok(atube);
    Peekkey = peek;
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
 * again. A cursor key answers a sentinel instead of a byte (ex_tune.h), so
 * that insert mode can tell it from the same character typed.
 */
int key_byte(Key k)
{
    /* A named key means the same thing whatever is held down with it. */
    switch (k.code) {
    case KEY_ESCAPE:
        return (KESC);
    case KEY_ENTER:
        return ('\r');
    case KEY_TAB:
        return ('\t');
    case KEY_BACKSPACE:
        return ('\b');
    case KEY_DELETE:
        return (KDEL);
    case KEY_UP:
        return (KUP);
    case KEY_DOWN:
        return (KDOWN);
    case KEY_LEFT:
        return (KLEFT);
    case KEY_RIGHT:
        return (KRIGHT);
    case KEY_HOME:
        return (KHOME);
    case KEY_END:
        return (KEND);
    case KEY_PAGE_UP:
        return (KPGUP);
    case KEY_PAGE_DOWN:
        return (KPGDN);
    }
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
    /* A line is UTF-8, and getbr() hands the rest of the sequence back. */
    return ((int)k.code);
}

/*
 * A cursor key as the command decoder wants it: the arrows answer ^P, ^N, ^H
 * and space rather than k, j, h and l, so that a :map on them still works.
 * Anything else is itself.
 */
int keycmd(int c)
{
    switch (c) {
    case KESC:
        return (ESCAPE);
    case KUP:
        return (CTRL('p'));
    case KDOWN:
        return (CTRL('n'));
    case KLEFT:
        return ('\b');
    case KRIGHT:
        return (' ');
    case KHOME:
        return ('0'); /* column 0, not the first non-blank */
    case KEND:
        return ('$');
    case KPGUP:
        return (CTRL('b'));
    case KPGDN:
        return (CTRL('f'));
    case KDEL:
        return ('x'); /* vi has no forward delete */
    }
    return (c);
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
    OutcharFn save;

    if (vscreen == 0 || !inopen)
        co_return;
    if (Task<Result<void>> t = vscreen_take())
        co_await t;
    /*
     * The escape left Outchar at termchar and half the redraw below goes
     * through it, so without this the whole screen goes to the console as
     * bytes. fixol() puts it back for good, later.
     */
    save    = Outchar;
    Outchar = vputchar;
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
    Outchar = save;
}

/* A newline unless the console cursor is in column zero already. */
static Task<void> freshline(void)
{
    Result<CursorAt> at = Err(Error::NoMemory);

    if (Task<Result<CursorAt>> t = cursor_get())
        at = co_await t;
    if (at.is_ok() && res_of(at).x != 0)
        if (Task<Result<void>> t = write_all(SYS_STDOUT, "\n"))
            co_await t;
}

/*
 * [Hit return to continue], and the key that answers it. Answers the key as
 * vi's decoder spells it, or zero when there is no keyboard to wait on.
 *
 * Neither half goes vi's way. The prompt cannot go through vtube: what it is
 * protecting was written to stdout, which lands on the same cells the Grid
 * does, and vflush()'s diff would put vtube back over it. The key cannot come
 * from getkey(), which begins with that very vflush() -- and during a shell
 * escape has no keyboard to read.
 */
Task<int> vpause_key(void)
{
    exbool took = 0;
    int c       = 0;

    /* Twice: once for the buffered output, once for the prompt under it. */
    co_await freshline();
    co_await exflush();
    co_await freshline();
    if (Task<Result<void>> t = write_all(SYS_STDOUT, "[Hit return to continue]"))
        co_await t;

    /* Claimed for this one keystroke: a claim held twice is Err(Perm). */
    if (!vkeys) {
        Result<Geometry> r = Err(Error::NoMemory);

        if (Task<Result<Geometry>> t = keys_claim(true))
            r = co_await t;
        if (r.is_err()) {
            if (Task<Result<void>> t = write_all(SYS_STDOUT, "\n"))
                co_await t;
            co_return (0);
        }
        took = 1;
    }
    for (;;) {
        Result<KeyPress> r = Err(Error::NoMemory);

        if (Task<Result<KeyPress>> t = key_read())
            r = co_await t;
        if (r.is_ok()) {
            Key k;

            k.code = res_of(r).code;
            k.mods = res_of(r).mods;
            c      = key_byte(k);
            break;
        }
        /* Intr is ^C, which answers, or a resize, which vspawn_end() takes. */
        if (r.error() == Error::Intr) {
            if (sig_take(SIG_INT)) {
                c = ATTN;
                break;
            }
            continue;
        }
        if (r.error() != Error::Again)
            break;
    }
    if (took)
        if (Task<Result<Geometry>> t = keys_claim(false))
            co_await t;
    if (Task<Result<void>> t = write_all(SYS_STDOUT, "\n"))
        co_await t;
    co_return (c);
}
