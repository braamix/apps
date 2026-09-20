// `_zstd`: the floor under compression.zstd, over braam::zstd, which is
// libzstd 1.6.0 vendored verbatim -- so the output is libzstd's byte for byte.
//
// Unlike zlib's and bzip2's there is no Braam-shaped pair over this one:
// zstd/zstd.h is libzstd's C API, a context and two cursors, and this module
// calls it as C does. Three things bite, and each is answered here:
//
//   - ZSTD_decompressStream answers 0 when a frame ends and not after, so the
//     loop stops on "input used up", never on a second call that would read
//     the next frame's header that is not there.
//   - ZSTD_getErrorName returns a const char *, and nothing in this
//     interpreter defines strlen, so `c_len` counts to the NUL itself.
//   - There are no threads, so ZSTD_c_nbWorkers accepts 0 alone, and the
//     dictionary builder (zdict.h) is not in the library: train_dict and
//     finalize_dict say so rather than pretend.
#include "bigint.h"
#include "exc.h"
#include "gc.h"
#include "info.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "posix.h"
#include "type.h"
#include "zstd/lib/zstd_errors.h"
#include "zstd/zstd.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// libzstd hands back C strings and nothing here defines strlen; the attribute
// stops the compiler turning this loop back into a call to it.
__attribute__((no_builtin("strlen"))) usize c_len(const char *s)
{
    usize n = 0;
    while (s && s[n])
        n++;
    return n;
}

Str c_str(const char *s)
{
    return Str(s, c_len(s));
}

struct Home {
    Value error;  // _zstd.ZstdError
    Value cparam; // the CompressionParameter enum, from set_parameter_types
    Value dparam; // and DecompressionParameter
};

Home *home;

void home_mark()
{
    if (!home)
        return;
    gc_mark(home->error);
    gc_mark(home->cparam);
    gc_mark(home->dparam);
}

R zstd_raise(Str msg)
{
    if (home && !home->error.is_nil())
        return mod_raise(home->error, msg);
    return err_set("Exception", msg);
}

R zstd_error(Str what, usize code)
{
    Buf<256> b;
    b.put(what).put(": ").put(c_str(ZSTD_getErrorName(code)));
    return zstd_raise(b.str());
}

// ------------------------------------------------------------- the buffers

constexpr usize CHUNK = 8192;

struct Growing {
    Vec<u8> out;
    usize at = 0;

    bool room(usize want) { return out.size() >= at + want || out.resize(at + want); }

    Str str() const { return Str(reinterpret_cast<const char *>(out.data()), at); }
};

bool grow_once(Growing &g, ZSTD_outBuffer &o, usize limit)
{
    usize want = g.out.size() ? g.out.size() : CHUNK;
    if (limit && g.at + want > limit)
        want = limit - g.at;
    if (!g.room(want))
        return false;
    o.dst  = g.out.data();
    o.size = g.out.size();
    o.pos  = g.at;
    return true;
}

// A buffer that will not grow is usually one a dropped codec object is
// still holding megabytes from: collect and try once more.
bool grow_for(Growing &g, ZSTD_outBuffer &o, usize limit)
{
    return grow_once(g, o, limit) || (gc_collect(), grow_once(g, o, limit));
}

// ---------------------------------------------------------------- arguments

bool bytes_arg(Value v, Str who, Str what, Str &out)
{
    if (buffer_like(v, out))
        return true;
    if (err_pending())
        return false;
    Buf<160> b;
    b.put(who).put("() argument '").put(what).put("' must be a bytes-like object, not '");
    b.put(type_name(v)).put("'");
    return err_set("TypeError", b.str()) == R::Ok;
}

bool int_arg(Value v, Str who, Str what, i64 &out)
{
    if (!v.is_nil() && as_int_arg(v, out))
        return true;
    if (err_pending())
        return false;
    if (is_intval(v))
        return err_set("OverflowError", "Python int too large to convert to C int") == R::Ok;
    Buf<160> b;
    b.put(who).put("() argument '").put(what).put("' must be an integer, not '");
    b.put(type_name(v)).put("'");
    return err_set("TypeError", b.str()) == R::Ok;
}

bool given(Value v)
{
    return !v.is_nil() && !is_none(v);
}

bool positional_only(const CallArgs &a, Str who, Str name)
{
    for (u32 k = 0; k < a.nkw; k++)
        if (is_str(a.kwnames[k]) && str_of(a.kwnames[k])->str() == name) {
            Buf<128> b;
            b.put(who).put("() got an unexpected keyword argument '").put(name).put("'");
            return err_set("TypeError", b.str()) == R::Ok;
        }
    return true;
}

