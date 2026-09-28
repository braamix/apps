/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2007 Kai Wang
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
#pragma once

#include <stdint.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <time.h>

#include "kernel/span.h"
#include "kernel/string.h"
#include "kernel/vec.h"

#define BSDAR_VERSION "1.1.0"

/*
 * ar(1) options.
 */
#define AR_A  0x0001 /* position-after */
#define AR_B  0x0002 /* position-before */
#define AR_C  0x0004 /* creating new archive */
#define AR_CC 0x0008 /* do not overwrite when extracting */
#define AR_J  0x0010 /* bzip2 compression */
#define AR_O  0x0020 /* preserve original mtime when extracting */
#define AR_S  0x0040 /* write archive symbol table */
#define AR_SS 0x0080 /* do not write archive symbol table */
#define AR_TR 0x0100 /* only keep first 15 chars for member name */
#define AR_U  0x0200 /* only extract or update newer members.*/
#define AR_V  0x0400 /* verbose mode */
#define AR_Z  0x0800 /* gzip compression */
#define AR_D  0x1000 /* insert dummy mode, mtime, uid and gid */

/*
 * In-memory representation of archive member(object).
 */
struct ar_obj {
    char *name;      /* member name */
    const u8 *maddr; /* its bytes: a view of what the front end read */
    uid_t uid;       /* user id */
    gid_t gid;       /* group id */
    mode_t md;       /* octal file permissions */
    size_t size;     /* member size */
    time_t mtime;    /* modification time */

    TAILQ_ENTRY(ar_obj) objs;
};

/*
 * Braam: a path the front end looked at before ar ran, in place of open(2),
 * fstat(2) and mmap(2).
 */
struct ar_file {
    const char *path;
    int error;    /* errno of the stat or the read; 0 if found */
    bool regular; /* an ordinary file */
    time_t mtime; /* seconds */
    Bytes data;   /* read, if it was wanted */
};

/*
 * Braam: a file the front end writes after ar ran. An archive is replaced
 * whole; a member extracted is written in place.
 */
struct ar_write {
    const char *path;
    Vec<u8> image; /* an archive's bytes */
    Bytes data;    /* what is written: the image, or a member's bytes */
    bool archive;
};

/*
 * Braam: output, in the order it was made, for the front end to write.
 */
struct ar_chunk {
    int fd;
    String text;
};

/*
 * Structure encapsulates the "global" data for "ar" program.
 */
struct bsdar {
    const char *filename; /* archive name. */
    const char *posarg;   /* position arg for modifiers -a, -b. */
    char mode;            /* program mode */
    int options;          /* command line options */

    const char *progname; /* program name */
    int argc;
    char **argv;

    /*
     * Fields for the archive string table.
     */
    char *as;      /* buffer for archive string table. */
    size_t as_sz;  /* current size of as table. */
    size_t as_cap; /* capacity of as table buffer. */

    /*
     * Fields for the archive symbol table.
     */
    uint64_t s_cnt;    /* current number of symbols. */
    uint64_t *s_so;    /* symbol offset table. */
    uint64_t s_so_max; /* maximum symbol offset. */
    size_t s_so_cap;   /* capacity of so table buffer. */

    char *s_sn;      /* symbol name table */
    size_t s_sn_cap; /* capacity of sn table buffer. */
    size_t s_sn_sz;  /* current size of sn table. */
    /* Current member's offset (relative to the end of pseudo members.) */
    off_t rela_off;

    TAILQ_HEAD(, ar_obj) v_obj; /* object(member) list */

    /*
     * Braam: what the front end found and will do, and how ar stopped.
     */
    bool ranlib;  /* invoked as ranlib: argv names archives */
    bool stopped; /* bsdar_errc, usage or version: exit(status) */
    int status;
    long tz;          /* seconds east of UTC, in place of localtime */
    bool time_warned; /* -o's warning is given once */
    Vec<ar_file> files;
    Vec<ar_write> writes;
    Vec<ar_chunk> output;
};

/*
 * Braam: main in two halves. ar_options parses the command line; then the
 * front end reads the files it names, and ar_run does the rest. Each
 * returns the exit status; ar_options returns -1 to go on.
 */
int ar_options(struct bsdar *bsdar, int argc, char **argv);
int ar_run(struct bsdar *bsdar);
struct ar_file *ar_lookup(struct bsdar *bsdar, const char *path);

int ar_read_archive(struct bsdar *ar, int mode, int out);
void ar_read_error(struct bsdar *bsdar, Bytes file, const String &why, int fatal);
int ar_write_archive(struct bsdar *ar, int mode);
void bsdar_errc(struct bsdar *, int _code, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));
void bsdar_warnc(struct bsdar *, int _code, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));
void bsdar_exit(struct bsdar *, int status);
const char *bsdar_basename(const char *path);

/*
 * Braam: fprintf(3) and write(2) to stdout or stderr, into bsdar->output.
 */
void bsdar_printf(struct bsdar *, int fd, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));
void bsdar_write(struct bsdar *, int fd, const void *buf, size_t s);
