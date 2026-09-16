// `_codecs`: the registry, the error handlers, and a function per codec. The
// registry is what CPython's codecs.py and encodings package stand on; the
// codecs themselves are codec.cpp's.
#include "builtin.h"
#include "codec.h"
#include "egroup.h"
#include "exc.h"
#include "gc.h"
#include "intern.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "type.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// The search functions, what they found, and the error handlers by name.
struct Registry {
    Value search;  // ListObj
    Value cache;   // DictObj: normalized name -> CodecInfo
    Value errors;  // DictObj: name -> handler
    bool imported; // `encodings` has been asked for
};

Registry *reg;

void reg_mark()
{
    if (!reg)
        return;
    gc_mark(reg->search);
    gc_mark(reg->cache);
    gc_mark(reg->errors);
}

constexpr Str HANDLER_NAMES[] = { "strict",           "ignore",
                                  "replace",          "xmlcharrefreplace",
                                  "backslashreplace", "namereplace",
                                  "surrogatepass",    "surrogateescape" };

R h_strict(const CallArgs &a, Value &out)
{
    if (!args_only(a, "strict_errors", 1, 1))
        return R::Err;
    return codec_handler_call(ErrH::Strict, a.args[0], out);
}

R h_ignore(const CallArgs &a, Value &out)
{
    if (!args_only(a, "ignore_errors", 1, 1))
        return R::Err;
    return codec_handler_call(ErrH::Ignore, a.args[0], out);
}

R h_replace(const CallArgs &a, Value &out)
{
    if (!args_only(a, "replace_errors", 1, 1))
        return R::Err;
    return codec_handler_call(ErrH::Replace, a.args[0], out);
}

R h_xml(const CallArgs &a, Value &out)
{
    if (!args_only(a, "xmlcharrefreplace_errors", 1, 1))
        return R::Err;
    return codec_handler_call(ErrH::XmlCharRefReplace, a.args[0], out);
}

R h_backslash(const CallArgs &a, Value &out)
{
    if (!args_only(a, "backslashreplace_errors", 1, 1))
        return R::Err;
    return codec_handler_call(ErrH::BackslashReplace, a.args[0], out);
}

R h_name(const CallArgs &a, Value &out)
{
    if (!args_only(a, "namereplace_errors", 1, 1))
        return R::Err;
    return codec_handler_call(ErrH::NameReplace, a.args[0], out);
}

R h_pass(const CallArgs &a, Value &out)
{
    if (!args_only(a, "surrogatepass", 1, 1))
        return R::Err;
    return codec_handler_call(ErrH::SurrogatePass, a.args[0], out);
}

R h_escape(const CallArgs &a, Value &out)
{
    if (!args_only(a, "surrogateescape", 1, 1))
        return R::Err;
    return codec_handler_call(ErrH::SurrogateEscape, a.args[0], out);
}

constexpr ModDef HANDLERS[] = {
    { "strict_errors", h_strict },
    { "ignore_errors", h_ignore },
    { "replace_errors", h_replace },
    { "xmlcharrefreplace_errors", h_xml },
    { "backslashreplace_errors", h_backslash },
    { "namereplace_errors", h_name },
    { "surrogatepass", h_pass },
    { "surrogateescape", h_escape },
};

Registry *registry()
{
    if (reg)
        return reg;
    Registry *r = heap_new<Registry>();
    if (!r)
        return oom(), nullptr;
    reg = r;
    gc_root_hook(reg_mark);
    ListObj *l = list_new();
    if (!l)
        return oom(), nullptr;
    r->search  = obj_value(l);
    DictObj *c = dict_new();
    if (!c)
        return oom(), nullptr;
    r->cache   = obj_value(c);
    DictObj *e = dict_new();
    if (!e)
        return oom(), nullptr;
    r->errors = obj_value(e);
    for (usize i = 0; i < sizeof HANDLERS / sizeof HANDLERS[0]; i++) {
        Root fn{ native_new(HANDLERS[i].name, HANDLERS[i].fn) };
        Root nm{ str_new(HANDLER_NAMES[i]) };
        if (fn.v.is_nil() || nm.v.is_nil() ||
            dict_set(static_cast<DictObj *>(r->errors.obj()), nm.v, fn.v) != R::Ok)
            return nullptr;
    }
    return r;
}

// --------------------------------------------------------------- arguments

