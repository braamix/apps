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
 * Upstream forked, gave the child three pipes, and poll()ed all of them at
 * once: the block went out on one, the result came back on another, and the
 * errors on a third, incrementally, so neither end could fill and stall.
 *
 * One task cannot park on two descriptors here, so that shape is not
 * available -- a filter driven through two pipes would deadlock the moment
 * either filled. A temp file on each side is what vi's filter does and what
 * this does: the block is written out whole, the child runs with the file as
 * its input, and the result is read back whole.
 *
 * What that costs: the errors are not separated from the output any more (the
 * child's stderr is the console's), and a filter that reads for ever does not
 * see end of input until it is asked for.
 */

#include "block.h"
#include "clipbrd.h"
#include "cmd.h"
#include "config.h"
#include "edit.h"
#include "kernel/sysabi.h"
#include "leio.h"
#include "lesys.h"
#include "proc/io.h"
#include "proc/rt.h"

static char in_name[32];
static char out_name[32];
static char err_name[32];

static void tmpname(char *buf, unsigned size, char which)
{
    snprintf(buf, size, "/tmp/le%c.%u", which, (unsigned)proc_pid());
}

Task<int> PipeBlock(const char *filter, bool in, bool out)
{
    offs oldpos = Offset();
    int fd;
    int res      = ERR;
    int exitcode = -1;
    struct stat st;

    CheckBlock();
    if (hide)
        out = 0;

    if (!in && !out)
        co_return OK; /* nothing to do */

    if (View && in) {
        beep();
        co_return ERR;
    }

    tmpname(in_name, sizeof(in_name), 'I');
    tmpname(out_name, sizeof(out_name), 'O');
    tmpname(err_name, sizeof(err_name), 'E');

    /* The block, out to a file the child will read. Rectangular blocks go
       through the clipboard, which is what linearises them. */
    if (out) {
        fd = co_await le_open(out_name, O_CREAT | O_TRUNC | O_WRONLY, 0600);
        if (fd == -1) {
            FError(out_name);
            co_return ERR;
        }
        if (rblock) {
            ClipBoard cb;
            if (!cb.Copy() || co_await cb.Write(fd) != OK) {
                co_await le_close(fd);
                co_await le_unlink(out_name);
                co_return ERR;
            }
        } else {
            num act_written;
            if (co_await WriteBlock(fd, BlockBegin, BlockEnd - BlockBegin, &act_written) != OK) {
                FError(out_name);
                co_await le_close(fd);
                co_await le_unlink(out_name);
                co_return ERR;
            }
        }
        co_await le_close(fd);
    }

    if (in && !out) {
        BlockBegin = CurrentPos;
        BlockEnd   = CurrentPos;
        hide       = 0;
    }

    /* The child: stdin is the block or nothing, stdout the file the result
       comes back in or the console. The screen goes back either way -- the
       command may want to draw on it. */
    {
        Str words[3];
        Args v;
        ChildIo cio;
        Result<u32> pid_r = Err(Error::NoMemory);
        int fdin = -1, fdout = -1, fderr = -1;

        fdin = co_await le_open(out ? out_name : "/dev/null", O_RDONLY);
        if (in)
            fdout = co_await le_open(in_name, O_CREAT | O_TRUNC | O_WRONLY, 0600);
        /* The third file is upstream's third pipe: the editor holds the screen,
           so anything the child says on stderr would land under it as bytes. */
        fderr = co_await le_open(err_name, O_CREAT | O_TRUNC | O_WRONLY, 0600);

        words[0] = Str("/bin/sh", 7);
        words[1] = Str("-c", 2);
        words[2] = Str(filter, strlen(filter));
        v.v      = Span<const Str>(words, 3);
        cio.in   = fdin >= 0 ? (u32)fdin : SYS_STDIN;
        cio.out  = fdout >= 0 ? (u32)fdout : SYS_STDOUT;
        cio.err  = fderr >= 0 ? (u32)fderr : SYS_STDERR;

        if (Task<Result<u32>> t = spawn(v, cio))
            pid_r = co_await t;
        if (pid_r.is_ok()) {
            Result<Exited> w = Err(Error::NoMemory);

            if (Task<Result<Exited>> t = wait_child(pid_r.value()))
                w = co_await t;
            exitcode = w.is_err() ? -1 : (int)w.value().status;
            res      = OK;
        } else {
            errno = int(pid_r.error());
            FError(filter);
        }
        co_await le_close(fdin);
        if (fdout != -1)
            co_await le_close(fdout);
        if (fderr != -1)
            co_await le_close(fderr);
    }

    /* What the child said, in the box upstream showed it in. */
    {
        struct stat est;

        if (co_await le_stat(err_name, &est) != -1 && est.st_size > 0) {
            static char errtext[512];
            int efd = co_await le_open(err_name, O_RDONLY);

            if (efd != -1) {
                ssize_t n = co_await le_read(efd, errtext, sizeof(errtext) - 1);
                co_await le_close(efd);
                if (n > 0) {
                    errtext[n] = 0;
                    ErrMsg(errtext);
                }
            }
        }
    }

    /* And the result back in, over the block it replaces. */
    if (res == OK && in) {
        if (exitcode == 0 && co_await le_stat(in_name, &st) != -1 && st.st_size > 0) {
            fd = co_await le_open(in_name, O_RDONLY);
            if (fd == -1) {
                FError(in_name);
                res = ERR;
            } else {
                num act_read;

                CurrentPos = BlockBegin;
                if (out)
                    DeleteBlock(0, BlockEnd - BlockBegin);
                if (co_await ReadBlock(fd, st.st_size, &act_read) != OK) {
                    NoMemory();
                    res = ERR;
                }
                co_await le_close(fd);
            }
        } else if (out && exitcode == 0) {
            /* The filter answered nothing and said it meant to: the block goes.
               A command that failed leaves the text alone -- upstream streamed and
               could not tell the two apart, and the status says which it was. */
            CurrentPos = BlockBegin;
            DeleteBlock(0, BlockEnd - BlockBegin);
        }
    }

    if (out)
        co_await le_unlink(out_name);
    if (in)
        co_await le_unlink(in_name);
    co_await le_unlink(err_name);

    if (!in)
        CurrentPos = oldpos;
    flag = REDISPLAY_ALL;
    co_return res;
}
