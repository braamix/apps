/* What upstream's headers assume the .c file included before them: the BSD
 * spellings, the integer names, and stdio's SEEK_* and EOF. They are
 * force-included so that sorting the includes cannot break a build. */
#ifndef _ICONV_PRELUDE_H_
#define _ICONV_PRELUDE_H_

#include <sys/cdefs.h>
#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

/* The port kit's PATH_MAX is 512, which is a filesystem answer. This is a
 * frame-budget answer: citrus builds paths in coroutine locals -- three of
 * them in _citrus_esdb_open, four in _citrus_csmapper_open -- and a frame past
 * 512 bytes costs a whole 64 KiB span. The longest path this library ever
 * builds is <prefix>/share/i18n/csmapper/<dir>/<a>%<b>.mps, which measures 81
 * bytes against the longest real prefix. */
#undef PATH_MAX
#define PATH_MAX 256
#undef LINE_MAX
#define LINE_MAX 256

#endif