// Positional parameters, then keywords by name, as a clinic signature takes
// them. `npos` of them may only be positional.
bool take(const CallArgs &a, Str who, const Str *names, u32 n, u32 least, u32 npos, Value *out)
{
    for (u32 i = 0; i < n; i++)
        out[i] = Value();
    if (a.nargs > n) {
        char tmp[24];
        Buf<128> b;
        b.put(who).put("() takes at most ").put(int_text(tmp, sizeof tmp, i64(n)));
        b.put(" arguments (").put(int_text(tmp, sizeof tmp, i64(a.nargs))).put(" given)");
        return err_set("TypeError", b.str()), false;
    }
    for (u32 i = 0; i < a.nargs; i++)
        out[i] = a.args[i];
    for (u32 k = 0; k < a.nkw; k++) {
        Str nm = is_str(a.kwnames[k]) ? str_of(a.kwnames[k])->str() : Str();
        u32 i  = npos;
        while (i < n && !(names[i] == nm))
            i++;
        if (i == n) {
            Buf<128> b;
            b.put(who).put("() got an unexpected keyword argument '").put(nm).put("'");
            return err_set("TypeError", b.str()), false;
        }
        if (!out[i].is_nil()) {
            Buf<128> b;
            b.put(who).put("() got multiple values for argument '").put(nm).put("'");
            return err_set("TypeError", b.str()), false;
        }
        out[i] = a.kwvals[k];
    }
    for (u32 i = 0; i < least; i++)
        if (out[i].is_nil()) {
            char tmp[24];
            Buf<128> b;
            b.put(who).put("() takes at least ").put(int_text(tmp, sizeof tmp, i64(least)));
            b.put(" arguments (").put(int_text(tmp, sizeof tmp, i64(a.nargs))).put(" given)");
            return err_set("TypeError", b.str()), false;
        }
    return true;
}

bool want_str(Value v, Str who, u32 n, bool none_ok)
{
    if (is_str(v) || (none_ok && (v.is_nil() || is_none(v))))
        return true;
    char tmp[24];
    Buf<128> b;
    b.put(who).put("() argument ").put(int_text(tmp, sizeof tmp, i64(n)));
    b.put(none_ok ? Str(" must be str or None") : Str(" must be str"));
    return err_not(b.str(), v) == R::Ok;
}

// A buffer argument; `text` also takes a str, as its UTF-8.
bool want_data(Value v, bool text, Value &out)
{
    Str data;
    if (is_bytes(v)) {
        out = v;
        return true;
    }
    if (text && is_str(v)) {
        String b;
        if (!std_encode(str_of(v)->str(), false, b))
            return false;
        out = bytes_new(b.str());
        return !out.is_nil();
    }
    if (!bytes_like(v, data))
        return err_not("a bytes-like object is required", v, true) == R::Ok;
    out = bytes_new(data);
    return !out.is_nil();
}

// ---------------------------------------------------------------- the codecs

struct CodecFn {
    Str name;
    Codec codec;
    bool encode;
    u8 shape; // what else the function takes; see the S_* below
};

// errors only; errors and final; errors and byteorder (an encoder);
// errors, byteorder and final; errors and mapping; errors and final defaulting
// true.
enum : u8 { S_PLAIN, S_FINAL, S_ORDER, S_EX, S_MAP, S_FINAL_TRUE };

R run_codec(const CallArgs &a, const CodecFn &f, Value &out)
{
    static const Str PLAIN[] = { "", "errors" };
    static const Str FINAL[] = { "", "errors", "final" };
    static const Str ORDER[] = { "", "errors", "byteorder" };
    static const Str EX[]    = { "", "errors", "byteorder", "final" };
    static const Str MAP[]   = { "", "errors", "mapping" };
    const Str *names         = PLAIN;
    u32 n                    = 2;
    switch (f.shape) {
    case S_FINAL:
    case S_FINAL_TRUE:
        names = FINAL, n = 3;
        break;
    case S_ORDER:
        names = ORDER, n = 3;
        break;
    case S_EX:
        names = EX, n = 4;
        break;
    case S_MAP:
        names = MAP, n = 3;
        break;
    }
    Value got[4];
    if (!take(a, f.name, names, n, 1, n, got))
        return R::Err;
    if (!want_str(got[1], f.name, 2, true))
        return R::Err;
    CodecCall cc;
    cc.codec  = f.codec;
    cc.encode = f.encode;
    cc.errors = got[1];
    cc.wrap   = f.shape == S_EX ? WRAP_BYTEORDER : WRAP_CONSUMED;
    if (f.encode) {
        if (!is_str(got[0])) {
            Buf<96> b;
            b.put(f.name).put("() argument 1 must be str");
            return err_not(b.str(), got[0]);
        }
        cc.input = got[0];
    } else {
        Root data;
        bool text = f.codec == Codec::UnicodeEscape || f.codec == Codec::RawUnicodeEscape;
        if (!want_data(got[0], text, data.v))
            return R::Err;
        cc.input = data.v;
    }
    Value final = f.shape == S_EX ? got[3] : got[2];
    if (f.shape == S_FINAL || f.shape == S_EX)
        cc.final = !final.is_nil() && py_truth(final);
    if (f.shape == S_FINAL_TRUE)
        cc.final = final.is_nil() || py_truth(final);
    if (f.shape == S_ORDER || f.shape == S_EX) {
        i64 bo = 0;
        if (!got[2].is_nil() && !as_index(got[2], bo))
            return err_set2("TypeError", "byteorder must be an integer", type_name(got[2]));
        cc.byteorder = i32(bo < 0 ? -1 : bo > 0 ? 1 : 0);
    }
    if (f.shape == S_MAP)
        cc.mapping = got[2];
    // A decoder not told it is final still reports what it consumed.
    if (!f.encode && f.shape != S_EX && f.shape != S_FINAL && f.shape != S_FINAL_TRUE)
        cc.final = true;
    return codec_run(cc, out);
}

