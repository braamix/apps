/*	screen.cpp
 *
 *	The screen and the keyboard.  This replaces tcap.c, which looked a
 *	terminal up in termcap and drove it with escape sequences, and posix.c,
 *	which put the tty in raw mode and read bytes out of it.
 *
 *	Braam has neither: the screen is an array of cells the renderer reads
 *	out of linear memory, and a key arrives whole.  So a capability is a
 *	constant, cursor addressing is indexing, and there is no escape
 *	sequence to take apart.
 *
 *	The seam: ttputc() writes into a back buffer, not to a terminal, and
 *	ttflush() only says the frame is ready.  The diff and the blit happen
 *	in ttgetc(), which is the one place the editor parks -- so a keystroke
 *	costs one syscall carrying the cells that changed.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kernel/alloc.h"
#include "proc/io.h"
#include "proc/screen.h"

#include "estruct.h"
#include "globals.h"
#include "efunc.h"
#include "utf8.h"

/* What tcap.c set in struct terminal, less what termcap answered. */
#define MARGIN 8
#define SCRSIZ 64

/*
 * The screen size the back buffer is allocated for: the widest grid the
 * kernel will make, except in columns, where MAXCOL is what display.cpp's
 * mode line and message buffers are.
 */
#define TUBEROWS SCREEN_MAX_ROWS
#define TUBECOLS MAXCOL

struct terminal term = {
    TUBEROWS,            /* t_mrow */
    24,                  /* t_nrow, until the grid says */
    TUBECOLS,            /* t_mcol */
    80,                  /* t_ncol */
    MARGIN,   SCRSIZ, 0, /* t_pause: nothing to wait for */
};

/*
 * The back buffer.  One cell is a codepoint and the attribute in force when
 * it was written; vflush() copies out only what differs from the Grid, which
 * keeps the damage.
 */
struct tcell {
    unicode_t ch;
    unsigned char attrs;
};

static struct tcell *tube;
static int tuberows, tubecols;

/*
 * Send the next frame whole rather than by difference.  The kernel blanks its
 * screen on a resize, keeping only the rows above the cursor, but leaves the
 * Grid we diff against alone when the geometry did not change -- so every
 * cell would compare equal, nothing would be sent, and the screen would stay
 * black below wherever the cursor was.
 */
static int full_blit = TRUE;

static int curx, cury;             /* where ttputc() writes next */
static int reverse;                /* tcaprev() state */
static int lastx = -1, lasty = -1; /* cursor in the frame last sent */

static ProcScreen *scr;
static int have_keys, have_screen, opened;

/* One pushback, and the resize the last key read noticed. */
static int pushback = -1;
int chg_width, chg_height;

static struct tcell *cell_at(int row, int col)
{
    if (!tube || row < 0 || row >= tuberows || col < 0 || col >= tubecols)
        return NULL;
    return &tube[row * tubecols + col];
}

/*
 * Take the grid's size, capped by the back buffer.  A resize is noticed here
 * and reported the way the SIGWINCH handler used to: checkwinsize() acts on
 * it from somewhere that is allowed to paint.
 */
static void note_size(void)
{
    Grid *g;
    int w, h;

    if (!scr)
        return;
    g = &scr->grid();
    w = (int)g->cols;
    h = (int)g->rows;
    if (w > tubecols)
        w = tubecols;
    if (h > tuberows)
        h = tuberows;
    if (w <= 0 || h <= 0)
        return;
    if (h - 1 != term.t_nrow || w != term.t_ncol) {
        chg_width  = w;
        chg_height = h;
    }
}

void getscreensize(int *widthp, int *heightp)
{
    Grid *g;

    *widthp  = 0;
    *heightp = 0;
    if (!scr)
        return;
    g        = &scr->grid();
    *widthp  = (int)g->cols > tubecols ? tubecols : (int)g->cols;
    *heightp = (int)g->rows > tuberows ? tuberows : (int)g->rows;
}

/*
 * Claim the screen and the keyboard, and allocate the back buffer.  Both
 * claims are held by the kernel on the process record; ~Proc gives them back,
 * which it has to -- a killed program runs no destructor of its own.
 */
