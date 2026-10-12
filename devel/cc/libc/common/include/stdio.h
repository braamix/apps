/*
 * <stdio.h> — input/output (C11 §7.21).
 *
 * Status: printf / sprintf / snprintf / puts / putchar are implemented in the
 * runtime (over each target's __doprnt engine); the rest are declared for future
 * implementation (marked TODO).  The extensions block at the bottom
 * exposes the low-level console primitives of the runtime.
 */
#ifndef _STDIO_H
#define _STDIO_H

#include <stdarg.h>
#include <stddef.h>

#define EOF (-1)

/* Opaque file handle (no buffered file layer yet). */
typedef struct __FILE FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/* ---- implemented in the runtime ---- */
int printf(const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);
int snprintf(char *buf, int size, const char *fmt, ...);
int puts(const char *s);
int putchar(int c);

/* ---- declared for future implementation (TODO) ---- */
int fprintf(FILE *stream, const char *fmt, ...);
int vprintf(const char *fmt, va_list ap);
int vfprintf(FILE *stream, const char *fmt, va_list ap);
int vsprintf(char *buf, const char *fmt, va_list ap);
int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);

int scanf(const char *fmt, ...);
int sscanf(const char *str, const char *fmt, ...);
int fscanf(FILE *stream, const char *fmt, ...);

int getchar(void);
int fputs(const char *s, FILE *stream);
int fputc(int c, FILE *stream);
int fgetc(FILE *stream);
char *fgets(char *s, int n, FILE *stream);

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);

FILE *fopen(const char *path, const char *mode);
int fclose(FILE *stream);
int fflush(FILE *stream);
void perror(const char *s);

/* ---- runtime extensions ---- */
/*
 * Low-level console I/O.  putbyte writes a byte (on BESM-6 into a KOI7 buffer);
 * putch writes a character (on BESM-6 the bytes packed in a word); getch reads
 * one byte (BESM-6 only); flush forces the output buffer out.
 */
void putbyte(int b);
void putch(unsigned ch);
int getch(void);
void flush(void);

#endif /* _STDIO_H */
