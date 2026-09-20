// The codec engine: every codec _codecs names, the built-in error handlers,
// and the run that parks when a handler of the program's own has to be
// called. The messages, the error ranges and the order things are checked in
// are CPython's, from Objects/unicodeobject.c and Python/codecs.c.
#include "codec.h"

#include "exc.h"
#include "gc.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "method.h"
#include "ops.h"
#include "ucd.h"
#include "ustr.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// A run in progress. The input is the str or bytes the exception names; the
// output is octets for an encoder and a str's bytes for a decoder.
struct CodecObj : Obj {
    Value input;
    Value errors;
    Value handler; // the program's own, once looked up
    Value exc;     // made at the first fault, and reused
    Value mapping;
    String out;
    usize pos;  // bytes into the input
    usize cpos; // an encoder's position in characters
    usize fault_start, fault_end;
    Str reason;
    Str encname;
    i32 byteorder;
    u32 bits, buffer, surrogate; // UTF-7's shift state
    usize shift_out, shift_in;
    Codec codec;
    ErrH err;
    u8 wrap;
    bool encode, final, in_shift, begun, done;
};

void codec_trace(Obj *o)
{
    CodecObj *c = static_cast<CodecObj *>(o);
    gc_mark(c->input);
    gc_mark(c->errors);
    gc_mark(c->handler);
    gc_mark(c->exc);
    gc_mark(c->mapping);
}

void codec_fini(Obj *o)
{
    static_cast<CodecObj *>(o)->out.~String();
}

constexpr Type codec_type{ .name = "codec", .trace = codec_trace, .fini = codec_fini };

CodecObj *co_of(Value v)
{
    return static_cast<CodecObj *>(v.obj());
}

Str in_bytes(const CodecObj *c)
{
    return c->encode ? str_of(c->input)->str() : static_cast<BytesObj *>(c->input.obj())->str();
}

usize in_chars(const CodecObj *c)
{
    return c->encode ? str_of(c->input)->chars : static_cast<BytesObj *>(c->input.obj())->len;
}

enum class Run : u8 { Done, Fault, Error };

Run fault(CodecObj *c, usize start, usize end, Str reason)
{
    c->fault_start = start;
    c->fault_end   = end;
    c->reason      = reason;
    return Run::Fault;
}

bool put_byte(CodecObj *c, u8 b)
{
    return c->out.push(char(b));
}

bool put_cp(CodecObj *c, u32 cp)
{
    return cp_append(c->out, cp);
}

bool little(Codec k, i32 byteorder)
{
    if (k == Codec::Utf16Le || k == Codec::Utf32Le)
        return true;
    if (k == Codec::Utf16Be || k == Codec::Utf32Be)
        return false;
    return byteorder <= 0;
}

bool put_unit(CodecObj *c, u32 u, usize width, bool le)
{
    for (usize k = 0; k < width; k++) {
        u8 b = u8(u >> (8 * (le ? k : width - 1 - k)));
        if (!put_byte(c, b))
            return false;
    }
    return true;
}

bool is_utf16(Codec k)
{
    return k == Codec::Utf16 || k == Codec::Utf16Le || k == Codec::Utf16Be;
}

bool is_utf32(Codec k)
{
    return k == Codec::Utf32 || k == Codec::Utf32Le || k == Codec::Utf32Be;
}

constexpr char HEX[] = "0123456789abcdef";

// ----------------------------------------------------------------- encoders

// A run of characters none of which `ok` takes, from cpos: its end.
template <typename F>
usize bad_run(const CodecObj *c, usize at, usize cpos, F ok)
{
    Str s    = in_bytes(c);
    usize to = cpos;
    while (at < s.size()) {
        u32 cp  = 0;
        usize w = cp_decode(s, at, cp);
        if (ok(cp))
            break;
        at += w;
        to++;
    }
    return to;
}

Run enc_utf8(CodecObj *c)
{
    Str s = in_bytes(c);
    while (c->pos < s.size()) {
        // Everything but a surrogate is already UTF-8.
        usize run = c->pos;
        while (run < s.size() &&
               !(u8(s[run]) == 0xed && run + 1 < s.size() && u8(s[run + 1]) >= 0xa0)) {
            usize w = cp_width(u8(s[run]));
            run += w;
            c->cpos++;
        }
        if (!c->out.append(s.substr(c->pos, run - c->pos)))
            return oom(), Run::Error;
        c->pos = run;
        if (run >= s.size())
            break;
        usize end = bad_run(c, c->pos, c->cpos, [](u32 cp) { return !is_surrogate(cp); });
        return fault(c, c->cpos, end, "surrogates not allowed");
    }
    return Run::Done;
}

Run enc_ucs1(CodecObj *c, u32 limit)
{
    Str s = in_bytes(c);
    while (c->pos < s.size()) {
        u32 cp  = 0;
        usize w = cp_decode(s, c->pos, cp);
        if (cp >= limit) {
            usize end = bad_run(c, c->pos, c->cpos, [limit](u32 x) { return x < limit; });
            return fault(
                c, c->cpos, end,
                limit == 256 ? Str("ordinal not in range(256)") : Str("ordinal not in range(128)"));
        }
        if (!put_byte(c, u8(cp)))
            return oom(), Run::Error;
        c->pos += w;
        c->cpos++;
    }
    return Run::Done;
}

Run enc_utf16_32(CodecObj *c, usize width)
{
    Str s   = in_bytes(c);
    bool le = little(c->codec, c->byteorder);
    if (!c->begun) {
        c->begun = true;
        if ((c->codec == Codec::Utf16 || c->codec == Codec::Utf32) && c->byteorder == 0 &&
            !put_unit(c, 0xfeff, width, true))
            return oom(), Run::Error;
    }
    while (c->pos < s.size()) {
        u32 cp  = 0;
        usize w = cp_decode(s, c->pos, cp);
        if (is_surrogate(cp))
            return fault(c, c->cpos, c->cpos + 1, "surrogates not allowed");
        bool ok = true;
        if (width == 4)
            ok = put_unit(c, cp, 4, le);
        else if (cp < 0x10000)
            ok = put_unit(c, cp, 2, le);
        else
            ok = put_unit(c, 0xd800 | ((cp - 0x10000) >> 10), 2, le) &&
                 put_unit(c, 0xdc00 | ((cp - 0x10000) & 0x3ff), 2, le);
        if (!ok)
            return oom(), Run::Error;
        c->pos += w;
        c->cpos++;
    }
    return Run::Done;
}

// RFC 2152: set D and set O are written as themselves, and the rest in
// base64 between a '+' and, where it is needed, a '-'.
constexpr u8 UTF7_CATEGORY[128] = {
    3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 3, 3, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    2, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 3, 1, 1, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 3, 3,
};

constexpr char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

bool is_b64(u32 c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '+' ||
           c == '/';
}

u32 from_b64(u32 c)
{
    return c >= 'A' && c <= 'Z'   ? c - 'A'
           : c >= 'a' && c <= 'z' ? c - 'a' + 26
           : c >= '0' && c <= '9' ? c - '0' + 52
           : c == '+'             ? 62
                                  : 63;
}

bool utf7_direct(u32 c)
{
    return c < 128 && c > 0 && UTF7_CATEGORY[c] != 3;
}

Run enc_utf7(CodecObj *c)
{
    Str s         = in_bytes(c);
    bool shift    = false;
    u32 bits      = 0;
    u64 buffer    = 0;
    auto put_bits = [&](u32 unit) {
        bits += 16;
        buffer = (buffer << 16) | unit;
        while (bits >= 6) {
            if (!put_byte(c, u8(B64[(buffer >> (bits - 6)) & 0x3f])))
                return false;
            bits -= 6;
        }
        return true;
    };
    for (usize at = 0; at < s.size();) {
        u32 ch = 0;
        at += cp_decode(s, at, ch);
        bool ok = true;
        if (shift) {
            if (utf7_direct(ch)) {
                if (bits) {
                    ok     = put_byte(c, u8(B64[(buffer << (6 - bits)) & 0x3f]));
                    buffer = 0;
                    bits   = 0;
                }
                shift = false;
                if (is_b64(ch) || ch == '-')
                    ok = ok && put_byte(c, '-');
                ok = ok && put_byte(c, u8(ch));
            } else if (ch >= 0x10000) {
                ok = put_bits(0xd800 | ((ch - 0x10000) >> 10)) &&
                     put_bits(0xdc00 | ((ch - 0x10000) & 0x3ff));
            } else {
                ok = put_bits(ch);
            }
        } else if (ch == '+') {
            ok = put_byte(c, '+') && put_byte(c, '-');
        } else if (utf7_direct(ch)) {
            ok = put_byte(c, u8(ch));
        } else {
            ok    = put_byte(c, '+');
            shift = true;
            if (ch >= 0x10000)
                ok = ok && put_bits(0xd800 | ((ch - 0x10000) >> 10)) &&
                     put_bits(0xdc00 | ((ch - 0x10000) & 0x3ff));
            else
                ok = ok && put_bits(ch);
        }
        if (!ok)
            return oom(), Run::Error;
    }
    if (bits && !put_byte(c, u8(B64[(buffer << (6 - bits)) & 0x3f])))
        return oom(), Run::Error;
    if (shift && !put_byte(c, '-'))
        return oom(), Run::Error;
    c->pos  = s.size();
    c->cpos = in_chars(c);
    return Run::Done;
}

