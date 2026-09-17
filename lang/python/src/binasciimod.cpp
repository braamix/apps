// `binascii`: conversions between octets and ASCII.
//
// Jack Jansen's Modules/binascii.c, kept in its own shape: uuencode, base64,
// base32, Ascii85 and Base85, hex, quoted-printable and the two CRCs. What a
// buffer is here is posix.h's and module.h's; the error is binascii.Error.
#include "bigint.h"
#include "gc.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "posix.h"
#include "ustr.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

struct Home {
    Value error;      // binascii.Error
    Value incomplete; // binascii.Incomplete
    Value cache;      // an alphabet's reverse table, by the alphabet
};

Home *home;

void home_mark()
{
    if (!home)
        return;
    gc_mark(home->error);
    gc_mark(home->incomplete);
    gc_mark(home->cache);
}

R fail(Str msg)
{
    return mod_raise(home->error, msg);
}

const u8 TABLE_A2B_BASE64[256] = {
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 62,  255, 255, 255, 63,  52,  53,  54,  55,  56,  57,  58,  59,  60,
    61,  255, 255, 255, 64,  255, 255, 255, 0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,
    11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  255, 255, 255, 255,
    255, 255, 26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,
    43,  44,  45,  46,  47,  48,  49,  50,  51,  255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255,
};

constexpr u8 BASE64_PAD = '=';

constexpr char TABLE_B2A_BASE64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

constexpr char TABLE_B2A_BASE85[] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz!#$%&()*+-;<=>?@^_`{|}~";

constexpr char TABLE_B2A_BASE85_A85[] =
    "!\"#$%&'()*+,-./0123456789:;<=>?@"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstu";

constexpr u8 BASE85_A85_PREFIX = '<';
constexpr u8 BASE85_A85_AFFIX  = '~';
constexpr u8 BASE85_A85_SUFFIX = '>';
constexpr u32 BASE85_A85_Z     = 0x00000000;
constexpr u32 BASE85_A85_Y     = 0x20202020;

constexpr u32 POW85[] = { 1, 85, 7225, 614125, 52200625 };