// An options dict: {CompressionParameter.x: value}. The key is an enum member
// whose value is the ZSTD_c_* number, or the number itself.
bool each_option(Value opts, bool compressing, void *ctx, bool (*set)(void *ctx, i32 key, i32 val))
{
    Root ro{ opts };
    if (!is_anydict(ro.v))
        return err_set("TypeError", "invalid type for options, expected dict") == R::Ok;
    ListObj *keys = py_list_of(ro.v);
    if (!keys)
        return false;
    Root rk{ obj_value(keys) };
    for (usize i = 0; i < list_of(rk.v)->items.size(); i++) {
        Root key{ list_of(rk.v)->items[i] };
        Value val;
        if (dict_get(static_cast<DictObj *>(ro.v.obj()), key.v, val) != R::Ok)
            return false;
        Root rv{ val };
        Value wrong = compressing ? home->dparam : home->cparam;
        if (!wrong.is_nil() && type_isinstance(key.v, wrong))
            return err_set("TypeError",
                           compressing ? Str("compression options dictionary key must not be a "
                                             "DecompressionParameter attribute")
                                       : Str("decompression options dictionary key must not be a "
                                             "CompressionParameter attribute")) == R::Ok;
        i64 k = 0, n = 0;
        if (!as_int_arg(key.v, k))
            return err_set2("TypeError",
                            compressing ? Str("key of the options dict should be a "
                                              "CompressionParameter attribute")
                                        : Str("key of the options dict should be a "
                                              "DecompressionParameter attribute"),
                            type_name(key.v)) == R::Ok;
        if (!as_int_arg(rv.v, n)) {
            if (err_pending())
                return false;
            if (!is_intval(rv.v))
                return err_set2("TypeError", "value of the options dict should be an int",
                                type_name(rv.v)) == R::Ok;
            return err_set("OverflowError", "Python int too large to convert to C int") == R::Ok;
        }
        if (n > 0x7fffffffll || n < -0x80000000ll)
            return err_set("OverflowError", "Python int too large to convert to C int") == R::Ok;
        if (!set(ctx, i32(k), i32(n)))
            return false;
    }
    return true;
}

// -------------------------------------------------------------- ZstdDict

extern const Type zdict_type;

struct ZDictObj : Obj {
    Value content; // bytes
    ZSTD_CDict *cd;
    ZSTD_DDict *dd;
    u32 id;
    bool is_raw;
};

ZDictObj *zdict_of(Value v)
{
    return static_cast<ZDictObj *>(v.obj());
}

void zdict_trace(Obj *o)
{
    gc_mark(static_cast<ZDictObj *>(o)->content);
}

void zdict_fini(Obj *o)
{
    ZDictObj *d = static_cast<ZDictObj *>(o);
    if (d->cd)
        ZSTD_freeCDict(d->cd);
    if (d->dd)
        ZSTD_freeDDict(d->dd);
}

R zdict_repr(Value v, String &out)
{
    char buf[24];
    Buf<80> b;
    b.put("<ZstdDict dict_id=").put(int_text(buf, sizeof buf, i64(zdict_of(v)->id)));
    Str c;
    buffer_like(zdict_of(v)->content, c);
    b.put(" dict_size=").put(int_text(buf, sizeof buf, i64(c.size()))).put(">");
    return out.append(b.str()) ? R::Ok : oom();
}

R zdict_len(Value v, usize &out)
{
    Str c;
    if (!buffer_like(zdict_of(v)->content, c))
        return R::Err;
    out = c.size();
    return R::Ok;
}

R zdict_getattr(Value v, StrObj *name, Value &out)
{
    ZDictObj *d = zdict_of(v);
    if (name->str() == "dict_content")
        return out = d->content, R::Ok;
    if (name->str() == "dict_id") {
        out = int_from_i64(i64(d->id));
        return out.is_nil() ? R::Err : R::Ok;
    }
    // A digested, an undigested and a prefix dictionary are the same content
    // used three ways; the tuple says which, as CPython's do.
    if (name->str() == "as_digested_dict" || name->str() == "as_undigested_dict" ||
        name->str() == "as_prefix") {
        TupleObj *t = tuple_new(2);
        if (!t)
            return R::Err;
        t->items()[0] = v;
        t->items()[1] = Value::of_int(name->str() == "as_digested_dict"     ? 0
                                      : name->str() == "as_undigested_dict" ? 1
                                                                            : 2);
        out           = obj_value(t);
        return R::Ok;
    }
    return R::NotImpl;
}

constexpr Type zdict_type{ .name    = "_zstd.ZstdDict",
                           .trace   = zdict_trace,
                           .fini    = zdict_fini,
                           .repr    = zdict_repr,
                           .len     = zdict_len,
                           .getattr = zdict_getattr };

R b_zdict_new(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "dict_content", "is_raw" };
    Value v[2];
    if (a.nargs > 1)
        return err_set("TypeError", "ZstdDict() takes 1 positional argument but more were given");
    if (!fn_take(a, "ZstdDict", NAMES, 1, v))
        return R::Err;
    Str content;
    if (!bytes_arg(v[0], "ZstdDict", "dict_content", content))
        return R::Err;
    bool is_raw = given(v[1]) && py_truth(v[1]);
    if (!is_raw && content.size() < 8)
        return err_set("ValueError",
                       "Zstandard dictionary content should at least 8 bytes. Maybe you "
                       "should set is_raw parameter to True.");
    Root rc{ bytes_new(content) };
    if (rc.v.is_nil())
        return R::Err;
    ZDictObj *d = static_cast<ZDictObj *>(obj_alloc(&zdict_type, sizeof(ZDictObj)));
    if (!d)
        return oom();
    d->content = rc.v;
    d->cd      = nullptr;
    d->dd      = nullptr;
    d->is_raw  = is_raw;
    d->id      = ZSTD_getDictID_fromDict(content.data(), content.size());
    out        = obj_value(d);
    return R::Ok;
}

// The dictionary a compressor or decompressor was handed: the object itself,
// or the (dict, how) tuple one of the three attributes makes.
bool dict_arg(Value v, Value &dict, i32 &how)
{
    how = 0;
    if (is_tuple(v) && static_cast<TupleObj *>(v.obj())->len == 2) {
        TupleObj *t = static_cast<TupleObj *>(v.obj());
        i64 n       = 0;
        if (t->items()[0].is_obj() && t->items()[0].obj()->type == &zdict_type &&
            as_int_arg(t->items()[1], n)) {
            dict = t->items()[0];
            how  = i32(n);
            return true;
        }
    }
    if (v.is_obj() && v.obj()->type == &zdict_type) {
        dict = v;
        return true;
    }
    return err_set2("TypeError", "zstd_dict argument should be a ZstdDict object", type_name(v)) ==
           R::Ok;
}

