/*
 * Copyright (c) 1993-2021 by Alexander V. Lukyanov (lav@yars.free.net)
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

/* The three names every other header here is written against.
 *
 * Upstream declared them in edit.h, above the twelve headers edit.h then
 * includes -- so those twelve compiled only in that order and only through
 * it. They are here instead, so each of them can say what it needs and be
 * included on its own. */

#ifndef LETYPES_H
#define LETYPES_H

typedef unsigned char byte;
typedef long offs;
typedef long num;

enum { NO_POS = -1L };

#endif /* LETYPES_H */