#define CODEC_FN(fn, name, codec, encode, shape)              \
    R fn(const CallArgs &a, Value &out)                       \
    {                                                         \
        static const CodecFn f{ name, codec, encode, shape }; \
        return run_codec(a, f, out);                          \
    }

CODEC_FN(c_utf8_encode, "utf_8_encode", Codec::Utf8, true, S_PLAIN)
CODEC_FN(c_utf8_decode, "utf_8_decode", Codec::Utf8, false, S_FINAL)
CODEC_FN(c_utf7_encode, "utf_7_encode", Codec::Utf7, true, S_PLAIN)
CODEC_FN(c_utf7_decode, "utf_7_decode", Codec::Utf7, false, S_FINAL)
CODEC_FN(c_utf16_encode, "utf_16_encode", Codec::Utf16, true, S_ORDER)
CODEC_FN(c_utf16le_encode, "utf_16_le_encode", Codec::Utf16Le, true, S_PLAIN)
CODEC_FN(c_utf16be_encode, "utf_16_be_encode", Codec::Utf16Be, true, S_PLAIN)
CODEC_FN(c_utf16_decode, "utf_16_decode", Codec::Utf16, false, S_FINAL)
CODEC_FN(c_utf16le_decode, "utf_16_le_decode", Codec::Utf16Le, false, S_FINAL)
CODEC_FN(c_utf16be_decode, "utf_16_be_decode", Codec::Utf16Be, false, S_FINAL)
CODEC_FN(c_utf16ex_decode, "utf_16_ex_decode", Codec::Utf16, false, S_EX)
CODEC_FN(c_utf32_encode, "utf_32_encode", Codec::Utf32, true, S_ORDER)
CODEC_FN(c_utf32le_encode, "utf_32_le_encode", Codec::Utf32Le, true, S_PLAIN)
CODEC_FN(c_utf32be_encode, "utf_32_be_encode", Codec::Utf32Be, true, S_PLAIN)
CODEC_FN(c_utf32_decode, "utf_32_decode", Codec::Utf32, false, S_FINAL)
CODEC_FN(c_utf32le_decode, "utf_32_le_decode", Codec::Utf32Le, false, S_FINAL)
CODEC_FN(c_utf32be_decode, "utf_32_be_decode", Codec::Utf32Be, false, S_FINAL)
CODEC_FN(c_utf32ex_decode, "utf_32_ex_decode", Codec::Utf32, false, S_EX)
CODEC_FN(c_uesc_encode, "unicode_escape_encode", Codec::UnicodeEscape, true, S_PLAIN)
CODEC_FN(c_uesc_decode, "unicode_escape_decode", Codec::UnicodeEscape, false, S_FINAL_TRUE)
CODEC_FN(c_raw_encode, "raw_unicode_escape_encode", Codec::RawUnicodeEscape, true, S_PLAIN)
CODEC_FN(c_raw_decode, "raw_unicode_escape_decode", Codec::RawUnicodeEscape, false, S_FINAL_TRUE)
CODEC_FN(c_latin1_encode, "latin_1_encode", Codec::Latin1, true, S_PLAIN)
CODEC_FN(c_latin1_decode, "latin_1_decode", Codec::Latin1, false, S_PLAIN)
CODEC_FN(c_ascii_encode, "ascii_encode", Codec::Ascii, true, S_PLAIN)
CODEC_FN(c_ascii_decode, "ascii_decode", Codec::Ascii, false, S_PLAIN)
CODEC_FN(c_charmap_encode, "charmap_encode", Codec::Charmap, true, S_MAP)
CODEC_FN(c_charmap_decode, "charmap_decode", Codec::Charmap, false, S_MAP)

