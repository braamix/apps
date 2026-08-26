/* PATH_MAX is 256 rather than 1024 on purpose: citrus builds paths on the
 * stack, and a coroutine's locals live in a heap frame that must stay under
 * 512 bytes. The longest path this library ever builds is
 * <prefix>/share/i18n/csmapper/<dir>/<a>%<b>.mps, which measures 81 bytes
 * against the longest real prefix. */
#ifndef _LIMITS_H_
#define _LIMITS_H_

#define PATH_MAX 256
#define LINE_MAX 256

#define CHAR_BIT   8
#define UCHAR_MAX  0xFF
#define SCHAR_MAX  0x7F
#define USHRT_MAX  0xFFFF
#define SHRT_MAX   0x7FFF
#define INT_MIN    (-0x7FFFFFFF - 1)
#define INT_MAX    0x7FFFFFFF
#define UINT_MAX   0xFFFFFFFFU
#define LONG_MIN   (-0x7FFFFFFFL - 1)
#define LONG_MAX   0x7FFFFFFFL
#define ULONG_MAX  0xFFFFFFFFUL
#define LLONG_MIN  (-0x7FFFFFFFFFFFFFFFLL - 1)
#define LLONG_MAX  0x7FFFFFFFFFFFFFFFLL
#define ULLONG_MAX 0xFFFFFFFFFFFFFFFFULL

#endif
