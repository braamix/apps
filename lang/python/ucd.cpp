// The Unicode character database, read out of ucddb.cpp.
#include "ucd.h"

#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "kernel/hash.h"
#include "ucddb.h"
#include "ustr.h"

const Str UCD_VERSION = "16.0.0";

namespace {

constexpr Str CATEGORIES[] = { "Cn", "Lu", "Ll", "Lt", "Lm", "Lo", "Mn", "Mc", "Me", "Nd",
                               "Nl", "No", "Pc", "Pd", "Ps", "Pe", "Pi", "Pf", "Po", "Sm",
                               "Sc", "Sk", "So", "Zs", "Zl", "Zp", "Cc", "Cf", "Cs", "Co" };
constexpr Str BIDI[]       = { "",    "L",  "LRE", "LRO", "R",   "AL",  "RLE", "RLO",
                               "PDF", "EN", "ES",  "ET",  "AN",  "CS",  "NSM", "BN",
                               "B",   "S",  "WS",  "ON",  "LRI", "RLI", "FSI", "PDI" };
constexpr Str WIDTHS[]     = { "N", "Na", "A", "W", "H", "F" };
constexpr Str TAGS[]       = { "",           "<font>",     "<noBreak>", "<initial>", "<medial>",
                               "<final>",    "<isolated>", "<circle>",  "<super>",   "<sub>",
                               "<vertical>", "<wide>",     "<narrow>",  "<small>",   "<square>",
                               "<fraction>", "<compat>" };

// Hangul syllables are composed, decomposed and named by arithmetic.
constexpr u32 S_BASE = 0xAC00, L_BASE = 0x1100, V_BASE = 0x1161, T_BASE = 0x11A7;
constexpr u32 L_COUNT = 19, V_COUNT = 21, T_COUNT = 28;
constexpr u32 N_COUNT = V_COUNT * T_COUNT, S_COUNT = L_COUNT * N_COUNT;

constexpr Str JAMO_L[] = { "G",  "GG", "N", "D",  "DD", "R", "M", "B", "BB", "S",
                           "SS", "",   "J", "JJ", "C",  "K", "T", "P", "H" };
constexpr Str JAMO_V[] = { "A",  "AE", "YA", "YAE", "EO", "E",  "YEO", "YE", "O",  "WA", "WAE",
                           "OE", "YO", "U",  "WEO", "WE", "WI", "YU",  "EU", "YI", "I" };
constexpr Str JAMO_T[] = { "",   "G",  "GG", "GS", "N",  "NJ", "NH", "D", "L",  "LG",
                           "LM", "LB", "LS", "LT", "LP", "LH", "M",  "B", "BS", "S",
                           "SS", "NG", "J",  "C",  "K",  "T",  "P",  "H" };

template <typename A, typename B, typename C>
u32 trie(const A &t1, const B &t2, const C &t3, u32 s1, u32 s2, u32 cp)
{
    if (cp >= 0x110000)
        return 0;
    u32 blk = t2[(u32(t1[cp >> (s1 + s2)]) << s2) + ((cp >> s1) & ((1u << s2) - 1))];
    return t3[(blk << s1) + (cp & ((1u << s1) - 1))];
}

const UcdCase &case_of(u32 cp)
{
    return UCD_CASES[ucd_rec(cp).casing];
}

// The decomposition entry of `cp`, or 0.
u32 dec_at(u32 cp)
{
    return trie(UCD_DEC_1, UCD_DEC_2, UCD_DEC_3, UCD_DEC_SHIFT1, UCD_DEC_SHIFT2, cp);
}

// The codepoints of entry `at`, and its tag.
usize dec_read(u32 at, u32 &tag, u32 *out)
{
    u32 head = UCD_DEC[at];
    usize n  = head & 0xff;
    tag      = head >> 8;
    u32 k    = at + 1;
    for (usize i = 0; i < n; i++) {
        u32 u = UCD_DEC[k++];
        if (u >= 0xd800 && u < 0xdc00)
            u = 0x10000 + ((u - 0xd800) << 10) + (UCD_DEC[k++] - 0xdc00);
        out[i] = u;
    }
    return n;
}

constexpr usize DEC_MAX = 32;

bool hex_put(String &out, u32 v, int least)
{
    char tmp[8];
    int n = 0;
    do {
        tmp[n++] = "0123456789ABCDEF"[v & 15];
        v >>= 4;
    } while (v || n < least);
    while (n)
        if (!out.push(tmp[--n]))
            return false;
    return true;
}

// Full decomposition of one codepoint, appended; canonical only unless
// `compat`.
bool decompose(u32 cp, bool compat, Vec<u32> &out)
{
    if (cp >= S_BASE && cp < S_BASE + S_COUNT) {
        u32 s = cp - S_BASE;
        u32 t = s % T_COUNT;
        if (!out.push(L_BASE + s / N_COUNT) || !out.push(V_BASE + (s % N_COUNT) / T_COUNT))
            return false;
        return !t || out.push(T_BASE + t);
    }
    u32 at  = dec_at(cp);
    u32 tag = 0;
    u32 parts[DEC_MAX];
    usize n = at ? dec_read(at, tag, parts) : 0;
    if (!n || (tag && !compat))
        return out.push(cp);
    for (usize i = 0; i < n; i++)
        if (!decompose(parts[i], compat, out))
            return false;
    return true;
}

// The primary composite of a pair, or 0.
u32 compose(u32 a, u32 b)
{
    if (a >= L_BASE && a < L_BASE + L_COUNT && b >= V_BASE && b < V_BASE + V_COUNT)
        return S_BASE + ((a - L_BASE) * V_COUNT + (b - V_BASE)) * T_COUNT;
    if (a >= S_BASE && a < S_BASE + S_COUNT && (a - S_BASE) % T_COUNT == 0 && b > T_BASE &&
        b < T_BASE + T_COUNT)
        return a + (b - T_BASE);
    usize lo = 0, hi = sizeof(UCD_COMPOSE) / sizeof(UCD_COMPOSE[0]) / 3;
    while (lo < hi) {
        usize mid = (lo + hi) / 2;
        u32 x = UCD_COMPOSE[mid * 3], y = UCD_COMPOSE[mid * 3 + 1];
        if (x == a && y == b)
            return UCD_COMPOSE[mid * 3 + 2];
        if (x < a || (x == a && y < b))
            lo = mid + 1;
        else
            hi = mid;
    }
    return 0;
}

// ------------------------------------------------------------------ names

// Word `id` of the lexicon.
Str lex_word(u32 id)
{
    u32 at = UCD_LEX_AT[id >> 4];
    for (u32 k = id & 15; k; k--)
        at += 1 + UCD_LEX[at];
    return Str(reinterpret_cast<const char *>(UCD_LEX + at + 1), UCD_LEX[at]);
}

// Name number `n` into `out`.
bool name_text(u32 n, String &out)
{
    u32 at = UCD_NAMES_AT[n >> 5];
    for (u32 k = n & 31; k; k--) {
        u32 words = UCD_NAMES[at++];
        while (words--)
            at += UCD_NAMES[at] >= 0xc0 ? 2 : 1;
    }
    u32 words = UCD_NAMES[at++];
    for (u32 w = 0; w < words; w++) {
        u32 id = UCD_NAMES[at++];
        if (id >= 0xc0)
            id = 0xc0 + ((id - 0xc0) << 8 | UCD_NAMES[at++]);
        if ((w && !out.push(' ')) || !out.append(lex_word(id)))
            return false;
    }
    return true;
}

constexpr usize RUNS  = sizeof(UCD_NAME_RUNS) / sizeof(UCD_NAME_RUNS[0]) / 3;
constexpr usize ALGOS = sizeof(UCD_ALGO) / sizeof(UCD_ALGO[0]) / 3;

// The name number of `cp`, or -1.
i32 name_number(u32 cp)
{
    usize lo = 0, hi = RUNS;
    while (lo < hi) {
        usize mid = (lo + hi) / 2;
        u32 first = UCD_NAME_RUNS[mid * 3], n = UCD_NAME_RUNS[mid * 3 + 1];
        if (cp < first)
            hi = mid;
        else if (cp >= first + n)
            lo = mid + 1;
        else
            return i32(UCD_NAME_RUNS[mid * 3 + 2] + (cp - first));
    }
    return -1;
}

u32 number_cp(u32 n)
{
    usize lo = 0, hi = RUNS;
    while (hi - lo > 1) {
        usize mid = (lo + hi) / 2;
        if (UCD_NAME_RUNS[mid * 3 + 2] <= n)
            lo = mid;
        else
            hi = mid;
    }
    return UCD_NAME_RUNS[lo * 3] + (n - UCD_NAME_RUNS[lo * 3 + 2]);
}

// The reverse index: a name's hash to its number, built on first use.
struct Index {
    Vec<u16> slots; // name number + 1, 0 empty
    u32 mask = 0;
};

Index *names_index;

u32 name_hash(Str s)
{
    u32 h = 2166136261u;
    for (usize i = 0; i < s.size(); i++)
        h = (h ^ u8(s[i])) * 16777619u;
    return h;
}

Index *built()
{
    if (names_index)
        return names_index;
    Index *x = heap_new<Index>();
    if (!x)
        return nullptr;
    u32 size = 1;
    while (size < UCD_NAMED * 2)
        size <<= 1;
    if (!x->slots.resize(size)) {
        heap_delete(x);
        return nullptr;
    }
    for (u32 i = 0; i < size; i++)
        x->slots[i] = 0;
    x->mask = size - 1;
    String buf;
    for (u32 n = 0; n < UCD_NAMED; n++) {
        buf.clear();
        if (!name_text(n, buf)) {
            heap_delete(x);
            return nullptr;
        }
        u32 at = name_hash(buf.str()) & x->mask;
        while (x->slots[at])
            at = (at + 1) & x->mask;
        x->slots[at] = u16(n + 1);
    }
    names_index = x;
    return x;
}

bool same_blind(Str a, const u8 *b, usize n)
{
    if (a.size() != n)
        return false;
    for (usize i = 0; i < n; i++)
        if (a[i] != char(b[i]))
            return false;
    return true;
}

usize jamo(Str s, const Str *col, usize n, int &which)
{
    usize best = 0;
    which      = -1;
    for (usize i = 0; i < n; i++)
        if (s.starts_with(col[i]) && (which < 0 || col[i].size() > best)) {
            best  = col[i].size();
            which = int(i);
        }
    return best;
}

} // namespace

