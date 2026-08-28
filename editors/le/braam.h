// The C library LE expects, supplied here because there is not one.
//
// Everything is extern "C": clang emits calls to these names itself and a
// C++-mangled definition does not satisfy them.
//
// stdio is not here. The config parsers went over to proc/file.h and the
// editor itself to syscalls; what is left of printf is the message line.
#pragma once

#include "kernel/types.h"

#define NULL nullptr

using va_list = __builtin_va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_end(ap)         __builtin_va_end(ap)
#define va_arg(ap, t)      __builtin_va_arg(ap, t)

extern "C" {

void *malloc(usize n);
void *calloc(usize n, usize size);
void *realloc(void *p, usize n);
void free(void *p);

void *memcpy(void *d, const void *s, usize n);
void *memmove(void *d, const void *s, usize n);
void *memset(void *d, int c, usize n);
int memcmp(const void *a, const void *b, usize n);
void *memchr(const void *s, int c, usize n);

usize strlen(const char *s);
char *strcpy(char *d, const char *s);
char *strncpy(char *d, const char *s, usize n);
char *strcat(char *d, const char *s);
char *strncat(char *d, const char *s, usize n);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, usize n);
int strcasecmp(const char *a, const char *b);
int strncasecmp(const char *a, const char *b, usize n);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strstr(const char *h, const char *n);
char *strdup(const char *s);
usize strspn(const char *s, const char *set);
usize strcspn(const char *s, const char *set);
char *strpbrk(const char *s, const char *set);
char *strtok(char *s, const char *sep);

int toupper(int c);
int tolower(int c);
int isspace(int c);
int isblank(int c);
int isdigit(int c);
int isxdigit(int c);
int isalpha(int c);
int isalnum(int c);
int islower(int c);
int isupper(int c);
int isprint(int c);
int isgraph(int c);
int ispunct(int c);
int iscntrl(int c);

char *getenv(const char *name);
int atoi(const char *s);
long strtol(const char *s, char **end, int base);
unsigned long strtoul(const char *s, char **end, int base);
long long strtoll(const char *s, char **end, int base);
unsigned long long strtoull(const char *s, char **end, int base);
int abs(int n);
long labs(long n);

void qsort(void *base, usize n, usize size, int (*cmp)(const void *, const void *));

// %% %c %s %n$ no; %d %i %u %o %x %X with l/ll, %g/%e/%f, a field width, a
// precision, and the - 0 # flags. Anything else is copied through as typed.
int vsnprintf(char *buf, usize size, const char *fmt, va_list ap);
int snprintf(char *buf, usize size, const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);

} // extern "C"

// A failed assertion is a trap; there is nothing to print to at that point.
#define assert(e) ((e) ? (void)0 : __builtin_trap())
