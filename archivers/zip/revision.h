// revision.h — Zip 3. The version this port is of, and the OS code it stamps
// into a central header's "version made by".
#pragma once

#define Z_MAJORVER   3
#define Z_MINORVER   0
#define Z_PATCHLEVEL 0
#define REVDATE      "July 5th 2008"
#define VERSION      "3.0"

// 3 is Unix. Braam is not Unix, but a zip's OS code says what the external
// attributes mean, and these are Unix mode bits — synthesized, since the
// filesystem has none.
#define OS_CODE 0x300
