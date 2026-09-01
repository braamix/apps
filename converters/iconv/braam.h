// The porting layer, in one header: the BSD spellings upstream's #includes
// name that the port kit does not answer, and the primitives that replace
// dlopen and mmap. Force-included ahead of the first line of every source,
// because upstream's headers use __BEGIN_DECLS, the BSD integer names and
// stdio's SEEK_* without including anything for them.
//
// Everything a port supplies for itself must be extern "C": the compiler emits
// calls under the C names.
#pragma once

// Includes first, before the macros below: a macro named MIN, assert or
// __unused must not reach an SDK header.
#include <limits.h>
#include <stdio.h>
#include <sys/queue.h>
#include <wchar.h>

#include "kernel/result.h"
#include "kernel/str.h"
#include "kernel/types.h"
#include "proc/rt.h"

// ------------------------------------------------------------------- limits
//
// The kit's PATH_MAX is 512 and its LINE_MAX 2048, both filesystem answers.
// These are frame-budget ones and they stay: citrus builds paths in coroutine
// locals -- three in _citrus_esdb_open, four in _citrus_csmapper_open -- and a
// frame past 512 bytes costs a whole 64 KiB span. The longest path built is
// <prefix>/share/i18n/csmapper/<dir>/<a>%<b>.mps, 81 bytes against the longest
// real prefix, so 256 is four times what any of them needs.
//
// The kit's own archive is compiled against 512, and that divergence is silent
// by construction -- so the rule here is that no kit function is ever handed
// one of these buffers with an implied size. Every call that takes one passes
// the length explicitly (strlcpy, snprintf, _lookup_alias), and there is no
// getcwd or realpath in this port to write past the end of one.
#undef PATH_MAX
#define PATH_MAX 256
#undef LINE_MAX
#define LINE_MAX 256

// -------------------------------------------------------------------- types
//
// The integer names citrus spells the BSD way, over the kernel's own sizes.

typedef unsigned char __uint8_t;
typedef unsigned short __uint16_t;
typedef unsigned int __uint32_t;
typedef unsigned long long __uint64_t;
typedef signed char __int8_t;
typedef short __int16_t;
typedef int __int32_t;
typedef long long __int64_t;

typedef unsigned char u_int8_t;
typedef unsigned short u_int16_t;
typedef unsigned int u_int32_t;
typedef unsigned long long u_int64_t;
typedef unsigned int u_int;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef signed char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long long int64_t;

#ifndef NULL
#define NULL nullptr
#endif

typedef usize size_t;
typedef isize ssize_t;

// -------------------------------------------------------------------- cdefs
//
// The BSD spellings citrus uses. The rest of the real header is about
// compilers and platforms that are not this one.

#define __BEGIN_DECLS extern "C" {
#define __END_DECLS   }

#define __restrict __restrict__
#define __inline   inline
#define __packed   __attribute__((packed))
#define __unused   __attribute__((unused))

// The module templates build their function names with these.
#define __CONCAT1(x, y) x##y
#define __CONCAT(x, y)  __CONCAT1(x, y)
#define __STRING(x)     #x
#define __XSTRING(x)    __STRING(x)

// Cast away a const, where a region is handed to a reader.
#define __DECONST(type, var) ((type)(unsigned long)(const void *)(var))

// ELF symbol versioning; one link, no compatibility symbols.
#define __sym_compat(sym, impl, verid)

// -------------------------------------------------------------------- param

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#define powerof2(x) ((((x) - 1) & (x)) == 0)

// -------------------------------------------------------------------- queues
//
// The BSD intrusive lists are the port kit's <sys/queue.h> now; this header
// carried FreeBSD's own definitions of the three citrus uses until it did.


// ---------------------------------------------------------------- byte order
//
// The names Apple's branches use, over the same builtins.

#define OSSwapInt16(v) __builtin_bswap16(v)
#define OSSwapInt32(v) __builtin_bswap32(v)
#define OSSwapInt64(v) __builtin_bswap64(v)

#define OSSwapBigToHostInt16(v) __builtin_bswap16(v)
#define OSSwapBigToHostInt32(v) __builtin_bswap32(v)
#define OSSwapBigToHostInt64(v) __builtin_bswap64(v)
#define OSSwapHostToBigInt16(v) __builtin_bswap16(v)
#define OSSwapHostToBigInt32(v) __builtin_bswap32(v)
#define OSSwapHostToBigInt64(v) __builtin_bswap64(v)

#define OSSwapLittleToHostInt16(v) (v)
#define OSSwapLittleToHostInt32(v) (v)
#define OSSwapLittleToHostInt64(v) (v)
#define OSSwapHostToLittleInt16(v) (v)
#define OSSwapHostToLittleInt32(v) (v)
#define OSSwapHostToLittleInt64(v) (v)

// ------------------------------------------------------------------ threads
//
// One thread. citrus_lock.h's WLOCK/UNLOCK are the only users, and they become
// nothing.

typedef char pthread_rwlock_t;
#define PTHREAD_RWLOCK_INITIALIZER 0

// --------------------------------------------------------------- wide chars
//
// The kit's <wchar.h>. Its mbstate_t is this one -- {buf[4], len} -- because
// citrus keeps one per conversion in sc_mbstate and feeds it a batch at a
// time, which is the case a placeholder cannot answer.


// --------------------------------------------------------------- the prefix
//
// Where /share/i18n lives. The package puts the data beside the binary, under
// a store path that carries the version, so the program works the path out at
// startup rather than carrying one. libiconv_set_relocation_prefix is
// upstream's own hook for exactly this.
Task<void> citrus_prefix_init();
Str citrus_prefix();

// The rest of the path, relative to the store directory.
#define _PATH_ESDB     "/share/i18n/esdb"
#define _PATH_CSMAPPER "/share/i18n/csmapper"

// --------------------------------------------------------------- file access
//
// _citrus_map_file's replacement. There is no mmap, so a region is a heap
// block holding the whole file. The prefix is prepended here and nowhere else,
// which is what lets every path in the library stay the literal upstream
// wrote.
// citrus_mmap.h declares the synchronous half, _citrus_map_file, which answers
// out of the preloaded index files and keeps upstream's signature. This is the
// half that reads a file the conversion asked for.
struct _citrus_region;
Task<int> _citrus_load_file(struct _citrus_region *r, const char *path);

// Whether a path is one of the preloaded index files, for the one place citrus
// stats before reading. Synchronous, because the answer is already in hand.
extern "C" int citrus_have_file(const char *path);

// Reads the index files, once. A region that came from there is borrowed, and
// _citrus_unmap_file does not free it.
Task<Result<void>> citrus_preload();

// ------------------------------------------------------------------ the locale
//
// One locale and it is UTF-8; braam.cpp answers with that. No setuid either,
// so the environment is always trusted.
#define CODESET 0

extern "C" {
char *nl_langinfo(int item);
char *locale_charset(void);
int issetugid(void);
}

// ------------------------------------------------------------------ reporting
//
// Citrus signals "cannot happen" with abort(), and asserts its own invariants
// 46 times over. A wasm trap is the whole report, so this writes a line first.
extern "C" void iconv_assert_fail(const char *what, const char *file, int line);

#ifdef NDEBUG
#define assert(e) ((void)0)
#else
#define assert(e) ((e) ? (void)0 : iconv_assert_fail(#e, __FILE__, __LINE__))
#endif

// What the library's errno means to the process that called it.
Error iconv_error(int err);