const UcdRec &ucd_rec(u32 cp)
{
    return UCD_RECS[trie(UCD_REC_1, UCD_REC_2, UCD_REC_3, UCD_REC_SHIFT1, UCD_REC_SHIFT2, cp)];
}

Str ucd_category(u32 cp)
{
    return CATEGORIES[ucd_rec(cp).cat];
}

Str ucd_bidirectional(u32 cp)
{
    return BIDI[ucd_rec(cp).bidi];
}

Str ucd_east_asian_width(u32 cp)
{
    return WIDTHS[ucd_rec(cp).width];
}

u8 ucd_combining(u32 cp)
{
    return ucd_rec(cp).comb;
}

int ucd_decimal(u32 cp)
{
    const UcdRec &r = ucd_rec(cp);
    return (r.flags & UCD_DECIMAL) ? r.decimal : -1;
}

int ucd_digit(u32 cp)
{
    const UcdRec &r = ucd_rec(cp);
    return (r.flags & UCD_DIGIT) ? r.digit : -1;
}

bool ucd_numeric(u32 cp, f64 &out)
{
    const UcdRec &r = ucd_rec(cp);
    if (!(r.flags & UCD_NUMERIC))
        return false;
    out = UCD_NUMBERS[r.numeric];
    return true;
}

u32 ucd_lower(u32 cp)
{
    return u32(i64(cp) + case_of(cp).lower);
}