constexpr char TABLE_B2A_BASE32[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

constexpr u8 BASE32_PAD = '=';

// The decoding tables of the two base85 dialects and base32, built once from
// the encoding ones.
u8 table_a2b_base85[256];
u8 table_a2b_base85_a85[256];
u8 table_a2b_base32[256];

void reverse(u8 *out, const char *alphabet, u32 size, i32 pad)
{
    for (u32 i = 0; i < 256; i++)
        out[i] = 255;
    for (u32 i = 0; i < size; i++)
        out[u8(alphabet[i])] = u8(i);
    if (pad >= 0)
        out[pad] = u8(size);
}

constexpr u16 CRCTAB_HQX[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7, 0x8108, 0x9129, 0xa14a, 0xb16b,
    0xc18c, 0xd1ad, 0xe1ce, 0xf1ef, 0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
    0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de, 0x2462, 0x3443, 0x0420, 0x1401,
    0x64e6, 0x74c7, 0x44a4, 0x5485, 0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4, 0xb75b, 0xa77a, 0x9719, 0x8738,
    0xf7df, 0xe7fe, 0xd79d, 0xc7bc, 0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b, 0x5af5, 0x4ad4, 0x7ab7, 0x6a96,
    0x1a71, 0x0a50, 0x3a33, 0x2a12, 0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
    0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41, 0xedae, 0xfd8f, 0xcdec, 0xddcd,
    0xad2a, 0xbd0b, 0x8d68, 0x9d49, 0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
    0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78, 0x9188, 0x81a9, 0xb1ca, 0xa1eb,
    0xd10c, 0xc12d, 0xf14e, 0xe16f, 0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e, 0x02b1, 0x1290, 0x22f3, 0x32d2,
    0x4235, 0x5214, 0x6277, 0x7256, 0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
    0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405, 0xa7db, 0xb7fa, 0x8799, 0x97b8,
    0xe75f, 0xf77e, 0xc71d, 0xd73c, 0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab, 0x5844, 0x4865, 0x7806, 0x6827,
    0x18c0, 0x08e1, 0x3882, 0x28a3, 0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
    0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92, 0xfd2e, 0xed0f, 0xdd6c, 0xcd4d,
    0xbdaa, 0xad8b, 0x9de8, 0x8dc9, 0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
    0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8, 0x6e17, 0x7e36, 0x4e55, 0x5e74,
    0x2e93, 0x3eb2, 0x0ed1, 0x1ef0,
};

// ------------------------------------------------------------- arguments

// ascii_buffer: an ASCII str, or anything with a buffer.
bool ascii_buffer(Value v, Str &out)
{
    if (is_str(v)) {
        if (!(v.obj()->flags & OBJ_ASCII))
            return err_set("ValueError", "string argument should contain only ASCII characters") ==
                   R::Ok;
        out = str_of(v)->str();
        return true;
    }
    if (buffer_like(v, out))
        return true;
    Buf<128> b;
    b.put("argument should be bytes, buffer or ASCII string, not '").put(type_name(v)).put("'");
    return err_set("TypeError", b.str()) == R::Ok;
}

// Py_buffer: anything with a buffer.
bool data_buffer(Value v, Str who, Str &out)
{
    if (buffer_like(v, out))
        return true;
    if (err_pending())
        return false;
    Buf<128> b;
    if (is_str(v))
        b.put(who).put("() argument 1 must be bytes-like object, not str");
    else
        b.put("a bytes-like object is required, not '").put(type_name(v)).put("'");
    return err_set("TypeError", b.str()) == R::Ok;
}

// A keyword argument that is a buffer: `what` names it in the message.
bool kw_buffer(Value v, Str who, Str what, Str &out)
{
    if (buffer_like(v, out))
        return true;
    if (err_pending())
        return false;
    Buf<160> b;
    b.put(who).put("() argument '").put(what).put("' must be a bytes-like object, not ");
    b.put(type_name(v));
    return err_set("TypeError", b.str()) == R::Ok;
}

bool kw_bool(Value v, bool dflt)
{
    return v.is_nil() ? dflt : py_truth(v);
}

// size_t: a non-negative integer.
bool kw_size(Value v, u64 &out)
{
    out = 0;
    if (v.is_nil())
        return true;
    if (is_float(v))
        return err_set("TypeError", "'float' object cannot be interpreted as an integer") == R::Ok;
    if (!is_intval(v))
        return err_not_index(v) == R::Ok;
    i64 n = 0;
    if (!int_to_i64(v, n)) {
        if (is_big(v) && big_of(v)->neg)
            return err_set("ValueError", "Cannot convert negative int") == R::Ok;
        // A size_t is 64 bits in CPython, so only past that is too large.
        if (big_of(v)->len > 2)
            return err_set("OverflowError", "Python int too large for C size_t") == R::Ok;
        n = 0x7fffffffffffffffll;
    }
    if (n < 0)
        return err_set("ValueError", "Cannot convert negative int") == R::Ok;
    out = u64(n);
    return true;
}

// The positional-only data and the keyword-only rest.
bool take(const CallArgs &a, Str who, const Str *names, u32 n, u32 npos, Value *out)
{
    if (a.nargs > npos) {
        char t1[24], t2[24];
        Buf<160> b;
        b.put(who).put("() takes ").put(npos == 1 ? Str("exactly ") : Str("at most "));
        b.put(int_text(t1, sizeof t1, i64(npos))).put(" positional argument");
        b.put(npos == 1 ? Str("") : Str("s")).put(" (").put(int_text(t2, sizeof t2, i64(a.nargs)));
        b.put(" given)");
        return err_set("TypeError", b.str()) == R::Ok;
    }
    for (u32 k = 0; k < a.nkw; k++)
        for (u32 i = 0; i < npos && i < n; i++)
            if (is_str(a.kwnames[k]) && str_of(a.kwnames[k])->str() == names[i]) {
                Buf<160> b;
                b.put(who).put("() got some positional-only arguments passed as keyword ");
                b.put("arguments: '").put(names[i]).put("'");
                return err_set("TypeError", b.str()) == R::Ok;
            }
    return fn_take(a, who, names, n, npos, out);
}

template <usize N>
bool take(const CallArgs &a, Str who, const Str (&names)[N], u32 npos, Value (&out)[N])
{
    return take(a, who, names, N, npos, out);
}

// The reverse table for an alphabet the program gave, cached by it.
bool reverse_table(Value alphabet, u32 size, i32 pad, Str &out)
{
    if (!is_bytes(alphabet)) {
        Buf<96> b;
        b.put("alphabet must be bytes, not ").put(type_name(alphabet));
        return err_set("TypeError", b.str()) == R::Ok;
    }
    Str s = static_cast<BytesObj *>(alphabet.obj())->str();
    if (s.size() != size) {
        char tmp[24];
        Buf<64> b;
        b.put("alphabet must have length ").put(int_text(tmp, sizeof tmp, i64(size)));
        return err_set("ValueError", b.str()) == R::Ok;
    }
    Value got;
    R r = dict_get(static_cast<DictObj *>(home->cache.obj()), alphabet, got);
    if (r == R::Err)
        return false;
    if (r == R::NotImpl) {
        u8 t[256];
        for (u32 i = 0; i < 256; i++)
            t[i] = 255;
        for (u32 i = 0; i < size; i++)
            t[u8(s[i])] = u8(i);
        if (pad >= 0)
            t[pad] = u8(size);
        Root rt{ bytes_new(Str(reinterpret_cast<const char *>(t), 256)) };
        if (rt.v.is_nil() ||
            dict_set(static_cast<DictObj *>(home->cache.obj()), alphabet, rt.v) != R::Ok)
            return false;
        got = rt.v;
    }
    out = static_cast<BytesObj *>(got.obj())->str();
    return true;
}

using IgnoreCache = u8[32];

bool ignorechar(u8 c, Str ignore, IgnoreCache cache)
{
    if (ignore.empty())
        return false;
    if (cache[c >> 3] & (1 << (c & 7)))
        return true;
    for (usize i = 0; i < ignore.size(); i++)
        if (u8(ignore[i]) == c) {
            cache[c >> 3] |= u8(1 << (c & 7));
            return true;
        }
    return false;
}

// '\n' every `width` characters, moving the data right; the room is there.
usize wraplines(u8 *data, usize size, usize width)
{
    if (size <= width)
        return size;
    u8 *src        = data + size;
    usize newlines = (size - 1) / width;
    usize line_len = size - newlines * width;
    size += newlines;
    u8 *dst = data + size;
    while ((src -= line_len) != data) {
        dst -= line_len;
        for (usize i = line_len; i-- > 0;)
            dst[i] = src[i];
        *--dst   = '\n';
        line_len = width;
    }
    return size;
}

// The output: `n` octets of room, made a bytes of `used` at the end.
struct Out {
    String s;

    // One octet more than asked, so an empty result still has a pointer.
    u8 *start(usize n)
    {
        if (!string_sized(s, n + 1))
            return nullptr;
        return reinterpret_cast<u8 *>(s.data());
    }

    R finish(u8 *end, Value &out)
    {
        usize used = usize(end - reinterpret_cast<u8 *>(s.data()));
        out        = bytes_new(Str(s.data(), used));
        return out.is_nil() ? R::Err : R::Ok;
    }
};

// ------------------------------------------------------------------ uu

R b_a2b_uu(const CallArgs &a, Value &out)
{
    Str d;
    if (!args_only(a, "a2b_uu", 1, 1) || !ascii_buffer(a.args[0], d))
        return R::Err;
    const u8 *ascii_data = reinterpret_cast<const u8 *>(d.data());
    i64 ascii_len        = i64(d.size());
    i32 leftbits         = 0;
    u32 leftchar         = 0;
    if (ascii_len == 0)
        return fail("Missing length byte");
    i64 bin_len = (*ascii_data++ - ' ') & 077;
    ascii_len--;
    Out o;
    u8 *bin_data = o.start(usize(bin_len));
    if (!bin_data)
        return oom();
    for (; bin_len > 0; ascii_len--, ascii_data++) {
        u8 this_ch = ascii_len > 0 ? *ascii_data : 0;
        if (this_ch == '\n' || this_ch == '\r' || ascii_len <= 0) {
            this_ch = 0;
        } else {
            if (this_ch < ' ' || this_ch > (' ' + 64))
                return fail("Illegal char");
            this_ch = (this_ch - ' ') & 077;
        }
        leftchar = (leftchar << 6) | this_ch;
        leftbits += 6;
        if (leftbits >= 8) {
            leftbits -= 8;
            *bin_data++ = u8((leftchar >> leftbits) & 0xff);
            leftchar &= (1u << leftbits) - 1;
            bin_len--;
        }
    }
    while (ascii_len-- > 0) {
        u8 this_ch = *ascii_data++;
        if (this_ch != ' ' && this_ch != ' ' + 64 && this_ch != '\n' && this_ch != '\r')
            return fail("Trailing garbage");
    }
    return o.finish(bin_data, out);
}

R b_b2a_uu(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "data", "backtick" };
    Value v[2];
    Str d;
    if (!take(a, "b2a_uu", NAMES, 1, v) || !data_buffer(v[0], "b2a_uu", d))
        return R::Err;
    bool backtick      = kw_bool(v[1], false);
    const u8 *bin_data = reinterpret_cast<const u8 *>(d.data());
    i64 bin_len        = i64(d.size());
    i32 leftbits       = 0;
    u32 leftchar       = 0;
    if (bin_len > 45)
        return fail("At most 45 bytes at once");
    Out o;
    u8 *ascii_data = o.start(usize(2 + (bin_len + 2) / 3 * 4));
    if (!ascii_data)
        return oom();
    if (backtick && !bin_len)
        *ascii_data++ = '`';
    else
        *ascii_data++ = u8(' ' + bin_len);
    for (; bin_len > 0 || leftbits != 0; bin_len--, bin_data++) {
        if (bin_len > 0)
            leftchar = (leftchar << 8) | *bin_data;
        else
            leftchar <<= 8;
        leftbits += 8;
        while (leftbits >= 6) {
            u8 this_ch = (leftchar >> (leftbits - 6)) & 0x3f;
            leftbits -= 6;
            if (backtick && !this_ch)
                *ascii_data++ = '`';
            else
                *ascii_data++ = u8(this_ch + ' ');
        }
    }
    *ascii_data++ = '\n';
    return o.finish(ascii_data, out);
}

