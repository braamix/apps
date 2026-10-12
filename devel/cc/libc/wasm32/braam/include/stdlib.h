/*
 * <stdlib.h> for Braam: libc/common's, but malloc is a real allocator (free gives the
 * block back) and exit ends the process at once: it tells the kernel the status and
 * traps, which the kernel reports as a crash.  There is no unwinding, so neither the
 * defers of the coroutines on the way nor the stdio buffers run: a program ends by
 * returning from main, after which the runtime flushes stdout and stderr.
 */
#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

int atoi(const char *s);

_Noreturn void exit(int status);
_Noreturn void abort(void);

void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);

#endif /* _STDLIB_H */
