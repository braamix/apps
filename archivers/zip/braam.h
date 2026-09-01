// The porting layer: what the port kit has not got.
//
// The C library is the kit's -- braam_add_program(... PORT) -- and Group B now
// answers the streams too, so <stdio.h> and compat/cio.h's b_* family are
// upstream's stdio and the z* layer that stood in for it is gone. What is left
// is zip's own: the two dates sscanf would have read, the DOS stamp, a clock
// that does not block, the argv copy, and the read a ^C has to reach.
// Replaces unix/unix.c and tailor.h's stream macros.
#pragma once

#include "kernel/types.h"

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "compat/cio.h"
#include "proc/io.h"

// -------------------------------------------------------- what the kit leaves

extern "C" {

// The only two sscanf formats upstream uses, both for -t and -tt: an ISO
// yyyy-mm-dd or an American mmddyyyy. The kit has no sscanf, and a general one
// would be the rest of stdio for two call sites.
int zparse_date(const char *s, int *yyyy, int *mm, int *dd);

} // extern "C"

// ---------------------------------------------------- what unix/unix.c did

constexpr u32 DOSTIME_MINIMUM_ = 0x00210000u; // 1980-01-01 00:00:00

// Filesystem mtime (ms since the epoch) to a packed DOS stamp. 0 — every
// directory, since OPFS keeps no timestamp on one — is the DOS epoch.
u32 zdostime(u64 mtime_ms);

// The inverse, in seconds.
i64 zdos2unix(u32 dostime);

extern i32 ztz_min;     // minutes east of UTC, read once at startup
extern i64 zstart_time; // seconds since the epoch, likewise

Task<void> zclock_init();

// time(). The wall clock read at startup plus the monotonic milliseconds
// since; under the harness proc_now() never advances, so it stands still. The
// kit's own time() is unavailable and names clock_now(), which blocks; this is
// the cheap answer the two call sites want.
time_t ztime(time_t *t);

// argv as a NUL-terminated char ** the option parser can rewrite. Str is not
// NUL-terminated, so this is a copy either way; args[0] is the program name,
// which get_option skips.
int zargv(Args argv, char ***out);

// ------------------------------------------- what unix/zipup.h did: raw reads
//
// The entry being compressed wants no buffer of its own: deflate reads it a
// window at a time and the store path a SBSZ at a time, so a descriptor is
// enough and a FILE would only copy. b_open and b_close are the kit's; the
// read is here because a ^C has to unwind it as ZE_ABORT.

typedef i32 ftype;

constexpr ftype fbad   = -1;
constexpr ftype zstdin = 0;
#define fhow O_RDONLY

Task<unsigned> zread(ftype f, char *buf, unsigned len);

// Upstream read this as "the last zread returned -1". A short read is the end
// of input here and never an error, so the flag is the port's own.
extern int zread_failed;
#define zerr(f) (zread_failed)