// ---------------------------------------------------------------- base64

R b_a2b_base64(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "data",     "strict_mode", "padded",
                                 "alphabet", "ignorechars", "canonical" };
    Value v[6];
    Str d;
    if (!take(a, "a2b_base64", NAMES, 1, v) || !ascii_buffer(v[0], d))
        return R::Err;
    Str ignore;
    bool have_ignore = !v[4].is_nil();
    if (have_ignore && !kw_buffer(v[4], "a2b_base64", "ignorechars", ignore))
        return R::Err;
    bool strict_mode = v[1].is_nil() ? have_ignore : py_truth(v[1]);
    bool padded      = kw_bool(v[2], true);
    bool canonical   = kw_bool(v[5], false);
    const u8 *table  = TABLE_A2B_BASE64;
    Str rev;
    if (!v[3].is_nil()) {
        if (!reverse_table(v[3], 64, BASE64_PAD, rev))
            return R::Err;
        table = reinterpret_cast<const u8 *>(rev.data());
    }
    if (!strict_mode)
        ignore = Str();
    IgnoreCache cache = {};

    const u8 *ascii_data = reinterpret_cast<const u8 *>(d.data());
    usize ascii_len      = d.size();
    Out o;
    u8 *const bin_start = o.start((ascii_len + 3) / 4 * 3);
    if (!bin_start)
        return oom();
    u8 *bin_data = bin_start;

    i32 quad_pos = 0;
    u8 leftchar  = 0;
    i32 pads     = 0;
    for (; ascii_len; ascii_data++, ascii_len--) {
        u8 this_ch = *ascii_data;
        if (padded && this_ch == BASE64_PAD) {
            pads++;
            if (quad_pos >= 2 && quad_pos + pads <= 4)
                continue;
            if (!strict_mode || ignorechar(BASE64_PAD, ignore, cache))
                continue;
            if (quad_pos == 1)
                break;
            return fail(quad_pos == 0 && bin_data == bin_start ? Str("Leading padding not allowed")
                                                               : Str("Excess padding not allowed"));
        }
        u8 c = table[this_ch];
        if (c >= 64) {
            if (strict_mode && !ignorechar(this_ch, ignore, cache))
                return fail(this_ch == BASE64_PAD ? Str("Padding not allowed")
                                                  : Str("Only base64 data is allowed"));
            continue;
        }
        if (pads && strict_mode && !ignorechar(BASE64_PAD, ignore, cache))
            return fail(quad_pos + pads == 4 ? Str("Excess data after padding")
                                             : Str("Discontinuous padding not allowed"));
        pads = 0;
        switch (quad_pos) {
        case 0:
            quad_pos = 1;
            leftchar = c;
            break;
        case 1:
            quad_pos    = 2;
            *bin_data++ = u8((leftchar << 2) | (c >> 4));
            leftchar    = c & 0x0f;
            break;
        case 2:
            quad_pos    = 3;
            *bin_data++ = u8((leftchar << 4) | (c >> 2));
            leftchar    = c & 0x03;
            break;
        case 3:
            quad_pos    = 0;
            *bin_data++ = u8((leftchar << 6) | c);
            leftchar    = 0;
            break;
        }
    }

    if (quad_pos == 1) {
        char tmp[24];
        Buf<160> b;
        b.put("Invalid base64-encoded string: number of data characters (");
        b.put(int_text(tmp, sizeof tmp, i64((bin_data - bin_start) / 3 * 4 + 1)));
        b.put(") cannot be 1 more than a multiple of 4");
        return fail(b.str());
    }
    if (padded && quad_pos != 0 && quad_pos + pads < 4)
        return fail("Incorrect padding");
    if (canonical && leftchar != 0)
        return fail("Non-zero padding bits");
    return o.finish(bin_data, out);
}

R b_b2a_base64(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "data", "padded", "wrapcol", "newline", "alphabet" };
    Value v[5];
    Str d;
    u64 wrapcol = 0;
    if (!take(a, "b2a_base64", NAMES, 1, v) || !data_buffer(v[0], "b2a_base64", d) ||
        !kw_size(v[2], wrapcol))
        return R::Err;
    bool padded       = kw_bool(v[1], true);
    bool newline      = kw_bool(v[3], true);
    const char *table = TABLE_B2A_BASE64;
    Str alpha;
    if (!v[4].is_nil()) {
        if (!kw_buffer(v[4], "b2a_base64", "alphabet", alpha))
            return R::Err;
        if (alpha.size() != 64)
            return err_set("ValueError", "alphabet must have length 64");
        table = alpha.data();
    }
    const u8 *bin_data = reinterpret_cast<const u8 *>(d.data());
    u64 bin_len        = d.size();
    u64 out_len        = (bin_len + 2) / 3 * 4;
    u32 pads           = u32((3 - (bin_len % 3)) % 3 * 4 / 3);
    if (!padded) {
        out_len -= pads;
        pads = 0;
    }
    if (wrapcol && out_len) {
        wrapcol = wrapcol < 4 ? 4 : wrapcol / 4 * 4;
        out_len += (out_len - 1) / wrapcol;
    }
    if (newline)
        out_len++;
    if (out_len > 0x7fffffff)
        return fail("Too much data for base64");
    Out o;
    u8 *const start = o.start(usize(out_len));
    if (!start)
        return oom();
    u8 *ascii_data = start;
    for (; bin_len >= 3; bin_len -= 3, bin_data += 3) {
        u32 c         = (u32(bin_data[0]) << 16) | (u32(bin_data[1]) << 8) | bin_data[2];
        ascii_data[0] = u8(table[(c >> 18) & 0x3f]);
        ascii_data[1] = u8(table[(c >> 12) & 0x3f]);
        ascii_data[2] = u8(table[(c >> 6) & 0x3f]);
        ascii_data[3] = u8(table[c & 0x3f]);
        ascii_data += 4;
    }
    if (bin_len == 1) {
        u32 c         = bin_data[0];
        *ascii_data++ = u8(table[(c >> 2) & 0x3f]);
        *ascii_data++ = u8(table[(c << 4) & 0x3f]);
    } else if (bin_len == 2) {
        u32 c         = (u32(bin_data[0]) << 8) | bin_data[1];
        *ascii_data++ = u8(table[(c >> 10) & 0x3f]);
        *ascii_data++ = u8(table[(c >> 4) & 0x3f]);
        *ascii_data++ = u8(table[(c << 2) & 0x3f]);
    }
    for (; pads; pads--)
        *ascii_data++ = BASE64_PAD;
    if (wrapcol)
        ascii_data = start + wraplines(start, usize(ascii_data - start), usize(wrapcol));
    if (newline)
        *ascii_data++ = '\n';
    return o.finish(ascii_data, out);
}

// ------------------------------------------------------------ Ascii85