// ---------------------------------------------------------- ZstdCompressor

extern const Type comp_type;

struct CompObj : Obj {
    ZSTD_CCtx *c;
    Value dict; // held so its CDict outlives this
    i32 last_mode;
    bool started; // a step has been taken, so the window is allocated
};

CompObj *comp_of(Value v)
{
    return static_cast<CompObj *>(v.obj());
}

void comp_trace(Obj *o)
{
    gc_mark(static_cast<CompObj *>(o)->dict);
}

void comp_fini(Obj *o)
{
    if (static_cast<CompObj *>(o)->c)
        ZSTD_freeCCtx(static_cast<CompObj *>(o)->c);
}

R comp_repr(Value v, String &out)
{
    char buf[24];
    Buf<72> b;
    b.put("<ZstdCompressor object at ").put(addr_text(buf, sizeof buf, v.obj())).put(">");
    return out.append(b.str()) ? R::Ok : oom();
}

enum : i32 { MODE_CONTINUE = 0, MODE_FLUSH_BLOCK = 1, MODE_FLUSH_FRAME = 2 };

R comp_getattr(Value v, StrObj *name, Value &out)
{
    if (name->str() == "last_mode") {
        out = Value::of_int(comp_of(v)->last_mode);
        return R::Ok;
    }
    return R::NotImpl;
}

// A constant in a native type's own namespace, which is where the class
// reaches it: ZstdCompressor.FLUSH_FRAME, not an instance's.
bool class_int(const Type *t, Str name, i64 v)
{
    Root w{ type_wrap(t) };
    if (w.v.is_nil() || type_obj(w.v)->dict.is_nil())
        return false;
    Root key{ obj_value(str_intern(name)) };
    Root val{ int_from_i64(v) };
    if (key.v.is_nil() || val.v.is_nil())
        return false;
    return dict_set(static_cast<DictObj *>(type_obj(w.v)->dict.obj()), key.v, val.v) == R::Ok;
}

constexpr Type comp_type{ .name    = "_zstd.ZstdCompressor",
                          .trace   = comp_trace,
                          .fini    = comp_fini,
                          .repr    = comp_repr,
                          .getattr = comp_getattr };

CompObj *self_comp(const CallArgs &a, Str who)
{
    Value s = method_self(a.args[0]);
    if (!s.is_obj() || s.obj()->type != &comp_type)
        return err_set2("TypeError", "descriptor requires a ZstdCompressor", who), nullptr;
    return comp_of(s);
}

R compress_run(Value self, Str in, i32 mode, Value &out)
{
    CompObj *c            = comp_of(self);
    ZSTD_EndDirective end = mode == MODE_FLUSH_FRAME   ? ZSTD_e_end
                            : mode == MODE_FLUSH_BLOCK ? ZSTD_e_flush
                                                       : ZSTD_e_continue;
    if (!c->started) {
        // libzstd grows its window on the first step, megabytes of it, and a
        // context another stream has dropped may not be swept yet. There is
        // no retrying afterwards: a context that has answered an error has to
        // be reset, which would restart the frame.
        c->started = true;
        gc_collect();
        c = comp_of(self);
    }
    ZSTD_inBuffer src{ in.data(), in.size(), 0 };
    Growing g;
    for (;;) {
        ZSTD_outBuffer dst{};
        if (!grow_for(g, dst, 0))
            return oom();
        usize left = ZSTD_compressStream2(c->c, &dst, &src, end);
        g.at       = dst.pos;
        if (ZSTD_isError(left))
            return zstd_error("Unable to compress zstd data", left);
        if (end == ZSTD_e_continue ? src.pos == src.size : left == 0)
            break;
        // Only a full output buffer means there is more to put out.
        if (dst.pos < dst.size)
            break;
    }
    comp_of(self)->last_mode = mode;
    out                      = bytes_new(g.str());
    return out.is_nil() ? R::Err : R::Ok;
}

R cm_compress(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "data", "mode" };
    Value v[2];
    CompObj *c = self_comp(a, "compress");
    Str data;
    if (!c || !positional_only(a, "compress", "data") || !meth_take(a, "compress", NAMES, 1, v) ||
        !bytes_arg(v[0], "compress", "data", data))
        return R::Err;
    i64 mode = MODE_CONTINUE;
    if (given(v[1]) && !int_arg(v[1], "compress", "mode", mode))
        return R::Err;
    if (mode != MODE_CONTINUE && mode != MODE_FLUSH_BLOCK && mode != MODE_FLUSH_FRAME)
        return err_set("ValueError",
                       "mode argument wrong value, it should be one of "
                       "ZstdCompressor.CONTINUE, ZstdCompressor.FLUSH_BLOCK, "
                       "ZstdCompressor.FLUSH_FRAME.");
    Root rs{ method_self(a.args[0]) };
    return compress_run(rs.v, data, i32(mode), out);
}

R cm_flush(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "mode" };
    Value v[1];
    CompObj *c = self_comp(a, "flush");
    if (!c || !meth_take(a, "flush", NAMES, 0, v))
        return R::Err;
    i64 mode = MODE_FLUSH_FRAME;
    if (given(v[0]) && !int_arg(v[0], "flush", "mode", mode))
        return R::Err;
    if (mode != MODE_FLUSH_BLOCK && mode != MODE_FLUSH_FRAME)
        return err_set("ValueError",
                       "mode argument wrong value, it should be "
                       "ZstdCompressor.FLUSH_FRAME or ZstdCompressor.FLUSH_BLOCK.");
    Root rs{ method_self(a.args[0]) };
    return compress_run(rs.v, Str(), i32(mode), out);
}

