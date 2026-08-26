/* What upstream's headers assume the .c file included before them: the BSD
 * spellings, the integer names, and stdio's SEEK_* and EOF. They are
 * force-included so that sorting the includes cannot break a build. */
#ifndef _ICONV_PRELUDE_H_
#define _ICONV_PRELUDE_H_

#include <sys/cdefs.h>
#include <sys/types.h>

#include <stdio.h>

#endif