R b_a2b_ascii85(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "data", "foldspaces", "adobe", "ignorechars", "canonical" };
    Value v[5];
    Str d, ignore;
    if (!take(a, "a2b_ascii85", NAMES, 1, v) || !ascii_buffer(v[0], d))
        return R::Err;
    if (!v[3].is_nil() && !kw_buffer(v[3], "a2b_ascii85", "ignorechars", ignore))
        return R::Err;
    bool foldspaces      = kw_bool(v[1], false);
    bool adobe           = kw_bool(v[2], false);
    bool canonical       = kw_bool(v[4], false);
    const u8 *ascii_data = reinterpret_cast<const u8 *>(d.data());
    i64 ascii_len        = i64(d.size());

    if (adobe) {
        if (ascii_len < 2 || ascii_data[ascii_len - 2] != BASE85_A85_AFFIX ||
            ascii_data[ascii_len - 1] != BASE85_A85_SUFFIX)
            return fail("Ascii85 encoded byte sequences must end with b'~>'");
        ascii_len -= 2;
        if (ascii_len >= 2 && ascii_data[0] == BASE85_A85_PREFIX &&
            ascii_data[1] == BASE85_A85_AFFIX) {
            ascii_data += 2;
            ascii_len -= 2;
        }
    }
    IgnoreCache cache = {};

    u64 count_yz = 0;
    u8 this_ch   = 0;
    for (i64 i = 0; i < ascii_len; i++)
        if (ascii_data[i] == 'y' || ascii_data[i] == 'z')
            count_yz++;
    u64 bin_len = (u64(ascii_len) - count_yz + 4) / 5 * 4 + count_yz * 4;
    if (bin_len > 0x7fffffff)
        return fail("Too much Ascii85 data");
    Out o;
    u8 *bin_data = o.start(usize(bin_len));
    if (!bin_data)
        return oom();

    u32 leftchar  = 0;
    i32 group_pos = 0;
    bool from_z   = false;
    for (; ascii_len > 0 || group_pos != 0; ascii_len--, ascii_data++) {
        u8 this_digit;
        if (ascii_len > 0) {
            this_ch    = *ascii_data;
            this_digit = table_a2b_base85_a85[this_ch];
        } else {
            this_digit = 84;
        }
        if (this_digit < 85) {
            if (group_pos == 4 &&
                (leftchar > 0xffffffffu / 85 || leftchar * 85 > 0xffffffffu - this_digit))
                return fail("Ascii85 overflow");
            leftchar = leftchar * 85 + this_digit;
            group_pos++;
        } else if ((this_ch == 'y' && foldspaces) || this_ch == 'z') {
            if (group_pos != 0) {
                Buf<64> b;
                b.put('\'').put(char(this_ch)).put("' inside Ascii85 5-tuple");
                return fail(b.str());
            }
            leftchar  = this_ch == 'y' ? BASE85_A85_Y : BASE85_A85_Z;
            from_z    = this_ch == 'z';
            group_pos = 5;
        } else if (!ignorechar(this_ch, ignore, cache)) {
            // %c of an octet: Latin-1 in the message.
            Buf<64> b;
            b.put("Non-Ascii85 digit found: ");
            char tmp[4];
            b.put(Str(tmp, cp_encode(this_ch, tmp)));
            return fail(b.str());
        }
        if (group_pos != 5)
            continue;
        i32 chunk_len = ascii_len < 1 ? 3 + i32(ascii_len) : 4;
        if (chunk_len == 0)
            return fail("Incomplete Ascii85 group");
        for (i32 i = 0; i < chunk_len; i++)
            *bin_data++ = u8((leftchar >> (24 - 8 * i)) & 0xff);
        if (canonical) {
            if (chunk_len == 4 && leftchar == 0 && !from_z)
                return fail("Non-canonical encoding, use 'z' for all-zero groups");
            if (chunk_len < 4) {
                i32 n_pad         = 4 - chunk_len;
                u32 canonical_top = (leftchar >> (n_pad * 8)) << (n_pad * 8);
                if (canonical_top / POW85[n_pad] != leftchar / POW85[n_pad])
                    return fail("Non-zero padding bits");
            }
        }
        from_z    = false;
        group_pos = 0;
        leftchar  = 0;
    }
    return o.finish(bin_data, out);
}

R b_b2a_ascii85(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "data", "foldspaces", "wrapcol", "pad", "adobe" };
    Value v[5];
    Str d;
    u64 wrapcol = 0;
    if (!take(a, "b2a_ascii85", NAMES, 1, v) || !data_buffer(v[0], "b2a_ascii85", d) ||
        !kw_size(v[2], wrapcol))
        return R::Err;
    bool foldspaces    = kw_bool(v[1], false);
    bool pad           = kw_bool(v[3], false);
    bool adobe         = kw_bool(v[4], false);
    const u8 *bin_data = reinterpret_cast<const u8 *>(d.data());
    i64 bin_len        = i64(d.size());
    if (adobe && wrapcol == 1)
        wrapcol = 2;
    u64 out_len = (u64(bin_len) + 3) / 4 * 5;
    if (adobe)
        out_len += 4;
    if (!pad && (bin_len % 4))
        out_len -= u64(4 - (bin_len % 4));
    if (wrapcol && out_len && out_len <= 0x7fffffff)
        out_len += (out_len - 1) / wrapcol;
    if (out_len > 0x7fffffff)
        return fail("Too much data for Ascii85");
    Out o;
    u8 *const start = o.start(usize(out_len));
    if (!start)
        return oom();
    u8 *ascii_data = start;
    if (adobe) {
        *ascii_data++ = BASE85_A85_PREFIX;
        *ascii_data++ = BASE85_A85_AFFIX;
    }
    for (; bin_len >= 4; bin_len -= 4, bin_data += 4) {
        u32 leftchar = (u32(bin_data[0]) << 24) | (u32(bin_data[1]) << 16) |
                       (u32(bin_data[2]) << 8) | bin_data[3];
        if (leftchar == BASE85_A85_Z) {
            *ascii_data++ = 'z';
        } else if (foldspaces && leftchar == BASE85_A85_Y) {
            *ascii_data++ = 'y';
        } else {
            for (i32 i = 4; i >= 0; i--) {
                ascii_data[i] = u8(TABLE_B2A_BASE85_A85[leftchar % 85]);
                leftchar /= 85;
            }
            ascii_data += 5;
        }
    }
    if (bin_len > 0) {
        u32 leftchar = 0;
        for (i64 i = 0; i < 4; i++) {
            leftchar <<= 8;
            if (i < bin_len)
                leftchar |= *bin_data++;
        }
        if (pad && leftchar == BASE85_A85_Z) {
            *ascii_data++ = 'z';
        } else {
            i64 group_len = pad ? 5 : bin_len + 1;
            for (i64 i = 4; i >= 0; i--) {
                if (i < group_len)
                    ascii_data[i] = u8(TABLE_B2A_BASE85_A85[leftchar % 85]);
                leftchar /= 85;
            }
            ascii_data += group_len;
        }
    }
    if (adobe) {
        *ascii_data++ = BASE85_A85_AFFIX;
        *ascii_data++ = BASE85_A85_SUFFIX;
    }
    if (wrapcol && out_len) {
        ascii_data = start + wraplines(start, usize(ascii_data - start), usize(wrapcol));
        if (adobe && ascii_data[-2] == '\n') {
            ascii_data[-3] = '\n';
            ascii_data[-2] = BASE85_A85_AFFIX;
        }
    }
    return o.finish(ascii_data, out);
}