u32 ucd_upper(u32 cp)
{
    return u32(i64(cp) + case_of(cp).upper);
}

usize ucd_map(u32 cp, UcdMap how, u32 *out)
{
    const UcdCase &c = case_of(cp);
    u16 x            = 0;
    i32 d            = 0;
    switch (how) {
    case UcdMap::Lower:
        x = c.xlower, d = c.lower;
        break;
    case UcdMap::Upper:
        x = c.xupper, d = c.upper;
        break;
    case UcdMap::Title:
        x = c.xtitle, d = c.title;
        break;
    case UcdMap::Fold:
        x = c.xfold ? c.xfold : c.xlower, d = c.lower;
        break;
    }
    if (!x) {
        out[0] = u32(i64(cp) + d);
        return 1;
    }
    usize n = UCD_EXT[x];
    for (usize i = 0; i < n; i++)
        out[i] = UCD_EXT[x + 1 + i];
    return n;
}

bool ucd_decomposition(u32 cp, String &out)
{
    u32 at = dec_at(cp);
    if (!at)
        return true;
    u32 tag = 0;
    u32 parts[DEC_MAX];
    usize n = dec_read(at, tag, parts);
    if (tag && (!out.append(TAGS[tag]) || !out.push(' ')))
        return false;
    for (usize i = 0; i < n; i++)
        if ((i && !out.push(' ')) || !hex_put(out, parts[i], 4))
            return false;
    return true;
}

