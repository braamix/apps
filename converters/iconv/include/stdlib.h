#ifndef _STDLIB_H_
#define _STDLIB_H_

#include <sys/types.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

extern "C" {
void *malloc(size_t n);
void *calloc(size_t n, size_t sz);
void *realloc(void *p, size_t n);
void *reallocarray(void *p, size_t n, size_t sz);
void free(void *p);

/* No process exits from inside the library; these record and trap. */
void abort(void);
void exit(int status);

char *getenv(const char *name);
int issetugid(void);

unsigned long strtoul(const char *s, char **end, int base);
long strtol(const char *s, char **end, int base);

void qsort(void *base, size_t n, size_t sz, int (*cmp)(const void *, const void *));
}

#endif
