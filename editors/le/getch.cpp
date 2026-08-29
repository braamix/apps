/*
 * Copyright (c) 1993-1997 by Alexander V. Lukyanov (lav@yars.free.net)
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

/* The one place the editor parks. Upstream blocked in getch() and a SIGWINCH
   longjmp'd out of it; here the resize arrives as Err(Intr) from next_key(),
   which is the same thing without the jump -- so there is no sigsetjmp and no
   getch_return. */

#include "getch.h"

#include "config.h"
#include "edit.h"
#include "kernel/key.h"
#include "kernel/sysabi.h"
#include "keymap.h"
#include "proc/rt.h"

int resize_flag;

void UnrefKey(int)
{
    /* Upstream drained the terminal's typeahead after an unknown escape
       sequence. There are no escape sequences here and nothing to drain. */
}

/* One Braam key as the byte, or the named code, the rest of the editor wants.
   Anything left over is pushed back and comes out of the next call. */
static int decode(Key k)
{
    int named = 0;

    switch (k.code) {
    case KEY_UP:
        named = K_UP;
        break;
    case KEY_DOWN:
        named = K_DOWN;
        break;
    case KEY_LEFT:
        named = K_LEFT;
        break;
    case KEY_RIGHT:
        named = K_RIGHT;
        break;
    case KEY_HOME:
        named = K_HOME;
        break;
    case KEY_END:
        named = K_END;
        break;
    case KEY_PAGE_UP:
        named = K_PGUP;
        break;
    case KEY_PAGE_DOWN:
        named = K_PGDN;
        break;
    case KEY_INSERT:
        named = K_INSERT;
        break;
    case KEY_DELETE:
        named = K_DELETE;
        break;
    case KEY_F1:
        named = K_F1;
        break;
    case KEY_F2:
        named = K_F2;
        break;
    case KEY_F3:
        named = K_F3;
        break;
    case KEY_F4:
        named = K_F4;
        break;
    case KEY_F5:
        named = K_F5;
        break;
    case KEY_F6:
        named = K_F6;
        break;
    case KEY_F7:
        named = K_F7;
        break;
    case KEY_F8:
        named = K_F8;
        break;
    case KEY_F9:
        named = K_F9;
        break;
    case KEY_F10:
        named = K_F10;
        break;
    case KEY_F11:
        named = K_F11;
        break;
    case KEY_F12:
        named = K_F12;
        break;

    /* These three have a byte of their own and always have had. */
    case KEY_ENTER:
        return '\n';
    case KEY_ESCAPE:
        return 27;
    case KEY_BACKSPACE:
        return 0177;
    case KEY_TAB:
        if (k.mods & MOD_SHIFT)
            return K_BACKTAB;
        return '\t';
    }

    if (named) {
        if (k.mods & MOD_SHIFT)
            named |= K_SHIFT;
        if (k.mods & MOD_CTRL)
            named |= K_CTRL;
        /* Alt is an ESC in front, on a named key as on any other, because that
           is what upstream's \e|$kcub1 bindings mean. */
        if (k.mods & MOD_ALT) {
            ungetch(named);
            return 27;
        }
        return named;
    }

    /* Ctrl on a printable key is the control character it has always been. */
    if (k.mods & MOD_CTRL) {
        int c = k.code;
        if (c >= 'a' && c <= 'z')
            c = c - 'a' + 1;
        else if (c >= 'A' && c <= 'Z')
            c = c - 'A' + 1;
        else if (c == '[')
            c = 27;
        else if (c == '\\')
            c = 28;
        else if (c == ']')
            c = 29;
        else if (c == '^')
            c = 30;
        else if (c == '_' || c == '/')
            c = 31;
        else if (c == '@' || c == ' ')
            c = 0;
        else if (c == '?')
            c = 0177;
        else
            return ERR;
        if (k.mods & MOD_ALT) {
            ungetch(c);
            return 27;
        }
        return c;
    }

    if (k.mods & MOD_ALT) {
        ungetch((int)k.code);
        return 27;
    }

    /* A printable key, as its UTF-8. Upstream got these one byte at a time and
       the editor inserts them one at a time, so hand back the first and queue
       the rest -- and queue them backwards, since ungetch is a stack. */
    {
        char buf[4];
        int n = wctomb(buf, (wchar_t)k.code);

        if (n < 1)
            return ERR;
        for (int i = n - 1; i > 0; i--)
            ungetch((unsigned char)buf[i]);
        return (unsigned char)buf[0];
    }
}

Task<int> GetRawKey()
{
    if (curses_unget_pending())
        co_return curses_unget_take();

    for (;;) {
        /* refresh() only raised a flag; this is where the frame goes out, and
           it must go out before the process parks or the screen would lag a
           keystroke behind. */
        if (Task<Result<void>> t = curses_flush())
            co_await t;

        Result<Key> r = Err(Error::NoMemory);
        if (Task<Result<Key>> t = curses_screen().next_key())
            r = co_await t;

        if (r.is_err()) {
            if (r.error() != Error::Intr)
                co_return ERR;
            /* next_key() takes SIG_WINCH itself and reshapes before it reports,
               so note the geometry before answering whatever signal is behind
               this -- a resize arriving with a ^C is dropped otherwise. */
            bool resized = (int)curses_grid().cols != COLS || (int)curses_grid().rows != LINES;
            if (sig_take(SIG_TERM))
                co_return ERR;
            if (sig_take(SIG_INT))
                co_return 3; /* ^C, as the keystroke upstream bound */
            if (resized) {
                curses_resized();
                resize_flag = 1;
                co_return ERR;
            }
            continue;
        }

        int key = decode(r.value());
        if (key != ERR)
            co_return key;
        if (curses_unget_pending())
            co_return curses_unget_take();
    }
}

Task<int> WaitForKey()
{
    int key = co_await GetRawKey();

    if (key != ERR)
        ungetch(key);
    co_return key;
}

Task<int> GetKey()
{
    /* Upstream set the background so that recent ncurses cleared with the
       editor's own colours; the Grid is cleared with an attribute the caller
       already chose, so there is nothing to set. */
    co_return co_await GetRawKey();
}
