// Generated from FreeBSD gzip — do not edit by hand; run tools/convert.py
#include <bzlib.h>
#include <lzma.h>
#include <zstd.h>

#include "braam.h"
#include "kernel/args.h"
#include "kernel/vec.h"

static Task<off_t> gz_compress(int, int, off_t *, const char *, uint32_t);
static Task<off_t> gz_uncompress(int, int, char *, size_t, off_t *, const char *);
static Task<off_t> file_compress(char *, char *, size_t);
static Task<off_t> file_uncompress(char *, char *, size_t);
static Task<off_t> cat_fd(unsigned char *, size_t, off_t *, int);
static Task<int> check_outfile(const char *);
static Task<void> handle_file(char *, struct stat *);
static Task<void> handle_dir(char *);
static Task<void> print_list_out(off_t, off_t, const char *);
static Task<void> print_verbage(const char *, const char *, off_t, off_t);
static Task<void> print_test(const char *, int);
static Task<void> unlink_input(const char *, const struct stat *);

/*	$NetBSD: gzip.c,v 1.116 2018/10/27 11:39:12 skrll Exp $	*/

/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1997, 1998, 2003, 2004, 2006, 2008, 2009, 2010, 2011, 2015, 2017
 *    Matthew R. Green
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * gzip.c -- GPL free gzip using zlib.
 *
 * RFC 1950 covers the zlib format
 * RFC 1951 covers the deflate format
 * RFC 1952 covers the gzip format
 *
 * TODO:
 *	- use mmap where possible
 *	- make bzip2/compress -v/-t/-l support work as well as possible
 */

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/endian.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>

/* what type of file are we dealing with */
enum filetype {
    FT_GZIP,
#ifndef NO_BZIP2_SUPPORT
    FT_BZIP2,
#endif
#ifndef NO_COMPRESS_SUPPORT
    FT_Z,
#endif
#ifndef NO_PACK_SUPPORT
    FT_PACK,
#endif
#ifndef NO_XZ_SUPPORT
    FT_XZ,
#endif
#ifndef NO_LZ_SUPPORT
    FT_LZ,
#endif
#ifndef NO_ZSTD_SUPPORT
    FT_ZSTD,
#endif
    FT_LAST,
    FT_UNKNOWN
};

#ifndef NO_BZIP2_SUPPORT
#include <bzlib.h>

#define BZ2_SUFFIX  ".bz2"
#define BZIP2_MAGIC "BZh"
#endif

#ifndef NO_COMPRESS_SUPPORT
#define Z_SUFFIX ".Z"
#define Z_MAGIC  "\037\235"
#endif

#ifndef NO_PACK_SUPPORT
#define PACK_MAGIC "\037\036"
#endif

#ifndef NO_XZ_SUPPORT
#include <lzma.h>
#define XZ_SUFFIX ".xz"
#define XZ_MAGIC  "\3757zXZ"
#endif

#ifndef NO_LZ_SUPPORT
#define LZ_SUFFIX ".lz"
#define LZ_MAGIC  "LZIP"
#endif

#ifndef NO_ZSTD_SUPPORT
#include <zstd.h>
#define ZSTD_SUFFIX ".zst"
#define ZSTD_MAGIC  "\050\265\057\375"
#endif

#define GZ_SUFFIX ".gz"

#define BUFLEN (64 * 1024)

#define GZIP_MAGIC0  0x1F
#define GZIP_MAGIC1  0x8B
#define GZIP_OMAGIC1 0x9E

#define GZIP_TIMESTAMP (off_t)4
#define GZIP_ORIGNAME  (off_t)10

#define HEAD_CRC    0x02
#define EXTRA_FIELD 0x04
#define ORIG_NAME   0x08
#define COMMENT     0x10

#define OS_CODE 3 /* Unix */

suffixes_t suffixes[] = {
#define SUFFIX(Z, N) { Z, sizeof Z - 1, N }
    SUFFIX(GZ_SUFFIX, ""), /* Overwritten by -S .xxx */
    SUFFIX(GZ_SUFFIX, ""),   SUFFIX(".z", ""),       SUFFIX("-gz", ""),       SUFFIX("-z", ""),
    SUFFIX("_z", ""),        SUFFIX(".taz", ".tar"), SUFFIX(".tgz", ".tar"),
#ifndef NO_BZIP2_SUPPORT
    SUFFIX(BZ2_SUFFIX, ""),  SUFFIX(".tbz", ".tar"), SUFFIX(".tbz2", ".tar"),
#endif
#ifndef NO_COMPRESS_SUPPORT
    SUFFIX(Z_SUFFIX, ""),
#endif
#ifndef NO_XZ_SUPPORT
    SUFFIX(XZ_SUFFIX, ""),
#endif
#ifndef NO_LZ_SUPPORT
    SUFFIX(LZ_SUFFIX, ""),
#endif
#ifndef NO_ZSTD_SUPPORT
    SUFFIX(ZSTD_SUFFIX, ""),
#endif
    SUFFIX(GZ_SUFFIX, ""), /* Overwritten by -S "" */
#undef SUFFIX
};
const int gzip_num_suffixes = (int)(sizeof(suffixes) / sizeof(suffixes[0]));
#define NUM_SUFFIXES  gzip_num_suffixes
#define SUFFIX_MAXLEN 30

static const char gzip_version[] = "FreeBSD gzip 20190107";

static const char gzip_copyright[] =
    "   Copyright (c) 1997, 1998, 2003, 2004, 2006 Matthew R. Green\n"
    "   All rights reserved.\n"
    "\n"
    "   Redistribution and use in source and binary forms, with or without\n"
    "   modification, are permitted provided that the following conditions\n"
    "   are met:\n"
    "   1. Redistributions of source code must retain the above copyright\n"
    "      notice, this list of conditions and the following disclaimer.\n"
    "   2. Redistributions in binary form must reproduce the above copyright\n"
    "      notice, this list of conditions and the following disclaimer in the\n"
    "      documentation and/or other materials provided with the distribution.\n"
    "\n"
    "   THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR\n"
    "   IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES\n"
    "   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.\n"
    "   IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,\n"
    "   INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,\n"
    "   BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;\n"
    "   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED\n"
    "   AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,\n"
    "   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY\n"
    "   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF\n"
    "   SUCH DAMAGE.";