// ------------------------------------------------------------- Base85

R b_a2b_base85(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "data", "alphabet", "ignorechars", "canonical" };
    Value v[4];
    Str d, ignore;
    if (!take(a, "a2b_base85", NAMES, 1, v) || !ascii_buffer(v[0], d))
        return R::Err;
    if (!v[2].is_nil() && !kw_buffer(v[2], "a2b_base85", "ignorechars", ignore))
        return R::Err;
    bool canonical  = kw_bool(v[3], false);
    const u8 *table = table_a2b_base85;
    Str rev;
    if (!v[1].is_nil()) {
        if (!reverse_table(v[1], 85, -1, rev))
            return R::Err;
        table = reinterpret_cast<const u8 *>(rev.data());
    }
    IgnoreCache cache    = {};
    const u8 *ascii_data = reinterpret_cast<const u8 *>(d.data());
    i64 total            = i64(d.size());
    i64 ascii_len        = total;
    Out o;
    u8 *bin_data = o.start(usize((u64(ascii_len) + 4) / 5 * 4));
    if (!bin_data)
        return oom();

    u32 leftchar  = 0;
    i32 group_pos = 0;
    for (; ascii_len > 0 || group_pos != 0; ascii_len--, ascii_data++) {
        u8 this_ch = 0;
        u8 this_digit;
        if (ascii_len > 0) {
            this_ch    = *ascii_data;
            this_digit = table[this_ch];
        } else {
            this_digit = 84;
        }
        if (this_digit < 85) {
            if (group_pos == 4 &&
                (leftchar > 0xffffffffu / 85 || leftchar * 85 > 0xffffffffu - this_digit)) {
                char tmp[24];
                Buf<96> b;
                b.put("Base85 overflow in hunk starting at byte ");
                b.put(int_text(tmp, sizeof tmp, (total - ascii_len) / 5 * 5));
                return fail(b.str());
            }
            leftchar = leftchar * 85 + this_digit;
            group_pos++;
        } else if (!ignorechar(this_ch, ignore, cache)) {
            char tmp[24];
            Buf<96> b;
            b.put("bad Base85 character at position ");
            b.put(int_text(tmp, sizeof tmp, total - ascii_len));
            return fail(b.str());
        }
        if (group_pos != 5)
            continue;
        i32 chunk_len = ascii_len < 1 ? 3 + i32(ascii_len) : 4;
        if (chunk_len == 0)
            return fail("Incomplete Base85 group");
        for (i32 i = 0; i < chunk_len; i++)
            *bin_data++ = u8((leftchar >> (24 - 8 * i)) & 0xff);
        if (canonical && chunk_len < 4) {
            i32 n_pad         = 4 - chunk_len;
            u32 canonical_top = (leftchar >> (n_pad * 8)) << (n_pad * 8);
            if (canonical_top / POW85[n_pad] != leftchar / POW85[n_pad])
                return fail("Non-zero padding bits");
        }
        group_pos = 0;
        leftchar  = 0;
    }
    return o.finish(bin_data, out);
}

R b_b2a_base85(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "data", "pad", "wrapcol", "alphabet" };
    Value v[4];
    Str d;
    u64 wrapcol = 0;
    if (!take(a, "b2a_base85", NAMES, 1, v) || !data_buffer(v[0], "b2a_base85", d) ||
        !kw_size(v[2], wrapcol))
        return R::Err;
    bool pad          = kw_bool(v[1], false);
    const char *table = TABLE_B2A_BASE85;
    Str alpha;
    if (!v[3].is_nil()) {
        if (!kw_buffer(v[3], "b2a_base85", "alphabet", alpha))
            return R::Err;
        if (alpha.size() != 85)
            return err_set("ValueError", "alphabet must have length 85");
        table = alpha.data();
    }
    const u8 *bin_data = reinterpret_cast<const u8 *>(d.data());
    i64 bin_len        = i64(d.size());
    u64 out_len        = (u64(bin_len) + 3) / 4 * 5;
    if (!pad && (bin_len % 4))
        out_len -= u64(4 - (bin_len % 4));
    if (wrapcol && out_len) {
        wrapcol = wrapcol < 5 ? 5 : wrapcol / 5 * 5;
        out_len += (out_len - 1) / wrapcol;
    }
    if (out_len > 0x7fffffff)
        return fail("Too much data for Base85");
    Out o;
    u8 *const start = o.start(usize(out_len));
    if (!start)
        return oom();
    u8 *ascii_data = start;
    for (; bin_len >= 4; bin_len -= 4, bin_data += 4) {
        u32 leftchar = (u32(bin_data[0]) << 24) | (u32(bin_data[1]) << 16) |
                       (u32(bin_data[2]) << 8) | bin_data[3];
        for (i32 i = 4; i >= 0; i--) {
            ascii_data[i] = u8(table[leftchar % 85]);
            leftchar /= 85;
        }
        ascii_data += 5;
    }
    if (bin_len > 0) {
        u32 leftchar = 0;
        for (i64 i = 0; i < 4; i++) {
            leftchar <<= 8;
            if (i < bin_len)
                leftchar |= *bin_data++;
        }
        i64 group_len = pad ? 5 : bin_len + 1;
        for (i64 i = 4; i >= 0; i--) {
            if (i < group_len)
                ascii_data[i] = u8(table[leftchar % 85]);
            leftchar /= 85;
        }
        ascii_data += group_len;
    }
    if (wrapcol && out_len)
        ascii_data = start + wraplines(start, usize(ascii_data - start), usize(wrapcol));
    return o.finish(ascii_data, out);
}

// ------------------------------------------------------------- base32

