// UTF-8, in place of the locale's multibyte encoding. wcwidth is upstream's
// own wcwidth.c; the rest is here.

#include "lewchar.h"

#include "braam.h"
#include "kernel/text.h"

extern "C" int mbtowc(wchar_t *out, const char *s, usize n)
{
    char32_t c;
    usize used;

    if (s == nullptr)
        return 0; // "is this encoding stateful?" -- no
    if (n == 0)
        return -1;
    if ((unsigned char)*s == 0) {
        if (out)
            *out = 0;
        return 0;
    }
    used = utf8_decode(Str(s, n), 0, c);
    if (used == 0)
        return -1; // the sequence runs past the end
    // A malformed sequence answers U+FFFD, which is not the same as a real
    // U+FFFD in the text; upstream wants -1 for the first and a length for the
    // second, and MBCharInvalid turns on it.
    if (c == 0xFFFD && !(used == 3 && (unsigned char)s[0] == 0xEF))
        return -1;
    if (out)
        *out = (wchar_t)c;
    return (int)used;
}

extern "C" int mblen(const char *s, usize n)
{
    return mbtowc(nullptr, s, n);
}

extern "C" int wctomb(char *out, wchar_t c)
{
    char buf[4];
    usize n;

    if (out == nullptr)
        return 0;
    n = utf8_encode((char32_t)c, buf);
    if (n == 0)
        return -1;
    memcpy(out, buf, n);
    return (int)n;
}

extern "C" usize mbstowcs(wchar_t *out, const char *s, usize n)
{
    usize k = 0;

    while (*s) {
        wchar_t c;
        int used = mbtowc(&c, s, 4);

        if (used <= 0)
            return (usize)-1;
        if (out) {
            if (k >= n)
                return k;
            out[k] = c;
        }
        k++;
        s += used;
    }
    if (out && k < n)
        out[k] = 0;
    return k;
}

extern "C" usize wcslen(const wchar_t *s)
{
    const wchar_t *p = s;
    while (*p)
        p++;
    return (usize)(p - s);
}

// The classifications, by range: ASCII exactly, and everything above Latin-1
// treated as a letter unless it is a separator. That is what the editor uses
// them for -- word boundaries and whether a cell prints.
extern "C" int iswalpha(wint_t c)
{
    if (c < 0x80)
        return isalpha((int)c);
    return c != 0x00A0 && c != 0x2007 && c != 0x202F && !(c >= 0x2000 && c <= 0x200A);
}

extern "C" int iswdigit(wint_t c)
{
    return c < 0x80 && isdigit((int)c);
}

extern "C" int iswalnum(wint_t c)
{
    return iswalpha(c) || iswdigit(c);
}

extern "C" int iswspace(wint_t c)
{
    if (c < 0x80)
        return isspace((int)c);
    return c == 0x00A0 || c == 0x2007 || c == 0x202F || (c >= 0x2000 && c <= 0x200A);
}

extern "C" int iswprint(wint_t c)
{
    if (c < 0x80)
        return isprint((int)c);
    if (c < 0xA0)
        return 0; // C1 controls
    return wcwidth((wchar_t)c) >= 0;
}

extern "C" int iswupper(wint_t c)
{
    return c != (wint_t)rune_lower((char32_t)c);
}

extern "C" int iswlower(wint_t c)
{
    return c != (wint_t)rune_upper((char32_t)c);
}

extern "C" wint_t towupper(wint_t c)
{
    return (wint_t)rune_upper((char32_t)c);
}

extern "C" wint_t towlower(wint_t c)
{
    return (wint_t)rune_lower((char32_t)c);
}

extern "C" wctrans_t wctrans(const char *name)
{
    if (!strcmp(name, "toupper"))
        return towupper;
    if (!strcmp(name, "tolower"))
        return towlower;
    return nullptr;
}

extern "C" wint_t towctrans(wint_t c, wctrans_t t)
{
    return t ? t(c) : c;
}