bool put_ascii(CodecObj *c, Str s)
{
    return c->out.append(s);
}

bool put_escape(CodecObj *c, u32 ch, bool raw)
{
    Buf<12> b;
    if (ch < 0x100) {
        if (raw)
            return put_byte(c, u8(ch));
        if (ch >= ' ' && ch < 127) {
            if (ch == '\\')
                b.put("\\\\");
            else
                b.put(char(ch));
        } else if (ch == '\t') {
            b.put("\\t");
        } else if (ch == '\n') {
            b.put("\\n");
        } else if (ch == '\r') {
            b.put("\\r");
        } else {
            put_hexw(b.put("\\x"), ch, 2);
        }
    } else if (ch < 0x10000) {
        put_hexw(b.put("\\u"), ch, 4);
    } else {
        put_hexw(b.put("\\U"), ch, 8);
    }
    return put_ascii(c, b.str());
}

Run enc_escape(CodecObj *c, bool raw)
{
    Str s = in_bytes(c);
    for (usize at = 0; at < s.size();) {
        u32 ch = 0;
        at += cp_decode(s, at, ch);
        if (!put_escape(c, ch, raw))
            return oom(), Run::Error;
    }
    c->pos  = s.size();
    c->cpos = in_chars(c);
    return Run::Done;
}

// ------------------------------------------------------------------ charmap

struct EncMapObj : Obj {
    u32 n;
    u32 cps[256];
    u8 bytes[256];
};

// -1 where the map has nothing for `ch`.
int encmap_find(const EncMapObj *m, u32 ch)
{
    if (ch == 0)
        return 0;
    usize lo = 0, hi = m->n;
    while (lo < hi) {
        usize mid = (lo + hi) / 2;
        if (m->cps[mid] == ch)
            return m->bytes[mid];
        if (m->cps[mid] < ch)
            lo = mid + 1;
        else
            hi = mid;
    }
    return -1;
}

R encmap_size(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "size", 0, 0))
        return R::Err;
    // CPython's layout: a header, and the trie's two levels.
    out = Value::of_int(i32(sizeof(u32) * 8 + 32 + 16 * 2 + 128 * 2));
    return R::Ok;
}

constexpr Method ENCMAP_METHODS[] = { { "size", encmap_size } };

// What the mapping gives `ch`: a byte in `one`, octets in `many`, or nothing
// (NotImpl). Err where the mapping answered something it may not.
R charmap_lookup(Value mapping, u32 ch, int &one, Value &many)
{
    one  = -1;
    many = Value();
    if (mapping.obj()->type == &encmap_type) {
        one = encmap_find(static_cast<EncMapObj *>(mapping.obj()), ch);
        return one < 0 ? R::NotImpl : R::Ok;
    }
    Value got;
    R r = py_getitem(mapping, Value::of_int(i32(ch)), got);
    if (r == R::Err) {
        if (!err_pending())
            return R::NotImpl;
        Str k = err_kind();
        if (k != "KeyError" && k != "IndexError" && k != "LookupError")
            return R::Err;
        err_clear();
        return R::NotImpl;
    }
    if (is_none(got))
        return R::NotImpl;
    i64 v = 0;
    if (got.is_int() || (got.is_obj() && got.obj()->type == &int_type)) {
        if (!as_index(got, v) || v < 0 || v > 255)
            return err_set("TypeError", "character mapping must be in range(256)");
        one = int(v);
        return R::Ok;
    }
    if (is_bytes(got)) {
        many = got;
        return R::Ok;
    }
    Buf<128> b;
    b.put("character mapping must return integer, bytes or None, not ").put(type_name(got));
    return err_set("TypeError", b.str());
}

// One character through the mapping. NotImpl: the mapping has none.
R charmap_put(CodecObj *c, u32 ch)
{
    int one = -1;
    Value many;
    R r = charmap_lookup(c->mapping, ch, one, many);
    if (r != R::Ok)
        return r;
    bool ok =
        one >= 0 ? put_byte(c, u8(one)) : c->out.append(static_cast<BytesObj *>(many.obj())->str());
    return ok ? R::Ok : oom();
}

Run enc_charmap(CodecObj *c)
{
    Str s = in_bytes(c);
    while (c->pos < s.size()) {
        u32 cp  = 0;
        usize w = cp_decode(s, c->pos, cp);
        R r     = charmap_put(c, cp);
        if (r == R::Err)
            return Run::Error;
        if (r == R::NotImpl) {
            usize at = c->pos + w, end = c->cpos + 1;
            while (at < s.size()) {
                u32 x   = 0;
                usize k = cp_decode(s, at, x);
                int one = -1;
                Value many;
                R q = charmap_lookup(c->mapping, x, one, many);
                if (q == R::Err)
                    return Run::Error;
                if (q == R::Ok)
                    break;
                at += k;
                end++;
            }
            return fault(c, c->cpos, end, "character maps to <undefined>");
        }
        c->pos += w;
        c->cpos++;
    }
    return Run::Done;
}

// ----------------------------------------------------------------- decoders

// One UTF-8 sequence at `i`: 0 and its width, or CPython's code -- 1 an
// invalid start byte, 2..4 an invalid continuation at that byte, 5 the input
// ending inside the sequence.
int utf8_one(Str s, usize i, usize &width)
{
    auto cont  = [](u8 b) { return b >= 0x80 && b < 0xc0; };
    usize left = s.size() - i;
    u8 ch      = u8(s[i]);
    if (ch < 0x80) {
        width = 1;
        return 0;
    }
    if (ch < 0xe0) {
        if (ch < 0xc2)
            return 1;
        if (left < 2)
            return 5;
        if (!cont(u8(s[i + 1])))
            return 2;
        width = 2;
        return 0;
    }
    if (ch < 0xf0) {
        if (left < 3) {
            if (left < 2)
                return 5;
            u8 c2 = u8(s[i + 1]);
            if (!cont(c2) || (c2 < 0xa0 ? ch == 0xe0 : ch == 0xed))
                return 2;
            return 5;
        }
        u8 c2 = u8(s[i + 1]), c3 = u8(s[i + 2]);
        if (!cont(c2))
            return 2;
        if (ch == 0xe0 ? c2 < 0xa0 : (ch == 0xed && c2 >= 0xa0))
            return 2;
        if (!cont(c3))
            return 3;
        width = 3;
        return 0;
    }
    if (ch < 0xf5) {
        if (left < 4) {
            if (left < 2)
                return 5;
            u8 c2 = u8(s[i + 1]);
            if (!cont(c2) || (c2 < 0x90 ? ch == 0xf0 : ch == 0xf4))
                return 2;
            if (left < 3)
                return 5;
            if (!cont(u8(s[i + 2])))
                return 3;
            return 5;
        }
        u8 c2 = u8(s[i + 1]), c3 = u8(s[i + 2]), c4 = u8(s[i + 3]);
        if (!cont(c2))
            return 2;
        if (ch == 0xf0 ? c2 < 0x90 : (ch == 0xf4 && c2 >= 0x90))
            return 2;
        if (!cont(c3))
            return 3;
        if (!cont(c4))
            return 4;
        width = 4;
        return 0;
    }
    return 1;
}