Task<void> ttopen(void)
{
    int w, h;

    if (!scr) {
        scr = heap_new<ProcScreen>();
        if (!scr) {
            quitting = TRUE;
            co_return;
        }
    }
    if (!tube) {
        tuberows = TUBEROWS;
        tubecols = TUBECOLS;
        tube     = (struct tcell *)calloc((usize)tuberows * tubecols, sizeof(*tube));
        if (!tube) {
            quitting = TRUE;
            co_return;
        }
    }
    if (!have_keys) {
        if (Task<Result<void>> t = scr->take_keys())
            co_await t;
        have_keys = TRUE;
    }
    if (!have_screen) {
        if (Task<Result<void>> t = scr->take_screen())
            co_await t;
        have_screen = TRUE;
    }
    scr->grid().cursor_on = true;

    /*
     * The first time only.  A grid resized during a shell escape would
     * otherwise leave the windows their old height and nothing to notice:
     * term would already agree with it, so checkwinsize() never runs.
     */
    if (!opened) {
        getscreensize(&w, &h);
        if (w > 0 && h > 0) {
            term.t_ncol = (short)w;
            term.t_nrow = (short)(h - 1);
        }
        opened = TRUE;
    }

    /* The kernel blanks its screen when it hands one back. */
    full_blit = TRUE;

    /* We do not know where the cursor is. */
    shown_row = 999;
    shown_col = 999;
    lastx = lasty = -1;
}

/* Give both claims back, screen first: a child races us for the keyboard. */
Task<void> ttclose(void)
{
    if (have_screen) {
        if (Task<Result<Geometry>> t = screen_claim(false))
            co_await t;
        have_screen = FALSE;
    }
    if (have_keys) {
        if (Task<Result<Geometry>> t = keys_claim(false))
            co_await t;
        have_keys = FALSE;
    }
}

/*
 * A character into the back buffer.  Upstream wrote it to the terminal and
 * relied on the terminal to advance the cursor; that is what the ++curx is.
 *
 * The three control characters it sends have to keep meaning what a terminal
 * in raw mode made them mean, because the echo in getstring() rubs a
 * character out with "\b \b" and expects the cursor to have moved.
 */
int ttputc(int c)
{
    struct tcell *p;

    switch (c) {
    case '\b':
        if (curx > 0)
            curx--;
        return 0;
    case '\r':
        curx = 0;
        return 0;
    case '\n': /* raw: a linefeed, no return */
        if (cury + 1 < tuberows)
            cury++;
        return 0;
    case BELL:
        return 0;
    }

    p = cell_at(cury, curx);
    if (p) {
        p->ch    = (unicode_t)c;
        p->attrs = (unsigned char)(reverse ? ATTR_REVERSE : 0);
    }
    curx++;
    return 0;
}

/*
 * Nothing to flush: the frame goes out in ttgetc(), which is where a syscall
 * can be awaited.  Kept because upstream calls it forty times and each of
 * them marks a point the screen is meant to be consistent at.
 */
void ttflush(void)
{
}

void ttpause(void)
{
}

void tcapmove(int row, int col)
{
    cury = row;
    curx = col;
}

void tcapeeol(void)
{
    int x;

    for (x = curx; x < tubecols; x++) {
        struct tcell *p = cell_at(cury, x);

        if (p) {
            p->ch    = ' ';
            p->attrs = 0;
        }
    }
}

void tcapeeop(void)
{
    int y, x;

    for (y = cury; y < tuberows; y++)
        for (x = y == cury ? curx : 0; x < tubecols; x++) {
            struct tcell *p = cell_at(y, x);

            if (p) {
                p->ch    = ' ';
                p->attrs = 0;
            }
        }
}

void tcaprev(int state)
{
    reverse = state;
}

/*
 * The bell.  There is no beep syscall and nothing to write it into, so this
 * is where a visible one would go; a terminal with the bell turned off did
 * exactly this.
 */
void tcapbeep(void)
{
}

