// Braam port layer for FreeBSD gzip.
#pragma once

#define NO_PACK_SUPPORT

#include "kernel/types.h"
#include "fs/path.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/endian.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef unsigned char u_char;
typedef unsigned int u_int;

static inline uint32_t
le32dec(const void *pp)
{
    const unsigned char *p = (const unsigned char *)pp;
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
#include <time.h>
#include <unistd.h>
#include <zlib.h>

#include "compat/cio.h"
#include "proc/io.h"
#include "proc/rt.h"

#ifndef nitems
#define nitems(x) (sizeof(x) / sizeof((x)[0]))
#endif

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

typedef struct {
    const char *zipped;
    int ziplen;
    const char *normal;
} suffixes_t;

extern suffixes_t suffixes[];
extern const int gzip_num_suffixes;

extern int cflag, dflag, lflag, numflag, fflag, kflag, nflag, Nflag, qflag, rflag, tflag,
    vflag;
extern int exit_value;
extern const char *remove_file;
extern const char *infile;
extern off_t infile_total, infile_current;
extern int gzip_fatal;

const char *gzip_progname(Str argv0);
void gzip_clear_fatal();
void gzip_set_time(time_t t);
time_t ztime(time_t *t);
Task<void> gzip_on_int();

Task<ssize_t> read_retry(int fd, void *buf, size_t sz);
Task<ssize_t> write_retry(int fd, const void *buf, size_t sz);
Task<ssize_t> gzip_pread(int fd, void *buf, size_t sz, off_t pos);

void maybe_warn(const char *fmt, ...);
void maybe_warnx(const char *fmt, ...);
void maybe_err(const char *fmt, ...);
void maybe_errx(const char *fmt, ...);
Task<void> gzip_flush_diag();

void infile_newdata(size_t newdata);
void infile_set(const char *newinfile, off_t total);
void infile_clear(void);

#define check_siginfo() ((void)0)

Task<i32> gzip_main(Args args);

Task<void> handle_stdin();
Task<void> handle_stdout();
Task<void> handle_pathname(char *path);
Task<void> usage();
Task<void> display_version();
Task<void> display_license();
Task<void> print_list(int fd, off_t out, const char *outfile, time_t ts);

#ifndef NO_BZIP2_SUPPORT
Task<off_t> unbzip2(int in, int out, char *pre, size_t prelen, off_t *bytes_in);
#endif
#ifndef NO_COMPRESS_SUPPORT
Task<off_t> zuncompress_fd(int in, int out, char *pre, size_t prelen, off_t *compressed_bytes);
#endif
#ifndef NO_PACK_SUPPORT
Task<off_t> unpack(int in, int out, char *pre, size_t prelen, off_t *bytes_in);
#endif
#ifndef NO_XZ_SUPPORT
Task<off_t> unxz(int in, int out, char *pre, size_t prelen, off_t *bytes_in);
Task<off_t> unxz_len(int fd);
#endif
#ifndef NO_LZ_SUPPORT
Task<off_t> unlz(int in, int out, char *pre, size_t prelen, off_t *bytes_in);
#endif
#ifndef NO_ZSTD_SUPPORT
Task<off_t> unzstd(int in, int out, char *pre, size_t prelen, off_t *bytes_in);
#endif
