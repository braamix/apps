// wchar.h and wctype.h, for one encoding. Braam is UTF-8 throughout, so there
// is no locale to ask and mb_mode is always on.
#pragma once

#ifdef __cplusplus
#include "kernel/types.h"
#else
typedef unsigned long usize;
typedef int wchar_t; /* clang's for wasm32 */
#endif

#ifndef WEOF
#define WEOF ((wint_t) - 1)
#endif

typedef int wint_t;

// Upstream keeps one of these per decode point; there is no shift state in
// UTF-8, so it is a placeholder that keeps the call sites unchanged.
typedef struct {
    int dummy;
} mbstate_t;

enum { MB_LEN_MAX = 4 };
#define MB_CUR_MAX 4

#ifdef __cplusplus
extern "C" {
#endif

// -1 for a sequence that is not UTF-8, 0 for a NUL. `n` bounds the read.
int mbtowc(wchar_t *out, const char *s, usize n);
int mblen(const char *s, usize n);
int wctomb(char *out, wchar_t c);
usize mbstowcs(wchar_t *out, const char *s, usize n);
usize wcslen(const wchar_t *s);

// Markus Kuhn's, vendored as upstream had it. wcwidth.c spells it
// mk_wcwidth; upstream reached it through a gnulib #define.
int mk_wcwidth(wchar_t c);
int wcwidth(wchar_t c);

int iswalnum(wint_t c);
int iswalpha(wint_t c);
int iswdigit(wint_t c);
int iswspace(wint_t c);
int iswprint(wint_t c);
int iswupper(wint_t c);
int iswlower(wint_t c);
wint_t towupper(wint_t c);

// wctrans is the locale's named case mappings. There is one locale here, so a
// transform is the function itself rather than a name to look up.
typedef wint_t (*wctrans_t)(wint_t);

wctrans_t wctrans(const char *name);
wint_t towctrans(wint_t c, wctrans_t t);
wint_t towlower(wint_t c);

#ifdef __cplusplus
}
#endif