bool ucd_normalize(UcdForm form, Vec<u32> &cps)
{
    bool compat = form == UcdForm::NFKC || form == UcdForm::NFKD;
    Vec<u32> d;
    if (!d.reserve(cps.size()))
        return false;
    for (usize i = 0; i < cps.size(); i++)
        if (!decompose(cps[i], compat, d))
            return false;

    // Canonical order: a stable sort of each run of non-starters by class.
    for (usize i = 1; i < d.size(); i++) {
        u8 c = ucd_combining(d[i]);
        if (!c)
            continue;
        usize j = i;
        while (j > 0 && ucd_combining(d[j - 1]) > c) {
            u32 t    = d[j];
            d[j]     = d[j - 1];
            d[j - 1] = t;
            j--;
        }
    }

    cps.clear();
    if (form == UcdForm::NFD || form == UcdForm::NFKD) {
        for (usize i = 0; i < d.size(); i++)
            if (!cps.push(d[i]))
                return false;
        return true;
    }

    // Canonical composition, UAX #15.
    usize starter = 0;
    bool have     = false;
    u32 last      = 0;
    for (usize i = 0; i < d.size(); i++) {
        u32 ch = d[i];
        u8 cls = ucd_combining(ch);
        if (have && (last == 0 ? cps.size() == starter + 1 : last < cls)) {
            u32 c = compose(cps[starter], ch);
            if (c) {
                cps[starter] = c;
                continue;
            }
        }
        if (!cls) {
            starter = cps.size();
            have    = true;
        }
        last = cls;
        if (!cps.push(ch))
            return false;
    }
    return true;
}

bool ucd_name(u32 cp, String &out)
{
    if (cp >= S_BASE && cp < S_BASE + S_COUNT) {
        u32 s = cp - S_BASE;
        return out.append("HANGUL SYLLABLE ") && out.append(JAMO_L[s / N_COUNT]) &&
               out.append(JAMO_V[(s % N_COUNT) / T_COUNT]) && out.append(JAMO_T[s % T_COUNT]);
    }
    for (usize i = 0; i < ALGOS; i++)
        if (cp >= UCD_ALGO[i * 3] && cp <= UCD_ALGO[i * 3 + 1] && (UCD_ALGO[i * 3 + 2] & 0x100))
            return out.append(UCD_ALGO_PREFIX[UCD_ALGO[i * 3 + 2] & 0xff]) && hex_put(out, cp, 4);
    i32 n = name_number(cp);
    return n >= 0 && name_text(u32(n), out);
}

usize ucd_longest_name()
{
    return UCD_LONGEST_NAME;
}