/*
 * Upstream's tcapopen()/tcapclose() were the termcap half and ttopen()/
 * ttclose() the tty half; there is one thing to do now, so these are it.
 * They stay separate because spawn.cpp hands the screen over and back and
 * calls all four.
 */
Task<void> tcapopen(void)
{
    if (Task<void> t = ttopen())
        co_await t;
}

Task<void> tcapclose(void)
{
    if (Task<void> t = ttclose())
        co_await t;
}

void tcapkopen(void)
{
    shown_row      = 999;
    shown_col      = 999;
    screen_garbage = TRUE;
}

void tcapkclose(void)
{
}

/*
 * The frame.  A cell reaches the Grid only where it differs, so the damage
 * the Grid accumulates is the union of what actually changed and the blit
 * sends that and no more.
 *
 * Colour is this port's, since a termcap terminal had none: the message line
 * is bright yellow, and the mode line is black on cyan.  Upstream's reverse
 * video becomes that pair rather than riding along with it -- the renderer
 * swaps fg and bg for the attribute, which would undo the colours.
 */
static Task<void> vflush(void)
{
    Grid *g;
    int y, x, rows, cols;
    int whole = full_blit;

    if (!scr || !tube)
        co_return;
    g         = &scr->grid();
    rows      = (int)g->rows < tuberows ? (int)g->rows : tuberows;
    cols      = (int)g->cols < tubecols ? (int)g->cols : tubecols;
    full_blit = FALSE;

    for (y = 0; y < (int)g->rows; y++) {
        unsigned char row_fg = y == term.t_nrow ? COLOR_YELLOW | COLOR_BRIGHT : COLOR_WHITE;

        for (x = 0; x < (int)g->cols; x++) {
            struct tcell *p = y < rows && x < cols ? cell_at(y, x) : NULL;
            Cell *cl        = g->at((u32)x, (u32)y);
            char32_t ch;
            unsigned char fg, bg, attrs;

            if (!cl)
                continue;
            /* Beyond the back buffer there is nothing to say but blank: the
               grid is damaged whole by a reshape and would ship zeroed
               cells, which are black on black. */
            if (!p) {
                if (!whole)
                    continue;
                ch = U' ', attrs = 0, fg = row_fg, bg = COLOR_BLACK;
            } else {
                ch    = p->ch ? (char32_t)p->ch : U' ';
                attrs = p->attrs & ~ATTR_REVERSE;
                fg    = p->attrs & ATTR_REVERSE ? COLOR_BLACK : row_fg;
                bg    = p->attrs & ATTR_REVERSE ? COLOR_CYAN : COLOR_BLACK;
            }
            if (!whole && cl->ch == ch && cl->attrs == attrs && cl->fg == fg && cl->bg == bg)
                continue;
            cl->ch    = ch;
            cl->attrs = attrs;
            cl->fg    = fg;
            cl->bg    = bg;
            g->touch((u32)x, (u32)y, 1, 1);
        }
    }

    g->cursor_x  = shown_col >= 0 && shown_col < cols ? (u32)shown_col : 0;
    g->cursor_y  = shown_row >= 0 && shown_row < rows ? (u32)shown_row : 0;
    g->cursor_on = true;

    /*
     * The cursor rides in the blit's header, and a blit with nothing
     * damaged in it is not sent at all.  A motion changes no cell, so
     * without this the cursor would move in the editor and stay put on
     * the screen.
     */
    if ((int)g->cursor_x != lastx || (int)g->cursor_y != lasty) {
        g->touch(g->cursor_x, g->cursor_y, 1, 1);
        lastx = (int)g->cursor_x;
        lasty = (int)g->cursor_y;
    }
    /* The damage is taken before the blit is awaited, so a frame the kernel
       refuses -- a resize landing under it -- is gone.  Ask for it again. */
    if (Task<Result<void>> t = scr->flush()) {
        if ((co_await t).is_err())
            full_blit = TRUE;
    }
}

