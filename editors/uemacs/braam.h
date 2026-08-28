// The C library uemacs expects, supplied here because there is not one.
//
// The string and ctype functions, a heap, and the printf conversions the
// message line uses. Everything is extern "C": clang emits calls to these
// names itself and a C++-mangled definition does not satisfy them.
//
// stdio is not here. fileio.cpp and main.cpp were the only users and both
// went over to syscalls.
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

usize strlen(const char *s);
char *strcpy(char *d, const char *s);
char *strncpy(char *d, const char *s, usize n);
char *strcat(char *d, const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, usize n);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strstr(const char *h, const char *n);

int toupper(int c);
int tolower(int c);
int isspace(int c);
int isdigit(int c);
int isalpha(int c);
int isalnum(int c);
int islower(int c);
int isupper(int c);
int isprint(int c);
int iscntrl(int c);

char *getenv(const char *name);
int atoi(const char *s);

void qsort(void *base, usize n, usize size, int (*cmp)(const void *, const void *));

// %% %c %s %.*s %d %ld %u %o %x, a field width and a zero flag. That is every
// conversion this tree uses; anything else is copied through as typed.
int vsnprintf(char *buf, usize size, const char *fmt, va_list ap);
int snprintf(char *buf, usize size, const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);

} // extern "C"
