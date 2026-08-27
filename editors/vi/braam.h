// The C library ex expects, supplied here because there is not one.
//
// ex was written to avoid stdio ("Ex means to avoid stdio like the plague",
// READ_ME) and calls read/write directly, so the surface is small: the string
// and ctype functions, a heap, and the handful of system calls that survive
// the port. Everything is extern "C" because clang emits calls to these names
// itself and a C++-mangled definition does not satisfy them.
#pragma once

#include "kernel/types.h"

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

} // extern "C"