R b_a2b_base32(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "data", "padded", "alphabet", "ignorechars", "canonical" };
    Value v[5];
    Str d, ignore;
    if (!take(a, "a2b_base32", NAMES, 1, v) || !ascii_buffer(v[0], d))
        return R::Err;
    if (!v[3].is_nil() && !kw_buffer(v[3], "a2b_base32", "ignorechars", ignore))
        return R::Err;
    bool padded     = kw_bool(v[1], true);
    bool canonical  = kw_bool(v[4], false);
    const u8 *table = table_a2b_base32;
    Str rev;
    if (!v[2].is_nil()) {
        if (!reverse_table(v[2], 32, BASE32_PAD, rev))
            return R::Err;
        table = reinterpret_cast<const u8 *>(rev.data());
    }
    IgnoreCache cache    = {};
    const u8 *ascii_data = reinterpret_cast<const u8 *>(d.data());
    usize ascii_len      = d.size();
    Out o;
    u8 *const bin_start = o.start((ascii_len + 7) / 8 * 5);
    if (!bin_start)
        return oom();
    u8 *bin_data = bin_start;

    u8 leftchar  = 0;
    i32 octa_pos = 0;
    i32 pads     = 0;
    for (; ascii_len; ascii_len--, ascii_data++) {
        u8 this_ch = *ascii_data;
        if (padded && this_ch == BASE32_PAD) {
            pads++;
            if ((octa_pos == 2 || octa_pos == 4 || octa_pos == 5 || octa_pos == 7) &&
                octa_pos + pads <= 8)
                continue;
            if (ignorechar(BASE32_PAD, ignore, cache))
                continue;
            if (octa_pos == 1 || octa_pos == 3 || octa_pos == 6)
                break;
            return fail(octa_pos == 0 && bin_data == bin_start ? Str("Leading padding not allowed")
                                                               : Str("Excess padding not allowed"));
        }
        u8 c = table[this_ch];
        if (c >= 32) {
            if (!ignorechar(this_ch, ignore, cache))
                return fail(this_ch == BASE32_PAD ? Str("Padding not allowed")
                                                  : Str("Only base32 data is allowed"));
            continue;
        }
        if (pads && !ignorechar(BASE32_PAD, ignore, cache))
            return fail(octa_pos + pads == 8 ? Str("Excess data after padding")
                                             : Str("Discontinuous padding not allowed"));
        switch (octa_pos) {
        case 0:
            octa_pos = 1;
            leftchar = c;
            break;
        case 1:
            octa_pos    = 2;
            *bin_data++ = u8((leftchar << 3) | (c >> 2));
            leftchar    = c & 0x03;
            break;
        case 2:
            octa_pos = 3;
            leftchar = u8((leftchar << 5) | c);
            break;
        case 3:
            octa_pos    = 4;
            *bin_data++ = u8((leftchar << 1) | (c >> 4));
            leftchar    = c & 0x0f;
            break;
        case 4:
            octa_pos    = 5;
            *bin_data++ = u8((leftchar << 4) | (c >> 1));
            leftchar    = c & 0x01;
            break;
        case 5:
            octa_pos = 6;
            leftchar = u8((leftchar << 5) | c);
            break;
        case 6:
            octa_pos    = 7;
            *bin_data++ = u8((leftchar << 2) | (c >> 3));
            leftchar    = c & 0x07;
            break;
        case 7:
            // CPython's goto back to its fast path starts the next group clean.
            octa_pos    = 0;
            *bin_data++ = u8((leftchar << 5) | c);
            leftchar    = 0;
            pads        = 0;
            break;
        }
    }

    if (octa_pos == 1 || octa_pos == 3 || octa_pos == 6) {
        char tmp[24];
        Buf<160> b;
        b.put("Invalid base32-encoded string: number of data characters (");
        b.put(int_text(tmp, sizeof tmp, i64((bin_data - bin_start) / 5 * 8 + octa_pos)));
        b.put(") cannot be 1, 3, or 6 more than a multiple of 8");
        return fail(b.str());
    }
    if (padded && octa_pos != 0 && octa_pos + pads < 8)
        return fail("Incorrect padding");
    if (canonical && leftchar != 0)
        return fail("Non-zero padding bits");
    return o.finish(bin_data, out);
}

R b_b2a_base32(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "data", "padded", "wrapcol", "alphabet" };
    Value v[4];
    Str d;
    u64 wrapcol = 0;
    if (!take(a, "b2a_base32", NAMES, 1, v) || !data_buffer(v[0], "b2a_base32", d) ||
        !kw_size(v[2], wrapcol))
        return R::Err;
    bool padded       = kw_bool(v[1], true);
    const char *table = TABLE_B2A_BASE32;
    Str alpha;
    if (!v[3].is_nil()) {
        if (!kw_buffer(v[3], "b2a_base32", "alphabet", alpha))
            return R::Err;
        if (alpha.size() != 32)
            return err_set("ValueError", "alphabet must have length 32");
        table = alpha.data();
    }
    const u8 *bin_data = reinterpret_cast<const u8 *>(d.data());
    u64 bin_len        = d.size();
    u64 ascii_len      = (bin_len + 4) / 5 * 8;
    u32 pads           = u32((5 - (bin_len % 5)) % 5 * 8 / 5);
    if (!padded) {
        ascii_len -= pads;
        pads = 0;
    }
    if (wrapcol && ascii_len) {
        wrapcol = wrapcol < 8 ? 8 : wrapcol / 8 * 8;
        ascii_len += (ascii_len - 1) / wrapcol;
    }
    if (ascii_len > 0x7fffffff)
        return fail("Too much data for base32");
    Out o;
    u8 *const start = o.start(usize(ascii_len));
    if (!start)
        return oom();
    u8 *ascii_data = start;
    auto T         = [&](u64 x) { return u8(table[x & 0x1f]); };
    for (; bin_len >= 5; bin_len -= 5, bin_data += 5) {
        u64 c = (u64(bin_data[0]) << 32) | (u64(bin_data[1]) << 24) | (u64(bin_data[2]) << 16) |
                (u64(bin_data[3]) << 8) | bin_data[4];
        for (i32 i = 0; i < 8; i++)
            ascii_data[i] = T(c >> (35 - 5 * i));
        ascii_data += 8;
    }
    if (bin_len == 1) {
        u32 c         = bin_data[0];
        *ascii_data++ = T(c >> 3);
        *ascii_data++ = T(c << 2);
    } else if (bin_len == 2) {
        u32 c         = (u32(bin_data[0]) << 8) | bin_data[1];
        *ascii_data++ = T(c >> 11);
        *ascii_data++ = T(c >> 6);
        *ascii_data++ = T(c >> 1);
        *ascii_data++ = T(c << 4);
    } else if (bin_len == 3) {
        u32 c         = (u32(bin_data[0]) << 16) | (u32(bin_data[1]) << 8) | bin_data[2];
        *ascii_data++ = T(c >> 19);
        *ascii_data++ = T(c >> 14);
        *ascii_data++ = T(c >> 9);
        *ascii_data++ = T(c >> 4);
        *ascii_data++ = T(c << 1);
    } else if (bin_len == 4) {
        u32 c = (u32(bin_data[0]) << 24) | (u32(bin_data[1]) << 16) | (u32(bin_data[2]) << 8) |
                bin_data[3];
        *ascii_data++ = T(c >> 27);
        *ascii_data++ = T(c >> 22);
        *ascii_data++ = T(c >> 17);
        *ascii_data++ = T(c >> 12);
        *ascii_data++ = T(c >> 7);
        *ascii_data++ = T(c >> 2);
        *ascii_data++ = T(c << 3);
    }
    for (; pads; pads--)
        *ascii_data++ = BASE32_PAD;
    if (wrapcol && ascii_len)
        ascii_data = start + wraplines(start, usize(ascii_data - start), usize(wrapcol));
    return o.finish(ascii_data, out);
}

// ---------------------------------------------------------------- CRCs

// unsigned_int(bitwise=True): any integer, its low 32 bits. `wide` says it
// did not fit, which CPython warns about.
bool crc_arg(Value v, u32 &out, bool &wide)
{
    if (is_float(v))
        return err_set("TypeError", "'float' object cannot be interpreted as an integer") == R::Ok;
    if (!is_intval(v))
        return err_not_index(v) == R::Ok;
    if (v.is_int() || is_bool(v)) {
        i64 n = 0;
        as_index(v, n);
        out  = u32(n);
        wide = n < -i64(0x80000000) || n > 0xffffffffll;
        return true;
    }
    BigObj *b = big_of(v);
    u32 lo    = b->limbs()[0];
    out       = b->neg ? u32(0) - lo : lo;
    wide      = b->len > 1 || (b->neg && lo > 0x80000000u);
    return true;
}