R cm_set_pledged(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "size" };
    Value v[1];
    CompObj *c = self_comp(a, "set_pledged_input_size");
    if (!c || !meth_take(a, "set_pledged_input_size", NAMES, 1, v))
        return R::Err;
    u64 size = ZSTD_CONTENTSIZE_UNKNOWN;
    if (given(v[0])) {
        i64 n = 0;
        if (as_int_arg(v[0], n)) {
            if (n < 0)
                return err_set("ValueError",
                               "size argument should be a positive int less than 2**64");
            size = u64(n);
        } else if (is_big(v[0]) && !big_of(v[0])->neg && big_of(v[0])->len <= 2) {
            // Between 2**63 and 2**64: two limbs, which an i64 cannot hold.
            BigObj *b = big_of(v[0]);
            size      = u64(b->limbs()[0]);
            if (b->len == 2)
                size |= u64(b->limbs()[1]) << 32;
        } else {
            return err_set("ValueError", "size argument should be a positive int less than 2**64");
        }
        // The top two values are libzstd's own sentinels: 2**64-1 is
        // "unknown", which None asks for, and 2**64-2 is "error".
        if (size >= ZSTD_CONTENTSIZE_ERROR)
            return err_set("ValueError", "size argument should be a positive int less than 2**64");
    }
    if (c->last_mode != MODE_FLUSH_FRAME)
        return zstd_raise(
            "Only a ZstdCompressor at the start of a frame can have its "
            "pledged input size set");
    usize r = ZSTD_CCtx_setPledgedSrcSize(c->c, size);
    if (ZSTD_isError(r))
        return zstd_error("Unable to set pledged input size", r);
    out = value_none();
    return R::Ok;
}

constexpr Method COMP_METHODS[] = {
    { "compress", cm_compress },
    { "flush", cm_flush },
    { "set_pledged_input_size", cm_set_pledged },
};

// What CPython's CompressionParameter and DecompressionParameter call each
// one, since the message quotes the name and not the number.
Str param_name(i32 key, bool compressing)
{
    struct Named {
        i32 key;
        Str name;
    };
    static const Named C[] = {
        { ZSTD_c_compressionLevel, "compression_level" },
        { ZSTD_c_windowLog, "window_log" },
        { ZSTD_c_hashLog, "hash_log" },
        { ZSTD_c_chainLog, "chain_log" },
        { ZSTD_c_searchLog, "search_log" },
        { ZSTD_c_minMatch, "min_match" },
        { ZSTD_c_targetLength, "target_length" },
        { ZSTD_c_strategy, "strategy" },
        { ZSTD_c_enableLongDistanceMatching, "enable_long_distance_matching" },
        { ZSTD_c_ldmHashLog, "ldm_hash_log" },
        { ZSTD_c_ldmMinMatch, "ldm_min_match" },
        { ZSTD_c_ldmBucketSizeLog, "ldm_bucket_size_log" },
        { ZSTD_c_ldmHashRateLog, "ldm_hash_rate_log" },
        { ZSTD_c_contentSizeFlag, "content_size_flag" },
        { ZSTD_c_checksumFlag, "checksum_flag" },
        { ZSTD_c_dictIDFlag, "dict_id_flag" },
        { ZSTD_c_nbWorkers, "nb_workers" },
        { ZSTD_c_jobSize, "job_size" },
        { ZSTD_c_overlapLog, "overlap_log" },
    };
    static const Named D[] = { { ZSTD_d_windowLogMax, "window_log_max" } };
    const Named *t         = compressing ? C : D;
    usize n                = compressing ? sizeof C / sizeof C[0] : sizeof D / sizeof D[0];
    for (usize i = 0; i < n; i++)
        if (t[i].key == key)
            return t[i].name;
    return Str();
}

// The level has a sentence of its own. `named` says whether the number is
// worth quoting: an int past a C int has none CPython would print.
R bad_level(i64 val, bool named)
{
    ZSTD_bounds b = ZSTD_cParam_getBounds(ZSTD_c_compressionLevel);
    char v[24], lo[24], hi[24];
    Buf<160> m;
    m.put("illegal compression level");
    if (named)
        m.put(" ").put(int_text(v, sizeof v, val));
    m.put("; the valid range is [").put(int_text(lo, sizeof lo, i64(b.lowerBound)));
    m.put(", ").put(int_text(hi, sizeof hi, i64(b.upperBound))).put("]");
    return err_set("ValueError", m.str());
}

// The bounds are libzstd's own, and a value outside them is the caller's
// mistake rather than the library's: CPython checks them the same way.
bool in_bounds(i32 key, i32 val, bool compressing)
{
    ZSTD_bounds b = compressing ? ZSTD_cParam_getBounds(ZSTD_cParameter(key))
                                : ZSTD_dParam_getBounds(ZSTD_dParameter(key));
    char n[24], lo[24], hi[24];
    if (ZSTD_isError(b.error)) {
        // libzstd has no bounds for it because it is not a parameter at all.
        Buf<160> m;
        m.put(compressing ? Str("invalid compression parameter ")
                          : Str("invalid decompression parameter "));
        m.put("'unknown parameter (key ").put(int_text(n, sizeof n, i64(key))).put(")'");
        return err_set("ValueError", m.str()) == R::Ok;
    }
    if (val >= b.lowerBound && val <= b.upperBound)
        return true;
    // The level has a sentence of its own, as CPython gives it.
    if (compressing && key == ZSTD_c_compressionLevel)
        return bad_level(val, true) == R::Ok;
    Buf<220> m;
    Str pname = param_name(key, compressing);
    m.put(compressing ? Str("compression parameter '") : Str("decompression parameter '"));
    m.put(pname.empty() ? Str("unknown") : pname).put("' received an illegal value ");
    m.put(int_text(lo, sizeof lo, i64(val)));
    m.put("; the valid range is [").put(int_text(lo, sizeof lo, i64(b.lowerBound)));
    m.put(", ").put(int_text(hi, sizeof hi, i64(b.upperBound))).put("]");
    return err_set("ValueError", m.str()) == R::Ok;
}