#undef CODEC_FN

R c_charmap_build(const CallArgs &a, Value &out)
{
    if (!args_only(a, "charmap_build", 1, 1))
        return R::Err;
    if (!want_str(a.args[0], "charmap_build", 1, false))
        return R::Err;
    out = charmap_build(a.args[0]);
    return out.is_nil() ? R::Err : R::Ok;
}

R c_escape_decode(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "", "errors" };
    Value got[2];
    if (!take(a, "escape_decode", NAMES, 2, 1, 2, got) ||
        !want_str(got[1], "escape_decode", 2, true))
        return R::Err;
    Root data;
    if (!want_data(got[0], true, data.v))
        return R::Err;
    return escape_decode(static_cast<BytesObj *>(data.v.obj())->str(), got[1], out);
}

R c_escape_encode(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "", "errors" };
    Value got[2];
    if (!take(a, "escape_encode", NAMES, 2, 1, 2, got) ||
        !want_str(got[1], "escape_encode", 2, true))
        return R::Err;
    if (!is_bytes(got[0]))
        return err_not("escape_encode() argument 1 must be bytes", got[0]);
    return escape_encode(static_cast<BytesObj *>(got[0].obj())->str(), out);
}

R c_readbuffer_encode(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "", "errors" };
    Value got[2];
    if (!take(a, "readbuffer_encode", NAMES, 2, 1, 2, got) ||
        !want_str(got[1], "readbuffer_encode", 2, true))
        return R::Err;
    Root data;
    if (!want_data(got[0], true, data.v))
        return R::Err;
    TupleObj *t = tuple_new(2);
    if (!t)
        return oom();
    t->items()[0] = data.v;
    t->items()[1] = int_from_i64(static_cast<BytesObj *>(data.v.obj())->len);
    out           = obj_value(t);
    return R::Ok;
}

R c_normalize(const CallArgs &a, Value &out)
{
    if (!args_only(a, "_normalize_encoding", 1, 1))
        return R::Err;
    if (!want_str(a.args[0], "_normalize_encoding", 1, false))
        return R::Err;
    Str s = str_of(a.args[0])->str();
    String buf;
    if (!buf.reserve(s.size() + 1))
        return oom();
    usize n = 0;
    enc_normalize(s, false, buf.data(), s.size() + 1, n);
    out = str_new(Str(buf.data(), n));
    return out.is_nil() ? R::Err : R::Ok;
}

// ---------------------------------------------------------- the error names

R c_register_error(const CallArgs &a, Value &out)
{
    if (!args_only(a, "register_error", 2, 2))
        return R::Err;
    if (!want_str(a.args[0], "register_error", 1, false))
        return R::Err;
    if (!py_callable(a.args[1]))
        return err_set("TypeError", "handler must be callable");
    Registry *r = registry();
    if (!r || dict_set(static_cast<DictObj *>(r->errors.obj()), a.args[0], a.args[1]) != R::Ok)
        return R::Err;
    out = value_none();
    return R::Ok;
}

R c_lookup_error(const CallArgs &a, Value &out)
{
    if (!args_only(a, "lookup_error", 1, 1))
        return R::Err;
    if (!want_str(a.args[0], "lookup_error", 1, false))
        return R::Err;
    out = codec_lookup_error(str_of(a.args[0])->str());
    return out.is_nil() ? R::Err : R::Ok;
}

R c_unregister_error(const CallArgs &a, Value &out)
{
    if (!args_only(a, "_unregister_error", 1, 1))
        return R::Err;
    if (!want_str(a.args[0], "_unregister_error", 1, false))
        return R::Err;
    Str n = str_of(a.args[0])->str();
    for (Str b : HANDLER_NAMES)
        if (b == n) {
            Buf<128> m;
            m.put("cannot un-register built-in error handler '").put(n).put("'");
            return err_set("ValueError", m.str());
        }
    Registry *r = registry();
    if (!r)
        return R::Err;
    R d = dict_del(static_cast<DictObj *>(r->errors.obj()), a.args[0]);
    if (d == R::Err)
        return R::Err;
    out = value_bool(d == R::Ok);
    return R::Ok;
}

