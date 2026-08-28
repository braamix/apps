/*
 * Copyright (c) 1993-2012 by Alexander V. Lukyanov (lav@yars.free.net)
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

/* Signals, of which three are catchable and none is a handler.
 *
 * A signal is delivered where the process parks, so getch.cpp answers all
 * three and there is nothing to install but the mask. What went with the
 * handlers: the SIGSEGV/SIGBUS/SIGILL dumps to ~/.le/tmp/DUMP-*, the SIGHUP
 * rescue, and SIGTSTP -- there is no job control to stop into.
 *
 * The autosave survives. It was already a resumable chunked state machine
 * driven by alarm(), which is what makes it fit: co_await AutoSaveTick() is the same
 * machine, called from co_await Edit()'s loop between keystrokes rather than from a
 * handler. */

#include "config.h"
#include "lesys.h"
#include "edit.h"
#include "leio.h"

#include "kernel/sysabi.h"
#include "proc/io.h"
#include "proc/rt.h"

void BlockSignals()
{
    /* Upstream masked signals around getch so a handler could not run inside
       curses. Nothing runs asynchronously here. */
}

void UnblockSignals()
{
}

void CheckWindowResize()
{
    /* next_key() reshapes the grid before it reports the resize, and getch.cpp
       has already called curses_resized(); all that is left is to lay the
       editor out again. Upstream had to ask the terminal for its size and
       restart curses. */
    CorrectParameters();
    flag |= REDISPLAY_ALL;
}

/* A path, which is all TmpFileName builds; upstream sized this for the
   crash dumps as well. */
static char mem[LE_PATHMAX];

char *TmpFileName()
{
    snprintf(mem, sizeof(mem), "%s/.le/tmp/", HOME);
    int len          = strlen(mem);
    char *store      = mem + len;
    const char *scan = FileName;
    /* The whole path, with the slashes turned into ! -- so two files of the
       same name in different directories do not collide. */
    while (*scan && len < (int)sizeof(mem) - 2) {
        *store++ = (*scan == '/' ? '!' : *scan);
        scan++;
        len++;
    }
    *store = 0;
    return mem;
}

Task<void> SuspendEditor()
{
    /* There is no job control to stop into. */
    Message("Suspend is not available here");
    co_return;
}

Task<void> InstallSignalHandlers()
{
    /* Ask for the three that are catchable; the mask starts empty, so without
       this a ^C would kill the editor with the file unsaved. */
    if (Task<Result<void>> t = sig_catch(SIG_INT))
        co_await t;
    if (Task<Result<void>> t = sig_catch(SIG_TERM))
        co_await t;
    if (Task<Result<void>> t = sig_catch(SIG_WINCH))
        co_await t;
}

void ReleaseSignalHandlers()
{
}

/* The autosave. `modified` is upstream's three-state: 1 the text changed,
   3 a dump is running, 2 the dump is complete. */
Task<void> AutoSaveTick()
{
    static offs dump_pos   = 0;
    static int fd          = -1;
    static int interrupted = 0;
    static u64 due         = 0;
    const int chunk        = 0x20000;

    u64 now = proc_now();
    if (now < due)
        co_return;

    if (modified == 1) {
        dump_pos = 0;
        if (fd != -1) {
            interrupted++;
            co_await le_close(fd);
        }
        char *s = TmpFileName();
        fd      = co_await le_open(s, O_CREAT | O_WRONLY | O_TRUNC, 0600);
        if (fd == -1) {
            due = now + ALARMDELAY * 1000;
            co_return;
        }
        modified = 3;
    }
    if (modified == 3) {
        num act_written;
        if ((co_await WriteBlock(fd, dump_pos, (interrupted > 5 ? Size() - dump_pos : chunk),
                                 &act_written)) != OK) {
        done:
            co_await le_close(fd);
            fd       = -1;
            modified = 2;
        } else {
            dump_pos += act_written;
            if (dump_pos >= Size()) {
                interrupted = 0;
                goto done;
            }
            /* After a chunk, come back for the next one at once. */
            due = now + 1000;
            co_return;
        }
    }
    due = now + ALARMDELAY * 1000;
}
