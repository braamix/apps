/*
 * Copyright (c) 1993-2005 by Alexander V. Lukyanov (lav@yars.free.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef WINDOW_H
#define WINDOW_H

#include "color.h"
#include "curses.h"
#include "letypes.h"

#define win_cell cchar_t
#ifdef CURSES_CCHAR_MAX // NetBSD
#define win_cell_set_attrs(ch, a) \
    (ch)->attributes = ((ch)->attributes & A_ALTCHARSET) | ((a) & ~(A_CHARTEXT | A_ALTCHARSET))
#else // ncurses
#define win_cell_set_attrs(ch, a) \
    (ch)->attr = ((ch)->attr & A_ALTCHARSET) | ((a) & ~(A_CHARTEXT | A_ALTCHARSET))
#endif
#define scr_get_cell(y, x, out) mvin_wch(y, x, out)
#define scr_put_cell(y, x, ch)  mvadd_wchnstr(y, x, ch, 1)

typedef struct win {
    win_cell *buf;
    int x, y;
    int w, h;
    /* As asked for. x and y may be symbolic (MIDDLE, FRIGHT) and w and h are
       unclamped, so a resize can lay the window out again from these; the
       fields above have been through Absolute() and cannot. */
    int ox, oy;
    unsigned ow, oh;
    int clip_x;
    const attr *a;
    const char *title;
    struct win *prev;
    int flags;
} WIN;

#define SIGN   0x1000
#define FRIGHT 0x2000
#define MIDDLE 0x4000
#define FDOWN  FRIGHT

/* window flags */
#define NOSHADOW 1

WIN *CreateWin(int x, int y, unsigned w, unsigned h, const attr *a, const char *title,
               int flags = 0);
void DisplayWin(WIN *);
void CloseWin();
void DestroyWin(WIN *);

/* Place a window again in the current COLS and LINES, from the ox/oy/ow/oh it
   was asked for. RefitWin moves one; WindowsResized does the displayed stack
   and redraws it. */
void RefitWin(WIN *);
void WindowsResized();

void Absolute(int *x, int width, int field);
void GotoXY(int x, int y);
void Clear();
void PutStr(int x, int y, const char *s);
void PutCh(int x, int y, chtype ch);
void PutWCh(int x, int y, wchar_t ch);
void PutCCh(int x, int y, const cchar_t *ch);
#define PutACS(x, y, a) PutCCh((x), (y), WACS_##a)

extern const struct attr *curr_attr;
extern WIN *Upper;

static inline void SetAttr(const struct attr *a)
{
    curr_attr = a;
    attrset(a->n_attr);
}

#endif /* WINDOW_H */
