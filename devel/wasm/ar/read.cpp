/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2007 Kai Wang
 * Copyright (c) 2007 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <time.h>

#include "ar.h"
#include "archive.h"

/*
 * Braam: strmode(3), for the permissions -t -v prints. The type, in buf[0],
 * is never printed here.
 */
static void strmode(mode_t mode, char *p)
{
    *p++ = '?';
    *p++ = mode & 0400 ? 'r' : '-';
    *p++ = mode & 0200 ? 'w' : '-';
    *p++ = mode & 04000 ? (mode & 0100 ? 's' : 'S') : (mode & 0100 ? 'x' : '-');
    *p++ = mode & 040 ? 'r' : '-';
    *p++ = mode & 020 ? 'w' : '-';
    *p++ = mode & 02000 ? (mode & 010 ? 's' : 'S') : (mode & 010 ? 'x' : '-');
    *p++ = mode & 04 ? 'r' : '-';
    *p++ = mode & 02 ? 'w' : '-';
    *p++ = mode & 01000 ? (mode & 01 ? 't' : 'T') : (mode & 01 ? 'x' : '-');
    *p++ = ' ';
    *p   = '\0';
}

/*
 * Braam: whether a path has a ".." component, which libarchive's
 * ARCHIVE_EXTRACT_SECURE_NODOTDOT refuses.
 */
static int has_dotdot(const char *name)
{
    const char *p;

    for (p = name; *p != '\0'; p++)
        if ((p == name || p[-1] == '/') && p[0] == '.' && p[1] == '.' &&
            (p[2] == '\0' || p[2] == '/'))
            return (1);
    return (0);
}

/*
 * Braam: libarchive's words for what it could not read.
 */
void ar_read_error(struct bsdar *bsdar, Bytes file, const String &why, int fatal)
{
    if (!is_archive(file)) {
        if (fatal)
            bsdar_errc(bsdar, 0, "Unrecognized archive format");
        else
            bsdar_warnc(bsdar, 0, "Unrecognized archive format");
        return;
    }
    if (fatal)
        bsdar_errc(bsdar, 0, "%.*s", int(why.size()), why.data());
    else
        bsdar_warnc(bsdar, 0, "%.*s", int(why.size()), why.data());
}

/*
 * Handle read modes: 'x', 't' and 'p'.
 */
int ar_read_archive(struct bsdar *bsdar, int mode, int out)
{
    struct ar_file *f, *sb;
    struct tm tm, *tp;
    const char *bname;
    char *name;
    mode_t md;
    size_t size;
    time_t mtime;
    uid_t uid;
    gid_t gid;
    char **av;
    char buf[25];
    char find;
    int exitcode, r, i;
    Vec<Member> members;
    Vec<ArchiveSymbol> index;
    Out why;

    f = ar_lookup(bsdar, bsdar->filename);
    if (f->error != 0) {
        bsdar_errc(bsdar, f->error, "Failed to open '%s'", bsdar->filename);
        return (EXIT_FAILURE);
    }
    /* The members before a malformed one are still read. */
    r = read_archive(Str(bsdar->filename), f->data, members, index, why);

    exitcode = EXIT_SUCCESS;

    for (const Member &m : members) {
        if ((name = strndup(m.name.data(), m.name.size())) == NULL)
            break;

        if (bsdar->argc > 0) {
            find = 0;
            for (i = 0; i < bsdar->argc; i++) {
                av = &bsdar->argv[i];
                if (*av == NULL)
                    continue;
                bname = bsdar_basename(*av);
                if (strcmp(bname, name) != 0)
                    continue;

                *av  = NULL;
                find = 1;
                break;
            }
            if (!find) {
                free(name);
                continue;
            }
        }

        if (mode == 't') {
            if (bsdar->options & AR_V) {
                md    = m.mode;
                uid   = m.uid;
                gid   = m.gid;
                size  = m.data.size();
                mtime = m.mtime;
                strmode(md, buf);
                bsdar_printf(bsdar, out, "%s %6d/%-6d %8ju ", buf + 1, uid, gid, (uintmax_t)size);
                mtime += bsdar->tz;
                tp = gmtime_r(&mtime, &tm);
                (void)strftime(buf, sizeof(buf), "%b %e %H:%M %Y", tp);
                bsdar_printf(bsdar, out, "%s %s", buf, name);
            } else
                bsdar_printf(bsdar, out, "%s", name);
            bsdar_printf(bsdar, out, "\n");
        } else {
            /* mode == 'x' || mode = 'p' */
            if (mode == 'p') {
                if (bsdar->options & AR_V) {
                    bsdar_printf(bsdar, out, "\n<%s>\n\n", name);
                }
                bsdar_write(bsdar, out, m.data.data(), m.data.size());
            } else {
                /* mode == 'x' */
                sb = ar_lookup(bsdar, name);
                if (sb->error != 0) {
                    if (sb->error != ENOENT) {
                        bsdar_warnc(bsdar, 0, "stat %s failed", bsdar->filename);
                        free(name);
                        continue;
                    }
                } else {
                    /* stat success, file exist */
                    if (bsdar->options & AR_CC) {
                        free(name);
                        continue;
                    }
                    if (bsdar->options & AR_U && (time_t)m.mtime <= sb->mtime) {
                        free(name);
                        continue;
                    }
                }

                if (bsdar->options & AR_V)
                    bsdar_printf(bsdar, out, "x - %s\n", name);
                /* Disallow absolute paths. */
                if (name[0] == '/') {
                    bsdar_warnc(bsdar, 0, "Absolute path '%s'", name);
                    free(name);
                    continue;
                }
                /* Basic path security flags. */
                if (has_dotdot(name)) {
                    bsdar_warnc(bsdar, 0, "Path contains '..'");
                    exitcode = EXIT_FAILURE;
                    free(name);
                    continue;
                }
                /*
                 * Braam: a file's time cannot be set, so -o
                 * is a warning, once, and the file is still
                 * extracted.
                 */
                if (bsdar->options & AR_O && !bsdar->time_warned) {
                    bsdar_warnc(bsdar, 0, "Can't restore time");
                    bsdar->time_warned = true;
                }

                ar_write w;
                w.path    = name;
                w.data    = m.data;
                w.archive = false;
                bsdar->writes.push(move(w));
                continue; /* the write keeps the name */
            }
        }
        free(name);
    }

    if (!r) {
        ar_read_error(bsdar, f->data, why.s, 0);
        exitcode = EXIT_FAILURE;
    }

    return (exitcode);
}
