/*
 * <stdlib.h> for Braam: libc/common's, but malloc is a real allocator (free gives the
 * block back) and exit ends the process at once: it tells the kernel the status and
 * traps, which the kernel reports as a crash.  There is no unwinding, so neither the
 * defers of the coroutines on the way nor the stdio buffers run: a program ends by
 * returning from main, after which the runtime flushes stdout and stderr.
 */
#ifndef _STDLIB_H
#define _STDLIB_H

#include <braam.h>
#include <stddef.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX     2147483647

typedef struct {
    int quot;
    int rem;
} div_t;

typedef struct {
    long quot;
    long rem;
} ldiv_t;

int atoi(const char *s);
long strtol(const char *s, char **end, int base);
unsigned long strtoul(const char *s, char **end, int base);
long long strtoll(const char *s, char **end, int base);
unsigned long long strtoull(const char *s, char **end, int base);
/* Correctly rounded: musl's scanner, from the SDK's libbraam_math. */
double strtod(const char *s, char **end);
float strtof(const char *s, char **end);

_Noreturn void exit(int status);
_Noreturn void _Exit(int status);
_Noreturn void abort(void);
/* Run, last first, when main returns: exit() runs nothing. */
int atexit(void (*func)(void));

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
              int (*compar)(const void *, const void *));

/* The environment the process was started with. */
char *getenv(const char *name);

/* path made absolute, with every symbolic link, "." and ".." resolved, into
   resolved (PATH_MAX bytes), or one from malloc when it is NULL; NULL when a part
   of it does not exist. */
coro(braam_call *) char *realpath(const char *path, char *resolved);

/* A new file from a template ending in six X, then `suffixlen` more bytes: the
   X are replaced, and the file is open for reading and writing. */
coro(braam_call *) int mkstemp(char *template);
coro(braam_call *) int mkstemps(char *template, int suffixlen);

void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);

#endif /* _STDLIB_H */
