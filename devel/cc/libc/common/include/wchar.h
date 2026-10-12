/*
 * <wchar.h> — extended multibyte and wide character utilities (C11 §7.29).
 *
 * TODO: implement the wide-character routines.  This header is the canonical
 * home for wint_t, mbstate_t and WEOF.
 */
#ifndef _WCHAR_H
#define _WCHAR_H

#include <stdarg.h>
#include <stddef.h>

typedef int wint_t;

typedef struct {
    int __count;
    unsigned __value;
} mbstate_t;

#define WEOF ((wint_t) - 1)

size_t wcslen(const wchar_t *s);
wchar_t *wcscpy(wchar_t *dest, const wchar_t *src);
wchar_t *wcsncpy(wchar_t *dest, const wchar_t *src, size_t n);
wchar_t *wcscat(wchar_t *dest, const wchar_t *src);
int wcscmp(const wchar_t *s1, const wchar_t *s2);
int wcsncmp(const wchar_t *s1, const wchar_t *s2, size_t n);
wchar_t *wcschr(const wchar_t *s, wchar_t c);
wchar_t *wcsrchr(const wchar_t *s, wchar_t c);

wint_t btowc(int c);
int wctob(wint_t c);
size_t mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps);
size_t wcrtomb(char *s, wchar_t wc, mbstate_t *ps);
size_t mbsrtowcs(wchar_t *dst, const char **src, size_t len, mbstate_t *ps);
size_t wcsrtombs(char *dst, const wchar_t **src, size_t len, mbstate_t *ps);

long wcstol(const wchar_t *nptr, wchar_t **endptr, int base);
double wcstod(const wchar_t *nptr, wchar_t **endptr);

#endif /* _WCHAR_H */