Run dec_utf8(CodecObj *c)
{
    Str s = in_bytes(c);
    while (c->pos < s.size()) {
        usize run = c->pos, width = 0;
        int code = 0;
        while (run < s.size() && (code = utf8_one(s, run, width)) == 0)
            run += width;
        if (!c->out.append(s.substr(c->pos, run - c->pos)))
            return oom(), Run::Error;
        c->pos = run;
        if (run >= s.size())
            break;
        if (code == 5) {
            if (!c->final)
                break;
            return fault(c, run, s.size(), "unexpected end of data");
        }
        if (code == 1)
            return fault(c, run, run + 1, "invalid start byte");
        // A surrogate cut short is only short, where more may follow.
        if (code == 2 && !c->final && u8(s[run]) == 0xed && s.size() - run == 2 &&
            u8(s[run + 1]) >= 0xa0 && u8(s[run + 1]) <= 0xbf)
            break;
        return fault(c, run, run + usize(code) - 1, "invalid continuation byte");
    }
    return Run::Done;
}

Run dec_latin1(CodecObj *c)
{
    Str s = in_bytes(c);
    for (; c->pos < s.size(); c->pos++)
        if (!put_cp(c, u8(s[c->pos])))
            return oom(), Run::Error;
    return Run::Done;
}

Run dec_ascii(CodecObj *c)
{
    Str s = in_bytes(c);
    for (; c->pos < s.size(); c->pos++) {
        u8 b = u8(s[c->pos]);
        if (b >= 0x80)
            return fault(c, c->pos, c->pos + 1, "ordinal not in range(128)");
        if (!put_byte(c, b))
            return oom(), Run::Error;
    }
    return Run::Done;
}

u32 unit_at(Str s, usize at, usize width, bool le)
{
    u32 v = 0;
    for (usize k = 0; k < width; k++)
        v |= u32(u8(s[at + k])) << (8 * (le ? k : width - 1 - k));
    return v;
}

// The BOM, at the start of a run whose byte order is not yet known.
void read_bom(CodecObj *c, usize width)
{
    if (c->begun)
        return;
    c->begun = true;
    Str s    = in_bytes(c);
    if ((c->codec == Codec::Utf16 || c->codec == Codec::Utf32) && c->byteorder == 0 &&
        s.size() - c->pos >= width) {
        u32 bom = unit_at(s, c->pos, width, true);
        if (bom == 0xfeff) {
            c->byteorder = -1;
            c->pos += width;
        } else if ((width == 2 && bom == 0xfffe) || (width == 4 && bom == 0xfffe0000)) {
            c->byteorder = 1;
            c->pos += width;
        }
    }
    bool le = little(c->codec, c->byteorder);
    if (width == 2)
        c->encname = le ? Str("utf-16-le") : Str("utf-16-be");
    else
        c->encname = le ? Str("utf-32-le") : Str("utf-32-be");
}

Run dec_utf16(CodecObj *c)
{
    read_bom(c, 2);
    Str s   = in_bytes(c);
    bool le = little(c->codec, c->byteorder);
    for (;;) {
        usize q = c->pos;
        if (s.size() - q < 2) {
            if (q == s.size() || !c->final)
                return Run::Done;
            return fault(c, q, s.size(), "truncated data");
        }
        u32 u = unit_at(s, q, 2, le);
        if (!is_surrogate(u)) {
            if (!put_cp(c, u))
                return oom(), Run::Error;
            c->pos = q + 2;
            continue;
        }
        if (u >= 0xdc00)
            return fault(c, q, q + 2, "illegal encoding");
        if (s.size() - (q + 2) < 2) {
            if (!c->final)
                return Run::Done;
            return fault(c, q, s.size(), "unexpected end of data");
        }
        u32 u2 = unit_at(s, q + 2, 2, le);
        if (u2 < 0xdc00 || u2 > 0xdfff)
            return fault(c, q, q + 2, "illegal UTF-16 surrogate");
        if (!put_cp(c, 0x10000 + ((u - 0xd800) << 10) + (u2 - 0xdc00)))
            return oom(), Run::Error;
        c->pos = q + 4;
    }
}

Run dec_utf32(CodecObj *c)
{
    read_bom(c, 4);
    Str s   = in_bytes(c);
    bool le = little(c->codec, c->byteorder);
    for (;;) {
        usize q = c->pos;
        if (s.size() - q < 4) {
            if (q == s.size() || !c->final)
                return Run::Done;
            return fault(c, q, s.size(), "truncated data");
        }
        u32 u = unit_at(s, q, 4, le);
        if (is_surrogate(u))
            return fault(c, q, q + 4, "code point in surrogate code point range(0xd800, 0xe000)");
        if (u >= 0x110000)
            return fault(c, q, q + 4, "code point not in range(0x110000)");
        if (!put_cp(c, u))
            return oom(), Run::Error;
        c->pos = q + 4;
    }
}

Run dec_utf7(CodecObj *c)
{
    Str s = in_bytes(c);
    for (;;) {
        usize at = c->pos;
        if (at >= s.size())
            break;
        u32 ch = u8(s[at]);
        if (c->in_shift) {
            if (is_b64(ch)) {
                c->buffer = (c->buffer << 6) | from_b64(ch);
                c->bits += 6;
                c->pos++;
                if (c->bits >= 16) {
                    u32 out = (c->buffer >> (c->bits - 16)) & 0xffff;
                    c->bits -= 16;
                    c->buffer &= (1u << c->bits) - 1;
                    if (c->surrogate) {
                        if (out >= 0xdc00 && out <= 0xdfff) {
                            if (!put_cp(c,
                                        0x10000 + ((c->surrogate - 0xd800) << 10) + (out - 0xdc00)))
                                return oom(), Run::Error;
                            c->surrogate = 0;
                            continue;
                        }
                        if (!put_cp(c, c->surrogate))
                            return oom(), Run::Error;
                        c->surrogate = 0;
                    }
                    if (out >= 0xd800 && out <= 0xdbff)
                        c->surrogate = out;
                    else if (!put_cp(c, out))
                        return oom(), Run::Error;
                }
                continue;
            }
            c->in_shift = false;
            if (c->bits > 0) {
                if (c->bits >= 6) {
                    c->pos++;
                    return fault(c, c->shift_in, c->pos, "partial character in shift sequence");
                }
                if (c->buffer != 0) {
                    c->pos++;
                    return fault(c, c->shift_in, c->pos, "non-zero padding bits in shift sequence");
                }
            }
            if (c->surrogate && ch <= 127 && ch != '+' && !put_cp(c, c->surrogate))
                return oom(), Run::Error;
            c->surrogate = 0;
            if (ch == '-')
                c->pos++;
            continue;
        }
        if (ch == '+') {
            c->shift_in = at;
            c->pos++;
            if (c->pos < s.size() && s[c->pos] == '-') {
                c->pos++;
                if (!put_byte(c, '+'))
                    return oom(), Run::Error;
            } else if (c->pos < s.size() && !is_b64(u8(s[c->pos]))) {
                c->pos++;
                return fault(c, c->shift_in, c->pos, "ill-formed sequence");
            } else {
                c->in_shift  = true;
                c->surrogate = 0;
                c->shift_out = c->out.size();
                c->bits      = 0;
                c->buffer    = 0;
            }
            continue;
        }
        if (ch <= 127) {
            c->pos++;
            if (!put_byte(c, u8(ch)))
                return oom(), Run::Error;
            continue;
        }
        c->shift_in = at;
        c->pos++;
        return fault(c, at, c->pos, "unexpected special character");
    }
    if (c->in_shift && c->final) {
        c->in_shift = false;
        if (c->surrogate || c->bits >= 6 || (c->bits > 0 && c->buffer != 0))
            return fault(c, c->shift_in, s.size(), "unterminated shift sequence");
    }
    if (c->in_shift && !c->final) {
        c->pos = c->shift_in;
        c->out.truncate(c->shift_out);
    }
    return Run::Done;
}

int hex_digit(u8 c)
{
    return c >= '0' && c <= '9'   ? c - '0'
           : c >= 'a' && c <= 'f' ? c - 'a' + 10
           : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                  : -1;
}

