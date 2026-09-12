// The whole of curses eh needs, over Braam's Grid. Trimmed from editors/le's
// shim: no colour, no windows, no wide-cell interface.
//
// There is no byte stream and no escape sequence here: the screen is an array
// of Cells with fg, bg and attrs as fields, so cursor addressing is indexing.
//
// refresh() does not send anything. It raises a flag; the one flush is in
// getch.cpp, just before the process parks on a key. That is what keeps
// display() and every painter in eh.cpp an ordinary function.
#pragma once

#include "kernel/screen.h"
#include "kernel/types.h"
#include "proc/screen.h"

// The attribute word. Upstream's layout, this tree's widths: 21 bits of
// codepoint rather than 8 of byte.
typedef u32 chtype;

enum : chtype {
    A_CHARTEXT   = 0x001FFFFF,
    A_NORMAL     = 0,
    A_BOLD       = 0x00200000,
    A_UNDERLINE  = 0x00400000,
    A_REVERSE    = 0x00800000,
    A_STANDOUT   = A_REVERSE,
    A_ATTRIBUTES = 0xFFE00000,
};

enum { ERR = -1, OK = 0 };

// There is one screen and no windows, so every WINDOW * a call takes is this,
// and every callee ignores it.
typedef void WINDOW;
extern WINDOW *stdscr;

// The named keys are the kernel's (kernel/key.h), which cmds[] can use as they
// are: they sit above the Unicode range, so eh's own `255 < ch` guard still
// tells them from a byte. Only the three curses spells differently are named
// here. There is no KEY_BTAB: Shift-Tab decodes to a tab, see getch.cpp.
enum {
    KEY_NPAGE = KEY_PAGE_DOWN,
    KEY_PPAGE = KEY_PAGE_UP,
    KEY_DC    = KEY_DELETE,
};

extern int LINES, COLS;

// The two halves that are syscalls are Tasks and everything else is not, which
// is the whole reason this shim exists in the shape it does.
Task<Result<void>> curses_open();  // initscr, raw, noecho, keypad
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

// Modes. Braam has no line discipline, so these only record what they can.
int cbreak();
int noecho();
int echo();
int nonl();
int nl();
int raw();
int noraw();
int keypad(WINDOW *win, bool on);

int move(int y, int x);
int getcury(WINDOW *win = nullptr);
int getcurx(WINDOW *win = nullptr);

int attrset(chtype a);
int attron(chtype a);
int attroff(chtype a);
int standout();
int standend();

int clear();
int erase();
int clrtoeol();

int addch(chtype c);
int mvaddch(int y, int x, chtype c);

// UTF-8 in, one cell per rune out. Upstream hands these whole multibyte
// sequences and expects a single column; `n` counts bytes, not cells.
int addstr(const char *s);
int addnstr(const char *s, int n);
int mvaddstr(int y, int x, const char *s);
int mvaddnstr(int y, int x, const char *s, int n);

int printw(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

int refresh();
int beep();
int delwin(WINDOW *win);

// The pushback queue. Upstream builds a dozen commands out of it -- openo() is
// ungetstr("$a\n\033hi") -- and prompt() primes a whole filename through it, so
// it holds a line and not a key.
int ungetch(int key);
int curses_unget_pending();
int curses_unget_take();

// ------------------------------------------------------------------ input
//
// The only three things here that block, and therefore the only reason any of
// eh.cpp is a coroutine. getch.cpp.

Task<int> getch();

// One line, echoed into row y from column x. The pushback is drained first, so
// prompt()'s primed default arrives as if it had been typed.
Task<int> mvgetnstr(int y, int x, char *buf, int n);

// Set when the geometry moved and cleared by whoever repaints. A bare resize
// returns ERR from getch(), with this raised.
extern int curses_resize_flag;