bool set_cparam(void *ctx, i32 key, i32 val)
{
    // There are no threads, so a worker count of anything but zero is a
    // promise this build cannot keep.
    if (key == ZSTD_c_nbWorkers && val != 0)
        return err_set("ValueError",
                       "ZSTD_c_nbWorkers is 0 here: this build of libzstd has no "
                       "threads") == R::Ok;
    if (!in_bounds(key, val, true))
        return false;
    usize r = ZSTD_CCtx_setParameter(static_cast<ZSTD_CCtx *>(ctx), ZSTD_cParameter(key), val);
    if (ZSTD_isError(r)) {
        char n[24];
        Buf<200> b;
        b.put("Error when setting zstd compression parameter ");
        b.put(int_text(n, sizeof n, i64(key))).put(": ");
        b.put(c_str(ZSTD_getErrorName(r)));
        return zstd_raise(b.str()) == R::Ok;
    }
    return true;
}

R b_comp_new(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "level", "options", "zstd_dict" };
    Value v[3];
    if (!fn_take(a, "ZstdCompressor", NAMES, 0, v))
        return R::Err;
    if (given(v[0]) && given(v[1]))
        return err_set("TypeError", "Only one of level or options should be used");
    CompObj *c = static_cast<CompObj *>(obj_alloc(&comp_type, sizeof(CompObj)));
    if (!c)
        return oom();
    c->c         = nullptr;
    c->dict      = Value();
    c->last_mode = MODE_FLUSH_FRAME;
    c->started   = false;
    Root rc{ obj_value(c) };
    comp_of(rc.v)->c = ZSTD_createCCtx();
    if (!comp_of(rc.v)->c) {
        // A context another compressor dropped may not be swept yet, and each
        // is megabytes: collect and ask once more before giving up.
        gc_collect();
        comp_of(rc.v)->c = ZSTD_createCCtx();
    }
    if (!comp_of(rc.v)->c)
        return oom();
    if (given(v[0])) {
        i64 level = 0;
        if (!as_int_arg(v[0], level)) {
            if (err_pending())
                return R::Err;
            if (!is_intval(v[0]))
                return err_set2("TypeError", "level must be an int", type_name(v[0]));
            // Past a C int, and so past the range either way.
            return bad_level(0, false);
        }
        if (level > 0x7fffffffll || level < -0x80000000ll)
            return bad_level(0, false);
        if (!set_cparam(comp_of(rc.v)->c, ZSTD_c_compressionLevel, i32(level)))
            return R::Err;
    }
    if (given(v[1]) && !each_option(v[1], true, comp_of(rc.v)->c, set_cparam))
        return R::Err;
    if (given(v[2])) {
        Value dv;
        i32 how = 0;
        if (!dict_arg(v[2], dv, how))
            return R::Err;
        Root rd{ dv };
        comp_of(rc.v)->dict = rd.v;
        Str content;
        if (!buffer_like(zdict_of(rd.v)->content, content))
            return R::Err;
        usize r = 0;
        if (how == 2) {
            r = ZSTD_CCtx_refPrefix(comp_of(rc.v)->c, content.data(), content.size());
        } else if (how == 1) {
            r = ZSTD_CCtx_loadDictionary(comp_of(rc.v)->c, content.data(), content.size());
        } else {
            // Digested: one CDict the object keeps and every compressor shares.
            ZDictObj *d = zdict_of(rd.v);
            if (!d->cd) {
                int level = ZSTD_CLEVEL_DEFAULT;
                d->cd     = ZSTD_createCDict(content.data(), content.size(), level);
                if (!d->cd)
                    return oom();
            }
            r = ZSTD_CCtx_refCDict(comp_of(rc.v)->c, zdict_of(rd.v)->cd);
        }
        if (ZSTD_isError(r))
            return zstd_error("Unable to load Zstandard dictionary or prefix", r);
    }
    out = rc.v;
    return R::Ok;
}

// -------------------------------------------------------- ZstdDecompressor

extern const Type decomp_type;

struct DecompObj : Obj {
    ZSTD_DCtx *d;
    Value dict;
    Value unused;
    Value input;
    bool eof;
    bool needs_input;
    bool started; // a step has been taken, so the window is allocated
};

DecompObj *decomp_of(Value v)
{
    return static_cast<DecompObj *>(v.obj());
}

void decomp_trace(Obj *o)
{
    DecompObj *d = static_cast<DecompObj *>(o);
    gc_mark(d->dict);
    gc_mark(d->unused);
    gc_mark(d->input);
}

void decomp_fini(Obj *o)
{
    if (static_cast<DecompObj *>(o)->d)
        ZSTD_freeDCtx(static_cast<DecompObj *>(o)->d);
}

R decomp_repr(Value v, String &out)
{
    char buf[24];
    Buf<72> b;
    b.put("<ZstdDecompressor object at ").put(addr_text(buf, sizeof buf, v.obj())).put(">");
    return out.append(b.str()) ? R::Ok : oom();
}

R decomp_getattr(Value v, StrObj *name, Value &out)
{
    DecompObj *d = decomp_of(v);
    if (name->str() == "unused_data")
        return out = d->unused, R::Ok;
    if (name->str() == "eof")
        return out = value_bool(d->eof), R::Ok;
    if (name->str() == "needs_input")
        return out = value_bool(d->needs_input), R::Ok;
    return R::NotImpl;
}