Run dec_escape(CodecObj *c)
{
    Str s = in_bytes(c);
    while (c->pos < s.size()) {
        u8 ch = u8(s[c->pos++]);
        if (ch != '\\') {
            if (!put_cp(c, ch))
                return oom(), Run::Error;
            continue;
        }
        usize start = c->pos - 1;
        Str message;
        if (c->pos >= s.size()) {
            message = "\\ at end of string";
            goto incomplete;
        }
        ch = u8(s[c->pos++]);
        {
            u32 simple = 0;
            switch (ch) {
            case '\n':
                continue;
            case '\\':
            case '\'':
            case '"':
                simple = ch;
                break;
            case 'b':
                simple = '\b';
                break;
            case 'f':
                simple = '\f';
                break;
            case 't':
                simple = '\t';
                break;
            case 'n':
                simple = '\n';
                break;
            case 'r':
                simple = '\r';
                break;
            case 'v':
                simple = '\v';
                break;
            case 'a':
                simple = '\a';
                break;
            default:
                break;
            }
            if (simple) {
                if (!put_byte(c, u8(simple)))
                    return oom(), Run::Error;
                continue;
            }
        }
        if (ch >= '0' && ch <= '7') {
            u32 v = ch - '0';
            for (int k = 0; k < 2 && c->pos < s.size() && s[c->pos] >= '0' && s[c->pos] <= '7'; k++)
                v = (v << 3) + u32(s[c->pos++] - '0');
            if (!put_cp(c, v))
                return oom(), Run::Error;
            continue;
        }
        if (ch == 'x' || ch == 'u' || ch == 'U') {
            int count = ch == 'x' ? 2 : ch == 'u' ? 4 : 8;
            message   = ch == 'x'   ? Str("truncated \\xXX escape")
                        : ch == 'u' ? Str("truncated \\uXXXX escape")
                                    : Str("truncated \\UXXXXXXXX escape");
            u32 v     = 0;
            for (; count; ++c->pos, --count) {
                if (c->pos >= s.size())
                    goto incomplete;
                int d = hex_digit(u8(s[c->pos]));
                if (d < 0)
                    goto error;
                v = (v << 4) + u32(d);
            }
            if (v > 0x10ffff) {
                message = "illegal Unicode character";
                goto error;
            }
            if (!put_cp(c, v))
                return oom(), Run::Error;
            continue;
        }
        if (ch == 'N') {
            message = "malformed \\N character escape";
            if (c->pos >= s.size())
                goto incomplete;
            if (s[c->pos] == '{') {
                usize from = ++c->pos;
                while (c->pos < s.size() && s[c->pos] != '}')
                    c->pos++;
                if (c->pos >= s.size())
                    goto incomplete;
                if (c->pos > from) {
                    Str name = s.substr(from, c->pos - from);
                    c->pos++;
                    Vec<u32> got;
                    if (ucd_lookup(name, false, got) && got.size() == 1) {
                        if (!put_cp(c, got[0]))
                            return oom(), Run::Error;
                        continue;
                    }
                    message = "unknown Unicode character name";
                }
            }
            goto error;
        }
        if (!put_byte(c, '\\') || !put_cp(c, ch))
            return oom(), Run::Error;
        continue;

    incomplete:
        if (!c->final) {
            c->pos = start;
            return Run::Done;
        }
    error:
        return fault(c, start, c->pos, message);
    }
    return Run::Done;
}

Run dec_raw_escape(CodecObj *c)
{
    Str s = in_bytes(c);
    while (c->pos < s.size()) {
        u8 ch = u8(s[c->pos++]);
        if (ch != '\\' || (c->pos >= s.size() && c->final)) {
            if (!put_cp(c, ch))
                return oom(), Run::Error;
            continue;
        }
        usize start = c->pos - 1;
        Str message;
        if (c->pos >= s.size()) {
            c->pos = start;
            return Run::Done;
        }
        ch = u8(s[c->pos++]);
        if (ch != 'u' && ch != 'U') {
            if (!put_byte(c, '\\') || !put_cp(c, ch))
                return oom(), Run::Error;
            continue;
        }
        int count = ch == 'u' ? 4 : 8;
        message = ch == 'u' ? Str("truncated \\uXXXX escape") : Str("truncated \\UXXXXXXXX escape");
        u32 v   = 0;
        bool bad = false;
        for (; count; ++c->pos, --count) {
            if (c->pos >= s.size()) {
                if (!c->final) {
                    c->pos = start;
                    return Run::Done;
                }
                bad = true;
                break;
            }
            int d = hex_digit(u8(s[c->pos]));
            if (d < 0) {
                bad = true;
                break;
            }
            v = (v << 4) + u32(d);
        }
        if (!bad && v > 0x10ffff) {
            message = "\\Uxxxxxxxx out of range";
            bad     = true;
        }
        if (bad)
            return fault(c, start, c->pos, message);
        if (!put_cp(c, v))
            return oom(), Run::Error;
    }
    return Run::Done;
}

Run dec_charmap(CodecObj *c)
{
    Str s   = in_bytes(c);
    Value m = c->mapping;
    while (c->pos < s.size()) {
        u8 ch = u8(s[c->pos]);
        if (is_str(m)) {
            StrObj *t = str_of(m);
            u32 x     = ch < t->chars ? str_char_at(t, ch) : 0xfffe;
            if (x == 0xfffe)
                return fault(c, c->pos, c->pos + 1, "character maps to <undefined>");
            if (!put_cp(c, x))
                return oom(), Run::Error;
            c->pos++;
            continue;
        }
        Value got;
        R r = py_getitem(m, Value::of_int(ch), got);
        if (r == R::Err) {
            Str k = err_kind();
            if (k != "KeyError" && k != "IndexError" && k != "LookupError")
                return Run::Error;
            err_clear();
            return fault(c, c->pos, c->pos + 1, "character maps to <undefined>");
        }
        m = c->mapping;
        if (is_none(got))
            return fault(c, c->pos, c->pos + 1, "character maps to <undefined>");
        i64 v = 0;
        if (is_str(got)) {
            StrObj *t = str_of(got);
            if (t->chars == 1 && str_char_at(t, 0) == 0xfffe)
                return fault(c, c->pos, c->pos + 1, "character maps to <undefined>");
            if (!c->out.append(t->str()))
                return oom(), Run::Error;
        } else if (!is_bool(got) && as_index(got, v)) {
            if (v == 0xfffe)
                return fault(c, c->pos, c->pos + 1, "character maps to <undefined>");
            if (v < 0 || v > 0x10ffff)
                return err_set("TypeError", "character mapping must be in range(0x110000)"),
                       Run::Error;
            if (!put_cp(c, u32(v)))
                return oom(), Run::Error;
        } else {
            return err_set("TypeError", "character mapping must return integer, None or str"),
                   Run::Error;
        }
        c->pos++;
    }
    return Run::Done;
}

Run step(CodecObj *c)
{
    if (c->encode) {
        switch (c->codec) {
        case Codec::Utf8:
            return enc_utf8(c);
        case Codec::Latin1:
            return enc_ucs1(c, 256);
        case Codec::Ascii:
            return enc_ucs1(c, 128);
        case Codec::Utf16:
        case Codec::Utf16Le:
        case Codec::Utf16Be:
            return enc_utf16_32(c, 2);
        case Codec::Utf32:
        case Codec::Utf32Le:
        case Codec::Utf32Be:
            return enc_utf16_32(c, 4);
        case Codec::Utf7:
            return enc_utf7(c);
        case Codec::UnicodeEscape:
            return enc_escape(c, false);
        case Codec::RawUnicodeEscape:
            return enc_escape(c, true);
        case Codec::Charmap:
            return enc_charmap(c);
        case Codec::None:
            break;
        }
        return Run::Done;
    }
    switch (c->codec) {
    case Codec::Utf8:
        return dec_utf8(c);
    case Codec::Latin1:
        return dec_latin1(c);
    case Codec::Ascii:
        return dec_ascii(c);
    case Codec::Utf16:
    case Codec::Utf16Le:
    case Codec::Utf16Be:
        return dec_utf16(c);
    case Codec::Utf32:
    case Codec::Utf32Le:
    case Codec::Utf32Be:
        return dec_utf32(c);
    case Codec::Utf7:
        return dec_utf7(c);
    case Codec::UnicodeEscape:
        return dec_escape(c);
    case Codec::RawUnicodeEscape:
        return dec_raw_escape(c);
    case Codec::Charmap:
        return dec_charmap(c);
    case Codec::None:
        break;
    }
    return Run::Done;
}

// ------------------------------------------------------------ the exception

