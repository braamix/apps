/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ar.h"
#include "compat/cerr.h"

static void bsdar_vwarnc(struct bsdar *, int code, const char *fmt, va_list ap);
static void bsdar_verrc(struct bsdar *bsdar, int code, const char *fmt, va_list ap);
static void bsdar_vprintf(struct bsdar *, int fd, const char *fmt, va_list ap);

/*
 * Braam: strerror(3) is the errno's name; this system words it so.
 */
static void bsdar_code(struct bsdar *bsdar, int code)
{
    Str why = error_name(error_of(code));

    bsdar_printf(bsdar, 2, ": %.*s", int(why.size()), why.data());
}

static void bsdar_vwarnc(struct bsdar *bsdar, int code, const char *fmt, va_list ap)
{
    bsdar_printf(bsdar, 2, "%s: warning: ", bsdar->progname);
    bsdar_vprintf(bsdar, 2, fmt, ap);
    if (code != 0)
        bsdar_code(bsdar, code);
    bsdar_printf(bsdar, 2, "\n");
}

void bsdar_warnc(struct bsdar *bsdar, int code, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    bsdar_vwarnc(bsdar, code, fmt, ap);
    va_end(ap);
}

static void bsdar_verrc(struct bsdar *bsdar, int code, const char *fmt, va_list ap)
{
    bsdar_printf(bsdar, 2, "%s: fatal: ", bsdar->progname);
    bsdar_vprintf(bsdar, 2, fmt, ap);
    if (code != 0)
        bsdar_code(bsdar, code);
    bsdar_printf(bsdar, 2, "\n");
}

/*
 * Braam: there is no exit(3) from any depth. The message is recorded, and
 * each caller returns in turn when it sees bsdar->stopped.
 */
void bsdar_errc(struct bsdar *bsdar, int code, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    bsdar_verrc(bsdar, code, fmt, ap);
    va_end(ap);
    bsdar_exit(bsdar, EXIT_FAILURE);
}

/*
 * Braam: exit(3). Nothing is said after it, and the front end does nothing
 * more than write what was said.
 */
void bsdar_exit(struct bsdar *bsdar, int status)
{
    if (bsdar->stopped)
        return;
    bsdar->stopped = true;
    bsdar->status  = status;
}

/*
 * Braam: basename(3), as far as ar needs it: what follows the last '/'.
 */
const char *bsdar_basename(const char *path)
{
    const char *slash = strrchr(path, '/');

    return (slash != NULL ? slash + 1 : path);
}

void bsdar_write(struct bsdar *bsdar, int fd, const void *buf, size_t s)
{
    if (bsdar->stopped)
        return;
    if (bsdar->output.empty() || bsdar->output.back().fd != fd) {
        ar_chunk c;
        c.fd = fd;
        bsdar->output.push(move(c));
    }
    bsdar->output.back().text.append(Str(static_cast<const char *>(buf), s));
}

static void bsdar_vprintf(struct bsdar *bsdar, int fd, const char *fmt, va_list ap)
{
    char small[256];
    va_list aq;

    va_copy(aq, ap);
    int n = vsnprintf(small, sizeof(small), fmt, aq);
    va_end(aq);
    if (n < 0)
        return;
    if (size_t(n) < sizeof(small)) {
        bsdar_write(bsdar, fd, small, n);
        return;
    }
    char *big = static_cast<char *>(malloc(n + 1));
    if (big == NULL)
        return;
    vsnprintf(big, n + 1, fmt, ap);
    bsdar_write(bsdar, fd, big, n);
    free(big);
}

void bsdar_printf(struct bsdar *bsdar, int fd, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    bsdar_vprintf(bsdar, fd, fmt, ap);
    va_end(ap);
}
