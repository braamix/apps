/* wchar_t is UTF-32 here and the locale is UTF-8, so the two conversions the
 * Apple wchar_t extension needs are the kernel's own codec. wchar_t itself is a
 * C++ keyword and needs no declaration. */
#ifndef _WCHAR_H_
#define _WCHAR_H_

#include <sys/types.h>

/* A UTF-8 sequence is at most four bytes, and the state holds one that
 * straddled a call. */
#define MB_LEN_MAX 4
#define MB_CUR_MAX 4

/* A wide character or WEOF, which is what a rune-at-a-time reader returns. */
typedef int wint_t;
#define WEOF ((wint_t) - 1)

typedef struct {
    unsigned char buf[4];
    unsigned char len;
} mbstate_t;

extern "C" {
size_t mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps);
size_t wcrtomb(char *s, wchar_t wc, mbstate_t *ps);
}

#endif