Value tuple5(Value a, Value b, Value c, Value d, Value e)
{
    Root ra{ a }, rb{ b }, rc{ c }, rd{ d }, re{ e };
    TupleObj *t = tuple_new(5);
    if (!t)
        return oom(), Value();
    t->items()[0] = ra.v;
    t->items()[1] = rb.v;
    t->items()[2] = rc.v;
    t->items()[3] = rd.v;
    t->items()[4] = re.v;
    return obj_value(t);
}

// The exception for the current fault: made once, then moved along.
Value fault_exc(Value cv)
{
    Root rc{ cv };
    CodecObj *c = co_of(rc.v);
    Root reason{ str_new(c->reason) };
    if (reason.v.is_nil())
        return Value();
    Root start{ int_from_i64(i64(co_of(rc.v)->fault_start)) };
    Root end{ int_from_i64(i64(co_of(rc.v)->fault_end)) };
    c = co_of(rc.v);
    if (!c->exc.is_nil()) {
        ExcObj *e          = static_cast<ExcObj *>(c->exc.obj());
        e->uni[UNI_START]  = start.v;
        e->uni[UNI_END]    = end.v;
        e->uni[UNI_REASON] = reason.v;
        return c->exc;
    }
    Root enc{ str_new(c->encname) };
    if (enc.v.is_nil())
        return Value();
    c = co_of(rc.v);
    Root args{ tuple5(enc.v, c->input, start.v, end.v, reason.v) };
    if (args.v.is_nil())
        return Value();
    Root e{ exc_new(exc_find(co_of(rc.v)->encode ? "UnicodeEncodeError" : "UnicodeDecodeError"),
                    args.v) };
    if (e.v.is_nil() || !unierr_init(e.v, args.v))
        return Value();
    co_of(rc.v)->exc = e.v;
    return e.v;
}

R raise_exc(Value e)
{
    UniKind k = unierr_kind(e);
    return err_set_value(e, k == UniKind::Encode   ? Str("UnicodeEncodeError")
                            : k == UniKind::Decode ? Str("UnicodeDecodeError")
                                                   : Str("UnicodeTranslateError"));
}

R raise_fault(Value cv)
{
    Value e = fault_exc(cv);
    return e.is_nil() ? R::Err : raise_exc(e);
}

// ------------------------------------------------------------- the handlers

// What a handler works over: the object, the clamped range, and for
// surrogatepass the encoding.
struct Over {
    UniKind kind;
    Value object;
    usize start, end, len;
    Str encoding;
};

// UTF-8, UTF-16 and UTF-32 in their two orders, as surrogatepass reads the
// exception's encoding. The width in `bytes`, or false.
bool standard_encoding(Str e, int &code, usize &bytes)
{
    auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? char(c + 32) : c; };
    if (e.size() >= 3 && lower(e[0]) == 'u' && lower(e[1]) == 't' && lower(e[2]) == 'f') {
        Str r = e.substr(3);
        if (!r.empty() && (r[0] == '-' || r[0] == '_'))
            r = r.substr(1);
        if (r == "8") {
            bytes       = 3;
            return code = 0, true;
        }
        if (r.starts_with("16") || r.starts_with("32")) {
            bytes  = r[0] == '1' ? 2 : 4;
            int le = bytes == 2 ? 2 : 4, be = bytes == 2 ? 1 : 3;
            r = r.substr(2);
            if (r.empty())
                return code = le, true;
            if (r[0] == '-' || r[0] == '_')
                r = r.substr(1);
            if (r.size() == 2 && lower(r[1]) == 'e') {
                if (lower(r[0]) == 'b')
                    return code = be, true;
                if (lower(r[0]) == 'l')
                    return code = le, true;
            }
        }
        return false;
    }
    if (e == "CP_UTF8" || e == "cp65001") {
        bytes       = 3;
        return code = 0, true;
    }
    return false;
}

Value pair(Value a, Value b)
{
    Root ra{ a }, rb{ b };
    TupleObj *t = tuple_new(2);
    if (!t)
        return oom(), Value();
    t->items()[0] = ra.v;
    t->items()[1] = rb.v;
    return obj_value(t);
}

// The replacement for [start, end) of `o`, and where to carry on. `internal`
// is the codec's own fast path, which takes the escapable head of a range
// rather than refusing the whole of it. NotImpl: this handler refuses, and
// the original exception is what should be raised.
R handle(ErrH h, const Over &o, bool internal, Value &rep, usize &next)
{
    next       = o.end;
    bool enc   = o.kind != UniKind::Decode;
    Str obj    = o.kind == UniKind::Decode ? static_cast<BytesObj *>(o.object.obj())->str()
                                           : str_of(o.object)->str();
    usize from = enc ? str_offset_of(str_of(o.object), o.start) : o.start;
    String b;
    switch (h) {
    case ErrH::Ignore:
        rep = str_new("");
        return rep.is_nil() ? R::Err : R::Ok;
    case ErrH::Replace:
        if (o.kind == UniKind::Decode) {
            rep = str_new("\xef\xbf\xbd");
            return rep.is_nil() ? R::Err : R::Ok;
        }
        for (usize i = o.start; i < o.end; i++)
            if (!(o.kind == UniKind::Encode ? b.push('?') : cp_append(b, 0xfffd)))
                return oom();
        break;
    case ErrH::XmlCharRefReplace: {
        if (o.kind != UniKind::Encode)
            return R::NotImpl;
        usize at = from;
        for (usize i = o.start; i < o.end; i++) {
            u32 cp = 0;
            at += cp_decode(obj, at, cp);
            Buf<16> t;
            t.put("&#").put(cp).put(';');
            if (!b.append(t.str()))
                return oom();
        }
        break;
    }
    case ErrH::BackslashReplace:
    case ErrH::NameReplace: {
        if (o.kind == UniKind::Decode) {
            if (h == ErrH::NameReplace)
                return R::NotImpl;
            for (usize i = o.start; i < o.end; i++) {
                Buf<8> t;
                put_hexw(t.put("\\x"), u8(obj[i]), 2);
                if (!b.append(t.str()))
                    return oom();
            }
            break;
        }
        if (h == ErrH::NameReplace && o.kind != UniKind::Encode)
            return R::NotImpl;
        usize at = from;
        for (usize i = o.start; i < o.end; i++) {
            u32 cp = 0;
            at += cp_decode(obj, at, cp);
            if (h == ErrH::NameReplace) {
                String name;
                if (ucd_name(cp, name)) {
                    if (!b.append("\\N{") || !b.append(name.str()) || !b.push('}'))
                        return oom();
                    continue;
                }
            }
            Buf<12> t;
            if (cp >= 0x10000)
                put_hexw(t.put("\\U"), cp, 8);
            else if (cp >= 0x100)
                put_hexw(t.put("\\u"), cp, 4);
            else
                put_hexw(t.put("\\x"), cp, 2);
            if (!b.append(t.str()))
                return oom();
        }
        break;
    }
    case ErrH::SurrogateEscape: {
        if (o.kind == UniKind::Encode) {
            usize at = from, i = o.start;
            for (; i < o.end; i++) {
                u32 cp  = 0;
                usize w = cp_decode(obj, at, cp);
                if (cp < 0xdc80 || cp > 0xdcff)
                    break;
                at += w;
                if (!b.push(char(cp - 0xdc00)))
                    return oom();
            }
            if (i < o.end && (!internal || i == o.start))
                return R::NotImpl;
            next = i;
            rep  = bytes_new(b.str());
            return rep.is_nil() ? R::Err : R::Ok;
        }
        if (o.kind != UniKind::Decode)
            return R::NotImpl;
        usize n = 0;
        while (n < 4 && o.start + n < o.end && u8(obj[o.start + n]) >= 128) {
            if (!cp_append(b, 0xdc00 + u8(obj[o.start + n])))
                return oom();
            n++;
        }
        if (!n)
            return R::NotImpl;
        if (internal)
            while (o.start + n < o.end && u8(obj[o.start + n]) >= 128) {
                if (!cp_append(b, 0xdc00 + u8(obj[o.start + n])))
                    return oom();
                n++;
            }
        next = o.start + n;
        break;
    }
    case ErrH::SurrogatePass: {
        int code    = 0;
        usize width = 0;
        if (o.kind == UniKind::Translate || !standard_encoding(o.encoding, code, width))
            return R::NotImpl;
        if (o.kind == UniKind::Encode) {
            usize at = from;
            for (usize i = o.start; i < o.end; i++) {
                u32 cp = 0;
                at += cp_decode(obj, at, cp);
                if (!is_surrogate(cp))
                    return R::NotImpl;
                char t[4];
                usize n = 0;
                if (code == 0)
                    n = cp_encode(cp, t);
                else
                    for (usize k = 0; k < width; k++, n++) {
                        bool le = code == 2 || code == 4;
                        t[n]    = char(cp >> (8 * (le ? k : width - 1 - k)));
                    }
                if (!b.append(Str(t, n)))
                    return oom();
            }
            rep = bytes_new(b.str());
            return rep.is_nil() ? R::Err : R::Ok;
        }
        u32 ch = 0;
        if (o.len - o.start >= width) {
            const u8 *p = reinterpret_cast<const u8 *>(obj.data()) + o.start;
            switch (code) {
            case 0:
                if ((p[0] & 0xf0) == 0xe0 && (p[1] & 0xc0) == 0x80 && (p[2] & 0xc0) == 0x80)
                    ch = ((p[0] & 0x0fu) << 12) + ((p[1] & 0x3fu) << 6) + (p[2] & 0x3fu);
                break;
            case 1:
                ch = u32(p[0]) << 8 | p[1];
                break;
            case 2:
                ch = u32(p[1]) << 8 | p[0];
                break;
            case 3:
                ch = u32(p[0]) << 24 | u32(p[1]) << 16 | u32(p[2]) << 8 | p[3];
                break;
            case 4:
                ch = u32(p[3]) << 24 | u32(p[2]) << 16 | u32(p[1]) << 8 | p[0];
                break;
            }
        }
        if (!is_surrogate(ch))
            return R::NotImpl;
        if (!cp_append(b, ch))
            return oom();
        next = o.start + width;
        break;
    }
    case ErrH::Strict:
    case ErrH::Other:
        return R::NotImpl;
    }
    StrObj *s = str_raw(b.str());
    if (!s)
        return oom();
    rep = obj_value(s);
    return R::Ok;
}

