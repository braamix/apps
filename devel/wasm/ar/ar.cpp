/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2007 Kai Wang
 * Copyright (c) 2007 Tim Kientzle
 * Copyright (c) 2007 Joseph Koshy
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

/*-
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Hugh Smith at The University of Guelph.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "ar.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/types.h>

#include "getopt.h"

enum options { OPTION_HELP };

static struct option longopts[] = { { "help", no_argument, NULL, OPTION_HELP },
                                    { "version", no_argument, NULL, 'V' },
                                    { NULL, 0, NULL, 0 } };

static void bsdar_usage(struct bsdar *bsdar);
static void ranlib_usage(struct bsdar *bsdar);
static void set_mode(struct bsdar *bsdar, char opt);
static void only_mode(struct bsdar *bsdar, const char *opt, const char *valid_modes);
static void bsdar_version(struct bsdar *bsdar);
static void ranlib_version(struct bsdar *bsdar);

/* Braam: whom getopt_long's warnx speaks for. */
static struct bsdar *warn_to;

void warnx(const char *fmt, ...)
{
    char buf[256];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    bsdar_printf(warn_to, 2, "%s: %s\n", warn_to->progname, buf);
}

/*
 * Braam: what the front end found at `path', or an entry saying it found
 * nothing, as if open(2) had failed.
 */
struct ar_file *ar_lookup(struct bsdar *bsdar, const char *path)
{
    static struct ar_file none;

    for (ar_file &f : bsdar->files)
        if (strcmp(f.path, path) == 0)
            return (&f);
    none.path  = path;
    none.error = ENOENT;
    return (&none);
}

/*
 * Braam: the first half of main(), up to the point where files are read.
 */
int ar_options(struct bsdar *bsdar, int argc, char **argv)
{
    char *p;
    size_t len;
    int opt, Dflag, Uflag;

    warn_to = bsdar;
    Dflag   = 0;
    Uflag   = 0;

    if (bsdar->progname == NULL)
        bsdar->progname = "ar";

    /* Act like ranlib if our name ends in "ranlib"; this
     * accommodates arm-freebsd7.1-ranlib, bsdranlib, etc. */
    len = strlen(bsdar->progname);
    if (len >= strlen("ranlib") &&
        strcmp(bsdar->progname + len - strlen("ranlib"), "ranlib") == 0) {
        while (!bsdar->stopped && (opt = getopt_long(argc, argv, "tDUV", longopts, NULL)) != -1) {
            switch (opt) {
            case 't':
                /* Ignored. */
                break;
            case 'D':
                Dflag = 1;
                Uflag = 0;
                break;
            case 'U':
                Uflag = 1;
                Dflag = 0;
                break;
            case 'V':
                ranlib_version(bsdar);
                break;
            case OPTION_HELP:
                ranlib_usage(bsdar);
                break;
            default:
                ranlib_usage(bsdar);
            }
        }
        if (bsdar->stopped)
            return (bsdar->status);
        argv += optind;
        argc -= optind;

        if (*argv == NULL) {
            ranlib_usage(bsdar);
            return (bsdar->status);
        }

        /* Enable determinstic mode unless -U is set. */
        if (Uflag == 0)
            bsdar->options |= AR_D;
        bsdar->options |= AR_S;
        bsdar->ranlib = true;
        bsdar->argc   = argc;
        bsdar->argv   = argv;
        return (-1);
    } else {
        if (argc < 2) {
            bsdar_usage(bsdar);
            return (bsdar->status);
        }

        if (*argv[1] != '-') {
            len = strlen(argv[1]) + 2;
            if ((p = static_cast<char *>(malloc(len))) == NULL) {
                bsdar_errc(bsdar, errno, "malloc failed");
                return (bsdar->status);
            }
            *p = '-';
            (void)strlcpy(p + 1, argv[1], len - 1);
            argv[1] = p;
        }
    }

    while (!bsdar->stopped &&
           (opt = getopt_long(argc, argv, "abCcdDfijlmopqrSsTtUuVvxz", longopts, NULL)) != -1) {
        switch (opt) {
        case 'a':
            bsdar->options |= AR_A;
            break;
        case 'b':
        case 'i':
            bsdar->options |= AR_B;
            break;
        case 'C':
            bsdar->options |= AR_CC;
            break;
        case 'c':
            bsdar->options |= AR_C;
            break;
        case 'd':
            set_mode(bsdar, opt);
            break;
        case 'D':
            Dflag = 1;
            Uflag = 0;
            break;
        case 'f':
            bsdar->options |= AR_TR;
            break;
        case 'j':
            /* ignored */
            break;
        case 'l':
            /* ignored, for GNU ar comptibility */
            break;
        case 'm':
            set_mode(bsdar, opt);
            break;
        case 'o':
            bsdar->options |= AR_O;
            break;
        case 'p':
            set_mode(bsdar, opt);
            break;
        case 'q':
            set_mode(bsdar, opt);
            break;
        case 'r':
            set_mode(bsdar, opt);
            break;
        case 'S':
            bsdar->options |= AR_SS;
            break;
        case 's':
            bsdar->options |= AR_S;
            break;
        case 'T':
            /* ignored */
            break;
        case 't':
            set_mode(bsdar, opt);
            break;
        case 'U':
            Uflag = 1;
            Dflag = 0;
            break;
        case 'u':
            bsdar->options |= AR_U;
            break;
        case 'V':
            bsdar_version(bsdar);
            break;
        case 'v':
            bsdar->options |= AR_V;
            break;
        case 'x':
            set_mode(bsdar, opt);
            break;
        case 'z':
            /* ignored */
            break;
        case OPTION_HELP:
            bsdar_usage(bsdar);
            break;
        default:
            bsdar_usage(bsdar);
        }
    }
    if (bsdar->stopped)
        return (bsdar->status);

    argv += optind;
    argc -= optind;

    if (*argv == NULL)
        bsdar_usage(bsdar);

    if (bsdar->options & AR_A && bsdar->options & AR_B)
        bsdar_errc(bsdar, 0, "only one of -a and -[bi] options allowed");

    if (bsdar->options & AR_J && bsdar->options & AR_Z)
        bsdar_errc(bsdar, 0, "only one of -j and -z options allowed");

    if (bsdar->options & AR_S && bsdar->options & AR_SS)
        bsdar_errc(bsdar, 0, "only one of -s and -S options allowed");

    if (bsdar->stopped)
        return (bsdar->status);

    if (bsdar->options & (AR_A | AR_B)) {
        if (*argv == NULL) {
            bsdar_errc(bsdar, 0, "no position operand specified");
            return (bsdar->status);
        }
        bsdar->posarg = bsdar_basename(*argv);
        argc--;
        argv++;
    }

    /* Set determinstic mode for -D, and by default without -U. */
    if (Dflag || (Uflag == 0 && (bsdar->mode == 'q' || bsdar->mode == 'r' ||
                                 (bsdar->mode == '\0' && bsdar->options & AR_S))))
        bsdar->options |= AR_D;

    if (bsdar->options & AR_A)
        only_mode(bsdar, "-a", "mqr");
    if (bsdar->options & AR_B)
        only_mode(bsdar, "-b", "mqr");
    if (bsdar->options & AR_C)
        only_mode(bsdar, "-c", "qr");
    if (bsdar->options & AR_CC)
        only_mode(bsdar, "-C", "x");
    if (Dflag)
        only_mode(bsdar, "-D", "qr");
    if (Uflag)
        only_mode(bsdar, "-U", "qr");
    if (bsdar->options & AR_O)
        only_mode(bsdar, "-o", "x");
    if (bsdar->options & AR_SS)
        only_mode(bsdar, "-S", "mqr");
    if (bsdar->options & AR_U)
        only_mode(bsdar, "-u", "qrx");

    if ((bsdar->filename = *argv) == NULL)
        bsdar_usage(bsdar);
    if (bsdar->stopped)
        return (bsdar->status);

    bsdar->argc = --argc;
    bsdar->argv = ++argv;
    return (-1);
}

