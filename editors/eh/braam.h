// What the port kit has not got. The C library proper is the kit's, asked for
// with PORT in CMakeLists.txt; this is the remainder.
#pragma once

#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

// The kit has no <assert.h>.
// The kit has no <assert.h>, and there is nothing to print to at that point.
// NDEBUG matters: OFF_DEC() lets bol(-1) through under it on purpose, and
// bol() clips rather than asserting.
#ifdef NDEBUG
#define assert(e) ((void)0)
#else
#define assert(e) ((e) ? (void)0 : __builtin_trap())
#endif

// <uchar.h> is absent, but wchar_t is UTF-32 here, so these are the same two
// functions under the other name. Upstream's char32_t variables need no change:
// it is a C++ keyword.
#define mbrtoc32(o, s, n, st) mbrtowc((wchar_t *)(o), (s), (n), (st))
#define c32rtomb(o, c, st)    wcrtomb((o), (wchar_t)(c), (st))
