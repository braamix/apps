/*
 * Copyright (c) 1993-2017 by Alexander V. Lukyanov (lav@yars.free.net)
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

/* Running a command with the screen given back.
 *
 * Upstream left curses, called system(), and came back. Braam has spawn and
 * wait_child and the choreography is settled -- screen back first, then the
 * keyboard, both before the spawn, because a full-screen child claims the
 * keyboard in its first step -- but it is not written yet.
 *
 * See README.md, "What is not here yet". */

#include "config.h"
#include "edit.h"
#include "cmd.h"

Task<void>    cmd(const char *,bool,bool)
{
    ErrMsg("Running a command is not available yet");
    co_return;
}

Task<void>    DoMake()
{
    if(View)
        co_return;
    co_await cmd(Make,1,1);
}
Task<void>    DoShell()
{
    co_await cmd(Shell,0,0);
}
Task<void>    DoRun()
{
    if(View)
        co_return;
    co_await cmd(Run,1,1);
}
Task<void>    DoCompile()
{
    if(View)
        co_return;
    co_await cmd(Compile,1,1);
}