// The fault, as a handler sees it.
Over over_fault(const CodecObj *c)
{
    Over o;
    o.kind     = c->encode ? UniKind::Encode : UniKind::Decode;
    o.object   = c->input;
    o.len      = in_chars(c);
    o.start    = c->fault_start;
    o.end      = c->fault_end;
    o.encoding = c->encname;
    return o;
}

// ------------------------------------------------------------ the replacement

// `rep` in place of the fault, and carry on at `next`. Checks what CPython
// checks, in its order.
R apply(Value cv, Value rep, i64 next)
{
    Root rc{ cv }, rr{ rep };
    CodecObj *c = co_of(rc.v);
    i64 len     = i64(in_chars(c));
    if (next < 0)
        next += len;
    if (next < 0 || next > len) {
        Buf<96> b;
        put_i64(b.put("position "), next).put(" from error handler out of bounds");
        return err_set("IndexError", b.str());
    }
    if (!c->encode) {
        if (!c->out.append(str_of(rr.v)->str()))
            return oom();
        c->pos = usize(next);
        return R::Ok;
    }
    Codec k = c->codec;
    if (is_bytes(rr.v)) {
        Str b = static_cast<BytesObj *>(rr.v.obj())->str();
        if ((is_utf16(k) && b.size() % 2) || (is_utf32(k) && b.size() % 4))
            return raise_fault(rc.v);
        if (!c->out.append(b))
            return oom();
    } else {
        StrObj *s = str_of(rr.v);
        Str t     = s->str();
        for (usize at = 0; at < t.size();) {
            u32 cp = 0;
            at += cp_decode(t, at, cp);
            c       = co_of(rc.v);
            bool ok = true;
            if (k == Codec::Charmap) {
                R r = charmap_put(c, cp);
                if (r == R::Err)
                    return R::Err;
                if (r == R::NotImpl)
                    return raise_fault(rc.v);
                continue;
            }
            u32 limit = k == Codec::Latin1 ? 256 : 128;
            if (cp >= limit)
                return raise_fault(rc.v);
            if (is_utf16(k))
                ok = put_unit(c, cp, 2, little(k, c->byteorder));
            else if (is_utf32(k))
                ok = put_unit(c, cp, 4, little(k, c->byteorder));
            else
                ok = put_byte(c, u8(cp));
            if (!ok)
                return oom();
        }
    }
    c       = co_of(rc.v);
    c->cpos = usize(next);
    c->pos  = str_offset_of(str_of(c->input), c->cpos);
    return R::Ok;
}

Value wrap_result(Value cv)
{
    Root rc{ cv };
    CodecObj *c = co_of(rc.v);
    Root text{ c->encode ? bytes_new(c->out.str()) : obj_value(str_raw(c->out.str())) };
    if (text.v.is_nil())
        return err_pending() ? Value() : (oom(), Value());
    c = co_of(rc.v);
    if (c->wrap == WRAP_NONE)
        return text.v;
    usize used  = c->encode ? in_chars(c) : c->pos;
    TupleObj *t = tuple_new(c->wrap == WRAP_BYTEORDER ? 3 : 2);
    if (!t)
        return oom(), Value();
    t->items()[0] = text.v;
    t->items()[1] = int_from_i64(i64(used));
    if (co_of(rc.v)->wrap == WRAP_BYTEORDER)
        t->items()[2] = Value::of_int(co_of(rc.v)->byteorder);
    return obj_value(t);
}

// The run: until done, or until a handler of the program's own is wanted, in
// which case `call` is true and the exception to hand it is ready.
R drive(Value cv, Value &out, bool &call)
{
    Root rc{ cv };
    call = false;
    for (;;) {
        CodecObj *c = co_of(rc.v);
        Run r       = step(c);
        if (r == Run::Error)
            return R::Err;
        if (r == Run::Done) {
            out = wrap_result(rc.v);
            return out.is_nil() ? R::Err : R::Ok;
        }
        c = co_of(rc.v);
        if (c->err == ErrH::Strict)
            return raise_fault(rc.v);
        if (c->err == ErrH::Other) {
            if (c->handler.is_nil()) {
                Value h = codec_lookup_error(str_of(c->errors)->str());
                if (h.is_nil())
                    return R::Err;
                co_of(rc.v)->handler = h;
            }
            if (fault_exc(rc.v).is_nil())
                return R::Err;
            call = true;
            return R::Ok;
        }
        Root rep;
        usize next = 0;
        R h        = handle(c->err, over_fault(c), true, rep.v, next);
        if (h == R::Err)
            return R::Err;
        if (h == R::NotImpl)
            return raise_fault(rc.v);
        if (apply(rc.v, rep.v, i64(next)) != R::Ok)
            return R::Err;
    }
}

// What the program's handler answered: (str, int) for a decoder, and
// (str or bytes, int) for an encoder.
R handler_answer(Value cv, Value got)
{
    Root rc{ cv }, rg{ got };
    CodecObj *c = co_of(rc.v);
    Str msg     = c->encode ? Str("encoding error handler must return (str/bytes, int) tuple")
                            : Str("decoding error handler must return (str, int) tuple");
    if (!is_tuple(rg.v) || static_cast<TupleObj *>(rg.v.obj())->len != 2)
        return err_set("TypeError", msg);
    TupleObj *t = static_cast<TupleObj *>(rg.v.obj());
    Value rep   = t->items()[0];
    i64 next    = 0;
    // CPython's "On" and "Un": a decoder's str first, then the integer, then
    // an encoder's str or bytes.
    if (!c->encode && !is_str(rep))
        return err_set("TypeError", msg);
    if (!as_index(t->items()[1], next)) {
        Buf<96> m;
        m.put('\'')
            .put(type_name(t->items()[1]))
            .put("' object cannot be interpreted as an integer");
        return err_set("TypeError", m.str());
    }
    if (!is_str(rep) && !is_bytes(rep))
        return err_set("TypeError", msg);
    if (!c->encode) {
        // The handler may have put other bytes in the exception.
        Value o = static_cast<ExcObj *>(c->exc.obj())->uni[UNI_OBJECT];
        if (is_bytes(o))
            c->input = o;
    }
    return apply(rc.v, rep, next);
}