R crc_answer(u32 crc, bool wide, Value &out)
{
    Root v{ int_from_i64(crc) };
    if (v.v.is_nil())
        return R::Err;
    if (wide)
        return warn_then("DeprecationWarning", "integer value out of range", 1, v.v, out);
    out = v.v;
    return R::Ok;
}

R b_crc_hqx(const CallArgs &a, Value &out)
{
    Str d;
    u32 crc   = 0;
    bool wide = false;
    if (!args_only(a, "crc_hqx", 2, 2) || !data_buffer(a.args[0], "crc_hqx", d) ||
        !crc_arg(a.args[1], crc, wide))
        return R::Err;
    crc &= 0xffff;
    for (usize i = 0; i < d.size(); i++)
        crc = ((crc << 8) & 0xff00) ^ CRCTAB_HQX[(crc >> 8) ^ u8(d[i])];
    return crc_answer(crc, wide, out);
}

u32 crc32_table[256];

u32 internal_crc32(Str d, u32 crc)
{
    crc = ~crc;
    for (usize i = 0; i < d.size(); i++)
        crc = crc32_table[(crc ^ u8(d[i])) & 0xff] ^ (crc >> 8);
    return crc ^ 0xffffffffu;
}

R b_crc32(const CallArgs &a, Value &out)
{
    Str d;
    u32 crc   = 0;
    bool wide = false;
    if (!args_only(a, "crc32", 1, 2) || !data_buffer(a.args[0], "crc32", d) ||
        (a.nargs > 1 && !crc_arg(a.args[1], crc, wide)))
        return R::Err;
    return crc_answer(internal_crc32(d, crc), wide, out);
}

// ----------------------------------------------------------------- hex

R hexlify(const CallArgs &a, Str who, Value &out)
{
    static const Str NAMES[] = { "data", "sep", "bytes_per_sep" };
    Value v[3];
    Str d;
    if (!fn_take(a, who, NAMES, 1, v) || !data_buffer(v[0], who, d))
        return R::Err;
    i64 per = 1;
    if (!v[2].is_nil()) {
        if (!is_intval(v[2]))
            return err_not_index(v[2]);
        if (!int_to_i64(v[2], per) || per > 2147483647 || per < -2147483648ll)
            return err_set("OverflowError", "Python int too large to convert to C int");
    }
    String b;
    if (!hex_with_sep(d, v[1], per, true, b))
        return R::Err;
    out = bytes_new(b.str());
    return out.is_nil() ? R::Err : R::Ok;
}

R b_b2a_hex(const CallArgs &a, Value &out)
{
    return hexlify(a, "b2a_hex", out);
}

R b_hexlify(const CallArgs &a, Value &out)
{
    return hexlify(a, "hexlify", out);
}

i32 digit_value(u8 c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 10;
    return 37;
}

R unhexlify(const CallArgs &a, Str who, Value &out)
{
    static const Str NAMES[] = { "hexstr", "ignorechars" };
    Value v[2];
    Str d, ignore;
    if (!take(a, who, NAMES, 1, v) || !ascii_buffer(v[0], d))
        return R::Err;
    if (!v[1].is_nil() && !kw_buffer(v[1], who, "ignorechars", ignore))
        return R::Err;
    IgnoreCache cache = {};
    Out o;
    u8 *bin_data = o.start(d.size() / 2);
    if (!bin_data)
        return oom();
    bool pair_pos = false;
    u8 leftchar   = 0;
    for (usize i = 0; i < d.size(); i++) {
        u8 this_ch = u8(d[i]);
        i32 digit  = digit_value(this_ch);
        if (digit >= 16) {
            if (!ignorechar(this_ch, ignore, cache))
                return fail("Non-hexadecimal digit found");
            continue;
        }
        if (!pair_pos) {
            pair_pos = true;
            leftchar = u8(digit);
        } else {
            pair_pos    = false;
            *bin_data++ = u8((leftchar << 4) | digit);
        }
    }
    if (pair_pos)
        return fail("Odd number of hexadecimal digits");
    return o.finish(bin_data, out);
}

R b_a2b_hex(const CallArgs &a, Value &out)
{
    return unhexlify(a, "a2b_hex", out);
}

R b_unhexlify(const CallArgs &a, Value &out)
{
    return unhexlify(a, "unhexlify", out);
}

// ----------------------------------------------------- quoted-printable

constexpr u32 MAXLINESIZE = 76;

bool is_hexch(u8 c)
{
    return (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f') || (c >= '0' && c <= '9');
}

R b_a2b_qp(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "data", "header" };
    Value v[2];
    Str d;
    if (!fn_take(a, "a2b_qp", NAMES, 1, v) || !ascii_buffer(v[0], d))
        return R::Err;
    bool header          = kw_bool(v[1], false);
    const u8 *ascii_data = reinterpret_cast<const u8 *>(d.data());
    usize datalen        = d.size();
    Out o;
    u8 *const odata = o.start(datalen);
    if (!odata)
        return oom();
    usize in = 0, n = 0;
    while (in < datalen) {
        if (ascii_data[in] == '=') {
            in++;
            if (in >= datalen)
                break;
            if (ascii_data[in] == '\n' || ascii_data[in] == '\r') {
                if (ascii_data[in] != '\n')
                    while (in < datalen && ascii_data[in] != '\n')
                        in++;
                if (in < datalen)
                    in++;
            } else if (ascii_data[in] == '=') {
                odata[n++] = '=';
                in++;
            } else if (in + 1 < datalen && is_hexch(ascii_data[in]) &&
                       is_hexch(ascii_data[in + 1])) {
                u8 ch = u8(digit_value(ascii_data[in]) << 4);
                in++;
                ch |= u8(digit_value(ascii_data[in]));
                in++;
                odata[n++] = ch;
            } else {
                odata[n++] = '=';
            }
        } else if (header && ascii_data[in] == '_') {
            odata[n++] = ' ';
            in++;
        } else {
            odata[n++] = ascii_data[in++];
        }
    }
    return o.finish(odata + n, out);
}

void to_hex(u8 ch, u8 *s)
{
    s[1] = u8("0123456789ABCDEF"[ch % 16]);
    s[0] = u8("0123456789ABCDEF"[(ch / 16) % 16]);
}

// Whether the octet at `in` has to be written as =XX.
bool qp_quoted(const u8 *databuf, usize datalen, usize in, u32 linelen, bool quotetabs, bool istext,
               bool header)
{
    u8 c = databuf[in];
    return c > 126 || c == '=' || (header && c == '_') ||
           (c == '.' && linelen == 0 &&
            (in + 1 == datalen || databuf[in + 1] == '\n' || databuf[in + 1] == '\r' ||
             databuf[in + 1] == 0)) ||
           (!istext && (c == '\r' || c == '\n')) ||
           ((c == '\t' || c == ' ') && in + 1 == datalen) ||
           (c < 33 && c != '\r' && c != '\n' && (quotetabs || (c != '\t' && c != ' ')));
}