// ------------------------------------------------------------ the registry

R c_register(const CallArgs &a, Value &out)
{
    if (!args_only(a, "register", 1, 1))
        return R::Err;
    if (!py_callable(a.args[0]))
        return err_set("TypeError", "argument must be callable");
    Registry *r = registry();
    if (!r || !list_push(list_of(r->search), a.args[0]))
        return r ? oom() : R::Err;
    out = value_none();
    return R::Ok;
}

R c_unregister(const CallArgs &a, Value &out)
{
    if (!args_only(a, "unregister", 1, 1))
        return R::Err;
    Registry *r = registry();
    if (!r)
        return R::Err;
    Vec<Value> &xs = list_of(r->search)->items;
    for (usize i = 0; i < xs.size(); i++)
        if (xs[i] == a.args[0]) {
            for (usize k = i; k + 1 < xs.size(); k++)
                xs[k] = xs[k + 1];
            xs.pop();
            DictObj *nd = dict_new();
            if (!nd)
                return oom();
            reg->cache = obj_value(nd);
            break;
        }
    out = value_none();
    return R::Ok;
}

// What a lookup is for.
enum : u32 {
    L_ENCODE = 1 << 0, // call the encoder rather than the decoder
    L_TEXT   = 1 << 1, // str.encode or bytes.decode: a text codec, and its type
    L_ONLY   = 1 << 2, // codecs.lookup: the CodecInfo is the answer
};

// The steps of one lookup and call.
enum : u32 { ST_START, ST_IMPORTED, ST_SEARCH, ST_FOUND, ST_CALLED };

// The native codecs by the names encodings.aliases gives them, for when the
// encodings package cannot be imported at all.
struct Fallback {
    Str name;
    Codec codec;
};

constexpr Fallback FALLBACK[] = {
    { "utf_8", Codec::Utf8 },
    { "utf8", Codec::Utf8 },
    { "u8", Codec::Utf8 },
    { "utf", Codec::Utf8 },
    { "utf_16", Codec::Utf16 },
    { "utf16", Codec::Utf16 },
    { "u16", Codec::Utf16 },
    { "utf_16_le", Codec::Utf16Le },
    { "utf_16le", Codec::Utf16Le },
    { "utf_16_be", Codec::Utf16Be },
    { "utf_16be", Codec::Utf16Be },
    { "utf_32", Codec::Utf32 },
    { "utf32", Codec::Utf32 },
    { "u32", Codec::Utf32 },
    { "utf_32_le", Codec::Utf32Le },
    { "utf_32le", Codec::Utf32Le },
    { "utf_32_be", Codec::Utf32Be },
    { "utf_32be", Codec::Utf32Be },
    { "utf_7", Codec::Utf7 },
    { "utf7", Codec::Utf7 },
    { "u7", Codec::Utf7 },
    { "latin_1", Codec::Latin1 },
    { "latin1", Codec::Latin1 },
    { "iso8859_1", Codec::Latin1 },
    { "iso_8859_1", Codec::Latin1 },
    { "l1", Codec::Latin1 },
    { "ascii", Codec::Ascii },
    { "us_ascii", Codec::Ascii },
    { "646", Codec::Ascii },
    { "unicode_escape", Codec::UnicodeEscape },
    { "raw_unicode_escape", Codec::RawUnicodeEscape },
};

Codec fallback_of(Str encoding)
{
    char buf[32];
    usize n = 0;
    if (!enc_normalize(encoding, true, buf, sizeof buf, n))
        return Codec::None;
    for (const Fallback &f : FALLBACK)
        if (f.name == Str(buf, n))
            return f.codec;
    return Codec::None;
}

// The CodecInfo's item `i`: it is a tuple, or a class deriving from one.
Value info_item(Value info, usize i)
{
    Value t = method_self(info);
    return static_cast<TupleObj *>(t.obj())->items()[i];
}

// CodecInfo._is_text_encoding, which the instance or its class may hold.
bool is_text_codec(Value info)
{
    if (!is_inst(info))
        return true;
    StrObj *n = str_intern("_is_text_encoding");
    if (!n)
        return true;
    Value got;
    Value d = inst_of(info)->dict;
    if (!d.is_nil() && dict_get(static_cast<DictObj *>(d.obj()), obj_value(n), got) == R::Ok)
        return py_truth(got);
    if (type_lookup(inst_of(info)->cls, n, got) == R::Ok)
        return py_truth(got);
    err_clear();
    return true;
}

