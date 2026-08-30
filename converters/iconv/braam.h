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

#include "kernel/result.h"
#include "kernel/str.h"
#include "kernel/types.h"
#include "proc/rt.h"

// ------------------------------------------------------------------- limits
//
// The kit's PATH_MAX is 512, a filesystem answer. This is a frame-budget one:
// citrus builds paths in coroutine locals -- three in _citrus_esdb_open, four
// in _citrus_csmapper_open -- and a frame past 512 bytes costs a whole 64 KiB
// span. The longest path built is <prefix>/share/i18n/csmapper/<dir>/<a>%<b>.mps,
// 81 bytes against the longest real prefix.
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
// The BSD intrusive lists, in the subset citrus uses: a singly-linked tail
// queue, a doubly-linked one, and a plain list. Pointer arithmetic only, so
// these are FreeBSD's own definitions less the debugging and the variants
// nothing here names.

// -- singly-linked tail queue --------------------------------------------

#define STAILQ_HEAD(name, type)  \
    struct name {                \
        struct type *stqh_first; \
        struct type **stqh_last; \
    }

#define STAILQ_ENTRY(type)      \
    struct {                    \
        struct type *stqe_next; \
    }

#define STAILQ_INIT(head)                         \
    do {                                          \
        (head)->stqh_first = nullptr;             \
        (head)->stqh_last  = &(head)->stqh_first; \
    } while (0)

#define STAILQ_FIRST(head)      ((head)->stqh_first)
#define STAILQ_NEXT(elm, field) ((elm)->field.stqe_next)

#define STAILQ_FOREACH(var, head, field) \
    for ((var) = STAILQ_FIRST(head); (var); (var) = STAILQ_NEXT(var, field))

#define STAILQ_INSERT_TAIL(head, elm, field)                \
    do {                                                    \
        STAILQ_NEXT(elm, field) = nullptr;                  \
        *(head)->stqh_last      = (elm);                    \
        (head)->stqh_last       = &STAILQ_NEXT(elm, field); \
    } while (0)

#define STAILQ_REMOVE_HEAD(head, field)                                               \
    do {                                                                              \
        if (((head)->stqh_first = STAILQ_NEXT((head)->stqh_first, field)) == nullptr) \
            (head)->stqh_last = &(head)->stqh_first;                                  \
    } while (0)

// -- doubly-linked tail queue --------------------------------------------

#define TAILQ_HEAD(name, type)  \
    struct name {               \
        struct type *tqh_first; \
        struct type **tqh_last; \
    }

#define TAILQ_ENTRY(type)       \
    struct {                    \
        struct type *tqe_next;  \
        struct type **tqe_prev; \
    }

#define TAILQ_INIT(head)                        \
    do {                                        \
        (head)->tqh_first = nullptr;            \
        (head)->tqh_last  = &(head)->tqh_first; \
    } while (0)

#define TAILQ_FIRST(head)      ((head)->tqh_first)
#define TAILQ_NEXT(elm, field) ((elm)->field.tqe_next)
#define TAILQ_EMPTY(head)      ((head)->tqh_first == nullptr)

// headname is the struct tag, which is how the last element is reached without
// a back pointer in the head.
#define TAILQ_LAST(head, headname) (*(((struct headname *)((head)->tqh_last))->tqh_last))

#define TAILQ_PREV(elm, headname, field) (*(((struct headname *)((elm)->field.tqe_prev))->tqh_last))

#define TAILQ_FOREACH(var, head, field) \
    for ((var) = TAILQ_FIRST(head); (var); (var) = TAILQ_NEXT(var, field))

#define TAILQ_FOREACH_SAFE(var, head, field, tvar) \
    for ((var) = TAILQ_FIRST(head); (var) && ((tvar) = TAILQ_NEXT(var, field), 1); (var) = (tvar))

#define TAILQ_INSERT_TAIL(head, elm, field)               \
    do {                                                  \
        TAILQ_NEXT(elm, field) = nullptr;                 \
        (elm)->field.tqe_prev  = (head)->tqh_last;        \
        *(head)->tqh_last      = (elm);                   \
        (head)->tqh_last       = &TAILQ_NEXT(elm, field); \
    } while (0)

#define TAILQ_INSERT_BEFORE(listelm, elm, field)                \
    do {                                                        \
        (elm)->field.tqe_prev      = (listelm)->field.tqe_prev; \
        TAILQ_NEXT(elm, field)     = (listelm);                 \
        *(listelm)->field.tqe_prev = (elm);                     \
        (listelm)->field.tqe_prev  = &TAILQ_NEXT(elm, field);   \
    } while (0)

#define TAILQ_REMOVE(head, elm, field)                                      \
    do {                                                                    \
        if ((TAILQ_NEXT(elm, field)) != nullptr)                            \
            TAILQ_NEXT(elm, field)->field.tqe_prev = (elm)->field.tqe_prev; \
        else                                                                \
            (head)->tqh_last = (elm)->field.tqe_prev;                       \
        *(elm)->field.tqe_prev = TAILQ_NEXT(elm, field);                    \
    } while (0)

// -- list ------------------------------------------------------------------

#define LIST_HEAD(name, type)  \
    struct name {              \
        struct type *lh_first; \
    }

#define LIST_ENTRY(type)       \
    struct {                   \
        struct type *le_next;  \
        struct type **le_prev; \
    }

#define LIST_INIT(head)             \
    do {                            \
        (head)->lh_first = nullptr; \
    } while (0)

#define LIST_FIRST(head)      ((head)->lh_first)
#define LIST_NEXT(elm, field) ((elm)->field.le_next)

#define LIST_FOREACH(var, head, field) \
    for ((var) = LIST_FIRST(head); (var); (var) = LIST_NEXT(var, field))

#define LIST_INSERT_HEAD(head, elm, field)                            \
    do {                                                              \
        if ((LIST_NEXT(elm, field) = (head)->lh_first) != nullptr)    \
            (head)->lh_first->field.le_prev = &LIST_NEXT(elm, field); \
        (head)->lh_first     = (elm);                                 \
        (elm)->field.le_prev = &(head)->lh_first;                     \
    } while (0)

#define LIST_REMOVE(elm, field)                                          \
    do {                                                                 \
        if (LIST_NEXT(elm, field) != nullptr)                            \
            LIST_NEXT(elm, field)->field.le_prev = (elm)->field.le_prev; \
        *(elm)->field.le_prev = LIST_NEXT(elm, field);                   \
    } while (0)

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
// wchar_t is UTF-32 and the locale is UTF-8, so the two conversions the Apple
// wchar_t extension needs are the kernel's own codec. wchar_t is a C++ keyword
// and needs no declaration; MB_LEN_MAX is the kit's <limits.h>, which already
// says 4 for the same reason.
#define MB_CUR_MAX 4

typedef int wint_t;
#define WEOF ((wint_t) - 1)

// A UTF-8 sequence is at most four bytes; the state holds one that straddled a
// call.
typedef struct {
    unsigned char buf[4];
    unsigned char len;
} mbstate_t;

extern "C" {
size_t mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps);
size_t wcrtomb(char *s, wchar_t wc, mbstate_t *ps);
}

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
