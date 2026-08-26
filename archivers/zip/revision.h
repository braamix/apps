// revision.h — Zip 3. The version this port is of, and the OS code it stamps
// into a central header's "version made by".
#pragma once

// The strings below name ZCONST, so this says where that comes from rather
// than relying on the include order clang-format sorts.
#include "zip.h"

#define Z_MAJORVER   3
#define Z_MINORVER   0
#define Z_PATCHLEVEL 0
#define REVDATE      "July 5th 2008"
#define VERSION      "3.0"

// 3 is Unix. Braam is not Unix, but a zip's OS code says what the external
// attributes mean, and these are Unix mode bits — synthesized, since the
// filesystem has none.
#define OS_CODE 0x300

// The strings -h, -L and -v print. Upstream kept them here behind DEFCPYRT so
// they were defined once; there is one file that names them here too.
extern ZCONST char *copyright[1];
extern ZCONST char *far swlicense[50];
extern ZCONST char *far versinfolines[7];
extern ZCONST char *far cryptnote[7];