u32 how_of(const ContObj *k)
{
    return u32(k->s[5].as_int());
}

R lookup_step(ContObj *k, Value in)
{
    Root kv{ obj_value(k) }, rin{ in };
    Registry *r = registry();
    if (!r)
        return R::Err;
    for (;;) {
        k = cont_of(kv.v);
        switch (k->i) {
        case ST_START: {
            // What 3.14 hands a search function: _Py_normalize_encoding's
            // form, in lower case.
            Str e = str_of(k->s[1])->str();
            String n;
            usize len = 0;
            for (usize i = 0; i <= e.size(); i++)
                if (!n.push('\0'))
                    return oom();
            enc_normalize(e, true, n.data(), n.size(), len);
            n.truncate(len);
            Value norm = str_new(n.str());
            if (norm.is_nil())
                return R::Err;
            cont_of(kv.v)->s[3] = norm;
            Value hit;
            R g = dict_get(static_cast<DictObj *>(reg->cache.obj()), norm, hit);
            if (g == R::Err)
                return R::Err;
            k = cont_of(kv.v);
            if (g == R::Ok) {
                k->s[4] = hit;
                k->i    = ST_FOUND;
                continue;
            }
            if (!reg->imported) {
                reg->imported = true;
                StrObj *imp   = str_intern("__import__");
                Value fn;
                if (!imp || dict_get(builtins_dict(), obj_value(imp), fn) != R::Ok)
                    return err_pending() ? R::Err : oom();
                Value mod = str_new("encodings");
                if (mod.is_nil())
                    return R::Err;
                k           = cont_of(kv.v);
                k->i        = ST_IMPORTED;
                k->catching = CATCH_ANY;
                return cont_call(k, fn, mod);
            }
            k->i = ST_SEARCH;
            k->j = 0;
            continue;
        }
        case ST_IMPORTED:
            // No library to import is not a failure: the native codecs remain.
            if (!k->caught.is_nil()) {
                const ExcType *t = exc_type_of(k->caught);
                if (!t || !exc_is(t, exc_find("ImportError"))) {
                    Value c   = k->caught;
                    k->caught = Value();
                    return err_set_value(c);
                }
                k->caught = Value();
            }
            k->catching = CATCH_NONE;
            k->i        = ST_SEARCH;
            k->j        = 0;
            continue;
        case ST_SEARCH: {
            Vec<Value> &xs = list_of(reg->search)->items;
            if (k->j < xs.size()) {
                k->i = ST_FOUND + 100; // a search function's answer
                return cont_call(k, xs[k->j], k->s[3]);
            }
            // Nothing found. The native codecs answer where the library could
            // not be imported.
            Codec c = xs.size() ? Codec::None : fallback_of(str_of(k->s[1])->str());
            if (c != Codec::None && !(how_of(k) & L_ONLY)) {
                CodecCall cc;
                cc.codec  = c;
                cc.encode = (how_of(k) & L_ENCODE) != 0;
                cc.input  = k->s[0];
                cc.errors = k->s[2];
                if (!cc.encode && !is_bytes(cc.input)) {
                    Str data;
                    if (!bytes_like(cc.input, data))
                        return err_set2("TypeError", "a bytes-like object is required",
                                        type_name(cc.input));
                }
                if (cc.encode && !is_str(cc.input))
                    return err_set2("TypeError", "utf_8_encode() argument 1 must be str",
                                    type_name(cc.input));
                Root got;
                if (codec_run(cc, got.v) != R::Ok)
                    return R::Err;
                return cont_done(cont_of(kv.v), got.v);
            }
            Buf<256> m;
            m.put("unknown encoding: ").put(str_of(k->s[1])->str());
            return err_set("LookupError", m.str());
        }
        case ST_FOUND + 100: {
            if (is_none(rin.v)) {
                k->j++;
                k->i = ST_SEARCH;
                continue;
            }
            Value t = is_tuple(rin.v) ? rin.v : method_self(rin.v);
            if (!is_tuple(t) || static_cast<TupleObj *>(t.obj())->len != 4)
                return err_set("TypeError", "codec search functions must return 4-tuples");
            if (dict_set(static_cast<DictObj *>(reg->cache.obj()), k->s[3], rin.v) != R::Ok)
                return R::Err;
            k       = cont_of(kv.v);
            k->s[4] = rin.v;
            k->i    = ST_FOUND;
            continue;
        }
        case ST_FOUND: {
            if (how_of(k) & L_ONLY)
                return cont_done(k, k->s[4]);
            bool encode = (how_of(k) & L_ENCODE) != 0;
            if ((how_of(k) & L_TEXT) && !is_text_codec(k->s[4])) {
                Buf<256> m;
                m.put('\'').put(str_of(k->s[1])->str()).put("' is not a text encoding; use ");
                m.put(encode ? Str("codecs.encode()") : Str("codecs.decode()"));
                m.put(" to handle arbitrary codecs");
                return err_set("LookupError", m.str());
            }
            k           = cont_of(kv.v);
            Value fn    = info_item(k->s[4], encode ? 0 : 1);
            k->i        = ST_CALLED;
            k->catching = CATCH_ANY;
            if (k->s[2].is_nil())
                return cont_call(k, fn, k->s[0]);
            return cont_call(k, fn, k->s[0], 2, k->s[2]);
        }
        case ST_CALLED: {
            bool encode = (how_of(k) & L_ENCODE) != 0;
            k->catching = CATCH_NONE;
            if (!k->caught.is_nil()) {
                Root c{ k->caught };
                k->caught = Value();
                if (is_exc(c.v)) {
                    Buf<256> m;
                    m.put(encode ? Str("encoding") : Str("decoding")).put(" with '");
                    m.put(str_of(k->s[1])->str()).put("' codec failed");
                    if (!exc_note(c.v, m.str()))
                        err_clear();
                }
                return err_set_value(c.v);
            }
            Value t = is_tuple(rin.v) ? rin.v : Value();
            if (t.is_nil() || static_cast<TupleObj *>(t.obj())->len != 2)
                return err_set("TypeError", encode ? Str("encoder must return a tuple (object, "
                                                         "integer)")
                                                   : Str("decoder must return a tuple "
                                                         "(object,integer)"));
            Value v = static_cast<TupleObj *>(t.obj())->items()[0];
            if (how_of(k) & L_TEXT) {
                if (encode && is_bytearray(v)) {
                    v = bytes_new(array_of(v)->str());
                    if (v.is_nil())
                        return R::Err;
                } else if (encode ? !is_bytes(v) : !is_str(v)) {
                    Buf<256> m;
                    m.put('\'').put(str_of(k->s[1])->str()).put(encode ? "' encoder" : "' decoder");
                    m.put(" returned '").put(type_name(v)).put("' instead of '");
                    m.put(encode ? Str("bytes") : Str("str")).put("'; use ");
                    m.put(encode ? Str("codecs.encode() to encode")
                                 : Str("codecs.decode() to decode"));
                    m.put(" to arbitrary types");
                    return err_set("TypeError", m.str());
                }
            }
            return cont_done(k, v);
        }
        }
        return err_set("SystemError", "a codec lookup lost its place");
    }
}

