// Braam port layer for zstd 1.6.0.
#pragma once

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define ZSTD_STATIC_LINKING_ONLY /* ZSTD_compressionParameters, ZSTD_getCParams */
#include <zstd.h>

#include "compat/cio.h"
#include "kernel/types.h"
#include "proc/io.h"
#include "proc/rt.h"

// lib/common/mem.h, which braam::zstd does not export: the CLI wants these five
// names and two readers, and nothing else out of it.
typedef uint8_t BYTE;
typedef uint8_t U8;
typedef uint16_t U16;
typedef int32_t S32;
typedef uint32_t U32;
typedef uint64_t U64;

static inline U32 MEM_readLE32(const void *p)
{
    const BYTE *b = (const BYTE *)p;
    return (U32)b[0] | ((U32)b[1] << 8) | ((U32)b[2] << 16) | ((U32)b[3] << 24);
}

static inline U32 MEM_readLE24(const void *p)
{
    const BYTE *b = (const BYTE *)p;
    return (U32)b[0] | ((U32)b[1] << 8) | ((U32)b[2] << 16);
}

// What this build is. libzstd comes from braam::zstd; zlib and liblzma are
// here too, so upstream's four optional formats are three: gzip, xz and lzma
// are read and written, lz4 is neither. Single-threaded, no dictionary builder
// (ZDICT_* is not vendored), no benchmark, no trace log.
#define ZSTD_GZCOMPRESS     1
#define ZSTD_GZDECOMPRESS   1
#define ZSTD_LZMACOMPRESS   1
#define ZSTD_LZMADECOMPRESS 1
#define ZSTD_NOBENCH        1
#define ZSTD_NODICT         1
#define ZSTD_NOTRACE        1

#ifndef assert
#define assert(x) ((void)0)
#endif

// Upstream's INThandler removed the artefact and exit()ed 2. Here ^C is
// collected where the process parks: the read and write primitives fail with
// EINTR, and the artefact is unlinked on the way out.
extern const char *g_artefact;
bool zstd_interrupted();

// Upstream's EXM_THROW ended the process. There is no exit() and no longjmp
// here, so it records and the caller returns: the status becomes the process's,
// and zstd_fatal() then short-circuits every later read and write, so a loop
// that does not check ends rather than running on over a broken stream.
void zstd_fail(int code);
bool zstd_fatal();
int zstd_status();

// Two output buffers, because DISPLAY and DISPLAYOUT are called from plain
// functions that cannot await a write. Both are flushed where the process next
// parks.
void zstd_display(FILE *f, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
Task<void> zstd_flush(void);

// b_fread / b_fwrite, retried, with a ^C check and the diagnostics flushed
// first. Short only at end of input; -1 with errno on failure.
Task<size_t> zstd_fread(void *p, size_t size, size_t n, FILE *f);
Task<size_t> zstd_fwrite(const void *p, size_t size, size_t n, FILE *f);

Task<i32> zstd_main(Args args);