/* compress input to output. Return bytes read, -1 on error */
static Task<off_t> gz_compress(int in, int out, off_t *gsizep, const char *origname, uint32_t mtime)
{
    z_stream z;
    char *outbufp, *inbufp;
    off_t in_tot = 0, out_tot = 0;
    ssize_t in_size;
    int i, error;
    uLong crc;

    outbufp = (char *)malloc(BUFLEN);
    inbufp  = (char *)malloc(BUFLEN);
    if (outbufp == NULL || inbufp == NULL) {
        maybe_err("malloc failed");
        goto out;
    }

    memset(&z, 0, sizeof z);
    z.zalloc = Z_NULL;
    z.zfree  = Z_NULL;
    z.opaque = 0;

    if (nflag != 0) {
        mtime    = 0;
        origname = "";
    }

    i = snprintf(outbufp, BUFLEN, "%c%c%c%c%c%c%c%c%c%c%s", GZIP_MAGIC0, GZIP_MAGIC1, Z_DEFLATED,
                 *origname ? ORIG_NAME : 0, mtime & 0xff, (mtime >> 8) & 0xff, (mtime >> 16) & 0xff,
                 (mtime >> 24) & 0xff,
                 numflag == 1   ? 4
                 : numflag == 9 ? 2
                                : 0,
                 OS_CODE, origname);
    if (i >= BUFLEN)
        /* this need PATH_MAX > BUFLEN ... */
        maybe_err("snprintf");
    if (*origname)
        i++;

    z.next_out  = (unsigned char *)outbufp + i;
    z.avail_out = BUFLEN - i;

    error = deflateInit2(&z, numflag, Z_DEFLATED, (-MAX_WBITS), 8, Z_DEFAULT_STRATEGY);
    if (error != Z_OK) {
        maybe_warnx("deflateInit2 failed");
        in_tot = -1;
        goto out;
    }

    crc = crc32(0L, Z_NULL, 0);
    for (;;) {
        if (z.avail_out == 0) {
            if (co_await write_retry(out, outbufp, BUFLEN) != BUFLEN) {
                maybe_warn("write");
                out_tot = -1;
                goto out;
            }

            out_tot += BUFLEN;
            z.next_out  = (unsigned char *)outbufp;
            z.avail_out = BUFLEN;
        }

        if (z.avail_in == 0) {
            in_size = co_await read_retry(in, inbufp, BUFLEN);
            if (in_size < 0) {
                maybe_warn("read");
                in_tot = -1;
                goto out;
            }
            if (in_size == 0)
                break;
            infile_newdata(in_size);

            crc = crc32(crc, (const Bytef *)inbufp, (unsigned)in_size);
            in_tot += in_size;
            z.next_in  = (unsigned char *)inbufp;
            z.avail_in = in_size;
        }

        error = deflate(&z, Z_NO_FLUSH);
        if (error != Z_OK && error != Z_STREAM_END) {
            maybe_warnx("deflate failed");
            in_tot = -1;
            goto out;
        }
    }

    /* clean up */
    for (;;) {
        size_t len;
        ssize_t w;

        error = deflate(&z, Z_FINISH);
        if (error != Z_OK && error != Z_STREAM_END) {
            maybe_warnx("deflate failed");
            in_tot = -1;
            goto out;
        }

        len = (char *)z.next_out - outbufp;

        w = co_await write_retry(out, outbufp, len);
        if (w == -1 || (size_t)w != len) {
            maybe_warn("write");
            out_tot = -1;
            goto out;
        }
        out_tot += len;
        z.next_out  = (unsigned char *)outbufp;
        z.avail_out = BUFLEN;

        if (error == Z_STREAM_END)
            break;
    }

    if (deflateEnd(&z) != Z_OK) {
        maybe_warnx("deflateEnd failed");
        in_tot = -1;
        goto out;
    }

    i = snprintf(outbufp, BUFLEN, "%c%c%c%c%c%c%c%c", (int)crc & 0xff, (int)(crc >> 8) & 0xff,
                 (int)(crc >> 16) & 0xff, (int)(crc >> 24) & 0xff, (int)in_tot & 0xff,
                 (int)(in_tot >> 8) & 0xff, (int)(in_tot >> 16) & 0xff, (int)(in_tot >> 24) & 0xff);
    if (i != 8)
        maybe_err("snprintf");
    if (co_await write_retry(out, outbufp, i) != i) {
        maybe_warn("write");
        in_tot = -1;
    } else
        out_tot += i;

out:
    if (inbufp != NULL)
        free(inbufp);
    if (outbufp != NULL)
        free(outbufp);
    if (gsizep)
        *gsizep = out_tot;
    co_return in_tot;
}

/*
 * uncompress input to output then close the input.  return the
 * uncompressed size written, and put the compressed sized read
 * into `*gsizep'.
 */
static Task<off_t> gz_uncompress(int in, int out, char *pre, size_t prelen, off_t *gsizep,
                                 const char *filename)
{
    z_stream z;
    char *outbufp, *inbufp;
    off_t out_tot = -1, in_tot = 0;
    uint32_t out_sub_tot = 0;
    enum {
        GZSTATE_MAGIC0,
        GZSTATE_MAGIC1,
        GZSTATE_METHOD,
        GZSTATE_FLAGS,
        GZSTATE_SKIPPING,
        GZSTATE_EXTRA,
        GZSTATE_EXTRA2,
        GZSTATE_EXTRA3,
        GZSTATE_ORIGNAME,
        GZSTATE_COMMENT,
        GZSTATE_HEAD_CRC1,
        GZSTATE_HEAD_CRC2,
        GZSTATE_INIT,
        GZSTATE_READ,
        GZSTATE_CRC,
        GZSTATE_LEN,
    };
    int state = GZSTATE_MAGIC0;
    int flags = 0, skip_count = 0;
    int error = Z_STREAM_ERROR, done_reading = 0;
    uLong crc = 0;
    ssize_t wr;
    int needmore = 0;

#define ADVANCE()     \
    {                 \
        z.next_in++;  \
        z.avail_in--; \
    }

    if ((outbufp = (char *)malloc(BUFLEN)) == NULL) {
        maybe_err("malloc failed");
        goto out2;
    }
    if ((inbufp = (char *)malloc(BUFLEN)) == NULL) {
        maybe_err("malloc failed");
        goto out1;
    }

    memset(&z, 0, sizeof z);
    z.avail_in  = prelen;
    z.next_in   = (unsigned char *)pre;
    z.avail_out = BUFLEN;
    z.next_out  = (unsigned char *)outbufp;
    z.zalloc    = NULL;
    z.zfree     = NULL;
    z.opaque    = 0;

    in_tot  = prelen;
    out_tot = 0;

    for (;;) {
        check_siginfo();
        if ((z.avail_in == 0 || needmore) && done_reading == 0) {
            ssize_t in_size;

            if (z.avail_in > 0) {
                memmove(inbufp, z.next_in, z.avail_in);
            }
            z.next_in = (unsigned char *)inbufp;
            in_size   = co_await read_retry(in, z.next_in + z.avail_in, BUFLEN - z.avail_in);

            if (in_size == -1) {
                maybe_warn("failed to read stdin");
                goto stop_and_fail;
            } else if (in_size == 0) {
                done_reading = 1;
            }
            infile_newdata(in_size);

            z.avail_in += in_size;
            needmore = 0;

            in_tot += in_size;
        }
        if (z.avail_in == 0) {
            if (done_reading && state != GZSTATE_MAGIC0) {
                maybe_warnx("%s: unexpected end of file", filename);
                goto stop_and_fail;
            }
            goto stop;
        }
        switch (state) {
        case GZSTATE_MAGIC0:
            if (*z.next_in != GZIP_MAGIC0) {
                if (in_tot > 0) {
                    maybe_warnx(
                        "%s: trailing garbage "
                        "ignored",
                        filename);
                    exit_value = 2;
                    goto stop;
                }
                maybe_warnx("input not gziped (MAGIC0)");
                goto stop_and_fail;
            }
            ADVANCE();
            state       = (int)state + 1;
            out_sub_tot = 0;
            crc         = crc32(0L, Z_NULL, 0);
            break;

        case GZSTATE_MAGIC1:
            if (*z.next_in != GZIP_MAGIC1 && *z.next_in != GZIP_OMAGIC1) {
                maybe_warnx("input not gziped (MAGIC1)");
                goto stop_and_fail;
            }
            ADVANCE();
            state = (int)state + 1;
            break;

        case GZSTATE_METHOD:
            if (*z.next_in != Z_DEFLATED) {
                maybe_warnx("unknown compression method");
                goto stop_and_fail;
            }
            ADVANCE();
            state = (int)state + 1;
            break;

        case GZSTATE_FLAGS:
            flags = *z.next_in;
            ADVANCE();
            skip_count = 6;
            state      = (int)state + 1;
            break;

        case GZSTATE_SKIPPING:
            if (skip_count > 0) {
                skip_count--;
                ADVANCE();
            } else
                state = (int)state + 1;
            break;

        case GZSTATE_EXTRA:
            if ((flags & EXTRA_FIELD) == 0) {
                state = GZSTATE_ORIGNAME;
                break;
            }
            skip_count = *z.next_in;
            ADVANCE();
            state = (int)state + 1;
            break;

        case GZSTATE_EXTRA2:
            skip_count |= ((*z.next_in) << 8);
            ADVANCE();
            state = (int)state + 1;
            break;

        case GZSTATE_EXTRA3:
            if (skip_count > 0) {
                skip_count--;
                ADVANCE();
            } else
                state = (int)state + 1;
            break;

        case GZSTATE_ORIGNAME:
            if ((flags & ORIG_NAME) == 0) {
                state = (int)state + 1;
                break;
            }
            if (*z.next_in == 0)
                state = (int)state + 1;
            ADVANCE();
            break;

        case GZSTATE_COMMENT:
            if ((flags & COMMENT) == 0) {
                state = (int)state + 1;
                break;
            }
            if (*z.next_in == 0)
                state = (int)state + 1;
            ADVANCE();
            break;

        case GZSTATE_HEAD_CRC1:
            if (flags & HEAD_CRC)
                skip_count = 2;
            else
                skip_count = 0;
            state = (int)state + 1;
            break;

        case GZSTATE_HEAD_CRC2:
            if (skip_count > 0) {
                skip_count--;
                ADVANCE();
            } else
                state = (int)state + 1;
            break;

        case GZSTATE_INIT:
            if (inflateInit2(&z, -MAX_WBITS) != Z_OK) {
                maybe_warnx("failed to inflateInit");
                goto stop_and_fail;
            }
            state = (int)state + 1;
            break;

        case GZSTATE_READ:
            error = inflate(&z, Z_FINISH);
            switch (error) {
            /* Z_BUF_ERROR goes with Z_FINISH... */
            case Z_BUF_ERROR:
                if (z.avail_out > 0 && !done_reading)
                    continue;

            case Z_STREAM_END:
            case Z_OK:
                break;

            case Z_NEED_DICT:
                maybe_warnx("Z_NEED_DICT error");
                goto stop_and_fail;
            case Z_DATA_ERROR:
                maybe_warnx("data stream error");
                goto stop_and_fail;
            case Z_STREAM_ERROR:
                maybe_warnx("internal stream error");
                goto stop_and_fail;
            case Z_MEM_ERROR:
                maybe_warnx("memory allocation error");
                goto stop_and_fail;

            default:
                maybe_warn("unknown error from inflate(): %d", error);
            }
            wr = BUFLEN - z.avail_out;

            if (wr != 0) {
                crc = crc32(crc, (const Bytef *)outbufp, (unsigned)wr);
                if (
                    /* don't write anything with -t */
                    tflag == 0 && co_await write_retry(out, outbufp, wr) != wr) {
                    maybe_warn("error writing to output");
                    goto stop_and_fail;
                }

                out_tot += wr;
                out_sub_tot += wr;
            }

            if (error == Z_STREAM_END) {
                inflateEnd(&z);
                state = (int)state + 1;
            }

            z.next_out  = (unsigned char *)outbufp;
            z.avail_out = BUFLEN;

            break;
        case GZSTATE_CRC: {
            uLong origcrc;

            if (z.avail_in < 4) {
                if (!done_reading) {
                    needmore = 1;
                    continue;
                }
                maybe_warnx("truncated input");
                goto stop_and_fail;
            }
            origcrc = le32dec(&z.next_in[0]);
            if (origcrc != crc) {
                maybe_warnx(
                    "invalid compressed"
                    " data--crc error");
                goto stop_and_fail;
            }
        }

            z.avail_in -= 4;
            z.next_in += 4;

            if (!z.avail_in && done_reading) {
                goto stop;
            }
            state = (int)state + 1;
            break;
        case GZSTATE_LEN: {
            uLong origlen;

            if (z.avail_in < 4) {
                if (!done_reading) {
                    needmore = 1;
                    continue;
                }
                maybe_warnx("truncated input");
                goto stop_and_fail;
            }
            origlen = le32dec(&z.next_in[0]);

            if (origlen != out_sub_tot) {
                maybe_warnx(
                    "invalid compressed"
                    " data--length error");
                goto stop_and_fail;
            }
        }

            z.avail_in -= 4;
            z.next_in += 4;

            if (error < 0) {
                maybe_warnx("decompression error");
                goto stop_and_fail;
            }
            state = GZSTATE_MAGIC0;
            break;
        }
        continue;
    stop_and_fail:
        out_tot = -1;
    stop:
        break;
    }
    if (state > GZSTATE_INIT)
        inflateEnd(&z);

    free(inbufp);
out1:
    free(outbufp);
out2:
    if (gsizep)
        *gsizep = in_tot;
    co_return (out_tot);
}