/*
 * A Key as the code uemacs expects.
 *
 * A named key takes uEmacs's own PC name for it -- the DOS scan code as a
 * character, which is what emacs.rc lists.  The arrows move off the ANSI
 * FNA..FND the CSI decoder in getcmd() produced, because those are F7 to F10.
 * The number block keeps its upstream meaning, the VT220 keypad; nothing here
 * can send it, but paging means paging in both vocabularies, so the two page
 * keys stay on FN5 and FN6, which is what emacs.rc's help viewer rebinds.
 */
static int key_code(Key k, int *code)
{
    int c;

    *code = 0;

    /* Command, or Super, is the browser's: it acts on the chord and hands it
       over as well, so Cmd-V paged back before the paste arrived. */
    if (k.mods & MOD_META)
        return FALSE;

    /* F1 to F10, and the shifted block.  F11 and F12 are 0x85 and 0x86, which
       have no FN spelling: bind them by pressing the key. */
    if (k.code >= KEY_F1 && k.code <= KEY_F12) {
        int n = (int)(k.code - KEY_F1);

        if (n >= 10)
            *code = SPEC | (0x85 + n - 10);
        else
            *code = SPEC | ((k.mods & MOD_SHIFT ? 'T' : ';') + n);
        return TRUE;
    }

    /* Control on a motion key is the word and buffer motions, which are Meta
       keys here. */
    if (k.mods & MOD_CTRL) {
        switch (k.code) {
        case KEY_LEFT:
            *code = META | 'B';
            return TRUE;
        case KEY_RIGHT:
            *code = META | 'F';
            return TRUE;
        case KEY_HOME:
            *code = META | '<';
            return TRUE;
        case KEY_END:
            *code = META | '>';
            return TRUE;
        case KEY_BACKSPACE:
            *code = META | CONTROL | 'H';
            return TRUE;
        case KEY_DELETE:
            *code = META | 'D';
            return TRUE;
        }
    }

    switch (k.code) {
    case KEY_ENTER:
        *code = 0x0d;
        return TRUE;
    case KEY_TAB:
        *code = 0x09;
        return TRUE;
    case KEY_BACKSPACE:
        *code = 0x08;
        return TRUE;
    case KEY_ESCAPE:
        *code = CONTROL | '[';
        return TRUE;
    case KEY_HOME:
        *code = SPEC | 'G';
        return TRUE;
    case KEY_UP:
        *code = SPEC | 'H';
        return TRUE;
    case KEY_LEFT:
        *code = SPEC | 'K';
        return TRUE;
    case KEY_RIGHT:
        *code = SPEC | 'M';
        return TRUE;
    case KEY_END:
        *code = SPEC | 'O';
        return TRUE;
    case KEY_DOWN:
        *code = SPEC | 'P';
        return TRUE;
    case KEY_INSERT:
        *code = SPEC | 'R';
        return TRUE;
    case KEY_DELETE:
        *code = SPEC | 'S';
        return TRUE;
    case KEY_PAGE_UP:
        *code = SPEC | '5';
        return TRUE;
    case KEY_PAGE_DOWN:
        *code = SPEC | '6';
        return TRUE;
    }
    if (k.code >= KEY_NAMED) /* a named key we do not map */
        return FALSE;

    c = (int)k.code;
    if (k.mods & MOD_CTRL) {
        if (c >= 'a' && c <= 'z')
            c -= 'a' - 1;
        else if (c >= 'A' && c <= 'Z')
            c -= 'A' - 1;
        else if (c == '[' || c == '\\' || c == ']' || c == '^' || c == '_')
            c -= '@';
        else if (c == '@' || c == ' ')
            c = 0; /* C-@ is set-mark, and 0 is a key */
        else if (c == '?')
            c = 0x7f;
    }
    /* Alt is the prefix ESC produces the long way round. */
    if (k.mods & MOD_ALT) {
        if (c >= 'a' && c <= 'z')
            c ^= DIFCASE;
        if (c >= 0x00 && c <= 0x1F)
            c = CONTROL | (c + '@');
        *code = META | c;
        return TRUE;
    }
    *code = c;
    return TRUE;
}

