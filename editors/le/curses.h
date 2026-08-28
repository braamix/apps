// The whole of ncurses LE needs, over Braam's Grid.
//
// There is no byte stream and no escape sequence here: the screen is an array
// of Cells with fg, bg and attrs as fields, so cursor addressing is indexing
// and a colour is a number. That makes this a shim rather than a terminal
// driver -- what is left of curses is the cell writers, the attribute word and
// the colour pairs.
//
// refresh() does not send anything. It raises a flag; the one flush is in
// getch.cpp, just before the process parks on a key. So every painter in
// screen.cpp, window.cpp, menu.cpp and frames.cpp stays an ordinary function.
#pragma once

#include "kernel/screen.h"
#include "kernel/types.h"
#include "proc/screen.h"

// The attribute word. Upstream's layout, this tree's widths: 21 bits of
// codepoint rather than 8 of byte, and 6 bits of colour pair.
typedef u32 chtype;
typedef u32 attr_t;

enum : chtype {
    A_CHARTEXT   = 0x001FFFFF,
    A_NORMAL     = 0,
    A_BOLD       = 0x00200000,
    A_UNDERLINE  = 0x00400000,
    A_REVERSE    = 0x00800000,
    A_DIM        = 0x01000000,
    A_ALTCHARSET = 0x02000000,
    A_STANDOUT   = A_REVERSE,
    A_ATTRIBUTES = 0xFFE00000,
    A_COLOR      = 0xFC000000,
};

enum { COLOR_PAIR_SHIFT = 26, COLOR_PAIRS = 64 };

#define COLOR_PAIR(n)  ((chtype)((chtype)(n) << COLOR_PAIR_SHIFT) & A_COLOR)
#define PAIR_NUMBER(a) (int)(((chtype)(a) & A_COLOR) >> COLOR_PAIR_SHIFT)

// COLOR_BLACK..COLOR_WHITE are kernel/screen.h's, and they are curses'
// numbers already -- which is why nothing translates.
enum { COLORS = 8 };

enum { ERR = -1, OK = 0 };

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

// There is one screen and no windows, so every WINDOW * a call takes is this,
// and every callee ignores it.
typedef void WINDOW;
extern WINDOW *stdscr;

// A wide cell, ncurses' shape, so screen.cpp's cchar_t building is unchanged.
// Two rather than ncurses' five: a cell holds one codepoint here and the
// renderer composes no combining marks, so the rest were never drawn -- and
// Redisplay's line buffer is one of these per column.
enum { CCHARW_MAX = 2 };

struct cchar_t {
    attr_t attr;
    wchar_t chars[CCHARW_MAX];
};

// Line drawing. Unicode directly: there is no alternate character set to
// switch into, and the renderer has the glyphs.
extern const cchar_t WACS_ULCORNER_c, WACS_URCORNER_c, WACS_LLCORNER_c, WACS_LRCORNER_c,
    WACS_HLINE_c, WACS_VLINE_c, WACS_CKBOARD_c, WACS_LTEE_c, WACS_RTEE_c;

#define WACS_ULCORNER (&WACS_ULCORNER_c)
#define WACS_URCORNER (&WACS_URCORNER_c)
#define WACS_LLCORNER (&WACS_LLCORNER_c)
#define WACS_LRCORNER (&WACS_LRCORNER_c)
#define WACS_HLINE    (&WACS_HLINE_c)
#define WACS_VLINE    (&WACS_VLINE_c)
#define WACS_CKBOARD  (&WACS_CKBOARD_c)
#define WACS_LTEE     (&WACS_LTEE_c)
#define WACS_RTEE     (&WACS_RTEE_c)

#define ACS_ULCORNER (A_ALTCHARSET | 0x250C)
#define ACS_URCORNER (A_ALTCHARSET | 0x2510)
#define ACS_LLCORNER (A_ALTCHARSET | 0x2514)
#define ACS_LRCORNER (A_ALTCHARSET | 0x2518)
#define ACS_HLINE    (A_ALTCHARSET | 0x2500)
#define ACS_VLINE    (A_ALTCHARSET | 0x2502)
#define ACS_CKBOARD  (A_ALTCHARSET | 0x2592)
#define ACS_LTEE     (A_ALTCHARSET | 0x251C)
#define ACS_RTEE     (A_ALTCHARSET | 0x2524)

extern int LINES, COLS;

// The screen.
//
// The two halves that are syscalls are Tasks and everything else is not, which
// is the whole reason this shim exists in the shape it does.
Task<Result<void>> curses_open();  // initscr, cbreak, noecho, keypad
Task<Result<void>> curses_flush(); // what refresh() only asked for

ProcScreen &curses_screen();
Grid &curses_grid();

// Send the next frame whole rather than by difference. The kernel blanks its
// screen on a resize but leaves the Grid alone, so every cell would compare
// equal and nothing would go out.
void curses_full_blit();

// The geometry moved under us: resize and repaint everything.
void curses_resized();

int endwin();

int start_color();
int use_default_colors();
bool has_colors();
int init_pair(short pair, short fg, short bg);
int pair_content(short pair, short *fg, short *bg);

// Modes. Braam has no line discipline, so these only record what they can.
int cbreak();
int noecho();
int nonl();
int raw();
int meta(WINDOW *win, bool on);
int intrflush(WINDOW *win, bool on);
int keypad(WINDOW *win, bool on);
int idlok(WINDOW *win, bool on);
int scrollok(WINDOW *win, bool on);
int leaveok(WINDOW *win, bool on);
int clearok(WINDOW *win, bool on);
int curs_set(int visibility);

int move(int y, int x);
int getcury(WINDOW *win = nullptr);
int getcurx(WINDOW *win = nullptr);
#define getyx(win, y, x) ((void)(win), (y) = getcury(), (x) = getcurx())

int attrset(chtype a);
int bkgdset(chtype a);

int clear();
int erase();
int clrtoeol();

int addch(chtype c);
int mvaddch(int y, int x, chtype c);
int mvaddstr(int y, int x, const char *s);
int addnwstr(const wchar_t *s, int n);
int mvaddchnstr(int y, int x, const chtype *s, int n);
int add_wch(const cchar_t *c);
int mvadd_wch(int y, int x, const cchar_t *c);
int mvadd_wchnstr(int y, int x, const cchar_t *s, int n);
int mvin_wch(int y, int x, cchar_t *out);
chtype mvinch(int y, int x);

int refresh();
int doupdate();
int reset_prog_mode();

int beep();
int flushinp();

// The pushback queue. Upstream's, kept: the key decoder puts a whole sequence
// back when a prefix turns out not to be one.
int ungetch(int key);
int curses_unget_pending();
int curses_unget_take();
