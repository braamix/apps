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

#ifndef GETCH_H
#define GETCH_H

#include "kernel/task.h"

/* A key, as the rest of the editor sees it.
 *
 * Braam hands over {codepoint, modifiers}; upstream read bytes. So the two
 * that already have a byte form keep it -- Ctrl-letter is the control
 * character, and Alt is an ESC in front, which is what upstream's own
 * \e|X bindings mean -- and a printable key is its UTF-8, one byte per call.
 * That leaves the key tree, StringTyped and the self-insert path in Edit()
 * exactly as they were.
 *
 * What is left over is the named keys, which had no byte form that was not a
 * terminal's escape sequence. They are single codes above every byte. */
enum {
    K_NAMED = 0x100,
    K_UP    = K_NAMED,
    K_DOWN,
    K_LEFT,
    K_RIGHT,
    K_HOME,
    K_END,
    K_PGUP,
    K_PGDN,
    K_INSERT,
    K_DELETE,
    K_BACKTAB,
    K_F1,
    K_F2,
    K_F3,
    K_F4,
    K_F5,
    K_F6,
    K_F7,
    K_F8,
    K_F9,
    K_F10,
    K_F11,
    K_F12,
    /* ESC is one of these too: byte 27 is the Alt prefix and nothing else, so
       the key tree can act on ESC without waiting to see what follows. */
    K_ESCAPE,
    K_NAMED_LAST = K_ESCAPE,

    /* On a named key only: a printable one carries Ctrl in its control
       character and Alt in the ESC before it. */
    K_SHIFT = 0x1000,
    K_CTRL  = 0x2000,

    /* The resize that rides on a key reply, and the one LE already handles. */
    K_RESIZE = 0x8000,
};

/* Taken as data rather than as a binding, ESC is the byte again. */
static inline int key_byte(int k)
{
    return k == K_ESCAPE ? 27 : k;
}

Task<int> GetRawKey();
Task<int> WaitForKey(); /* peeks: the key is put back */
Task<int> GetKey();

#endif