/*
 * Braam: the second half of main(), once the front end has read the files.
 */
int ar_run(struct bsdar *bsdar)
{
    int exitcode, i;

    exitcode = EXIT_SUCCESS;

    if (bsdar->ranlib) {
        for (i = 0; i < bsdar->argc; i++) {
            bsdar->filename = bsdar->argv[i];
            if (ar_write_archive(bsdar, 's'))
                exitcode = EXIT_FAILURE;
            if (bsdar->stopped)
                return (bsdar->status);
        }
        return (exitcode);
    }

    if ((!bsdar->mode || strchr("ptx", bsdar->mode)) && bsdar->options & AR_S) {
        exitcode = ar_write_archive(bsdar, 's');
        if (bsdar->stopped)
            return (bsdar->status);
        if (!bsdar->mode)
            return (exitcode);
    }

    switch (bsdar->mode) {
    case 'd':
    case 'm':
    case 'q':
    case 'r':
        exitcode = ar_write_archive(bsdar, bsdar->mode);
        break;
    case 'p':
    case 't':
    case 'x':
        exitcode = ar_read_archive(bsdar, bsdar->mode, 1);
        break;
    default:
        bsdar_usage(bsdar);
        /* NOTREACHED */
    }
    if (bsdar->stopped)
        return (bsdar->status);

    for (i = 0; i < bsdar->argc; i++) {
        if (bsdar->argv[i] != NULL) {
            bsdar_warnc(bsdar, 0, "%s: not found in archive", bsdar->argv[i]);
            exitcode = EXIT_FAILURE;
        }
    }

    return (exitcode);
}

static void set_mode(struct bsdar *bsdar, char opt)
{
    if (bsdar->mode != '\0' && bsdar->mode != opt)
        bsdar_errc(bsdar, 0, "Can't specify both -%c and -%c", opt, bsdar->mode);
    bsdar->mode = opt;
}

static void only_mode(struct bsdar *bsdar, const char *opt, const char *valid_modes)
{
    if (strchr(valid_modes, bsdar->mode) == NULL)
        bsdar_errc(bsdar, 0, "Option %s is not permitted in mode -%c", opt, bsdar->mode);
}

static void bsdar_usage(struct bsdar *bsdar)
{
    /* Braam: the usage block every command here prints. */
    bsdar_printf(bsdar, 2, "%s", AR_USAGE);
    bsdar_exit(bsdar, EXIT_FAILURE);
}

static void ranlib_usage(struct bsdar *bsdar)
{
    bsdar_printf(bsdar, 2, "usage:	ranlib [-DtU] archive ...\n");
    bsdar_printf(bsdar, 2, "\tranlib -V\n");
    bsdar_exit(bsdar, EXIT_FAILURE);
}

static void bsdar_version(struct bsdar *bsdar)
{
    bsdar_printf(bsdar, 1, "BSD ar %s\n", BSDAR_VERSION);
    bsdar_exit(bsdar, EXIT_SUCCESS);
}

static void ranlib_version(struct bsdar *bsdar)
{
    bsdar_printf(bsdar, 1, "ranlib %s\n", BSDAR_VERSION);
    bsdar_exit(bsdar, EXIT_SUCCESS);
}
