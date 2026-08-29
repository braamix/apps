/*
 * Copyright (c) 1993-2004 by Alexander V. Lukyanov (lav@yars.free.net)
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

/* The gap buffer's own accessors.
 *
 * These were the first half of inline.h. mb.h is written against them --
 * CharSizeLeft() is CharSizeLeftAt(Offset()) -- and the second half of
 * inline.h is written against mb.h, so upstream's one file could only be read
 * in that order and only in that place. Splitting the half that mb.h needs is
 * what lets either be included on its own. */

#ifndef GAP_H
#define GAP_H

#include "braam.h"
#include "letypes.h"
#include "textpoin.h"

/* What the accessors below read. These are edit.h's own globals, declared
   here because edit.h includes inline.h, which includes this. */
extern char *buffer;
extern offs BufferSize;
extern offs GapSize;
extern offs ptr1, ptr2;

bool IsAlNumAt(offs);

static inline byte CharAt(offs offset)
{
    if (offset >= ptr1)
        offset += GapSize;
    if (offset >= 0 && offset < BufferSize)
        return (buffer[offset]);
    return (' ');
}
// same as above, but without bound check
static inline byte CharAt_NoCheck(offs offset)
{
    if (offset >= ptr1)
        offset += GapSize;
    return (buffer[offset]);
}
static inline offs Size()
{
    return (BufferSize - GapSize);
}
static inline offs Offset()
{
    return (CurrentPos.Offset());
}
static inline int Eof()
{
    return (Offset() >= Size());
}
static inline int EofAt(offs o)
{
    return (o >= Size());
}
static inline int Bof()
{
    return (Offset() <= 0);
}
static inline int BofAt(offs o)
{
    return (o <= 0);
}
static inline byte Char()
{
    return (CharAt(Offset()));
}
static inline byte CharRel(offs sh)
{
    return (CharAt(Offset() + sh));
}
static inline byte CharRel_NoCheck(offs sh)
{
    return (CharAt_NoCheck(Offset() + sh));
}
static inline bool IsAlNumRel(offs sh)
{
    return IsAlNumAt(Offset() + sh);
}

#endif /* GAP_H */
