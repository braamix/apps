// The screen eh paints, over Braam's Grid.
//
// The screen is an array of Cells with fg, bg and attrs as fields, so cursor
// addressing is indexing. Every writer takes the cell and the attribute; there
// is no current position and no current attribute.
//
// Painting sends nothing. The one flush is in getch.cpp, just before the
// process parks, which is what keeps display() an ordinary function.
//
// The names are eh_*: kernel/screen.h, which this header must include, has
// screen_flush(), screen_put(), screen_cursor() and screen_clear() of its own.
#pragma once

#include "kernel/key.h"
#include "kernel/screen.h"
#include "kernel/types.h"
#include "proc/screen.h"

// What getch() answers when there is no key. Upstream's Curses value.
enum { ERR = -1 };

// The named keys are the kernel's: they sit above the Unicode range, so eh's
// own `255 < ch` guard still tells them from a byte. Only the three curses
// spells differently are named. No KEY_BTAB: Shift-Tab decodes to a tab.
enum {
    KEY_NPAGE = KEY_PAGE_DOWN,
    KEY_PPAGE = KEY_PAGE_UP,
    KEY_DC    = KEY_DELETE,
};

extern int LINES, COLS;

// The two halves that are syscalls, and the only Tasks here.
Task<Result<void>> eh_open();  // take the keys and the screen
Task<Result<void>> eh_flush(); // send the frame; getch.cpp only

ProcScreen &eh_screen();
Grid &eh_grid();

// Send the next frame whole. The kernel blanks its screen on a resize but
// leaves the Grid alone, so every cell would compare equal.
void eh_full_blit();

// The geometry moved: resize and repaint everything.
void eh_resized();

// ----------------------------------------------------------------- painting

// The whole screen to blanks, in no attribute.
void eh_erase();

// Row y from column x rightwards, in attr. The status line fills its tail in
// reverse and a prompt field clears its own in none.
void eh_fill(int y, int x, u8 attr);

// n bytes of UTF-8 from column x, one cell per rune, and the column after the
// last -- clamped to the edge, so a fill behind it still blanks that cell. A
// byte that starts no sequence draws as a reverse ~. LF and CR draw nothing and
// take no column: display() places every run itself. A TAB blanks to the next
// eight-column stop, which is what Curses drew.
int eh_put(int y, int x, const char *s, int n, u8 attr);

// One rune into one cell. Neither caller can hand it a TAB or a newline.
void eh_put_rune(int y, int x, char32_t ch, u8 attr);

// Where the real cursor goes, once a frame. Out of range is ignored, as move()
// was: display()'s cur_row/cur_col are only assigned when the cursor is on the
// page, so a shrink can leave them stale.
void eh_cursor(int y, int x);

// No bell in this system. Upstream's diagnostics stay anyway.
void beep();

// ------------------------------------------------------------------- input
//
// The only two things that block, and the only reason eh.cpp has coroutines.

Task<int> getch();

// One line, echoed into row y from column x in attr. The pushback is drained
// first, so prompt()'s primed default arrives as if it had been typed.
Task<int> mvgetnstr(int y, int x, char *buf, int n, u8 attr);

// The pushback queue. Upstream builds a dozen commands out of it -- openo() is
// ungetstr("$a\n\033hi") -- so it holds a line and not a key. Zero when taken,
// ERR when full.
int ungetch(int key);

// Set when the geometry moved, cleared by whoever repaints. A bare resize
// returns ERR from getch() with this raised.
extern int eh_resize_flag;
