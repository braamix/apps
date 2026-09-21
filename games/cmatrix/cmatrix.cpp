/**************************************************************************
 *   cmatrix.c                                                            *
 *                                                                        *
 *   Copyright (C) 1999 Chris Allegretta                                  *
 *   This program is free software; you can redistribute it and/or modify *
 *   it under the terms of the GNU General Public License as published by *
 *   the Free Software Foundation; either version 1, or (at your option)  *
 *   any later version.                                                   *
 *                                                                        *
 *   This program is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 *   GNU General Public License for more details.                         *
 *                                                                        *
 *   You should have received a copy of the GNU General Public License    *
 *   along with this program; if not, write to the Free Software          *
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.            *
 *                                                                        *
 **************************************************************************/

// CMatrix 1.2a, Chris Allegretta's Matrix screensaver. ncurses is a
// ProcScreen here; the rain is the same program.

#include "kernel/alloc.h"
#include "kernel/key.h"
#include "kernel/screen.h"
#include "kernel/str.h"
#include "kernel/text.h"
#include "proc/io.h"
#include "proc/opt.h"
#include "proc/rt.h"
#include "proc/screen.h"
#include "proc/usage.h"

namespace {

constexpr Str USAGE =
    " Usage: cmatrix -[abBfhlsVx] [-u delay] [-C color]\n"
    " -a: Synchronous scroll\n"
    " -b: Bold characters on\n"
    " -B: All bold characters (overrides -b)\n"
    " -f: Force the linux $TERM type to be on\n"
    " -l: Linux mode (uses matrix console font)\n"
    " -o: Use old-style scrolling\n"
    " -h: Print usage and exit\n"
    " -n: No bold characters (overrides -b and -B, default)\n"
    " -s: \"Screensaver\" mode, exits on first keystroke\n"
    " -x: X window mode, use if your xterm is using mtx.pcf\n"
    " -V: Print version information and exit\n"
    " -u delay (0 - 10, default 4): Screen update delay\n"
    " -C [color]: Use this color for matrix (default green)\n";

constexpr Str VERSION_TEXT =
    " CMatrix version 1.2a by Chris Allegretta\n"
    " Email: cmatrix@asty.org  Web: http://www.asty.org/cmatrix\n";

constexpr Str BAD_COLOR =
    " Invalid color selection\n Valid "
    "colors are green, red, blue, "
    "white, yellow, cyan, magenta and black.\n";

// Matrix typedef
struct cmatrix {
    int val;
    int bold;
};

// Global variables, unfortunately
int console = 0, xwindow = 0;
cmatrix **matrix = nullptr;
int *length      = nullptr;
int *spaces      = nullptr;
int *updates     = nullptr;

int LINES = 24, COLS = 80;
int matrix_lines = -1;

int screensaver = 0, asynch = 1, bold = -1, oldstyle = 0, update = 4,
    mcolor = COLOR_GREEN, count = 0;
int randnum = 93, randmin = 33, highnum = 123;

bool whole = true;

ProcScreen *scr = nullptr;

// POSIX.1 rand, so a seeded run is the same every time. Upstream's was
// time(NULL).
u32 rand_state = 1;

void srand(u32 seed)
{
    rand_state = seed ? seed : 1;
}

int matrix_rand()
{
    rand_state = rand_state * 1103515245u + 12345u;
    return int((rand_state / 65536u) % 32768u);
}

// CMATRIX_SEED pins the dice, for the tests.
void seed_from_env()
{
    Str env = proc_env("CMATRIX_SEED");
    if (!env.empty()) {
        u32 seed = 0;
        for (usize i = 0; i < env.size(); i++) {
            if (env[i] < '0' || env[i] > '9')
                break;
            seed = seed * 10 + u32(env[i] - '0');
        }
        srand(seed);
        return;
    }
    srand(proc_random());
}

bool eq_ci(Str a, Str b)
{
    if (a.size() != b.size())
        return false;
    for (usize i = 0; i < a.size(); i++) {
        char ca = a[i], cb = b[i];
        if (ca >= 'A' && ca <= 'Z')
            ca = char(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z')
            cb = char(cb - 'A' + 'a');
        if (ca != cb)
            return false;
    }
    return true;
}

int color_of(Str s)
{
    if (eq_ci(s, "green"))
        return COLOR_GREEN;
    if (eq_ci(s, "red"))
        return COLOR_RED;
    if (eq_ci(s, "blue"))
        return COLOR_BLUE;
    if (eq_ci(s, "white"))
        return COLOR_WHITE;
    if (eq_ci(s, "yellow"))
        return COLOR_YELLOW;
    if (eq_ci(s, "cyan"))
        return COLOR_CYAN;
    if (eq_ci(s, "magenta"))
        return COLOR_MAGENTA;
    if (eq_ci(s, "black"))
        return COLOR_BLACK;
    return -1;
}

// nmalloc from nano by Big Gaute
void *nmalloc(usize howmuch)
{
    if (howmuch == 0)
        howmuch = 1;
    void *r = heap_alloc(howmuch);
    if (r)
        __builtin_memset(r, 0, howmuch);
    return r;
}

void var_free()
{
    if (matrix != nullptr) {
        if (matrix_lines >= 0) {
            for (int i = 0; i <= matrix_lines; i++)
                heap_free(matrix[i]);
        }
        heap_free(matrix);
        matrix = nullptr;
    }
    if (length != nullptr) {
        heap_free(length);
        length = nullptr;
    }
    if (spaces != nullptr) {
        heap_free(spaces);
        spaces = nullptr;
    }
    if (updates != nullptr) {
        heap_free(updates);
        updates = nullptr;
    }
    matrix_lines = -1;
}

// Initialize the global variables
bool var_init()
{
    int i, j;

    var_free();

    if (LINES < 1 || COLS < 1)
        return false;

    matrix = static_cast<cmatrix **>(nmalloc(sizeof(cmatrix *) * usize(LINES + 1)));
    if (!matrix)
        return false;
    matrix_lines = LINES;
    for (i = 0; i <= LINES; i++) {
        matrix[i] = static_cast<cmatrix *>(nmalloc(sizeof(cmatrix) * usize(COLS)));
        if (!matrix[i])
            return false;
    }

    length = static_cast<int *>(nmalloc(usize(COLS) * sizeof(int)));
    if (!length)
        return false;
    spaces = static_cast<int *>(nmalloc(usize(COLS) * sizeof(int)));
    if (!spaces)
        return false;
    updates = static_cast<int *>(nmalloc(usize(COLS) * sizeof(int)));
    if (!updates)
        return false;

    int len_span = LINES - 3;
    if (len_span < 1)
        len_span = 1;

    /* Make the matrix */
    for (i = 0; i <= LINES; i++)
        for (j = 0; j <= COLS - 1; j += 2)
            matrix[i][j].val = -1;

    for (j = 0; j <= COLS - 1; j += 2) {
        /* Set up spaces[] array of how many spaces to skip */
        spaces[j] = (int)matrix_rand() % LINES + 1;

        /* And length of the stream */
        length[j] = (int)matrix_rand() % len_span + 3;

        /* Sentinel value for creation of new objects */
        matrix[1][j].val = ' ';

        /* And set updates[] array for update speed. */
        updates[j] = (int)matrix_rand() % 3 + 1;
    }
    return true;
}

constexpr usize RING = 64;

struct KeyRing {
    int code[RING];
    usize head;
    usize tail;
    bool resized;
    bool closed;
    bool quit;
};

KeyRing ring;

void ring_push(int code)
{
    usize next = (ring.tail + 1) % RING;
    if (next == ring.head)
        return;
    ring.code[ring.tail] = code;
    ring.tail            = next;
}

bool ring_pop(int &code)
{
    if (ring.head == ring.tail)
        return false;
    code      = ring.code[ring.head];
    ring.head = (ring.head + 1) % RING;
    return true;
}

Task<i32> keyboard(ProcScreen *s)
{
    for (;;) {
        Result<Key> r = co_await s->next_key();
        if (r.is_err()) {
            if (r.error() == Error::Intr) {
                if (sig_take(SIG_INT))
                    ring.quit = true;
                else
                    ring.resized = true;
                continue;
            }
            if (r.error() == Error::Again)
                continue;
            ring.closed = true;
            co_return 0;
        }
        const Key &k = r.value();
        if (k.mods & (MOD_CTRL | MOD_ALT))
            continue;
        ring_push(int(k.code));
    }
}

void put_cell(Grid &g, int y, int x, char32_t ch, u8 fg, u8 attrs)
{
    Cell *c = g.at(u32(x), u32(y));
    if (!c)
        return;
    if (ch == 0)
        ch = ' ';
    if (!whole && c->ch == ch && c->fg == fg && c->bg == u8(COLOR_BLACK) && c->attrs == attrs)
        return;
    c->ch    = ch;
    c->fg    = fg;
    c->bg    = u8(COLOR_BLACK);
    c->attrs = attrs;
    g.touch(u32(x), u32(y), 1, 1);
}

void clear_grid(Grid &g)
{
    for (u32 y = 0; y < g.rows; y++)
        for (u32 x = 0; x < g.cols; x++)
            put_cell(g, int(y), int(x), ' ', u8(COLOR_WHITE), 0);
    whole = true;
}

bool process_key(int keypress)
{
    if (screensaver == 1)
        return true;
    switch (keypress) {
    case 'q':
        return true;
    case 'a':
        asynch = 1 - asynch;
        break;
    case 'b':
        bold = 1;
        break;
    case 'B':
        bold = 2;
        break;
    case 'n':
        bold = 0;
        break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        update = keypress - 48;
        break;
    case '!':
        mcolor = COLOR_RED;
        break;
    case '@':
        mcolor = COLOR_GREEN;
        break;
    case '#':
        mcolor = COLOR_YELLOW;
        break;
    case '$':
        mcolor = COLOR_BLUE;
        break;
    case '%':
        mcolor = COLOR_MAGENTA;
        break;
    case '^':
        mcolor = COLOR_CYAN;
        break;
    case '&':
        mcolor = COLOR_WHITE;
        break;
    }
    return false;
}

char32_t glyph(int val)
{
    if (val == 0) {
        if (console || xwindow)
            return 183;
        return '&';
    }
    if (val == 1)
        return '|';
    if (val == -1)
        return ' ';
    if (val < 0)
        return ' ';
    return char32_t(val);
}

// Not a coroutine: the column walk belongs on the stack, not in the frame.
__attribute__((noinline)) void step_and_draw(Grid &g)
{
    int i, j = 0, y, z, firstcoldone = 0, random = 0;

    count++;
    if (count > 4)
        count = 1;

    for (j = 0; j <= COLS - 1; j += 2) {
        if (count > updates[j] || asynch == 0) {

            /* I dont like old-style scrolling, yuck */
            if (oldstyle) {
                for (i = LINES - 1; i >= 1; i--)
                    matrix[i][j].val = matrix[i - 1][j].val;

                random = (int)matrix_rand() % (randnum + 8) + randmin;

                if (matrix[1][j].val == 0)
                    matrix[0][j].val = 1;
                else if (matrix[1][j].val == ' ' || matrix[1][j].val == -1) {
                    if (spaces[j] > 0) {
                        matrix[0][j].val = ' ';
                        spaces[j]--;
                    } else {

                        /* Random number to determine whether head of next collumn
                           of chars has a white 'head' on it. */

                        if (((int)matrix_rand() % 3) == 1)
                            matrix[0][j].val = 0;
                        else
                            matrix[0][j].val = (int)matrix_rand() % randnum + randmin;

                        spaces[j] = (int)matrix_rand() % LINES + 1;
                    }
                } else if (random > highnum && matrix[1][j].val != 1)
                    matrix[0][j].val = ' ';
                else
                    matrix[0][j].val = (int)matrix_rand() % randnum + randmin;

            } else { /* New style scrolling (default) */
                if (matrix[0][j].val == -1 && matrix[1][j].val == ' ' && spaces[j] > 0) {
                    matrix[0][j].val = -1;
                    spaces[j]--;
                } else if (matrix[0][j].val == -1 && matrix[1][j].val == ' ') {
                    int len_span = LINES - 3;
                    if (len_span < 1)
                        len_span = 1;
                    length[j]        = (int)matrix_rand() % len_span + 3;
                    matrix[0][j].val = (int)matrix_rand() % randnum + randmin;

                    if ((int)matrix_rand() % 2 == 1)
                        matrix[0][j].bold = 2;

                    spaces[j] = (int)matrix_rand() % LINES + 1;
                }
                i            = 0;
                y            = 0;
                firstcoldone = 0;
                while (i <= LINES) {

                    /* Skip over spaces */
                    while (i <= LINES && (matrix[i][j].val == ' ' || matrix[i][j].val == -1))
                        i++;

                    if (i > LINES)
                        break;

                    /* Go to the head of this collumn */
                    z = i;
                    y = 0;
                    while (i <= LINES && (matrix[i][j].val != ' ' && matrix[i][j].val != -1)) {
                        i++;
                        y++;
                    }

                    if (i > LINES) {
                        matrix[z][j].val    = ' ';
                        matrix[LINES][j].bold = 1;
                        continue;
                    }

                    matrix[i][j].val = (int)matrix_rand() % randnum + randmin;

                    if (matrix[i - 1][j].bold == 2) {
                        matrix[i - 1][j].bold = 1;
                        matrix[i][j].bold     = 2;
                    }

                    /* If we're at the top of the collumn and it's reached its
                     * full length (about to start moving down), we do this
                     * to get it moving.  This is also how we keep segments not
                     * already growing from growing accidentally =>
                     */
                    if (y > length[j] || firstcoldone) {
                        matrix[z][j].val = ' ';
                        matrix[0][j].val = -1;
                    }
                    firstcoldone = 1;
                    i++;
                }
            }
        }
        /* Hack =P */
        if (!oldstyle) {
            y = 1;
            z = LINES;
        } else {
            y = 0;
            z = LINES - 1;
        }
        for (i = y; i <= z; i++) {
            int val  = matrix[i][j].val;
            int mbld = matrix[i][j].bold;
            u8 fg;
            u8 attrs = 0;
            char32_t ch;

            if (val == 0 || mbld == 2) {
                fg = u8(COLOR_WHITE);
                if (bold)
                    attrs = ATTR_BOLD;
                ch = glyph(val);
            } else {
                fg = u8(mcolor);
                if (val == 1) {
                    if (bold)
                        attrs = ATTR_BOLD;
                    ch = '|';
                } else {
                    if (bold == 2 || (bold == 1 && val % 2 == 0))
                        attrs = ATTR_BOLD;
                    ch = glyph(val);
                }
            }
            put_cell(g, i - y, j, ch, fg, attrs);
        }
    }
}

void size_from_grid(Grid &g)
{
    LINES = int(g.rows);
    COLS  = int(g.cols);
}

u32 frame_ms()
{
    // napms(update * 10). Zero still parks, or ^C has no window.
    if (update <= 0)
        return 1;
    return u32(update) * 10;
}

} // namespace

Task<i32> proc_main(Args args)
{
    if (help_asked(args)) {
        if ((co_await write_all(SYS_STDOUT, USAGE)).is_err())
            co_return 1;
        co_return 0;
    }

    OptParse parse(args, Opts{ "abBfhlnosxV", "uC" });
    for (Opt o;;) {
        Result<bool> r = parse.next(o);
        if (r.is_err()) {
            if ((co_await write_all(SYS_STDOUT, USAGE)).is_err())
                co_return 1;
            co_return 0;
        }
        if (!r.value())
            break;
        switch (o.name) {
        case 's':
            screensaver = 1;
            break;
        case 'a':
            asynch = 0;
            break;
        case 'b':
            if (bold != 2 && bold != 0)
                bold = 1;
            break;
        case 'B':
            if (bold != 0)
                bold = 2;
            break;
        case 'C': {
            int c = color_of(o.value);
            if (c < 0) {
                if ((co_await write_all(SYS_STDOUT, BAD_COLOR)).is_err())
                    co_return 1;
                co_return 1;
            }
            mcolor = c;
            break;
        }
        case 'f':
            break;
        case 'l':
            console = 1;
            break;
        case 'n':
            bold = 0;
            break;
        case 'h':
            if ((co_await write_all(SYS_STDOUT, USAGE)).is_err())
                co_return 1;
            co_return 0;
        case 'o':
            oldstyle = 1;
            break;
        case 'u': {
            Option<u32> n = parse_u32(o.value);
            update        = n.has_value() ? int(n.value()) : 0;
            break;
        }
        case 'x':
            xwindow = 1;
            break;
        case 'V':
            if ((co_await write_all(SYS_STDOUT, VERSION_TEXT)).is_err())
                co_return 1;
            co_return 0;
        default:
            if ((co_await write_all(SYS_STDOUT, USAGE)).is_err())
                co_return 1;
            co_return 0;
        }
    }

    /* If bold hasn't been turned on or off yet, assume off */
    if (bold == -1)
        bold = 0;

    seed_from_env();

    /* Set up values for random number generation */
    if (console || xwindow) {
        randnum = 51;
        randmin = 166;
        highnum = 217;
    } else {
        randnum = 93;
        randmin = 33;
        highnum = 123;
    }

    scr = heap_new<ProcScreen>();
    if (!scr) {
        co_await errln("cmatrix", "the screen", Error::NoMemory);
        co_return 1;
    }
    if ((co_await scr->take_keys()).is_err()) {
        co_await write_all(SYS_STDERR, "cmatrix: no keyboard\n");
        co_return 1;
    }
    if ((co_await scr->take_screen()).is_err()) {
        co_await write_all(SYS_STDERR, "cmatrix: no screen\n");
        co_return 1;
    }

    Grid &g = scr->grid();
    size_from_grid(g);
    g.cursor_on = false;
    if (!var_init()) {
        co_await errln("cmatrix", "the matrix", Error::NoMemory);
        co_return 1;
    }
    clear_grid(g);

    if (Task<Result<void>> t = sig_catch(SIG_INT))
        co_await t;

    if (!proc_spawn(keyboard(scr))) {
        co_await errln("cmatrix", "the keyboard task", Error::NoMemory);
        co_return 1;
    }

    while (true) {
        if (ring.closed) {
            co_await errln("cmatrix", "the keyboard", Error::Closed);
            co_return 1;
        }
        if (ring.quit)
            break;
        if (ring.resized) {
            ring.resized = false;
            size_from_grid(g);
            if (!var_init())
                co_return 1;
            whole = true;
            clear_grid(g);
        }

        int keypress;
        if (ring_pop(keypress)) {
            if (process_key(keypress))
                break;
        }

        step_and_draw(g);

        g.cursor_on = false;
        if (whole)
            g.touch(0, 0, g.cols, g.rows);
        Result<void> flushed = co_await scr->flush();
        whole                = flushed.is_err();

        Result<void> slept = co_await sleep_for(frame_ms());
        if (slept.is_ok())
            continue;
        if (slept.error() == Error::Cancelled)
            co_return 130;
        if (slept.error() == Error::Intr) {
            if (sig_take(SIG_INT) || ring.quit)
                break;
            continue;
        }
        co_return 1;
    }
    co_return 0;
}