/*
 * set the owner, mode, flags & utimes using the given file descriptor.
 * file is only used in possible warning messages.
 */
static Task<void> copymodes(int fd, const struct stat *sbp, const char *file)
{
    (void)fd;
    (void)sbp;
    (void)file;
    co_return;
}

/* what sort of file is this? */
static enum filetype file_gettype(u_char *buf)
{
    if (buf[0] == GZIP_MAGIC0 && (buf[1] == GZIP_MAGIC1 || buf[1] == GZIP_OMAGIC1))
        return FT_GZIP;
#ifndef NO_BZIP2_SUPPORT
    else if (memcmp(buf, BZIP2_MAGIC, 3) == 0 && buf[3] >= '0' && buf[3] <= '9')
        return FT_BZIP2;
#endif
#ifndef NO_COMPRESS_SUPPORT
    else if (memcmp(buf, Z_MAGIC, 2) == 0)
        return FT_Z;
#endif
#ifndef NO_PACK_SUPPORT
    else if (memcmp(buf, PACK_MAGIC, 2) == 0)
        return FT_PACK;
#endif
#ifndef NO_XZ_SUPPORT
    else if (memcmp(buf, XZ_MAGIC, 4) == 0) /* XXX: We only have 4 bytes */
        return FT_XZ;
#endif
#ifndef NO_LZ_SUPPORT
    else if (memcmp(buf, LZ_MAGIC, 4) == 0)
        return FT_LZ;
#endif
#ifndef NO_ZSTD_SUPPORT
    else if (memcmp(buf, ZSTD_MAGIC, 4) == 0)
        return FT_ZSTD;
#endif
    else
        return FT_UNKNOWN;
}

/* check the outfile is OK. */
static Task<int> check_outfile(const char *outfile)
{
    struct stat sb;
    int ok = 1;

    if (lflag == 0 && co_await b_stat(outfile, &sb) == 0) {
        if (fflag)
            co_await b_unlink(outfile);
        else if (co_await b_isatty(STDIN_FILENO)) {
            char ans[10] = { 'n', '\0' }; /* default */

            co_await b_fprintf(stderr,
                               "%s already exists -- do you wish to "
                               "overwrite (y or n)? ",
                               outfile);
            (void)co_await b_fgets(ans, sizeof(ans) - 1, stdin);
            if (ans[0] != 'y' && ans[0] != 'Y') {
                co_await b_fprintf(stderr, "\tnot overwriting\n");
                ok = 0;
            } else
                co_await b_unlink(outfile);
        } else {
            maybe_warnx("%s already exists -- skipping", outfile);
            ok = 0;
        }
    }
    co_return ok;
}

static Task<void> unlink_input(const char *file, const struct stat *sb)
{
    struct stat nsb;

    if (kflag)
        co_return;
    if (co_await b_stat(file, &nsb) != 0)
        /* Must be gone already */
        co_return;
    if (nsb.st_dev != sb->st_dev || nsb.st_ino != sb->st_ino)
        /* Definitely a different file */
        co_return;
    co_await b_unlink(file);
}

static const suffixes_t *check_suffix(char *file, int xlate)
{
    const suffixes_t *s;
    int len = strlen(file);
    char *sp;

    for (s = suffixes; s != suffixes + NUM_SUFFIXES; s++) {
        /* if it doesn't fit in "a.suf", don't bother */
        if (s->ziplen >= len)
            continue;
        sp = file + len - s->ziplen;
        if (strcmp(s->zipped, sp) != 0)
            continue;
        if (xlate)
            strcpy(sp, s->normal);
        return s;
    }
    return NULL;
}

/*
 * compress the given file: create a corresponding .gz file and remove the
 * original.
 */