constexpr Type decomp_type{ .name    = "_zstd.ZstdDecompressor",
                            .trace   = decomp_trace,
                            .fini    = decomp_fini,
                            .repr    = decomp_repr,
                            .getattr = decomp_getattr };

R dm_decompress(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "data", "max_length" };
    Value v[2];
    Value self = method_self(a.args[0]);
    if (!self.is_obj() || self.obj()->type != &decomp_type)
        return err_set2("TypeError", "descriptor requires a ZstdDecompressor", "decompress");
    Str data;
    if (!positional_only(a, "decompress", "data") || !meth_take(a, "decompress", NAMES, 1, v) ||
        !bytes_arg(v[0], "decompress", "data", data))
        return R::Err;
    i64 max = -1;
    if (given(v[1]) && !int_arg(v[1], "decompress", "max_length", max))
        return R::Err;
    DecompObj *d = decomp_of(self);
    if (d->eof)
        return err_set("EOFError", "Already at the end of a Zstandard frame.");
    Root rs{ self };
    Str held;
    if (!buffer_like(d->input, held))
        return R::Err;
    String all;
    if (!held.empty()) {
        if (!all.append(held) || !all.append(data))
            return oom();
        data = all.str();
    }
    if (!d->started) {
        d->started = true;
        gc_collect();
        d = decomp_of(rs.v);
    }
    ZSTD_inBuffer src{ data.data(), data.size(), 0 };
    Growing g;
    bool end    = false;
    bool capped = false;
    for (;;) {
        if (max >= 0 && g.at >= usize(max)) {
            capped = true;
            break;
        }
        ZSTD_outBuffer dst{};
        if (!grow_for(g, dst, max < 0 ? 0 : usize(max)))
            return oom();
        usize left = ZSTD_decompressStream(decomp_of(rs.v)->d, &dst, &src);
        g.at       = dst.pos;
        if (ZSTD_isError(left))
            return zstd_error("Unable to decompress zstd data", left);
        if (left == 0) {
            // A frame ended. Calling again with no input would report the
            // *next* frame's header, so this is where the loop stops.
            end = true;
            break;
        }
        if (dst.pos == dst.size)
            continue; // full: libzstd has more to flush, so grow and ask again
        // Room to spare: it wants more input, which this call has not got.
        capped = false;
        break;
    }
    Str left(data.data() + src.pos, data.size() - src.pos);
    Root made{ bytes_new(g.str()) };
    Root rest{ bytes_new(end ? Str() : left) };
    Root after{ bytes_new(end ? left : Str()) };
    if (made.v.is_nil() || rest.v.is_nil() || after.v.is_nil())
        return R::Err;
    d              = decomp_of(rs.v);
    d->input       = rest.v;
    d->needs_input = left.empty() && !capped;
    if (end) {
        d->eof         = true;
        d->unused      = after.v;
        d->needs_input = false;
    }
    out = made.v;
    return R::Ok;
}

constexpr Method DECOMP_METHODS[] = {
    { "decompress", dm_decompress },
};

bool set_dparam(void *ctx, i32 key, i32 val)
{
    if (!in_bounds(key, val, false))
        return false;
    usize r = ZSTD_DCtx_setParameter(static_cast<ZSTD_DCtx *>(ctx), ZSTD_dParameter(key), val);
    if (ZSTD_isError(r)) {
        char n[24];
        Buf<200> b;
        b.put("Error when setting zstd decompression parameter ");
        b.put(int_text(n, sizeof n, i64(key))).put(": ");
        b.put(c_str(ZSTD_getErrorName(r)));
        return zstd_raise(b.str()) == R::Ok;
    }
    return true;
}

R b_decomp_new(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "zstd_dict", "options" };
    Value v[2];
    if (!fn_take(a, "ZstdDecompressor", NAMES, 0, v))
        return R::Err;
    DecompObj *d = static_cast<DecompObj *>(obj_alloc(&decomp_type, sizeof(DecompObj)));
    if (!d)
        return oom();
    d->d           = nullptr;
    d->dict        = Value();
    d->unused      = Value();
    d->input       = Value();
    d->eof         = false;
    d->needs_input = true;
    d->started     = false;
    Root rd{ obj_value(d) };
    Root empty{ bytes_new(Str()) };
    if (empty.v.is_nil())
        return R::Err;
    decomp_of(rd.v)->unused = empty.v;
    decomp_of(rd.v)->input  = empty.v;
    decomp_of(rd.v)->d      = ZSTD_createDCtx();
    if (!decomp_of(rd.v)->d) {
        gc_collect();
        decomp_of(rd.v)->d = ZSTD_createDCtx();
    }
    if (!decomp_of(rd.v)->d)
        return oom();
    // A frame may name a window this process cannot give it; saying so beats
    // an allocation failure with no explanation.
    ZSTD_DCtx_setParameter(decomp_of(rd.v)->d, ZSTD_d_windowLogMax, 27);
    if (given(v[0])) {
        Value dv;
        i32 how = 0;
        if (!dict_arg(v[0], dv, how))
            return R::Err;
        Root rdd{ dv };
        decomp_of(rd.v)->dict = rdd.v;
        Str content;
        if (!buffer_like(zdict_of(rdd.v)->content, content))
            return R::Err;
        usize r = 0;
        if (how == 2) {
            r = ZSTD_DCtx_refPrefix(decomp_of(rd.v)->d, content.data(), content.size());
        } else if (how == 1) {
            r = ZSTD_DCtx_loadDictionary(decomp_of(rd.v)->d, content.data(), content.size());
        } else {
            ZDictObj *zd = zdict_of(rdd.v);
            if (!zd->dd) {
                zd->dd = ZSTD_createDDict(content.data(), content.size());
                if (!zd->dd)
                    return oom();
            }
            r = ZSTD_DCtx_refDDict(decomp_of(rd.v)->d, zdict_of(rdd.v)->dd);
        }
        if (ZSTD_isError(r))
            return zstd_error("Unable to load Zstandard dictionary or prefix", r);
    }
    if (given(v[1]) && !each_option(v[1], false, decomp_of(rd.v)->d, set_dparam))
        return R::Err;
    out = rd.v;
    return R::Ok;
}

