// What the port kit has not got. The C library proper is the kit's, asked for
// with PORT in CMakeLists.txt; this is the remainder.
//
// C-safe: regex.c includes it too.
#pragma once

#include <ctype.h>
#include <fnmatch.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The kit has no <assert.h>, and there is nothing to print to at that point.
#define assert(e) ((e) ? (void)0 : __builtin_trap())