static Task<off_t> file_compress(char *file, char *outfile, size_t outsize)
{
    int in;
    int out;
    off_t size, in_size;
    struct stat isb, osb;
    const suffixes_t *suff;

    in = co_await b_open(file, O_RDONLY);
    if (in == -1) {
        maybe_warn("can't open %s", file);
        co_return (-1);
    }

    if (co_await b_fstat(in, &isb) != 0) {
        maybe_warn("couldn't stat: %s", file);
        co_await b_close(in);
        co_return (-1);
    }

    if (co_await b_fstat(in, &isb) != 0) {
        co_await b_close(in);
        maybe_warn("can't stat %s", file);
        co_return -1;
    }
    infile_set(file, isb.st_size);

    if (cflag == 0) {
        if (isb.st_nlink > 1 && fflag == 0) {
            maybe_warnx(
                "%s has %ju other link%s -- "
                "skipping",
                file, (uintmax_t)isb.st_nlink - 1, isb.st_nlink == 1 ? "" : "s");
            co_await b_close(in);
            co_return -1;
        }

        if (fflag == 0 && (suff = check_suffix(file, 0)) && suff->zipped[0] != 0) {
            maybe_warnx("%s already has %s suffix -- unchanged", file, suff->zipped);
            co_await b_close(in);
            co_return (-1);
        }

        /* Add (usually) .gz to filename */
        if ((size_t)snprintf(outfile, outsize, "%s%s", file, suffixes[0].zipped) >= outsize)
            memcpy(outfile + outsize - suffixes[0].ziplen - 1, suffixes[0].zipped,
                   suffixes[0].ziplen + 1);

        if (co_await check_outfile(outfile) == 0) {
            co_await b_close(in);
            co_return (-1);
        }
    }

    if (cflag == 0) {
        out = co_await b_open(outfile, O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (out == -1) {
            maybe_warn("could not create output: %s", outfile);
            /* stdin */
            co_return (-1);
        }
        remove_file = outfile;
    } else
        out = STDOUT_FILENO;

    in_size = co_await gz_compress(in, out, &size, path_basename(Str(file, strlen(file))).data(),
                                   (uint32_t)isb.st_mtime);

    (void)co_await b_close(in);

    /*
     * If there was an error, in_size will be -1.
     * If we compressed to stdout, just return the size.
     * Otherwise stat the file and check it is the correct size.
     * We only blow away the file if we can stat the output and it
     * has the expected size.
     */
    if (cflag != 0)
        co_return in_size == -1 ? -1 : size;

    if (co_await b_fstat(out, &osb) != 0) {
        maybe_warn("couldn't stat: %s", outfile);
        goto bad_outfile;
    }

    if (osb.st_size != size) {
        maybe_warnx("output file: %s wrong size (%ju != %ju), deleting", outfile,
                    (uintmax_t)osb.st_size, (uintmax_t)size);
        goto bad_outfile;
    }

    copymodes(out, &isb, outfile);
    remove_file = NULL;
    if (co_await b_close(out) == -1)
        maybe_warn("couldn't close output");

    /* output is good, ok to delete input */
    co_await unlink_input(file, &isb);
    co_return (size);

bad_outfile:
    if (co_await b_close(out) == -1)
        maybe_warn("couldn't close output");

    maybe_warnx("leaving original %s", file);
    co_await b_unlink(outfile);
    co_return (size);
}

/* uncompress the given file and remove the original */
static Task<off_t> file_uncompress(char *file, char *outfile, size_t outsize)
{
    struct stat isb, osb;
    off_t size;
    ssize_t rbytes;
    unsigned char fourbytes[4];
    enum filetype method;
    int fd, ofd, zfd = -1;
    /* int error */;
    size_t in_size;
    ssize_t rv;
    time_t timestamp = 0;
    char name[PATH_MAX + 1];

    /* gather the old name info */

    fd = co_await b_open(file, O_RDONLY);
    if (fd < 0) {
        maybe_warn("can't open %s", file);
        goto lose;
    }
    if (co_await b_fstat(fd, &isb) != 0) {
        maybe_warn("can't stat %s", file);
        goto lose;
    }
    if (S_ISREG(isb.st_mode))
        in_size = isb.st_size;
    else
        in_size = 0;
    infile_set(file, in_size);

    strlcpy(outfile, file, outsize);
    if (check_suffix(outfile, 1) == NULL && !(cflag || lflag)) {
        maybe_warnx("%s: unknown suffix -- ignored", file);
        goto lose;
    }

    rbytes = co_await read_retry(fd, fourbytes, sizeof fourbytes);
    if (rbytes != sizeof fourbytes) {
        /* we don't want to fail here. */
        if (fflag)
            goto lose;
        if (rbytes == -1)
            maybe_warn("can't read %s", file);
        else
            goto unexpected_EOF;
        goto lose;
    }
    infile_newdata(rbytes);

    method = file_gettype(fourbytes);
    if (fflag == 0 && method == FT_UNKNOWN) {
        maybe_warnx("%s: not in gzip format", file);
        goto lose;
    }

    if (method == FT_GZIP && Nflag) {
        unsigned char ts[4]; /* timestamp */

        rv = co_await gzip_pread(fd, ts, sizeof ts, GZIP_TIMESTAMP);
        if (rv >= 0 && rv < (ssize_t)(sizeof ts))
            goto unexpected_EOF;
        if (rv == -1) {
            if (!fflag)
                maybe_warn("can't read %s", file);
            goto lose;
        }
        infile_newdata(rv);
        timestamp = le32dec(&ts[0]);

        if (fourbytes[3] & ORIG_NAME) {
            rbytes = co_await gzip_pread(fd, name, sizeof(name) - 1, GZIP_ORIGNAME);
            if (rbytes < 0) {
                maybe_warn("can't read %s", file);
                goto lose;
            }
            if (name[0] != '\0') {
                char *dp, *nf;

                /* Make sure that name is NUL-terminated */
                name[rbytes] = '\0';

                /* strip saved directory name */
                nf = strrchr(name, '/');
                if (nf == NULL)
                    nf = name;
                else
                    nf++;

                /* preserve original directory name */
                dp = strrchr(file, '/');
                if (dp == NULL)
                    dp = file;
                else
                    dp++;
                snprintf(outfile, outsize, "%.*s%.*s", (int)(dp - file), file, (int)rbytes, nf);
            }
        }
    }
    co_await b_lseek(fd, 0, SEEK_SET);

    if (cflag == 0 || lflag) {
        if (isb.st_nlink > 1 && lflag == 0 && fflag == 0) {
            maybe_warnx("%s has %ju other links -- skipping", file, (uintmax_t)isb.st_nlink - 1);
            goto lose;
        }
        if (nflag == 0 && timestamp)
            isb.st_mtime = timestamp;
        if (co_await check_outfile(outfile) == 0)
            goto lose;
    }

    if (cflag)
        zfd = STDOUT_FILENO;
    else if (lflag)
        zfd = -1;
    else {
        zfd = co_await b_open(outfile, O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (zfd == STDOUT_FILENO) {
            /* We won't close STDOUT_FILENO later... */
            zfd = co_await b_dup(zfd);
            co_await b_close(STDOUT_FILENO);
        }
        if (zfd == -1) {
            maybe_warn("can't open %s", outfile);
            goto lose;
        }
        remove_file = outfile;
    }

    switch (method) {
#ifndef NO_BZIP2_SUPPORT
    case FT_BZIP2:
        /* XXX */
        if (lflag) {
            maybe_warnx("no -l with bzip2 files");
            goto lose;
        }

        size = co_await unbzip2(fd, zfd, NULL, 0, NULL);
        break;
#endif

#ifndef NO_COMPRESS_SUPPORT
    case FT_Z: {
        if (lflag) {
            maybe_warnx("no -l with Lempel-Ziv files");
            goto lose;
        }
        size = co_await zuncompress_fd(fd, zfd, (char *)fourbytes, sizeof fourbytes, NULL);
        break;
    }
#endif

#ifndef NO_PACK_SUPPORT
    case FT_PACK:
        if (lflag) {
            maybe_warnx("no -l with packed files");
            goto lose;
        }

        size = co_await unpack(fd, zfd, NULL, 0, NULL);
        break;
#endif

#ifndef NO_XZ_SUPPORT
    case FT_XZ:
        if (lflag) {
            size = co_await unxz_len(fd);
            if (!tflag) {
                co_await print_list_out(in_size, size, file);
                co_await b_close(fd);
                co_return -1;
            }
        } else
            size = co_await unxz(fd, zfd, NULL, 0, NULL);
        break;
#endif

#ifndef NO_LZ_SUPPORT
    case FT_LZ:
        if (lflag) {
            maybe_warnx("no -l with lzip files");
            goto lose;
        }
        size = co_await unlz(fd, zfd, NULL, 0, NULL);
        break;
#endif

#ifndef NO_ZSTD_SUPPORT
    case FT_ZSTD:
        if (lflag) {
            maybe_warnx("no -l with zstd files");
            goto lose;
        }
        size = co_await unzstd(fd, zfd, NULL, 0, NULL);
        break;
#endif
    case FT_UNKNOWN:
        if (lflag) {
            maybe_warnx("no -l for unknown filetypes");
            goto lose;
        }
        size = co_await cat_fd(NULL, 0, NULL, fd);
        break;
    default:
        if (lflag) {
            co_await print_list(fd, in_size, outfile, isb.st_mtime);
            if (!tflag) {
                co_await b_close(fd);
                co_return -1; /* XXX */
            }
        }

        size = co_await gz_uncompress(fd, zfd, NULL, 0, NULL, file);
        break;
    }

    if (co_await b_close(fd) != 0)
        maybe_warn("couldn't close input");
    if (zfd != STDOUT_FILENO && co_await b_close(zfd) != 0)
        maybe_warn("couldn't close output");

    if (size == -1) {
        if (cflag == 0)
            co_await b_unlink(outfile);
        maybe_warnx("%s: uncompress failed", file);
        co_return -1;
    }

    /* if testing, or we uncompressed to stdout, this is all we need */
    if (tflag)
        co_return size;
    /* if we are uncompressing to stdin, don't remove the file. */
    if (cflag)
        co_return size;

    /*
     * if we create a file...
     */
    /*
     * if we can't stat the file don't remove the file.
     */

    ofd = co_await b_open(outfile, O_RDWR, 0);
    if (ofd == -1) {
        maybe_warn("couldn't co_await b_open(leaving original): %s", outfile);
        co_return -1;
    }
    if (co_await b_fstat(ofd, &osb) != 0) {
        maybe_warn("couldn't co_await b_stat(leaving original): %s", outfile);
        co_await b_close(ofd);
        co_return -1;
    }
    if (osb.st_size != size) {
        maybe_warnx("stat gave different size: %ju != %ju (leaving original)", (uintmax_t)size,
                    (uintmax_t)osb.st_size);
        co_await b_close(ofd);
        co_await b_unlink(outfile);
        co_return -1;
    }
    copymodes(ofd, &isb, outfile);
    remove_file = NULL;
    co_await b_close(ofd);
    co_await unlink_input(file, &isb);
    co_return size;

unexpected_EOF:
    maybe_warnx("%s: unexpected end of file", file);
lose:
    if (fd != -1)
        co_await b_close(fd);
    if (zfd != -1 && zfd != STDOUT_FILENO)
        co_await b_close(zfd);
    co_return -1;
}

static Task<off_t> cat_fd(unsigned char *prepend, size_t count, off_t *gsizep, int fd)
{
    char buf[BUFLEN];
    off_t in_tot;
    ssize_t w;

    in_tot = count;
    w      = co_await write_retry(STDOUT_FILENO, prepend, count);
    if (w == -1 || (size_t)w != count) {
        maybe_warn("write to stdout");
        co_return -1;
    }
    for (;;) {
        ssize_t rv;

        rv = co_await read_retry(fd, buf, sizeof buf);
        if (rv == 0)
            break;
        if (rv < 0) {
            maybe_warn("read from fd %d", fd);
            break;
        }
        infile_newdata(rv);

        if (co_await write_retry(STDOUT_FILENO, buf, rv) != rv) {
            maybe_warn("write to stdout");
            break;
        }
        in_tot += rv;
    }

    if (gsizep)
        *gsizep = in_tot;
    co_return (in_tot);
}

Task<void> handle_stdin(void)
{
    struct stat isb;
    unsigned char fourbytes[4];
    size_t in_size;
    off_t usize, gsize;
    enum filetype method;
    ssize_t bytes_read;

    if (fflag == 0 && lflag == 0 && co_await b_isatty(STDIN_FILENO)) {
        maybe_warnx("standard input is a terminal -- ignoring");
        goto out;
    }

    if (co_await b_fstat(STDIN_FILENO, &isb) < 0) {
        maybe_warn("fstat");
        goto out;
    }
    if (S_ISREG(isb.st_mode))
        in_size = isb.st_size;
    else
        in_size = 0;
    infile_set("(stdin)", in_size);

    if (lflag) {
        co_await print_list(STDIN_FILENO, in_size, infile, isb.st_mtime);
        goto out;
    }

    bytes_read = co_await read_retry(STDIN_FILENO, fourbytes, sizeof fourbytes);
    if (bytes_read == -1) {
        maybe_warn("can't read stdin");
        goto out;
    } else if (bytes_read != sizeof(fourbytes)) {
        maybe_warnx("(stdin): unexpected end of file");
        goto out;
    }

    method = file_gettype(fourbytes);
    switch (method) {
    default:
        if (fflag == 0) {
            maybe_warnx("unknown compression format");
            goto out;
        }
        usize = co_await cat_fd(fourbytes, sizeof fourbytes, &gsize, STDIN_FILENO);
        break;
    case FT_GZIP:
        usize = co_await gz_uncompress(STDIN_FILENO, STDOUT_FILENO, (char *)fourbytes,
                                       sizeof fourbytes, &gsize, "(stdin)");
        break;
#ifndef NO_BZIP2_SUPPORT
    case FT_BZIP2:
        usize = co_await unbzip2(STDIN_FILENO, STDOUT_FILENO, (char *)fourbytes, sizeof fourbytes,
                                 &gsize);
        break;
#endif
#ifndef NO_COMPRESS_SUPPORT
    case FT_Z:
        usize = co_await zuncompress_fd(STDIN_FILENO, STDOUT_FILENO, (char *)fourbytes,
                                        sizeof fourbytes, &gsize);
        break;
#endif
#ifndef NO_PACK_SUPPORT
    case FT_PACK:
        usize = co_await unpack(STDIN_FILENO, STDOUT_FILENO, (char *)fourbytes, sizeof fourbytes,
                                &gsize);
        break;
#endif
#ifndef NO_XZ_SUPPORT
    case FT_XZ:
        usize =
            co_await unxz(STDIN_FILENO, STDOUT_FILENO, (char *)fourbytes, sizeof fourbytes, &gsize);
        break;
#endif
#ifndef NO_LZ_SUPPORT
    case FT_LZ:
        usize =
            co_await unlz(STDIN_FILENO, STDOUT_FILENO, (char *)fourbytes, sizeof fourbytes, &gsize);
        break;
#endif
#ifndef NO_ZSTD_SUPPORT
    case FT_ZSTD:
        usize = co_await unzstd(STDIN_FILENO, STDOUT_FILENO, (char *)fourbytes, sizeof fourbytes,
                                &gsize);
        break;
#endif
    }

    if (vflag && !tflag && usize != -1 && gsize != -1)
        co_await print_verbage(NULL, NULL, usize, gsize);
    if (vflag && tflag)
        co_await print_test("(stdin)", usize != -1);

out:
    infile_clear();
}

Task<void> handle_stdout(void)
{
    off_t gsize;
    off_t usize;
    struct stat sb;
    time_t systime;
    uint32_t mtime;
    int ret;

    infile_set("(stdout)", 0);

    if (fflag == 0 && co_await b_isatty(STDOUT_FILENO)) {
        maybe_warnx("standard output is a terminal -- ignoring");
        co_return;
    }

    /* If stdin is a file use its mtime, otherwise use current time */
    ret = co_await b_fstat(STDIN_FILENO, &sb);
    if (ret < 0) {
        maybe_warn("Can't stat stdin");
        co_return;
    }

    if (S_ISREG(sb.st_mode)) {
        infile_set("(stdout)", sb.st_size);
        mtime = (uint32_t)sb.st_mtime;
    } else {
        systime = ztime(NULL);
        if (systime == -1) {
            maybe_warn("time");
            co_return;
        }
        mtime = (uint32_t)systime;
    }

    usize = co_await gz_compress(STDIN_FILENO, STDOUT_FILENO, &gsize, "", mtime);
    if (vflag && !tflag && usize != -1 && gsize != -1)
        co_await print_verbage(NULL, NULL, usize, gsize);
}

/* do what is asked for, for the path name */
Task<void> handle_pathname(char *path)
{
    char *opath = path, *s = NULL;
    ssize_t len;
    int slen;
    struct stat sb;

    /* check for stdout/stdin */
    if (path[0] == '-' && path[1] == '\0') {
        if (dflag)
            co_await handle_stdin();
        else
            co_await handle_stdout();
        co_return;
    }

retry:
    if (co_await b_stat(path, &sb) != 0 ||
        (fflag == 0 && cflag == 0 && co_await b_lstat(path, &sb) != 0)) {
        /* lets try <path>.gz if we're decompressing */
        if (dflag && s == NULL && errno == ENOENT) {
            len  = strlen(path);
            slen = suffixes[0].ziplen;
            s    = (char *)malloc(len + slen + 1);
            if (s == NULL)
                maybe_err("malloc");
            memcpy(s, path, len);
            memcpy(s + len, suffixes[0].zipped, slen + 1);
            path = s;
            goto retry;
        }
        maybe_warn("can't stat: %s", opath);
        goto out;
    }

    if (S_ISDIR(sb.st_mode)) {
        if (rflag)
            co_await handle_dir(path);
        else
            maybe_warnx("%s is a directory", path);
        goto out;
    }

    if (S_ISREG(sb.st_mode))
        co_await handle_file(path, &sb);
    else
        maybe_warnx("%s is not a regular file", path);

out:
    if (s)
        free(s);
}

/* compress/decompress a file */
static Task<void> handle_file(char *file, struct stat *sbp)
{
    off_t usize, gsize;
    char outfile[PATH_MAX];

    infile_set(file, sbp->st_size);
    if (dflag) {
        usize = co_await file_uncompress(file, outfile, sizeof(outfile));
        if (vflag && tflag)
            co_await print_test(file, usize != -1);
        if (usize == -1)
            co_return;
        gsize = sbp->st_size;
    } else {
        gsize = co_await file_compress(file, outfile, sizeof(outfile));
        if (gsize == -1)
            co_return;
        usize = sbp->st_size;
    }
    infile_clear();

    if (vflag && !tflag)
        co_await print_verbage(file, (cflag) ? NULL : outfile, usize, gsize);
}

/* this is used with -r to recursively descend directories */
static Task<void> handle_dir(char *dir)
{
    DIR *d = co_await b_opendir(dir);
    if (!d) {
        maybe_warn("couldn't opendir %s", dir);
        co_return;
    }
    for (;;) {
        struct dirent *de = b_readdir(d);
        if (!de)
            break;
        if (de->d_name[0] == '.')
            continue;
        char path[PATH_MAX];
        if ((size_t)snprintf(path, sizeof path, "%s/%s", dir, de->d_name) >= sizeof path)
            continue;
        struct stat sb;
        if (co_await b_lstat(path, &sb) != 0)
            continue;
        if (S_ISDIR(sb.st_mode)) {
            if (rflag)
                co_await handle_dir(path);
        } else if (S_ISREG(sb.st_mode))
            co_await handle_file(path, &sb);
    }
    b_closedir(d);
}

/* print a ratio - size reduction as a fraction of uncompressed size */
static Task<void> print_ratio(off_t in, off_t out, FILE *where)
{
    int percent10; /* 10 * percent */
    off_t diff;
    char buff[8];
    int len;

    diff = in - out / 2;
    if (in == 0 && out == 0)
        percent10 = 0;
    else if (diff < 0)
        /*
         * Output is more than double size of input! print -99.9%
         * Quite possibly we've failed to get the original size.
         */
        percent10 = -999;
    else {
        /*
         * We only need 12 bits of result from the final division,
         * so reduce the values until a 32bit division will suffice.
         */
        while (in > 0x100000) {
            diff >>= 1;
            in >>= 1;
        }
        if (in != 0)
            percent10 = ((u_int)diff * 2000) / (u_int)in - 1000;
        else
            percent10 = 0;
    }

    len = snprintf(buff, sizeof buff, "%2.2d.", percent10);
    /* Move the '.' to before the last digit */
    buff[len - 1] = buff[len - 2];
    buff[len - 2] = '.';
    co_await b_fprintf(where, "%5s%%", buff);
}

/* print compression statistics, and the new name (if there is one!) */
static Task<void> print_verbage(const char *file, const char *nfile, off_t usize, off_t gsize)
{
    if (file)
        co_await b_fprintf(stderr, "%s:%s  ", file, strlen(file) < 7 ? "\t\t" : "\t");
    co_await print_ratio(usize, gsize, stderr);
    if (nfile)
        co_await b_fprintf(stderr, " -- replaced with %s", nfile);
    co_await b_fprintf(stderr, "\n");
    co_await b_fflush(stderr);
}

/* print test results */
static Task<void> print_test(const char *file, int ok)
{
    if (exit_value == 0 && ok == 0)
        exit_value = 1;
    co_await b_fprintf(stderr, "%s:%s  %s\n", file, strlen(file) < 7 ? "\t\t" : "\t",
                       ok ? "OK" : "NOT OK");
    co_await b_fflush(stderr);
}

/* print a file's info ala --list */
/* eg:
  compressed uncompressed  ratio uncompressed_name
      354841      1679360  78.8% /usr/pkgsrc/distfiles/libglade-2.0.1.tar
*/
Task<void> print_list(int fd, off_t out, const char *outfile, time_t ts)
{
    static int first = 1;
    static off_t in_tot, out_tot;
    uint32_t crc = 0;
    off_t in     = 0, rv;

    if (first) {
        if (vflag)
            co_await b_printf("method  crc     date  time  ");
        if (qflag == 0)
            co_await b_printf(
                "  compressed uncompressed  "
                "ratio uncompressed_name\n");
    }
    first = 0;

    /* print totals? */
    if (fd == -1) {
        in  = in_tot;
        out = out_tot;
    } else {
        /* read the last 4 bytes - this is the uncompressed size */
        rv = co_await b_lseek(fd, (off_t)(-8), SEEK_END);
        if (rv != -1) {
            unsigned char buf[8];
            uint32_t usize;

            rv = co_await read_retry(fd, (char *)buf, sizeof(buf));
            if (rv == -1)
                maybe_warn("read of uncompressed size");
            else if (rv != sizeof(buf))
                maybe_warnx("read of uncompressed size");

            else {
                usize = le32dec(&buf[4]);
                in    = (off_t)usize;
                crc   = le32dec(&buf[0]);
            }
        }
    }

    if (vflag && fd == -1)
        co_await b_printf("                            ");
    else if (vflag) {
        char datebuf[32];
        struct tm tm;
        gmtime_r(&ts, &tm);
        strftime(datebuf, sizeof datebuf, "%b %e %H:%M", &tm);
        char *date = datebuf;

        /* skip the day, 1/100th second, and year */
        date += 4;
        date[12] = 0;
        co_await b_printf("%5s %08x %11s ", "defla" /*XXX*/, crc, date);
    }
    in_tot += in;
    out_tot += out;
    co_await print_list_out(out, in, outfile);
}

Task<void> print_list_out(off_t out, off_t in, const char *outfile)
{
    co_await b_printf("%12llu %12llu ", (unsigned long long)out, (unsigned long long)in);
    co_await print_ratio(in, out, stdout);
    co_await b_printf(" %s\n", outfile);
}

/* display the usage of NetBSD gzip */
Task<void> usage(void)
{
    co_await b_fprintf(stderr, "%s\n", gzip_version);
    co_await b_fprintf(stderr,
                       "usage: %s [-123456789acdfhklLNnqrtVv] [-S .suffix] [<file> [<file> ...]]\n"
                       " -1 --fast            fastest (worst) compression\n"
                       " -2 .. -8             set compression level\n"
                       " -9 --best            best (slowest) compression\n"
                       " -c --stdout          write to stdout, keep original files\n"
                       "    --to-stdout\n"
                       " -d --decompress      uncompress files\n"
                       "    --uncompress\n"
                       " -f --force           force overwriting & compress links\n"
                       " -h --help            display this help\n"
                       " -k --keep            don't delete input files during operation\n"
                       " -l --list            list compressed file contents\n"
                       " -N --name            save or restore original file name and time stamp\n"
                       " -n --no-name         don't save original file name or time stamp\n"
                       " -q --quiet           output no warnings\n"
                       " -r --recursive       recursively compress files in directories\n"
                       " -S .suf              use suffix .suf instead of .gz\n"
                       "    --suffix .suf\n"
                       " -t --test            test compressed file\n"
                       " -V --version         display program version\n"
                       " -v --verbose         print extra statistics\n",
                       gzip_progname(Str("")));
    co_return;
}

/* display the license information of FreeBSD gzip */
Task<void> display_license(void)
{
    co_await b_fprintf(stderr, "%s (based on NetBSD gzip 20150113)\n", gzip_version);
    co_await b_fprintf(stderr, "%s\n", gzip_copyright);
    co_return;
}

/* display the version of NetBSD gzip */
Task<void> display_version(void)
{
    co_await b_fprintf(stderr, "%s\n", gzip_version);
    co_return;
}

/*	$NetBSD: unbzip2.c,v 1.14 2017/08/04 07:27:08 mrg Exp $	*/

/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Simon Burge.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* This file is #included by gzip.c */

Task<off_t> unbzip2(int in, int out, char *pre, size_t prelen, off_t *bytes_in)
{
    int ret, end_of_file, cold = 0;
    off_t bytes_out = 0;
    bz_stream bzs;
    static char *inbuf, *outbuf;

    if (inbuf == NULL)
        inbuf = (char *)malloc(BUFLEN);
    if (outbuf == NULL)
        outbuf = (char *)malloc(BUFLEN);
    if (inbuf == NULL || outbuf == NULL)
        maybe_err("malloc");

    bzs.bzalloc = NULL;
    bzs.bzfree  = NULL;
    bzs.opaque  = NULL;

    end_of_file = 0;
    ret         = BZ2_bzDecompressInit(&bzs, 0, 0);
    if (ret != BZ_OK)
        maybe_errx("bzip2 init");

    /* Prepend. */
    bzs.avail_in = prelen;
    bzs.next_in  = pre;

    if (bytes_in)
        *bytes_in = prelen;

    while (ret == BZ_OK) {
        check_siginfo();
        if (bzs.avail_in == 0 && !end_of_file) {
            ssize_t n;

            n = co_await read_retry(in, inbuf, BUFLEN);
            if (n < 0)
                maybe_err("read");
            if (n == 0)
                end_of_file = 1;
            infile_newdata(n);
            bzs.next_in  = inbuf;
            bzs.avail_in = n;
            if (bytes_in)
                *bytes_in += n;
        }

        bzs.next_out  = outbuf;
        bzs.avail_out = BUFLEN;
        ret           = BZ2_bzDecompress(&bzs);

        switch (ret) {
        case BZ_STREAM_END:
        case BZ_OK:
            if (ret == BZ_OK && end_of_file) {
                /*
                 * If we hit this after a stream end, consider
                 * it as the end of the whole file and don't
                 * bail out.
                 */
                if (cold == 1)
                    ret = BZ_STREAM_END;
                else
                    maybe_errx("truncated file");
            }
            cold = 0;
            if (!tflag && bzs.avail_out != BUFLEN) {
                ssize_t n;

                n = co_await write_retry(out, outbuf, BUFLEN - bzs.avail_out);
                if (n < 0)
                    maybe_err("write");
                bytes_out += n;
            }
            if (ret == BZ_STREAM_END && !end_of_file) {
                if (BZ2_bzDecompressEnd(&bzs) != BZ_OK || BZ2_bzDecompressInit(&bzs, 0, 0) != BZ_OK)
                    maybe_errx("bzip2 re-init");
                cold = 1;
                ret  = BZ_OK;
            }
            break;

        case BZ_DATA_ERROR:
            maybe_warnx("bzip2 data integrity error");
            break;

        case BZ_DATA_ERROR_MAGIC:
            maybe_warnx("bzip2 magic number error");
            break;

        case BZ_MEM_ERROR:
            maybe_warnx("bzip2 out of memory");
            break;

        default:
            maybe_warnx("unknown bzip2 error: %d", ret);
            break;
        }
    }

    if (ret != BZ_STREAM_END || BZ2_bzDecompressEnd(&bzs) != BZ_OK)
        co_return (-1);

    co_return (bytes_out);
}

/*	$NetBSD: unxz.c,v 1.8 2018/10/06 16:36:45 martin Exp $	*/

/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

Task<off_t> unxz(int i, int o, char *pre, size_t prelen, off_t *bytes_in)
{
    lzma_stream strm       = LZMA_STREAM_INIT;
    static const int flags = LZMA_TELL_UNSUPPORTED_CHECK | LZMA_CONCATENATED;
    lzma_ret ret;
    lzma_action action = LZMA_RUN;
    off_t bytes_out, bp;
    uint8_t ibuf[BUFSIZ];
    uint8_t obuf[BUFSIZ];

    if (bytes_in == NULL)
        bytes_in = &bp;

    strm.next_in = ibuf;
    memcpy(ibuf, pre, prelen);
    strm.avail_in = co_await read_retry(i, ibuf + prelen, sizeof(ibuf) - prelen);
    if (strm.avail_in == (size_t)-1)
        maybe_err("read failed");
    infile_newdata(strm.avail_in);
    strm.avail_in += prelen;
    *bytes_in = strm.avail_in;

    if ((ret = lzma_stream_decoder(&strm, UINT64_MAX, flags)) != LZMA_OK)
        maybe_errx("Can't initialize decoder (%d)", ret);

    strm.next_out  = NULL;
    strm.avail_out = 0;
    if ((ret = lzma_code(&strm, LZMA_RUN)) != LZMA_OK)
        maybe_errx("Can't read headers (%d)", ret);

    bytes_out      = 0;
    strm.next_out  = obuf;
    strm.avail_out = sizeof(obuf);

    for (;;) {
        check_siginfo();
        if (strm.avail_in == 0) {
            strm.next_in  = ibuf;
            strm.avail_in = co_await read_retry(i, ibuf, sizeof(ibuf));
            switch (strm.avail_in) {
            case (size_t)-1:
                maybe_err("read failed");
                /*NOTREACHED*/
            case 0:
                action = LZMA_FINISH;
                break;
            default:
                infile_newdata(strm.avail_in);
                *bytes_in += strm.avail_in;
                break;
            }
        }

        ret = lzma_code(&strm, action);

        // Write and check write error before checking decoder error.
        // This way as much data as possible gets written to output
        // even if decoder detected an error.
        if (strm.avail_out == 0 || ret != LZMA_OK) {
            const size_t write_size = sizeof(obuf) - strm.avail_out;

            if (co_await write_retry(o, obuf, write_size) != (ssize_t)write_size)
                maybe_err("write failed");

            strm.next_out  = obuf;
            strm.avail_out = sizeof(obuf);
            bytes_out += write_size;
        }

        if (ret != LZMA_OK) {
            if (ret == LZMA_STREAM_END) {
                // Check that there's no trailing garbage.
                if (strm.avail_in != 0 || co_await read_retry(i, ibuf, 1))
                    ret = LZMA_DATA_ERROR;
                else {
                    lzma_end(&strm);
                    co_return bytes_out;
                }
            }

            const char *msg = "Unknown error";
            switch (ret) {
            case LZMA_MEM_ERROR:
                msg = strerror(ENOMEM);
                break;

            case LZMA_FORMAT_ERROR:
                msg = "File format not recognized";
                break;

            case LZMA_OPTIONS_ERROR:
                // FIXME: Better message?
                msg = "Unsupported compression options";
                break;

            case LZMA_DATA_ERROR:
                msg = "File is corrupt";
                break;

            case LZMA_BUF_ERROR:
                msg = "Unexpected end of input";
                break;

            case LZMA_MEMLIMIT_ERROR:
                msg = "Reached memory limit";
                break;

            default:
                maybe_errx("Unknown error (%d)", ret);
                break;
            }
            maybe_errx("%s", msg);
        }
    }
}

/*
 * Copied various bits and pieces from xz support code or brute force
 * replacements.
 */

#define my_min(A, B) ((A) < (B) ? (A) : (B))

// Some systems have suboptimal BUFSIZ. Use a bit bigger value on them.
// We also need that IO_BUFFER_SIZE is a multiple of 8 (sizeof(uint64_t))
#if BUFSIZ <= 1024
#define IO_BUFFER_SIZE 8192
#else
#define IO_BUFFER_SIZE (BUFSIZ & ~7U)
#endif

/// is_sparse() accesses the buffer as uint64_t for maximum speed.
/// Use an union to make sure that the buffer is properly aligned.
typedef union {
    uint8_t u8[IO_BUFFER_SIZE];
    uint32_t u32[IO_BUFFER_SIZE / sizeof(uint32_t)];
    uint64_t u64[IO_BUFFER_SIZE / sizeof(uint64_t)];
} io_buf;

static Task<bool> io_pread(int fd, io_buf *buf, size_t size, off_t pos)
{
    // Using co_await b_lseek() and co_await read_retry() is more portable than co_await
    // gzip_pread() and for us it is as good as real co_await gzip_pread().
    if (co_await b_lseek(fd, pos, SEEK_SET) != pos) {
        co_return true;
    }

    const size_t amount = co_await read_retry(fd, buf, size);
    if (amount == SIZE_MAX)
        co_return true;

    if (amount != size) {
        co_return true;
    }

    co_return false;
}

/*
 * Most of the following is copied (mostly verbatim) from the xz
 * distribution, from file src/xz/list.c
 */

///////////////////////////////////////////////////////////////////////////////
//
/// \file       list.c
/// \brief      Listing information about .xz files
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

/// Information about a .xz file
typedef struct {
    /// Combined Index of all Streams in the file
    lzma_index *idx;

    /// Total amount of Stream Padding
    uint64_t stream_padding;

    /// Highest memory usage so far
    uint64_t memusage_max;

    /// True if all Blocks so far have Compressed Size and
    /// Uncompressed Size fields
    bool all_have_sizes;

    /// Oldest XZ Utils version that will decompress the file
    uint32_t min_version;

} xz_file_info;

#define XZ_FILE_INFO_INIT { NULL, 0, 0, true, 50000002 }

/// \brief      Parse the Index(es) from the given .xz file
///
/// \param      xfi     Pointer to structure where the decoded information
///                     is stored.
/// \param      pair    Input file
///
/// \return     On success, false is returned. On error, true is returned.
///
// TODO: This function is pretty big. liblzma should have a function that
// takes a callback function to parse the Index(es) from a .xz file to make
// it easy for applications.
static Task<bool> parse_indexes(xz_file_info *xfi, int src_fd)
{
    struct stat st;

    if (co_await b_fstat(src_fd, &st) != 0) {
        co_return true;
    }

    if (st.st_size < 2 * LZMA_STREAM_HEADER_SIZE) {
        co_return true;
    }

    io_buf buf;
    lzma_stream_flags header_flags;
    lzma_stream_flags footer_flags;
    lzma_ret ret;

    // lzma_stream for the Index decoder
    lzma_stream strm = LZMA_STREAM_INIT;

    // All Indexes decoded so far
    lzma_index *combined_index = NULL;

    // The Index currently being decoded
    lzma_index *this_index = NULL;

    // Current position in the file. We parse the file backwards so
    // initialize it to point to the end of the file.
    off_t pos = st.st_size;

    // Each loop iteration decodes one Index.
    do {
        // Check that there is enough data left to contain at least
        // the Stream Header and Stream Footer. This check cannot
        // fail in the first pass of this loop.
        if (pos < 2 * LZMA_STREAM_HEADER_SIZE) {
            goto error;
        }

        pos -= LZMA_STREAM_HEADER_SIZE;
        lzma_vli stream_padding = 0;

        // Locate the Stream Footer. There may be Stream Padding which
        // we must skip when reading backwards.
        while (true) {
            if (pos < LZMA_STREAM_HEADER_SIZE) {
                goto error;
            }

            if (co_await io_pread(src_fd, &buf, LZMA_STREAM_HEADER_SIZE, pos))
                goto error;

            // Stream Padding is always a multiple of four bytes.
            int i = 2;
            if (buf.u32[i] != 0)
                break;

            // To avoid calling co_await io_pread() for every four bytes
            // of Stream Padding, take advantage that we read
            // 12 bytes (LZMA_STREAM_HEADER_SIZE) already and
            // check them too before calling co_await io_pread() again.
            do {
                stream_padding += 4;
                pos -= 4;
                --i;
            } while (i >= 0 && buf.u32[i] == 0);
        }

        // Decode the Stream Footer.
        ret = lzma_stream_footer_decode(&footer_flags, buf.u8);
        if (ret != LZMA_OK) {
            goto error;
        }

        // Check that the Stream Footer doesn't specify something
        // that we don't support. This can only happen if the xz
        // version is older than liblzma and liblzma supports
        // something new.
        //
        // It is enough to check Stream Footer. Stream Header must
        // match when it is compared against Stream Footer with
        // lzma_stream_flags_compare().
        if (footer_flags.version != 0) {
            goto error;
        }

        // Check that the size of the Index field looks sane.
        lzma_vli index_size = footer_flags.backward_size;
        if ((lzma_vli)(pos) < index_size + LZMA_STREAM_HEADER_SIZE) {
            goto error;
        }

        // Set pos to the beginning of the Index.
        pos -= index_size;

        // Decode the Index.
        ret = lzma_index_decoder(&strm, &this_index, UINT64_MAX);
        if (ret != LZMA_OK) {
            goto error;
        }

        do {
            // Don't give the decoder more input than the
            // Index size.
            strm.avail_in = my_min(IO_BUFFER_SIZE, index_size);
            if (co_await io_pread(src_fd, &buf, strm.avail_in, pos))
                goto error;

            pos += strm.avail_in;
            index_size -= strm.avail_in;

            strm.next_in = buf.u8;
            ret          = lzma_code(&strm, LZMA_RUN);

        } while (ret == LZMA_OK);

        // If the decoding seems to be successful, check also that
        // the Index decoder consumed as much input as indicated
        // by the Backward Size field.
        if (ret == LZMA_STREAM_END)
            if (index_size != 0 || strm.avail_in != 0)
                ret = LZMA_DATA_ERROR;

        if (ret != LZMA_STREAM_END) {
            // LZMA_BUFFER_ERROR means that the Index decoder
            // would have liked more input than what the Index
            // size should be according to Stream Footer.
            // The message for LZMA_DATA_ERROR makes more
            // sense in that case.
            if (ret == LZMA_BUF_ERROR)
                ret = LZMA_DATA_ERROR;

            goto error;
        }

        // Decode the Stream Header and check that its Stream Flags
        // match the Stream Footer.
        pos -= footer_flags.backward_size + LZMA_STREAM_HEADER_SIZE;
        if ((lzma_vli)(pos) < lzma_index_total_size(this_index)) {
            goto error;
        }

        pos -= lzma_index_total_size(this_index);
        if (co_await io_pread(src_fd, &buf, LZMA_STREAM_HEADER_SIZE, pos))
            goto error;

        ret = lzma_stream_header_decode(&header_flags, buf.u8);
        if (ret != LZMA_OK) {
            goto error;
        }

        ret = lzma_stream_flags_compare(&header_flags, &footer_flags);
        if (ret != LZMA_OK) {
            goto error;
        }

        // Store the decoded Stream Flags into this_index. This is
        // needed so that we can print which Check is used in each
        // Stream.
        ret = lzma_index_stream_flags(this_index, &footer_flags);
        if (ret != LZMA_OK)
            goto error;

        // Store also the size of the Stream Padding field. It is
        // needed to show the offsets of the Streams correctly.
        ret = lzma_index_stream_padding(this_index, stream_padding);
        if (ret != LZMA_OK)
            goto error;

        if (combined_index != NULL) {
            // Append the earlier decoded Indexes
            // after this_index.
            ret = lzma_index_cat(this_index, combined_index, NULL);
            if (ret != LZMA_OK) {
                goto error;
            }
        }

        combined_index = this_index;
        this_index     = NULL;

        xfi->stream_padding += stream_padding;

    } while (pos > 0);

    lzma_end(&strm);

    // All OK. Make combined_index available to the caller.
    xfi->idx = combined_index;
    co_return false;

error:
    // Something went wrong, free the allocated memory.
    lzma_end(&strm);
    lzma_index_end(combined_index, NULL);
    lzma_index_end(this_index, NULL);
    co_return true;
}

/***************** end of copy form list.c *************************/

/*
 * Small wrapper to extract total length of a file
 */
Task<off_t> unxz_len(int fd)
{
    xz_file_info xfi = XZ_FILE_INFO_INIT;
    if (!co_await parse_indexes(&xfi, fd)) {
        off_t res = lzma_index_uncompressed_size(xfi.idx);
        lzma_index_end(xfi.idx, NULL);
        co_return res;
    }
    co_return 0;
}

/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Klara, Inc.
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

/* This file is #included by gzip.c */

Task<off_t> unzstd(int in, int out, char *pre, size_t prelen, off_t *bytes_in)
{
    static char *ibuf, *obuf;
    ZSTD_inBuffer zib;
    ZSTD_outBuffer zob;
    ZSTD_DCtx *zds;
    ssize_t res;
    size_t zres;
    size_t bytes_out = 0;
    int eof          = 0;

    if (ibuf == NULL)
        ibuf = (char *)malloc(BUFLEN);
    if (obuf == NULL)
        obuf = (char *)malloc(BUFLEN);
    if (ibuf == NULL || obuf == NULL)
        maybe_err("malloc");

    zds = ZSTD_createDStream();
    ZSTD_initDStream(zds);

    zib.src  = pre;
    zib.size = prelen;
    zib.pos  = 0;
    if (bytes_in != NULL)
        *bytes_in = prelen;
    zob.dst  = obuf;
    zob.size = BUFLEN;
    zob.pos  = 0;

    while (!eof) {
        if (zib.pos >= zib.size) {
            res = co_await read_retry(in, ibuf, BUFLEN);
            if (res < 0)
                maybe_err("read");
            if (res == 0)
                eof = 1;
            infile_newdata(res);
            zib.src  = ibuf;
            zib.size = res;
            zib.pos  = 0;
            if (bytes_in != NULL)
                *bytes_in += res;
        }
        zres = ZSTD_decompressStream(zds, &zob, &zib);
        if (ZSTD_isError(zres)) {
            maybe_errx("%s", ZSTD_getErrorName(zres));
        }
        if (zob.pos > 0) {
            res = co_await write_retry(out, obuf, zob.pos);
            if (res < 0)
                maybe_err("write");
            zob.pos = 0;
            bytes_out += res;
        }
    }
    ZSTD_freeDStream(zds);
    co_return (bytes_out);
}
