// What the port kit has not got. The C library proper is the kit's, asked for
// with PORT in CMakeLists.txt; this is the remainder.
//
// C-safe: regex.c and wcwidth.c include it too.
#pragma once

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// The kit's, declared here rather than by including <stdio.h>: that header
// typedefs FILE to a struct of its own, and lefile.h's FILE is proc/file.h's.
int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);
int snprintf(char *buf, size_t size, const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);

// fnmatch's *, ? and [...] over a whole string; FNM_PATHNAME and friends are
// not here because no caller passes a flag. 0 on a match, FNM_NOMATCH else.
enum { FNM_NOMATCH = 1 };
int fnmatch(const char *pattern, const char *s, int flags);

#ifdef __cplusplus
}
#endif

// The kit has no <assert.h>, and there is nothing to print to at that point.
#define assert(e) ((e) ? (void)0 : __builtin_trap())