// ------------------------------------------------------------ module level

R z_get_frame_info(const CallArgs &a, Value &out)
{
    Str d;
    if (!args_only(a, "get_frame_info", 1, 1) ||
        !bytes_arg(a.args[0], "get_frame_info", "frame_buffer", d))
        return R::Err;
    u64 size = ZSTD_getFrameContentSize(d.data(), d.size());
    if (size == ZSTD_CONTENTSIZE_ERROR)
        return zstd_raise(
            "Error when getting information from the header of a zstd "
            "frame. Make sure the frame_buffer argument starts from the "
            "beginning of a frame, and its length is not less than the "
            "frame header (6~18 bytes).");
    u32 id      = ZSTD_getDictID_fromFrame(d.data(), d.size());
    TupleObj *t = tuple_new(2);
    if (!t)
        return R::Err;
    Root rt{ obj_value(t) };
    Root a0{ size == ZSTD_CONTENTSIZE_UNKNOWN ? value_none() : int_from_i64(i64(size)) };
    Root a1{ int_from_i64(i64(id)) };
    if (a0.v.is_nil() || a1.v.is_nil())
        return R::Err;
    static_cast<TupleObj *>(rt.v.obj())->items()[0] = a0.v;
    static_cast<TupleObj *>(rt.v.obj())->items()[1] = a1.v;
    out                                             = rt.v;
    return R::Ok;
}

R z_get_frame_size(const CallArgs &a, Value &out)
{
    Str d;
    if (!args_only(a, "get_frame_size", 1, 1) ||
        !bytes_arg(a.args[0], "get_frame_size", "frame_buffer", d))
        return R::Err;
    usize n = ZSTD_findFrameCompressedSize(d.data(), d.size());
    if (ZSTD_isError(n))
        return zstd_raise(
            "Error when finding the compressed size of a zstd frame. Make "
            "sure the frame_buffer argument starts from the beginning of a "
            "frame, and its length is not less than this complete frame.");
    out = int_from_i64(i64(n));
    return out.is_nil() ? R::Err : R::Ok;
}

R z_get_param_bounds(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "parameter", "is_compress" };
    Value v[2];
    i64 param = 0;
    if (!fn_take(a, "get_param_bounds", NAMES, 2, v) ||
        !int_arg(v[0], "get_param_bounds", "parameter", param))
        return R::Err;
    bool compressing = py_truth(v[1]);
    ZSTD_bounds b    = compressing ? ZSTD_cParam_getBounds(ZSTD_cParameter(param))
                                   : ZSTD_dParam_getBounds(ZSTD_dParameter(param));
    if (ZSTD_isError(b.error))
        return zstd_error("Zstd parameter", b.error);
    TupleObj *t = tuple_new(2);
    if (!t)
        return R::Err;
    t->items()[0] = Value::of_int(b.lowerBound);
    t->items()[1] = Value::of_int(b.upperBound);
    out           = obj_value(t);
    return R::Ok;
}

R z_set_parameter_types(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "c_parameter_type", "d_parameter_type" };
    Value v[2];
    if (!fn_take(a, "set_parameter_types", NAMES, 2, v))
        return R::Err;
    home->cparam = v[0];
    home->dparam = v[1];
    out          = value_none();
    return R::Ok;
}

// zstd's own trainer is zdict.h's COVER, which braam::zstd does not carry, so
// what these two build is a **content-only dictionary**: the tail of the
// samples, which is what `is_raw=True` means and what zstd falls back to for
// any content without the dictionary magic. It is a real dictionary and it
// works -- a compressor given one finds matches in it -- but it carries no
// entropy tables and therefore no dictionary id, so `dict_id` is 0 and it
// compresses less well than a trained one would. The tail rather than the
// head because zstd matches backwards from the end of a dictionary, so the
// bytes nearest the end are the ones most likely to be found.
//
// Manual.md §6 says so, and test/pycompress.mjs pins it.
R content_dict(Str samples, i64 dict_size, Value &out)
{
    if (dict_size <= 0)
        return err_set("ValueError", "dict_size argument should be positive number");
    usize want = usize(dict_size);
    Str tail   = samples.size() > want ? samples.substr(samples.size() - want, want) : samples;
    out        = bytes_new(tail);
    return out.is_nil() ? R::Err : R::Ok;
}

R z_train_dict(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "samples_bytes", "samples_sizes", "dict_size" };
    Value v[3];
    Str samples;
    i64 dict_size = 0;
    if (!fn_take(a, "train_dict", NAMES, 3, v) ||
        !bytes_arg(v[0], "train_dict", "samples_bytes", samples) ||
        !int_arg(v[2], "train_dict", "dict_size", dict_size))
        return R::Err;
    return content_dict(samples, dict_size, out);
}