bool ucd_lookup(Str given, bool sequences, Vec<u32> &out)
{
    out.clear();
    if (given.size() > UCD_LONGEST_NAME)
        return false;
    char buf[UCD_LONGEST_NAME + 1];
    for (usize i = 0; i < given.size(); i++) {
        char c = given[i];
        buf[i] = c >= 'a' && c <= 'z' ? char(c - 32) : c;
    }
    Str name(buf, given.size());

    if (name.starts_with("HANGUL SYLLABLE ")) {
        Str rest = name.substr(16);
        int l, v, t;
        rest = rest.substr(jamo(rest, JAMO_L, L_COUNT, l));
        rest = rest.substr(jamo(rest, JAMO_V, V_COUNT, v));
        rest = rest.substr(jamo(rest, JAMO_T, T_COUNT, t));
        if (l < 0 || v < 0 || t < 0 || !rest.empty())
            return false;
        return out.push(S_BASE + (u32(l) * V_COUNT + u32(v)) * T_COUNT + u32(t));
    }

    for (usize i = 0; i < ALGOS; i++) {
        Str p = UCD_ALGO_PREFIX[UCD_ALGO[i * 3 + 2] & 0xff];
        if (!name.starts_with(p))
            continue;
        Str hex = name.substr(p.size());
        u32 v   = 0;
        if (hex.empty() || hex.size() > 6)
            return false;
        for (usize k = 0; k < hex.size(); k++) {
            char c = hex[k];
            int d  = c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            if (d < 0)
                return false;
            v = v * 16 + u32(d);
        }
        String canon;
        if (!hex_put(canon, v, 4) || canon.str() != hex)
            return false;
        for (usize k = 0; k < ALGOS; k++)
            if ((UCD_ALGO[k * 3 + 2] & 0xff) == (UCD_ALGO[i * 3 + 2] & 0xff) &&
                v >= UCD_ALGO[k * 3] && v <= UCD_ALGO[k * 3 + 1])
                return out.push(v);
        return false;
    }

    Index *x = built();
    if (!x)
        return false;
    String cand;
    for (u32 at = name_hash(name) & x->mask; x->slots[at]; at = (at + 1) & x->mask) {
        u32 n = x->slots[at] - 1u;
        cand.clear();
        if (!name_text(n, cand))
            return false;
        if (cand.str() == name)
            return out.push(number_cp(n));
    }

    constexpr usize NAL = sizeof(UCD_ALIASES) / sizeof(UCD_ALIASES[0]) / 3;
    for (usize i = 0; i < NAL; i++)
        if (same_blind(name, UCD_EXTRA_TEXT + UCD_ALIASES[i * 3], UCD_ALIASES[i * 3 + 1]))
            return out.push(UCD_ALIASES[i * 3 + 2]);

    if (!sequences)
        return false;
    constexpr usize NSEQ = sizeof(UCD_SEQS) / sizeof(UCD_SEQS[0]) / 4;
    for (usize i = 0; i < NSEQ; i++) {
        if (!same_blind(name, UCD_EXTRA_TEXT + UCD_SEQS[i * 4], UCD_SEQS[i * 4 + 1]))
            continue;
        u32 k = UCD_SEQS[i * 4 + 2], end = k + UCD_SEQS[i * 4 + 3];
        while (k < end) {
            u32 u = UCD_SEQ_UNITS[k++];
            if (u >= 0xd800 && u < 0xdc00)
                u = 0x10000 + ((u - 0xd800) << 10) + (UCD_SEQ_UNITS[k++] - 0xdc00);
            if (!out.push(u))
                return false;
        }
        return true;
    }
    return false;
}

bool ucd_ascii_number(Str text, String &out)
{
    for (usize i = 0; i < text.size();) {
        u8 c = u8(text[i]);
        if (c < 0x80) {
            if (!out.push(char(c)))
                return false;
            i++;
            continue;
        }
        u32 cp = 0;
        i += cp_decode(text, i, cp);
        const UcdRec &r = ucd_rec(cp);
        char put        = '?';
        if (r.flags & UCD_SPACE)
            put = ' ';
        else if (r.flags & UCD_DECIMAL)
            put = char('0' + r.decimal);
        if (!out.push(put))
            return false;
    }
    return true;
}