// i is 2 while the first call is still to be made, 1 once a handler has been
// asked.
R run_step(ContObj *k, Value in)
{
    Root kv{ obj_value(k) };
    if (k->i == 2) {
        k->i        = 1;
        CodecObj *c = co_of(k->s[0]);
        return cont_call(k, c->handler, c->exc);
    }
    if (k->i == 1) {
        if (handler_answer(k->s[0], in) != R::Ok)
            return R::Err;
    }
    Root out;
    bool call = false;
    if (drive(cont_of(kv.v)->s[0], out.v, call) != R::Ok)
        return R::Err;
    k = cont_of(kv.v);
    if (!call)
        return cont_done(k, out.v);
    k->i        = 1;
    CodecObj *c = co_of(k->s[0]);
    return cont_call(k, c->handler, c->exc);
}

Value codec_new(const CodecCall &cc)
{
    Root in{ cc.input }, er{ cc.errors }, mp{ cc.mapping };
    CodecObj *c = static_cast<CodecObj *>(obj_alloc(&codec_type, sizeof(CodecObj)));
    if (!c)
        return oom(), Value();
    new (&c->out) String();
    c->input       = in.v;
    c->errors      = er.v.is_nil() || is_none(er.v) ? Value() : er.v;
    c->handler     = Value();
    c->exc         = Value();
    c->mapping     = mp.v.is_nil() || is_none(mp.v) ? Value() : mp.v;
    c->pos         = 0;
    c->cpos        = 0;
    c->fault_start = 0;
    c->fault_end   = 0;
    c->reason      = Str();
    c->byteorder   = cc.byteorder;
    c->bits        = 0;
    c->buffer      = 0;
    c->surrogate   = 0;
    c->shift_out   = 0;
    c->shift_in    = 0;
    c->codec       = cc.codec;
    c->err         = errh_of(c->errors);
    c->wrap        = cc.wrap;
    c->encode      = cc.encode;
    c->final       = cc.final;
    c->in_shift    = false;
    c->begun       = false;
    c->done        = false;
    if (c->codec == Codec::Charmap && c->mapping.is_nil())
        c->codec = Codec::Latin1;
    switch (c->codec) {
    case Codec::Utf8:
        c->encname = "utf-8";
        break;
    case Codec::Utf7:
        c->encname = "utf7";
        break;
    case Codec::Latin1:
        c->encname = "latin-1";
        break;
    case Codec::Ascii:
        c->encname = "ascii";
        break;
    case Codec::Charmap:
        c->encname = "charmap";
        break;
    case Codec::UnicodeEscape:
        c->encname = "unicodeescape";
        break;
    case Codec::RawUnicodeEscape:
        c->encname = "rawunicodeescape";
        break;
    case Codec::Utf16:
        c->encname = c->byteorder < 0   ? Str("utf-16-le")
                     : c->byteorder > 0 ? Str("utf-16-be")
                                        : Str("utf-16");
        break;
    case Codec::Utf16Le:
        c->encname = "utf-16-le";
        break;
    case Codec::Utf16Be:
        c->encname = "utf-16-be";
        break;
    case Codec::Utf32:
        c->encname = c->byteorder < 0   ? Str("utf-32-le")
                     : c->byteorder > 0 ? Str("utf-32-be")
                                        : Str("utf-32");
        break;
    case Codec::Utf32Le:
        c->encname = "utf-32-le";
        break;
    case Codec::Utf32Be:
        c->encname = "utf-32-be";
        break;
    case Codec::None:
        break;
    }
    return obj_value(c);
}

} // namespace

// ------------------------------------------------------------------ the API

constexpr Type encmap_type{ .name = "EncodingMap" };

ErrH errh_of(Value errors)
{
    if (errors.is_nil() || is_none(errors) || !is_str(errors))
        return ErrH::Strict;
    Str n = str_of(errors)->str();
    if (n == "strict")
        return ErrH::Strict;
    if (n == "ignore")
        return ErrH::Ignore;
    if (n == "replace")
        return ErrH::Replace;
    if (n == "backslashreplace")
        return ErrH::BackslashReplace;
    if (n == "xmlcharrefreplace")
        return ErrH::XmlCharRefReplace;
    if (n == "namereplace")
        return ErrH::NameReplace;
    if (n == "surrogateescape")
        return ErrH::SurrogateEscape;
    if (n == "surrogatepass")
        return ErrH::SurrogatePass;
    return ErrH::Other;
}