// A lookup through the registry, and the call it leads to.
R via_registry(Value obj, Value encoding, Value errors, u32 how, Value &out)
{
    Root ro{ obj }, re{ encoding }, rr{ errors };
    if (!registry())
        return R::Err;
    Root kv{ cont_new(lookup_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = ro.v;
    k->s[1]    = re.v;
    k->s[2]    = rr.v.is_nil() || is_none(rr.v) ? Value() : rr.v;
    k->s[5]    = Value::of_int(i32(how));
    k->i       = ST_START;
    out        = kv.v;
    return R::Ok;
}

R c_lookup(const CallArgs &a, Value &out)
{
    if (!args_only(a, "lookup", 1, 1))
        return R::Err;
    if (!want_str(a.args[0], "lookup", 1, false))
        return R::Err;
    return via_registry(Value(), a.args[0], Value(), L_ONLY, out);
}

R generic(const CallArgs &a, Str who, bool encode, Value &out)
{
    static const Str NAMES[] = { "obj", "encoding", "errors" };
    Value got[3];
    if (!take(a, who, NAMES, 3, 1, 0, got))
        return R::Err;
    if (!got[1].is_nil() && !want_str(got[1], who, 2, false))
        return R::Err;
    if (!got[2].is_nil() && !want_str(got[2], who, 3, false))
        return R::Err;
    Root enc{ got[1].is_nil() ? str_new("utf-8") : got[1] };
    if (enc.v.is_nil())
        return R::Err;
    return via_registry(got[0], enc.v, got[2], encode ? L_ENCODE : 0, out);
}

R c_encode(const CallArgs &a, Value &out)
{
    return generic(a, "encode", true, out);
}

R c_decode(const CallArgs &a, Value &out)
{
    return generic(a, "decode", false, out);
}

constexpr ModDef DEFS[] = {
    { "register", c_register },
    { "unregister", c_unregister },
    { "lookup", c_lookup },
    { "encode", c_encode },
    { "decode", c_decode },
    { "escape_encode", c_escape_encode },
    { "escape_decode", c_escape_decode },
    { "utf_8_encode", c_utf8_encode },
    { "utf_8_decode", c_utf8_decode },
    { "utf_7_encode", c_utf7_encode },
    { "utf_7_decode", c_utf7_decode },
    { "utf_16_encode", c_utf16_encode },
    { "utf_16_le_encode", c_utf16le_encode },
    { "utf_16_be_encode", c_utf16be_encode },
    { "utf_16_decode", c_utf16_decode },
    { "utf_16_le_decode", c_utf16le_decode },
    { "utf_16_be_decode", c_utf16be_decode },
    { "utf_16_ex_decode", c_utf16ex_decode },
    { "utf_32_encode", c_utf32_encode },
    { "utf_32_le_encode", c_utf32le_encode },
    { "utf_32_be_encode", c_utf32be_encode },
    { "utf_32_decode", c_utf32_decode },
    { "utf_32_le_decode", c_utf32le_decode },
    { "utf_32_be_decode", c_utf32be_decode },
    { "utf_32_ex_decode", c_utf32ex_decode },
    { "unicode_escape_encode", c_uesc_encode },
    { "unicode_escape_decode", c_uesc_decode },
    { "raw_unicode_escape_encode", c_raw_encode },
    { "raw_unicode_escape_decode", c_raw_decode },
    { "latin_1_encode", c_latin1_encode },
    { "latin_1_decode", c_latin1_decode },
    { "ascii_encode", c_ascii_encode },
    { "ascii_decode", c_ascii_decode },
    { "charmap_encode", c_charmap_encode },
    { "charmap_decode", c_charmap_decode },
    { "charmap_build", c_charmap_build },
    { "readbuffer_encode", c_readbuffer_encode },
    { "register_error", c_register_error },
    { "lookup_error", c_lookup_error },
    { "_unregister_error", c_unregister_error },
    { "_normalize_encoding", c_normalize },
};

} // namespace

Value codec_lookup_error(Str name)
{
    Registry *r = registry();
    if (!r)
        return Value();
    Root key{ str_new(name) };
    if (key.v.is_nil())
        return Value();
    Value got;
    R g = dict_get(static_cast<DictObj *>(r->errors.obj()), key.v, got);
    if (g == R::Err)
        return Value();
    if (g == R::NotImpl) {
        Buf<512> m;
        m.put("unknown error handler name '").put(name).put("'");
        return err_set("LookupError", m.str()), Value();
    }
    return got;
}

R text_encode(Value s, Value encoding, Value errors, Value &out)
{
    if (!encoding.is_nil() && !is_str(encoding))
        return err_not("encode() argument 'encoding' must be str", encoding);
    if (!errors.is_nil() && !is_str(errors))
        return err_not("encode() argument 'errors' must be str", errors);
    Codec c = encoding.is_nil() ? Codec::Utf8 : codec_shortcut(str_of(encoding)->str());
    if (c == Codec::None)
        return via_registry(s, encoding, errors, L_ENCODE | L_TEXT, out);
    CodecCall cc;
    cc.codec  = c;
    cc.encode = true;
    cc.input  = s;
    cc.errors = errors;
    return codec_run(cc, out);
}

R text_decode(Value obj, Value encoding, Value errors, Value &out)
{
    if (!encoding.is_nil() && !is_str(encoding))
        return err_not("decode() argument 'encoding' must be str", encoding);
    if (!errors.is_nil() && !is_str(errors))
        return err_not("decode() argument 'errors' must be str", errors);
    if (is_str(obj))
        return err_set("TypeError", "decoding str is not supported");
    Str data;
    if (!bytes_like(obj, data))
        return err_not("decoding to str: need a bytes-like object", obj);
    Codec c = encoding.is_nil() ? Codec::Utf8 : codec_shortcut(str_of(encoding)->str());
    if (c == Codec::None) {
        Root b{ is_bytes(obj) ? obj : bytes_new(data) };
        if (b.v.is_nil())
            return R::Err;
        return via_registry(b.v, encoding, errors, L_TEXT, out);
    }
    CodecCall cc;
    cc.codec  = c;
    cc.input  = obj;
    cc.errors = errors;
    return codec_run(cc, out);
}

bool codecs_install(DictObj *into)
{
    return registry() && mod_defs(into, DEFS);
}
