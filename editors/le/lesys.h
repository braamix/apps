// What LE needs beside the port kit's own system headers.
//
// The POSIX types, struct stat, the S_IF* set, the O_* flags and the errno
// names were all written out here once; the kit answers every one of them now,
// so this is what is left. errno is the kit's, and error_of()/errno_of() in
// compat/cerr.h are the bridge wherever a kernel Error has to become one.
#pragma once

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __cplusplus
#include "compat/cerr.h"
#include "kernel/result.h"
#include "kernel/text.h" // the Str scanners, in place of sscanf
#include "kernel/types.h"
#else
/* regex.c is C and wants only the widths. */
typedef unsigned long usize;
typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long i64;
#endif

/* What a path buffer holds. alloca is gone -- a variable-length array in a
   coroutine would live in its frame, and a frame past 512 bytes costs a
   whole 64 KiB span -- so the buffers it sized are fixed and, where the
   holder is a coroutine, at file scope. The kit's PATH_MAX is the same 512. */
enum { LE_PATHMAX = PATH_MAX };