R codec_run(const CodecCall &cc, Value &out)
{
    Root cv{ codec_new(cc) };
    if (cv.v.is_nil())
        return R::Err;
    // A decoder's input is bytes by the time the exception names it.
    if (!cc.encode && !is_bytes(co_of(cv.v)->input)) {
        Str data;
        if (!bytes_like(co_of(cv.v)->input, data))
            return err_not("a bytes-like object is required", co_of(cv.v)->input, true);
        Value b = bytes_new(data);
        if (b.is_nil())
            return R::Err;
        co_of(cv.v)->input = b;
    }
    bool call = false;
    if (drive(cv.v, out, call) != R::Ok)
        return R::Err;
    if (!call)
        return R::Ok;
    Root kv{ cont_new(run_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = cv.v;
    k->i       = 2;
    out        = kv.v;
    return R::Ok;
}

bool enc_normalize(Str name, bool lower, char *out, usize cap, usize &len)
{
    len        = 0;
    bool punct = false;
    for (usize i = 0; i < name.size(); i++) {
        char c     = name[i];
        bool alnum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        if (alnum || c == '.') {
            if (punct && len) {
                if (len + 1 >= cap)
                    return false;
                out[len++] = '_';
            }
            punct = false;
            if (len + 1 >= cap)
                return false;
            out[len++] = lower && c >= 'A' && c <= 'Z' ? char(c + 32) : c;
        } else {
            punct = true;
        }
    }
    return true;
}

bool enc_has_surrogate(Str name)
{
    for (usize i = 0; i + 1 < name.size(); i++)
        if (u8(name[i]) == 0xed && u8(name[i + 1]) >= 0xa0)
            return true;
    return false;
}

Codec codec_shortcut(Str encoding)
{
    char buf[11];
    usize n = 0;
    // A name normalizes to the codec it names, but one holding a lone
    // surrogate does not name anything: the registry answers that.
    if (enc_has_surrogate(encoding) || !enc_normalize(encoding, true, buf, sizeof buf, n))
        return Codec::None;
    Str l(buf, n);
    if (l.starts_with("utf")) {
        Str r = l.substr(3);
        if (r.starts_with("_"))
            r = r.substr(1);
        if (r == "8")
            return Codec::Utf8;
        if (r == "16")
            return Codec::Utf16;
        if (r == "32")
            return Codec::Utf32;
        return Codec::None;
    }
    if (l == "ascii" || l == "us_ascii")
        return Codec::Ascii;
    if (l == "latin1" || l == "latin_1" || l == "iso_8859_1" || l == "iso8859_1")
        return Codec::Latin1;
    return Codec::None;
}

Value charmap_build(Value table)
{
    if (!is_str(table) || !str_of(table)->chars)
        return err_set("TypeError", "bad argument type for built-in operation"), Value();
    Root rt{ table };
    StrObj *s = str_of(rt.v);
    usize n   = s->chars < 256 ? s->chars : 256;
    bool dict = str_char_at(s, 0) != 0;
    u32 cps[256];
    for (usize i = 0; i < n; i++) {
        cps[i] = str_char_at(s, i);
        if (i && (cps[i] == 0 || cps[i] > 0xffff))
            dict = true;
    }
    if (dict) {
        Root d{ obj_value(dict_new()) };
        if (d.v.is_nil())
            return oom(), Value();
        for (usize i = 0; i < n; i++)
            if (dict_set(static_cast<DictObj *>(d.v.obj()), int_from_i64(cps[i]),
                         Value::of_int(i32(i))) != R::Ok)
                return Value();
        return d.v;
    }
    // The methods first: installing them allocates, and a map not yet
    // reachable from anything would go in that collection.
    if (!method_install(&encmap_type, ENCMAP_METHODS))
        return Value();
    EncMapObj *m = static_cast<EncMapObj *>(obj_alloc(&encmap_type, sizeof(EncMapObj)));
    if (!m)
        return oom(), Value();
    m->n = 0;
    for (usize i = 1; i < n; i++) {
        if (cps[i] == 0xfffe)
            continue;
        // Sorted as it goes; a later byte for the same character wins, as in
        // CPython's trie.
        usize at = 0;
        while (at < m->n && m->cps[at] < cps[i])
            at++;
        if (at < m->n && m->cps[at] == cps[i]) {
            m->bytes[at] = u8(i);
            continue;
        }
        for (usize k = m->n; k > at; k--) {
            m->cps[k]   = m->cps[k - 1];
            m->bytes[k] = m->bytes[k - 1];
        }
        m->cps[at]   = cps[i];
        m->bytes[at] = u8(i);
        m->n++;
    }
    return obj_value(m);
}

R codec_handler_call(ErrH h, Value exc, Value &out)
{
    UniKind kind = unierr_kind(exc);
    if (h == ErrH::Strict) {
        if (!is_exc(exc))
            return err_set("TypeError", "codec must pass exception instance");
        return err_set_value(exc);
    }
    if (kind == UniKind::None) {
        Buf<128> b;
        b.put("don't know how to handle ").put(type_name(exc)).put(" in error callback");
        return err_set("TypeError", b.str());
    }
    ExcObj *e = static_cast<ExcObj *>(exc.obj());
    Over o;
    o.kind        = kind;
    o.object      = e->uni[UNI_OBJECT];
    bool as_bytes = kind == UniKind::Decode;
    if (as_bytes ? !is_bytes(o.object) : !is_str(o.object))
        return err_set2("TypeError",
                        as_bytes ? Str("object attribute must be bytes")
                                 : Str("object attribute must be unicode"),
                        type_name(o.object));
    o.len     = as_bytes ? static_cast<BytesObj *>(o.object.obj())->len : str_of(o.object)->chars;
    i64 start = 0, end = 0;
    as_index(e->uni[UNI_START], start);
    as_index(e->uni[UNI_END], end);
    if (start < 0)
        start = 0;
    if (i64(o.len) <= start)
        start = o.len ? i64(o.len) - 1 : 0;
    if (end < 1)
        end = 1;
    if (end > i64(o.len))
        end = i64(o.len);
    o.start = usize(start);
    o.end   = end < start ? usize(start) : usize(end);
    if (kind != UniKind::Translate && is_str(e->uni[UNI_ENCODING]))
        o.encoding = str_of(e->uni[UNI_ENCODING])->str();
    Root re{ exc };
    Root rep;
    usize next = usize(end);
    R r        = handle(h, o, false, rep.v, next);
    if (r == R::Err)
        return R::Err;
    if (r == R::NotImpl) {
        if (h == ErrH::XmlCharRefReplace || (h == ErrH::NameReplace && kind != UniKind::Encode) ||
            (h == ErrH::SurrogateEscape && kind == UniKind::Translate) ||
            (h == ErrH::SurrogatePass && kind == UniKind::Translate)) {
            Buf<128> b;
            b.put("don't know how to handle ").put(type_name(re.v)).put(" in error callback");
            return err_set("TypeError", b.str());
        }
        return raise_exc(re.v);
    }
    // Every handler answers the clamped end, but a decoder's two surrogate
    // handlers, which say how many bytes they took.
    // namereplace walks from the start, so it answers the later of the two.
    bool took =
        (kind == UniKind::Decode && (h == ErrH::SurrogateEscape || h == ErrH::SurrogatePass)) ||
        h == ErrH::NameReplace;
    Root at{ int_from_i64(took ? i64(next) : end) };
    out = pair(rep.v, at.v);
    return out.is_nil() ? R::Err : R::Ok;
}

R escape_decode(Str s, Value errors, Value &out)
{
    Str how = errors.is_nil() || is_none(errors) ? Str("strict") : str_of(errors)->str();
    String b;
    for (usize i = 0; i < s.size();) {
        if (s[i] != '\\') {
            if (!b.push(s[i++]))
                return oom();
            continue;
        }
        i++;
        if (i == s.size())
            return err_set("ValueError", "Trailing \\ in string");
        char c  = s[i++];
        bool ok = true;
        switch (c) {
        case '\n':
            break;
        case '\\':
        case '\'':
        case '"':
            ok = b.push(c);
            break;
        case 'b':
            ok = b.push('\b');
            break;
        case 'f':
            ok = b.push('\f');
            break;
        case 't':
            ok = b.push('\t');
            break;
        case 'n':
            ok = b.push('\n');
            break;
        case 'r':
            ok = b.push('\r');
            break;
        case 'v':
            ok = b.push('\v');
            break;
        case 'a':
            ok = b.push('\a');
            break;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7': {
            int v = c - '0';
            for (int k = 0; k < 2 && i < s.size() && s[i] >= '0' && s[i] <= '7'; k++)
                v = (v << 3) + (s[i++] - '0');
            ok = b.push(char(v));
            break;
        }
        case 'x': {
            if (i + 1 < s.size() && hex_digit(u8(s[i])) >= 0 && hex_digit(u8(s[i + 1])) >= 0) {
                ok = b.push(char(hex_digit(u8(s[i])) * 16 + hex_digit(u8(s[i + 1]))));
                i += 2;
                break;
            }
            if (how == "strict") {
                Buf<64> m;
                m.put("invalid \\x escape at position ").put(i - 2);
                return err_set("ValueError", m.str());
            }
            if (how == "replace") {
                ok = b.push('?');
            } else if (how != "ignore") {
                Buf<128> m;
                m.put("decoding error; unknown error handling code: ").put(how);
                return err_set("ValueError", m.str());
            }
            if (i < s.size() && hex_digit(u8(s[i])) >= 0)
                i++;
            break;
        }
        default:
            ok = b.push('\\');
            i--;
        }
        if (!ok)
            return oom();
    }
    Root r{ bytes_new(b.str()) };
    if (r.v.is_nil())
        return R::Err;
    out = pair(r.v, int_from_i64(i64(s.size())));
    return out.is_nil() ? R::Err : R::Ok;
}

R escape_encode(Str s, Value &out)
{
    String b;
    for (usize i = 0; i < s.size(); i++) {
        u8 c = u8(s[i]);
        Buf<8> t;
        if (c == '\'' || c == '\\')
            t.put('\\').put(char(c));
        else if (c == '\t')
            t.put("\\t");
        else if (c == '\n')
            t.put("\\n");
        else if (c == '\r')
            t.put("\\r");
        else if (c < ' ' || c >= 0x7f)
            t.put("\\x").put(HEX[c >> 4]).put(HEX[c & 15]);
        else
            t.put(char(c));
        if (!b.append(t.str()))
            return oom();
    }
    Root r{ bytes_new(b.str()) };
    if (r.v.is_nil())
        return R::Err;
    out = pair(r.v, int_from_i64(i64(s.size())));
    return out.is_nil() ? R::Err : R::Ok;
}

bool std_encode(Str text, bool escape, String &out)
{
    if (!has_surrogate(text))
        return out.append(text) || oom() == R::Ok;
    Root s{ obj_value(str_raw(text)) };
    if (s.v.is_nil())
        return oom() == R::Ok;
    CodecCall cc;
    cc.codec  = Codec::Utf8;
    cc.encode = true;
    cc.input  = s.v;
    Root errors{ str_new(escape ? Str("surrogateescape") : Str("backslashreplace")) };
    if (errors.v.is_nil())
        return false;
    cc.errors = errors.v;
    Root got;
    if (codec_run(cc, got.v) != R::Ok)
        return false;
    return out.append(static_cast<BytesObj *>(got.v.obj())->str()) || oom() == R::Ok;
}

Value str_lossy(Str bytes)
{
    Utf8Bad bad;
    if (utf8_strict(bytes, bad))
        return str_new(bytes);
    Root raw{ bytes_new(bytes) };
    Root errors{ str_new("replace") };
    if (raw.v.is_nil() || errors.v.is_nil())
        return Value();
    CodecCall cc;
    cc.codec  = Codec::Utf8;
    cc.input  = raw.v;
    cc.errors = errors.v;
    Value out;
    return codec_run(cc, out) == R::Ok ? out : Value();
}

bool utf8_strict(Str s, Utf8Bad &bad)
{
    usize width = 0;
    for (usize i = 0; i < s.size();) {
        int code = utf8_one(s, i, width);
        if (!code) {
            i += width;
            continue;
        }
        bad.at     = i;
        bad.length = code == 5 ? s.size() - i : code == 1 ? 1 : usize(code) - 1;
        bad.reason = code == 5   ? Str("unexpected end of data")
                     : code == 1 ? Str("invalid start byte")
                                 : Str("invalid continuation byte");
        return false;
    }
    return true;
}
