/* snprintf and nothing else: the library formats paths with it forty times and
 * never opens a stream. The command's streams are proc/file.h's. */
#ifndef _STDIO_H_
#define _STDIO_H_

#include <sys/types.h>

#define EOF (-1)

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

extern "C" {
int snprintf(char *buf, size_t n, const char *fmt, ...);
int vsnprintf(char *buf, size_t n, const char *fmt, __builtin_va_list ap);
}

#endif
