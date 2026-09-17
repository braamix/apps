// A str's bytes are UTF-8 widened by one rule: a surrogate, U+D800 to
// U+DFFF, is written the way any other three-byte codepoint is. That is how
// "\ud800" is a str at all. Text from outside is never trusted to be in this
// form -- a codec decodes it -- so these helpers assume bytes that already are.
#pragma once

#include "kernel/fmt.h"
#include "kernel/str.h"
#include "kernel/string.h"
#include "kernel/types.h"

inline usize cp_width(u8 lead)
{
    return lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
}

// One codepoint at `at`, and how many bytes it took; 0 at the end.
inline usize cp_decode(Str s, usize at, u32 &out)
{
    if (at >= s.size())
        return 0;
    u8 c    = u8(s[at]);
    usize w = cp_width(c);
    if (at + w > s.size())
        w = s.size() - at;
    if (w == 1) {
        out = c;
        return 1;
    }
    u32 v = c & (0xff >> (w + 1));
    for (usize k = 1; k < w; k++)
        v = (v << 6) | (u8(s[at + k]) & 0x3f);
    out = v;
    return w;
}

// Any codepoint below U+110000, surrogates included. The length.
inline usize cp_encode(u32 cp, char *out)
{
    if (cp < 0x80) {
        out[0] = char(cp);
        return 1;
    }
    if (cp < 0x800) {
        out[0] = char(0xc0 | (cp >> 6));
        out[1] = char(0x80 | (cp & 0x3f));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = char(0xe0 | (cp >> 12));
        out[1] = char(0x80 | ((cp >> 6) & 0x3f));
        out[2] = char(0x80 | (cp & 0x3f));
        return 3;
    }
    out[0] = char(0xf0 | (cp >> 18));
    out[1] = char(0x80 | ((cp >> 12) & 0x3f));
    out[2] = char(0x80 | ((cp >> 6) & 0x3f));
    out[3] = char(0x80 | (cp & 0x3f));
    return 4;
}

inline bool cp_append(String &out, u32 cp)
{
    char tmp[4];
    return out.append(Str(tmp, cp_encode(cp, tmp)));
}

inline bool is_surrogate(u32 cp)
{
    return cp >= 0xd800 && cp <= 0xdfff;
}

// Does the text hold a surrogate? Only the lead byte 0xED can start one.
inline bool has_surrogate(Str s)
{
    for (usize i = 0; i < s.size(); i++)
        if (u8(s[i]) == 0xed && i + 1 < s.size() && u8(s[i + 1]) >= 0xa0)
            return true;
    return false;
}

// Strict UTF-8, as a codec checks it: the length of the valid prefix, and
// where it stops, how many bytes are wrong and why -- CPython's reason
// ("invalid start byte", "invalid continuation byte", "unexpected end of
// data"). A surrogate is invalid here.
struct Utf8Bad {
    usize at     = 0;
    usize length = 0;
    Str reason;
};

bool utf8_strict(Str s, Utf8Bad &bad);

// `v` in `w` hex digits at least, lower case unless `upper`.
template <usize N>
Buf<N> &put_hexw(Buf<N> &b, u32 v, int w, bool upper = false)
{
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int n              = 1;
    while (n < 8 && (v >> (4 * n)))
        n++;
    for (int k = n < w ? w : n; k > 0; k--)
        b.put(digits[(v >> (4 * (k - 1))) & 15]);
    return b;
}

template <usize N>
Buf<N> &put_i64(Buf<N> &b, i64 v)
{
    if (v < 0) {
        b.put('-');
        return b.put(u64(-(v + 1)) + 1);
    }
    return b.put(u64(v));
}