/*
 * One key.  The frame goes out first: this is where the editor parks, and a
 * screen painted after the read would arrive a keystroke late.
 */
Task<int> ttgetc(void)
{
    if (pushback >= 0) {
        int c = pushback;

        pushback = -1;
        co_return c;
    }

    for (;;) {
        Result<Key> r = Err(Error::Closed);
        int c;

        if (Task<void> t = vflush())
            co_await t;
        if (Task<Result<Key>> t = scr->next_key())
            r = co_await t;

        /*
         * Err(Intr) is a signal: the read was abandoned and nothing
         * was typed.  sig_take() says which one.
         *
         * SIG_INT is ^C, which reaches a foreground program as a
         * signal and never as a key.  Handing the keystroke back is
         * what makes upstream's bindings work: ^C is insert-space and
         * C-x ^C is exit-emacs.  It also arrives as an ordinary key
         * once a shell escape has cleared the foreground set, and both
         * routes now mean the same thing.
         *
         * SIG_WINCH is a resize -- next_key() has already taken it
         * and reshaped the grid -- so note the size and paint;
         * checkwinsize() is called from a place that may.
         */
        if (r.is_err()) {
            if (r.error() != Error::Intr)
                co_return 0;
            /*
             * next_key() takes SIG_WINCH itself and reshapes before it
             * reports, so the grid may have moved whatever signal is
             * behind this -- note the size before answering one, or a
             * resize arriving with a ^C is dropped.
             */
            note_size();
            full_blit = TRUE;
            if (sig_take(SIG_TERM)) {
                quitting = TRUE;
                co_return 0;
            }
            if (sig_take(SIG_INT))
                co_return keycode_to_char(CONTROL | 'C');
            co_await checkwinsize();
            continue;
        }
        note_size();
        if (chg_width || chg_height)
            co_await checkwinsize();

        /*
         * A key with nothing behind it -- a function key nothing
         * binds -- is dropped rather than answered.  It cannot be a
         * value: zero is C-@, which is a key, and SPEC is the sign
         * bit, so every named key is negative.
         */
        if (key_code(r.value(), &c))
            co_return c;
    }
}

/*
 * The pause after a shell escape.
 *
 * Upstream wrote "(End)" on the message line and read a key.  Neither half
 * works here: the message line is in the back buffer, which is not sent while
 * the screen claim is the child's, and the keyboard claim went with it -- and
 * taking the screen back is what paints over the output the pause exists to
 * let you read.  So the prompt is bytes on stdout, where the child's output
 * already is, and the keyboard is taken for the one keystroke and given
 * straight back.
 */
Task<void> tcappause(const char *prompt)
{
    if (Task<Result<void>> t = write_all(SYS_STDOUT, Str(prompt, strlen(prompt))))
        co_await t;
    if (Task<Result<Geometry>> t = keys_claim(true))
        co_await t;

    for (;;) {
        Result<KeyPress> r = Err(Error::Closed);

        if (Task<Result<KeyPress>> t = key_read())
            r = co_await t;
        if (r.is_err()) {
            /* ^C is a key here too; anything else Intr means is a resize. */
            if (r.error() == Error::Intr && !sig_take(SIG_INT)) {
                full_blit = TRUE;
                continue;
            }
            break;
        }
        {
            int c;

            if (key_code(Key{ r.value().code, r.value().mods }, &c))
                break;
        }
    }

    if (Task<Result<Geometry>> t = keys_claim(false))
        co_await t;
    if (Task<Result<void>> t = write_all(SYS_STDOUT, "\r\n"))
        co_await t;
}

void ttungetc(int c)
{
    if (pushback < 0)
        pushback = c;
}

/*
 * Whether a key is already waiting.  Upstream asked the tty with FIONREAD and
 * main() used the answer to skip a repaint while somebody was typing ahead.
 * The keyboard queue is the kernel's here and there is no syscall that peeks
 * at it, so only the pushback is known and every keystroke repaints -- which
 * costs the cells that changed, so it is not the bargain it was at 1200 baud.
 * $typahead reports the same.
 */
int typahead(void)
{
    return pushback >= 0;
}
