/*
 * Copyright (c) 2003-2015 by Alexander V. Lukyanov (lav@yars.free.net)
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

#ifndef MB_H
#define MB_H

#include "gap.h"
#include "kernel/task.h"
#include "lewchar.h"

extern bool mb_mode;
extern int MBCharSize;
extern int MBCharWidth;
extern bool MBCharInvalid;
extern bool MBCharSplit;

[[nodiscard]] bool MBCheckLeftAt(offs o);
[[nodiscard]] bool MBCheckAt(offs o);
wchar_t WCharAt(offs o);
wchar_t WCharLeftAt(offs o);
void InsertWChar(wchar_t ch);
wchar_t WCharAtLC(num, num);
Task<wchar_t> getcode_wchar();
Task<wchar_t> choose_wch();
void ReplaceWCharExt(wchar_t);
void ReplaceWCharExtMove(wchar_t);
void ReplaceWCharMove(wchar_t);

[[nodiscard]] static inline bool MBCheckRight()
{
    return MBCheckAt(Offset());
}
[[nodiscard]] static inline bool MBCheckLeft()
{
    return MBCheckLeftAt(Offset());
}
static inline int CharWidthAt(offs o)
{
    return MBCheckAt(o) ? MBCharWidth : 1;
}
static inline int CharSizeAt(offs o)
{
    return MBCheckAt(o) ? MBCharSize : 1;
}
static inline int CharSizeLeftAt(offs o)
{
    return MBCheckLeftAt(o) ? MBCharSize : 1;
}
static inline int CharSize()
{
    return CharSizeAt(Offset());
}
static inline int CharWidth()
{
    return CharWidthAt(Offset());
}
static inline int CharSizeLeft()
{
    return CharSizeLeftAt(Offset());
}
static inline int WChar()
{
    return WCharAt(Offset());
}
static inline int WCharLeft()
{
    return WCharLeftAt(Offset());
}

void mb_get_col(const char *buf, int pos, int *col, int len);
void mb_char_left(const char *buf, int *pos, int *col, int len);
void mb_char_right(const char *buf, int *pos, int *col, int len);
int mb_get_pos_for_col(const char *buf, int width, int len);
int mb_len(const char *buf, int len);
wchar_t mb_to_wc(const char *buf, int len, int *ch_len, int *ch_width);

#endif // MB_H
