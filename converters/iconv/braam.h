// The C library citrus expects, and the two primitives that replace dlopen and
// mmap. Everything a port supplies for itself must be extern "C": the compiler
// emits calls under the C names, and a C++-mangled definition does not satisfy
// them.
#pragma once

#include "kernel/result.h"
#include "kernel/str.h"
#include "kernel/types.h"
#include "proc/rt.h"

// --------------------------------------------------------------- the prefix
//
// Where /share/i18n lives. The package puts the data beside the binary, under
// a store path that carries the version, so the program works the path out at
// startup rather than carrying one. libiconv_set_relocation_prefix is
// upstream's own hook for exactly this.
Task<void> citrus_prefix_init();
Str citrus_prefix();

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

// ------------------------------------------------------------------ reporting
//
// Citrus signals "cannot happen" with abort(). A wasm trap is the whole report,
// so these say what happened first.
extern "C" void iconv_assert_fail(const char *what, const char *file, int line);

// What the library's errno means to the process that called it.
Error iconv_error(int err);
