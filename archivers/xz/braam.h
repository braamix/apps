// Braam port layer for xz 5.8.4.
#pragma once

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <lzma.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "compat/cio.h"
#include "fs/path.h"
#include "kernel/types.h"
#include "proc/io.h"
#include "proc/rt.h"

#define HAVE_ENCODERS     1
#define HAVE_DECODERS     1
#define HAVE_LZIP_DECODER 1
#define PACKAGE           "xz"

// No liblzma multithreading on Braam.
#undef MYTHREAD_ENABLED

#define _(msg)  (msg)
#define N_(msg) (msg)

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define my_min(a, b)  ((a) < (b) ? (a) : (b))
#define my_max(a, b)  ((a) > (b) ? (a) : (b))
#define FALLTHROUGH   ((void)0)
#define ctz32(x)      __builtin_ctz((unsigned)(x))

#ifndef PRIu64
#define PRIu64 "llu"
#endif
#ifndef PRIu32
#define PRIu32 "u"
#endif
#ifndef PRIx64
#define PRIx64 "llx"
#endif
#ifndef PRIx32
#define PRIx32 "x"
#endif

#define ngettext(s, p, n) ((n) == 1 ? (s) : (p))
#define conv32le(n)       ((uint32_t)(n))
#define conv64le(n)       ((uint64_t)(n))

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

// 100 MiB process cap; headroom for xz itself and non-lzma heap use.
static constexpr uint64_t XZ_PROC_RAM          = UINT64_C(100) << 20;
static constexpr uint64_t XZ_COMPRESS_HEADROOM = UINT64_C(20) << 20;

extern volatile int user_abort;
extern const char *xz_remove_out;

extern int xz_fatal;
extern int xz_stop;

Task<i32> xz_main(Args args);

Task<void> xz_flush_diag();
Task<void> xz_on_int();

void xz_maybe_err(const char *fmt, ...);
void xz_maybe_errx(const char *fmt, ...);

void xz_diag_vprint(const char *fmt, va_list ap);
void xz_diag_print(const char *fmt, ...);

void xz_out_print(const char *fmt, ...);
Task<void> xz_flush_out();

Task<ssize_t> xz_read_retry(int fd, void *buf, size_t sz);
Task<ssize_t> xz_write_retry(int fd, const void *buf, size_t sz);
Task<off_t> xz_lseek_retry(int fd, off_t off, int whence);