R b_b2a_qp(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "data", "quotetabs", "istext", "header" };
    Value v[4];
    Str d;
    if (!fn_take(a, "b2a_qp", NAMES, 1, v) || !data_buffer(v[0], "b2a_qp", d))
        return R::Err;
    bool quotetabs    = kw_bool(v[1], false);
    bool istext       = kw_bool(v[2], true);
    bool header       = kw_bool(v[3], false);
    const u8 *databuf = reinterpret_cast<const u8 *>(d.data());
    usize datalen     = d.size();
    bool crlf         = false;
    for (usize i = 0; i < datalen; i++)
        if (databuf[i] == '\n') {
            crlf = i > 0 && databuf[i - 1] == '\r';
            break;
        }

    usize in = 0, odatalen = 0;
    u32 linelen = 0;
    while (in < datalen) {
        usize delta = 0;
        if (qp_quoted(databuf, datalen, in, linelen, quotetabs, istext, header)) {
            if (linelen + 3 >= MAXLINESIZE) {
                linelen = 0;
                delta += crlf ? 3 : 2;
            }
            linelen += 3;
            delta += 3;
            in++;
        } else if (istext && (databuf[in] == '\n' || (in + 1 < datalen && databuf[in] == '\r' &&
                                                      databuf[in + 1] == '\n'))) {
            linelen = 0;
            if (in && (databuf[in - 1] == ' ' || databuf[in - 1] == '\t'))
                delta += 2;
            delta += crlf ? 2 : 1;
            in += databuf[in] == '\r' ? 2 : 1;
        } else {
            if (in + 1 != datalen && databuf[in + 1] != '\n' && linelen + 1 >= MAXLINESIZE) {
                linelen = 0;
                delta += crlf ? 3 : 2;
            }
            linelen++;
            delta++;
            in++;
        }
        odatalen += delta;
    }

    Out o;
    u8 *const odata = o.start(odatalen);
    if (!odata)
        return oom();
    usize n = 0;
    in = linelen = 0;
    while (in < datalen) {
        if (qp_quoted(databuf, datalen, in, linelen, quotetabs, istext, header)) {
            if (linelen + 3 >= MAXLINESIZE) {
                odata[n++] = '=';
                if (crlf)
                    odata[n++] = '\r';
                odata[n++] = '\n';
                linelen    = 0;
            }
            odata[n++] = '=';
            to_hex(databuf[in], odata + n);
            n += 2;
            in++;
            linelen += 3;
        } else if (istext && (databuf[in] == '\n' || (in + 1 < datalen && databuf[in] == '\r' &&
                                                      databuf[in + 1] == '\n'))) {
            linelen = 0;
            if (n && (odata[n - 1] == ' ' || odata[n - 1] == '\t')) {
                u8 ch        = odata[n - 1];
                odata[n - 1] = '=';
                to_hex(ch, odata + n);
                n += 2;
            }
            if (crlf)
                odata[n++] = '\r';
            odata[n++] = '\n';
            in += databuf[in] == '\r' ? 2 : 1;
        } else {
            if (in + 1 != datalen && databuf[in + 1] != '\n' && linelen + 1 >= MAXLINESIZE) {
                odata[n++] = '=';
                if (crlf)
                    odata[n++] = '\r';
                odata[n++] = '\n';
                linelen    = 0;
            }
            linelen++;
            if (header && databuf[in] == ' ') {
                odata[n++] = '_';
                in++;
            } else {
                odata[n++] = databuf[in++];
            }
        }
    }
    return o.finish(odata + n, out);
}

constexpr ModDef DEFS[] = {
    { "a2b_uu", b_a2b_uu },           { "b2a_uu", b_b2a_uu },
    { "a2b_base64", b_a2b_base64 },   { "b2a_base64", b_b2a_base64 },
    { "b2a_ascii85", b_b2a_ascii85 }, { "a2b_ascii85", b_a2b_ascii85 },
    { "a2b_base85", b_a2b_base85 },   { "b2a_base85", b_b2a_base85 },
    { "a2b_base32", b_a2b_base32 },   { "b2a_base32", b_b2a_base32 },
    { "a2b_hex", b_a2b_hex },         { "b2a_hex", b_b2a_hex },
    { "hexlify", b_hexlify },         { "unhexlify", b_unhexlify },
    { "crc_hqx", b_crc_hqx },         { "crc32", b_crc32 },
    { "a2b_qp", b_a2b_qp },           { "b2a_qp", b_b2a_qp },
};

struct Alphabet {
    Str name;
    Str text;
};

constexpr Alphabet ALPHABETS[] = {
    { "BASE64_ALPHABET", Str(TABLE_B2A_BASE64, 64) },
    { "URLSAFE_BASE64_ALPHABET",
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_" },
    { "CRYPT_ALPHABET", "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz" },
    { "UU_ALPHABET", " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_" },
    { "BINHEX_ALPHABET", "!\"#$%&'()*+,-012345689@ABCDEFGHIJKLMNPQRSTUVXYZ[`abcdefhijklmpqr" },
    { "BASE85_ALPHABET", Str(TABLE_B2A_BASE85, 85) },
    { "ASCII85_ALPHABET", Str(TABLE_B2A_BASE85_A85, 85) },
    { "Z85_ALPHABET",
      "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.-:+=^!/*?&<>()[]{}@%$#" },
    { "BASE32_ALPHABET", Str(TABLE_B2A_BASE32, 32) },
    { "BASE32HEX_ALPHABET", "0123456789ABCDEFGHIJKLMNOPQRSTUV" },
};

} // namespace

bool binascii_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    if (!home) {
        home = heap_new<Home>();
        if (!home)
            return oom() == R::Ok;
        gc_root_hook(home_mark);
        reverse(table_a2b_base85, TABLE_B2A_BASE85, 85, -1);
        reverse(table_a2b_base85_a85, TABLE_B2A_BASE85_A85, 85, -1);
        reverse(table_a2b_base32, TABLE_B2A_BASE32, 32, BASE32_PAD);
        for (u32 n = 0; n < 256; n++) {
            u32 c = n;
            for (u32 k = 0; k < 8; k++)
                c = c & 1 ? 0xedb88320u ^ (c >> 1) : c >> 1;
            crc32_table[n] = c;
        }
    }
    if (home->error.is_nil()) {
        home->error = mod_exc_class("binascii", "Error", "ValueError");
        if (home->error.is_nil())
            return false;
    }
    if (home->incomplete.is_nil()) {
        home->incomplete = mod_exc_class("binascii", "Incomplete", "Exception");
        if (home->incomplete.is_nil())
            return false;
    }
    if (home->cache.is_nil()) {
        DictObj *c = dict_new();
        if (!c)
            return oom() == R::Ok;
        home->cache = obj_value(c);
    }
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    if (!mod_defs(d, DEFS) || !mod_put(d, "Error", home->error) ||
        !mod_put(d, "Incomplete", home->incomplete))
        return false;
    for (const Alphabet &al : ALPHABETS) {
        Root b{ bytes_new(al.text) };
        if (b.v.is_nil() || !mod_put(static_cast<DictObj *>(rd.v.obj()), al.name, b.v))
            return false;
    }
    return true;
}
