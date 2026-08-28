/*
 * Copyright (c) 1993-2014 by Alexander V. Lukyanov (lav@yars.free.net)
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

/* Filtering a block through a shell command.
 *
 * Upstream forked a /bin/sh, then poll()ed the child's three descriptors at
 * once. Braam has spawn and make_pipe, but one task cannot park on both ends
 * of a pipeline, so this wants a temp file on each side -- which is what vi's
 * filter does and what this will be. Until then the action says so.
 *
 * See README.md, "What is not here yet". */

#include "config.h"
#include "edit.h"
#include "block.h"
#include "clipbrd.h"

Task<int>   PipeBlock(const char *,bool,bool)
{
   ErrMsg("Filtering through a command is not available yet");
   co_return ERR;
}
