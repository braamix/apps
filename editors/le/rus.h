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

/* The eight-bit Cyrillic codepages, which UTF-8 leaves nothing to do. Every
   caller reaches these only down its !mb_mode arm, and mb_mode is always on
   here; the wide arm beside it is iswalnum and towupper, which do know
   Cyrillic. rus.cc has no successor. */

#ifndef RUS_H
#define RUS_H

#include "letypes.h"

extern int coding;
#define NONE     0
#define ALT      1
#define JO_ALT   2
#define KOI8     3
#define D211_KOI 4
#define MAIN     5

static inline int isrussian(byte)
{
    return 0;
}
static inline int islowerrus(byte)
{
    return 0;
}
static inline int isupperrus(byte)
{
    return 0;
}
static inline byte tolowerrus(byte c)
{
    return c;
}
static inline byte toupperrus(byte c)
{
    return c;
}

#endif /* RUS_H */
