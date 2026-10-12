/*
 * <stdlib.h> — general utilities (C11 §7.22).
 *
 * Status: exit() and atoi() are implemented in the runtime.  malloc/calloc/
 * realloc/free are in the BESM-6 Unix libc0.a (not the Madlen libc.bin, which
 * has no heap) and in the RISC-V libc.a (a bump allocator; free does nothing,
 * realloc copies into a new block unless the old one is big enough).
 * The rest are declared for future implementation (TODO).
 */
#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

/* A target whose int cannot hold the default defines _RAND_MAX in its <stddef.h>. */
#ifdef _RAND_MAX
#define RAND_MAX _RAND_MAX
#else
#define RAND_MAX 2147483647
#endif

typedef struct {
    int quot;
    int rem;
} div_t;

typedef struct {
    long quot;
    long rem;
} ldiv_t;

/* ---- implemented in the runtime ---- */
_Noreturn void exit(int status);
int atoi(const char *nptr);

/* ---- implemented where the runtime has a heap (see above) ---- */
void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);

/* ---- declared for future implementation (TODO) ---- */
_Noreturn void abort(void);
int atexit(void (*func)(void));

long atol(const char *nptr);
double atof(const char *nptr);
long strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);
double strtod(const char *nptr, char **endptr);

int abs(int j);
long labs(long j);
div_t div(int numer, int denom);
ldiv_t ldiv(long numer, long denom);

int rand(void);
void srand(unsigned seed);

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
              int (*compar)(const void *, const void *));

char *getenv(const char *name);
int system(const char *command);

#endif /* _STDLIB_H */