R z_finalize_dict(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "custom_dict_bytes", "samples_bytes", "samples_sizes", "dict_size",
                                 "compression_level" };
    Value v[5];
    Str custom, samples;
    i64 dict_size = 0;
    if (!fn_take(a, "finalize_dict", NAMES, 5, v) ||
        !bytes_arg(v[0], "finalize_dict", "custom_dict_bytes", custom) ||
        !bytes_arg(v[1], "finalize_dict", "samples_bytes", samples) ||
        !int_arg(v[3], "finalize_dict", "dict_size", dict_size))
        return R::Err;
    // The basis first and the samples after it, so the samples' tail is what
    // survives the truncation, as in train_dict.
    String both;
    if (!both.append(custom) || !both.append(samples))
        return oom();
    return content_dict(both.str(), dict_size, out);
}

constexpr ModDef DEFS[] = {
    { "get_frame_info", z_get_frame_info },
    { "get_frame_size", z_get_frame_size },
    { "get_param_bounds", z_get_param_bounds },
    { "set_parameter_types", z_set_parameter_types },
    { "train_dict", z_train_dict },
    { "finalize_dict", z_finalize_dict },
};

INFO_TYPE(version_type, "_zstd.zstd_version_info");

constexpr Str VERSION_NAMES[] = { "major", "minor", "patch" };

Value version_info()
{
    u32 v      = ZSTD_VERSION_NUMBER;
    Value n[3] = { Value::of_int(i32(v / 10000u)), Value::of_int(i32(v / 100u % 100u)),
                   Value::of_int(i32(v % 100u)) };
    return info_new(&version_type, n, VERSION_NAMES, 3, 3);
}

} // namespace

bool zstd_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    if (!home) {
        home = heap_new<Home>();
        if (!home)
            return false;
        gc_root_hook(home_mark);
    }
    if (home->error.is_nil()) {
        home->error = mod_exc_class("_zstd", "ZstdError", "Exception");
        if (home->error.is_nil())
            return false;
    }
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    if (!mod_put(d, "ZstdError", home->error) || !mod_defs(d, DEFS))
        return false;
    if (!class_int(&comp_type, "CONTINUE", MODE_CONTINUE) ||
        !class_int(&comp_type, "FLUSH_BLOCK", MODE_FLUSH_BLOCK) ||
        !class_int(&comp_type, "FLUSH_FRAME", MODE_FLUSH_FRAME))
        return false;
    if (!method_install(&comp_type, COMP_METHODS) || !mod_type(d, &comp_type, b_comp_new) ||
        !method_install(&decomp_type, DECOMP_METHODS) || !mod_type(d, &decomp_type, b_decomp_new) ||
        !mod_type(d, &zdict_type, b_zdict_new))
        return false;
    const struct {
        Str name;
        i64 v;
    } NUMBERS[] = {
        { "ZSTD_CLEVEL_DEFAULT", ZSTD_CLEVEL_DEFAULT },
        { "ZSTD_DStreamOutSize", i64(ZSTD_DStreamOutSize()) },
        { "ZSTD_c_compressionLevel", ZSTD_c_compressionLevel },
        { "ZSTD_c_windowLog", ZSTD_c_windowLog },
        { "ZSTD_c_hashLog", ZSTD_c_hashLog },
        { "ZSTD_c_chainLog", ZSTD_c_chainLog },
        { "ZSTD_c_searchLog", ZSTD_c_searchLog },
        { "ZSTD_c_minMatch", ZSTD_c_minMatch },
        { "ZSTD_c_targetLength", ZSTD_c_targetLength },
        { "ZSTD_c_strategy", ZSTD_c_strategy },
        { "ZSTD_c_enableLongDistanceMatching", ZSTD_c_enableLongDistanceMatching },
        { "ZSTD_c_ldmHashLog", ZSTD_c_ldmHashLog },
        { "ZSTD_c_ldmMinMatch", ZSTD_c_ldmMinMatch },
        { "ZSTD_c_ldmBucketSizeLog", ZSTD_c_ldmBucketSizeLog },
        { "ZSTD_c_ldmHashRateLog", ZSTD_c_ldmHashRateLog },
        { "ZSTD_c_contentSizeFlag", ZSTD_c_contentSizeFlag },
        { "ZSTD_c_checksumFlag", ZSTD_c_checksumFlag },
        { "ZSTD_c_dictIDFlag", ZSTD_c_dictIDFlag },
        { "ZSTD_c_nbWorkers", ZSTD_c_nbWorkers },
        { "ZSTD_c_jobSize", ZSTD_c_jobSize },
        { "ZSTD_c_overlapLog", ZSTD_c_overlapLog },
        { "ZSTD_d_windowLogMax", ZSTD_d_windowLogMax },
        { "ZSTD_fast", ZSTD_fast },
        { "ZSTD_dfast", ZSTD_dfast },
        { "ZSTD_greedy", ZSTD_greedy },
        { "ZSTD_lazy", ZSTD_lazy },
        { "ZSTD_lazy2", ZSTD_lazy2 },
        { "ZSTD_btlazy2", ZSTD_btlazy2 },
        { "ZSTD_btopt", ZSTD_btopt },
        { "ZSTD_btultra", ZSTD_btultra },
        { "ZSTD_btultra2", ZSTD_btultra2 },
    };
    for (const auto &n : NUMBERS)
        if (!mod_int(d, n.name, n.v))
            return false;
    Root ver{ version_info() };
    Root text{ str_new(ZSTD_VERSION_STRING) };
    if (ver.v.is_nil() || text.v.is_nil())
        return false;
    d = static_cast<DictObj *>(rd.v.obj());
    return mod_put(d, "zstd_version_info", ver.v) && mod_put(d, "ZSTD_VERSION_INFO", ver.v) &&
           mod_put(d, "zstd_version", text.v) && mod_put(d, "ZSTD_VERSION", text.v);
}
