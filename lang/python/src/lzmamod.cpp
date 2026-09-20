// `_lzma`: the floor under lzma and compression.lzma, over braam::lzma, which
// is liblzma from xz 5.8.4 vendored verbatim -- so the output is what `xz -T1`
// writes, byte for byte.
//
// Unlike zlib's and bzip2's, this library is C and not a rewrite, so what is
// below is liblzma's own API used as C uses it: an lzma_stream, an array of
// lzma_filter, and lzma_code(). _lzmamodule.c's shape, without its refcounts.
//
// **The default preset does not fit.** An encoder is about 94 MiB at preset 6,
// which is lzma.PRESET_DEFAULT, against a 100 MB cap that also holds the
// interpreter; and because braam-core's allocator never returns a span, what
// matters is what fits *repeatedly*. Measured here: presets 0 to 2 compress
// over and over, 3 and 4 fit once, and 5 up never.
//
// So lzma_raw_encoder_memusage is asked before anything is allocated, and:
// a preset the caller named and that will not fit is a MemoryError saying so,
// because writing something other than what was asked for would be a lie;
// a preset nobody named -- CPython's default of 6 -- drops to the best that
// does fit, so lzma.compress(data) works and writes a good .xz. Manual.md §10
// records it. Decompression is unaffected: a decoder is about the size of the
// dictionary, and 8 MiB reads anything preset 6 wrote.
#include "bigint.h"
#include "exc.h"
#include "gc.h"
#include "info.h"
#include "kernel/fmt.h"
#include "lzma/lzma.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "posix.h"
#include "type.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

constexpr usize CHUNK = 8192;

struct Home {
    Value error; // _lzma.LZMAError
};

Home *home;

void home_mark()
{
    if (home)
        gc_mark(home->error);
}

R lzma_raise(Str msg)
{
    if (home && !home->error.is_nil())
        return mod_raise(home->error, msg);
    return err_set("Exception", msg);
}

// _lzmamodule.c's Catch_lzma_error, message for message.
R lzma_error(lzma_ret ret)
{
    switch (ret) {
    case LZMA_OK:
    case LZMA_GET_CHECK:
    case LZMA_NO_CHECK:
    case LZMA_STREAM_END:
        return R::Ok;
    case LZMA_UNSUPPORTED_CHECK:
        return lzma_raise("Unsupported integrity check");
    case LZMA_MEM_ERROR:
        return oom();
    case LZMA_MEMLIMIT_ERROR:
        return lzma_raise("Memory usage limit exceeded");
    case LZMA_FORMAT_ERROR:
        return lzma_raise("Input format not supported by decoder");
    case LZMA_OPTIONS_ERROR:
        return lzma_raise("Invalid or unsupported options");
    case LZMA_DATA_ERROR:
        return lzma_raise("Corrupt input data");
    case LZMA_BUF_ERROR:
        return lzma_raise("Insufficient buffer space");
    case LZMA_PROG_ERROR:
        return lzma_raise("Internal error");
    default: {
        char n[24];
        Buf<64> b;
        b.put("Unrecognized error from liblzma: ").put(int_text(n, sizeof n, i64(int(ret))));
        return lzma_raise(b.str());
    }
    }
}

R bad_preset(u64 preset)
{
    char n[24];
    Buf<64> b;
    b.put("Invalid compression preset: ").put(int_text(n, sizeof n, i64(preset)));
    return lzma_raise(b.str());
}

// ------------------------------------------------------------- the buffers

struct Growing {
    Vec<u8> out;
    usize at = 0;

    bool room(usize want) { return out.size() >= at + want || out.resize(at + want); }

    Str str() const { return Str(reinterpret_cast<const char *>(out.data()), at); }
};

// Points the stream at the next stretch of room, growing by doubling.
bool grow_once(Growing &g, lzma_stream &s, usize limit)
{
    usize want = g.out.size() ? g.out.size() : CHUNK;
    if (limit && g.at + want > limit)
        want = limit - g.at;
    if (!g.room(want))
        return false;
    s.next_out  = g.out.data() + g.at;
    s.avail_out = g.out.size() - g.at;
    return true;
}

// A buffer that will not grow is usually one a dropped codec object is
// still holding megabytes from: collect and try once more.
bool grow_for(Growing &g, lzma_stream &s, usize limit)
{
    return grow_once(g, s, limit) || (gc_collect(), grow_once(g, s, limit));
}

void took(Growing &g, const lzma_stream &s)
{
    g.at = g.out.size() - s.avail_out;
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
    if (!v.is_nil() && as_index(v, out))
        return true;
    if (err_pending())
        return false;
    Buf<160> b;
    b.put(who).put("() argument '").put(what).put("' must be an integer, not '");
    b.put(type_name(v)).put("'");
    return err_set("TypeError", b.str()) == R::Ok;
}

// An argument the caller left out, or passed as None, which lzma.py does for
// every one of these.
bool given(Value v)
{
    return !v.is_nil() && !is_none(v);
}

// An unsigned 32-bit argument, as CPython's uint32_converter reads one: a
// negative is a ValueError and a number past the range an OverflowError.
bool u32_arg(Value v, Str who, Str what, u32 &out)
{
    i64 n = 0;
    if (!is_intval(v)) {
        Buf<160> b;
        b.put(who).put("() argument '").put(what).put("' must be an integer, not '");
        b.put(type_name(v)).put("'");
        return err_set("TypeError", b.str()) == R::Ok;
    }
    if (!as_index(v, n))
        return err_set("OverflowError", "Python int too large to convert to C unsigned int") ==
               R::Ok;
    if (n < 0)
        return err_set("ValueError", "value must be positive") == R::Ok;
    if (n > 0xffffffffll)
        return err_set("OverflowError", "Python int too large to convert to C unsigned int") ==
               R::Ok;
    out = u32(n);
    return true;
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

// ----------------------------------------------------------- filter chains
//
// A filter is a dict: {"id": FILTER_LZMA2, "preset": 6} or the spelled-out
// options. One options struct per filter id, held in the chain beside it so
// nothing points into a temporary.

constexpr usize MAX_FILTERS = LZMA_FILTERS_MAX;

union FilterOpts {
    lzma_options_lzma lzma;
    lzma_options_delta delta;
    lzma_options_bcj bcj;
};

struct Chain {
    lzma_filter f[MAX_FILTERS + 1];
    FilterOpts opts[MAX_FILTERS];
    usize n = 0;

    Chain()
    {
        for (usize i = 0; i <= MAX_FILTERS; i++) {
            f[i].id      = LZMA_VLI_UNKNOWN;
            f[i].options = nullptr;
        }
    }
};

// A field of a filter dict, as an integer; missing leaves `out` alone.
bool spec_int(Value spec, Str name, u64 &out, bool &found)
{
    Root rs{ spec };
    Root key{ str_new(name) };
    if (key.v.is_nil())
        return false;
    Value got;
    // A dict without the key raises; a filter spec's fields are all optional
    // but "id", so that is what "not there" looks like.
    if (!is_anydict(rs.v))
        return err_set("TypeError", "Filter specifier must be a dict or dict-like object") == R::Ok;
    R r = dict_get(static_cast<DictObj *>(rs.v.obj()), key.v, got);
    if (r == R::Err)
        return false;
    if (r == R::NotImpl) {
        found = false;
        return true;
    }
    i64 n = 0;
    if (!as_int_arg(got, n)) {
        Buf<128> b;
        b.put("Filter specifier's '").put(name).put("' must be an integer");
        return err_set("TypeError", b.str()) == R::Ok;
    }
    out   = u64(n);
    found = true;
    return true;
}

// Every key a filter of this id understands, "id" included. A key outside
// the list is a ValueError, as _lzmamodule.c's converter makes it.
bool only_known(Value spec, const Str *names, usize n)
{
    Root rs{ spec };
    ListObj *keys = py_list_of(rs.v); // a dict iterates its keys
    if (!keys)
        return false;
    Root rk{ obj_value(keys) };
    for (Value k : list_of(rk.v)->items) {
        if (!is_str(k))
            return err_set("ValueError", "Invalid filter specifier key") == R::Ok;
        Str name = str_of(k)->str();
        usize i  = 0;
        while (i < n && names[i] != name)
            i++;
        if (i == n) {
            Buf<128> b;
            b.put("Invalid filter specifier for this filter: '").put(name).put("'");
            return err_set("ValueError", b.str()) == R::Ok;
        }
    }
    return true;
}

// One filter dict into `f`, with its options in `store`.
bool one_filter(Value spec, lzma_filter &f, FilterOpts &store)
{
    u64 id    = 0;
    bool have = false;
    if (!spec_int(spec, "id", id, have))
        return false;
    if (!have)
        return err_set("ValueError", "Filter specifier must have an \"id\" entry") == R::Ok;
    f.id      = id;
    f.options = nullptr;
    switch (id) {
    case LZMA_FILTER_LZMA1:
    case LZMA_FILTER_LZMA2: {
        static const Str KEYS[] = { "id", "preset", "dict_size", "lc", "lp",
                                    "pb", "mode",   "nice_len",  "mf", "depth" };
        if (!only_known(spec, KEYS, sizeof KEYS / sizeof KEYS[0]))
            return false;
        u64 preset   = LZMA_PRESET_DEFAULT;
        bool present = false;
        if (!spec_int(spec, "preset", preset, present))
            return false;
        if (lzma_lzma_preset(&store.lzma, u32(preset)))
            return bad_preset(preset) == R::Ok;
        struct {
            Str name;
            u32 lzma_options_lzma::*u32_at;
        } const U32S[] = {
            { "dict_size", &lzma_options_lzma::dict_size },
            { "lc", &lzma_options_lzma::lc },
            { "lp", &lzma_options_lzma::lp },
            { "pb", &lzma_options_lzma::pb },
            { "nice_len", &lzma_options_lzma::nice_len },
            { "depth", &lzma_options_lzma::depth },
        };
        for (const auto &e : U32S) {
            u64 v  = 0;
            bool g = false;
            if (!spec_int(spec, e.name, v, g))
                return false;
            if (g)
                store.lzma.*e.u32_at = u32(v);
        }
        u64 v  = 0;
        bool g = false;
        if (!spec_int(spec, "mode", v, g))
            return false;
        if (g)
            store.lzma.mode = lzma_mode(v);
        if (!spec_int(spec, "mf", v, g))
            return false;
        if (g)
            store.lzma.mf = lzma_match_finder(v);
        f.options = &store.lzma;
        break;
    }
    case LZMA_FILTER_DELTA: {
        static const Str KEYS[] = { "id", "dist" };
        if (!only_known(spec, KEYS, 2))
            return false;
        store.delta      = lzma_options_delta{};
        store.delta.type = LZMA_DELTA_TYPE_BYTE;
        store.delta.dist = 1;
        u64 v            = 0;
        bool g           = false;
        if (!spec_int(spec, "dist", v, g))
            return false;
        if (g)
            store.delta.dist = u32(v);
        f.options = &store.delta;
        break;
    }
    case LZMA_FILTER_X86:
    case LZMA_FILTER_POWERPC:
    case LZMA_FILTER_IA64:
    case LZMA_FILTER_ARM:
    case LZMA_FILTER_ARMTHUMB:
    case LZMA_FILTER_ARM64:
    case LZMA_FILTER_RISCV:
    case LZMA_FILTER_SPARC: {
        static const Str KEYS[] = { "id", "start_offset" };
        if (!only_known(spec, KEYS, 2))
            return false;
        store.bcj = lzma_options_bcj{};
        u64 v     = 0;
        bool g    = false;
        if (!spec_int(spec, "start_offset", v, g))
            return false;
        if (g) {
            store.bcj.start_offset = u32(v);
            f.options              = &store.bcj;
        }
        break;
    }
    default: {
        char n[24];
        Buf<64> b;
        b.put("Invalid filter ID: ").put(int_text(n, sizeof n, i64(id)));
        return err_set("ValueError", b.str()) == R::Ok;
    }
    }
    return true;
}

// A sequence of filter dicts into `c`.
bool build_chain(Value filters, Chain &c)
{
    Root rf{ filters };
    ListObj *l = py_list_of(rf.v);
    if (!l)
        return false;
    Root rl{ obj_value(l) };
    usize n = list_of(rl.v)->items.size();
    if (n == 0)
        return err_set("ValueError", "Empty filter chain") == R::Ok;
    if (n > MAX_FILTERS)
        return err_set("ValueError", "Too many filters - liblzma supports a maximum of 4") == R::Ok;
    for (usize i = 0; i < n; i++)
        if (!one_filter(list_of(rl.v)->items[i], c.f[i], c.opts[i]))
            return false;
    c.n       = n;
    c.f[n].id = LZMA_VLI_UNKNOWN;
    return true;
}

// What the encoder liblzma would not build costs, for the message. The
// figure is liblzma's own, and it is asked only after the allocation failed.
R too_big(const lzma_filter *f, Str who)
{
    u64 need = lzma_raw_encoder_memusage(f);
    if (need == UINT64_MAX)
        return err_set("ValueError", "Invalid or unsupported options");
    char a[24];
    Buf<200> b;
    b.put(who).put(" wanted ").put(int_text(a, sizeof a, i64(need >> 20)));
    b.put(" MiB and the process had none to give: use a lower preset");
    return err_set("MemoryError", b.str());
}

// --------------------------------------------------------- LZMACompressor

extern const Type comp_type;

struct CompObj : Obj {
    lzma_stream s;
    bool ended;
};

CompObj *comp_of(Value v)
{
    return static_cast<CompObj *>(v.obj());
}

void comp_fini(Obj *o)
{
    lzma_end(&static_cast<CompObj *>(o)->s);
}

R comp_repr(Value v, String &out)
{
    char buf[24];
    Buf<72> b;
    b.put("<_lzma.LZMACompressor object at ").put(addr_text(buf, sizeof buf, v.obj())).put(">");
    return out.append(b.str()) ? R::Ok : oom();
}

constexpr Type comp_type{ .name = "_lzma.LZMACompressor", .fini = comp_fini, .repr = comp_repr };

CompObj *self_comp(const CallArgs &a, Str who)
{
    Value s = method_self(a.args[0]);
    if (!s.is_obj() || s.obj()->type != &comp_type)
        return err_set2("TypeError", "descriptor requires an LZMACompressor", who), nullptr;
    return comp_of(s);
}

// _lzmamodule.c's compress loop. lzma_code returns after each step it can
// take, so "room left over" does not mean it is done -- an empty input does.
R compress_run(Value self, Str in, lzma_action action, Growing &g)
{
    lzma_stream &s = comp_of(self)->s;
    s.next_in      = reinterpret_cast<const u8 *>(in.data());
    s.avail_in     = in.size();
    for (;;) {
        if (!grow_for(g, s, 0))
            return oom();
        lzma_ret ret = lzma_code(&s, action);
        took(g, s);
        // No progress with nothing left to read is not an error: it is the
        // encoder asking for input it will not get.
        if (ret == LZMA_BUF_ERROR && s.avail_in == 0 && s.avail_out)
            ret = LZMA_OK;
        if (lzma_error(ret) != R::Ok)
            return R::Err;
        if (action == LZMA_RUN ? s.avail_in == 0 : ret == LZMA_STREAM_END)
            break;
    }
    s.next_in  = nullptr;
    s.avail_in = 0;
    return R::Ok;
}

R cm_compress(const CallArgs &a, Value &out)
{
    CompObj *c = self_comp(a, "compress");
    Str data;
    if (!c || !positional_only(a, "compress", "data") || !meth_args(a, "compress", 1, 1) ||
        !bytes_arg(a.args[1], "compress", "data", data))
        return R::Err;
    if (c->ended)
        return err_set("ValueError", "Compressor has been flushed");
    Root rs{ method_self(a.args[0]) };
    Growing g;
    if (compress_run(rs.v, data, LZMA_RUN, g) != R::Ok)
        return R::Err;
    out = bytes_new(g.str());
    return out.is_nil() ? R::Err : R::Ok;
}

R cm_flush(const CallArgs &a, Value &out)
{
    CompObj *c = self_comp(a, "flush");
    if (!c || !meth_args(a, "flush", 0, 0))
        return R::Err;
    if (c->ended)
        return err_set("ValueError", "Repeated call to flush()");
    Root rs{ method_self(a.args[0]) };
    Growing g;
    if (compress_run(rs.v, Str(), LZMA_FINISH, g) != R::Ok)
        return R::Err;
    comp_of(rs.v)->ended = true;
    out                  = bytes_new(g.str());
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr Method COMP_METHODS[] = {
    { "compress", cm_compress },
    { "flush", cm_flush },
};

// The encoder the format asks for, built again where the first try found no
// memory.
lzma_ret encoder_init(lzma_stream &s, i64 format, Chain &chain, i64 check)
{
    if (format == 2) // FORMAT_ALONE
        return lzma_alone_encoder(&s, &chain.opts[0].lzma);
    if (format == 3) // FORMAT_RAW
        return lzma_raw_encoder(&s, chain.f);
    return lzma_stream_encoder(&s, chain.f, check == -1 ? LZMA_CHECK_CRC64 : lzma_check(check));
}

R b_comp_new(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "format", "check", "preset", "filters" };
    Value v[4];
    if (!fn_take(a, "LZMACompressor", NAMES, 0, v))
        return R::Err;
    i64 format = 1; // FORMAT_XZ
    i64 check  = -1;
    if ((given(v[0]) && !int_arg(v[0], "LZMACompressor", "format", format)) ||
        (given(v[1]) && !int_arg(v[1], "LZMACompressor", "check", check)))
        return R::Err;
    if (check != -1 && format != 1)
        return err_set("ValueError",
                       "Integrity checks are only supported by FORMAT_XZ and FORMAT_RAW");
    if (given(v[2]) && given(v[3]))
        return err_set("ValueError", "Cannot specify both preset and filter chain");
    if (format == 3 && !given(v[3])) // FORMAT_RAW
        return err_set("ValueError", "Must specify filters for FORMAT_RAW");
    u64 preset = LZMA_PRESET_DEFAULT;
    if (given(v[2])) {
        u32 p = 0;
        if (!u32_arg(v[2], "LZMACompressor", "preset", p))
            return R::Err;
        preset = p;
    }
    Chain chain;
    bool named = given(v[2]) || given(v[3]);
    if (given(v[3])) {
        if (!build_chain(v[3], chain))
            return R::Err;
    } else {
        chain.f[0].id      = format == 2 ? LZMA_FILTER_LZMA1 : LZMA_FILTER_LZMA2;
        chain.f[0].options = &chain.opts[0].lzma;
        chain.f[1].id      = LZMA_VLI_UNKNOWN;
        chain.n            = 1;
        if (lzma_lzma_preset(&chain.opts[0].lzma, u32(preset)))
            return bad_preset(preset);
    }
    CompObj *c = static_cast<CompObj *>(obj_alloc(&comp_type, sizeof(CompObj)));
    if (!c)
        return oom();
    c->s     = lzma_stream LZMA_STREAM_INIT;
    c->ended = false;
    Root rc{ obj_value(c) };
    if (format < 1 || format > 3)
        return err_set("ValueError", "Invalid container format");
    lzma_stream &s = comp_of(rc.v)->s;
    lzma_ret ret   = encoder_init(s, format, chain, check);
    if (ret == LZMA_MEM_ERROR) {
        // The coder another compressor dropped may not be swept yet, and each
        // is megabytes: collect and ask once more.
        gc_collect();
        ret = encoder_init(s, format, chain, check);
    }
    // Still nothing, and nobody named a preset: CPython's default of 6 wants
    // 94 MiB, which this process will sometimes have and sometimes not, so
    // come down until one fits rather than fail a call that named nothing.
    // A preset or a chain that *was* named is refused instead, because
    // writing something other than what was asked for would be a lie.
    if (ret == LZMA_MEM_ERROR && !named && format != 3) {
        u32 p = u32(preset) & ~LZMA_PRESET_EXTREME;
        while (ret == LZMA_MEM_ERROR && p > 0) {
            p--;
            if (lzma_lzma_preset(&chain.opts[0].lzma, p))
                return bad_preset(p);
            ret = encoder_init(s, format, chain, check);
        }
    }
    if (ret == LZMA_MEM_ERROR)
        return too_big(chain.f, "LZMACompressor");
    if (lzma_error(ret) != R::Ok)
        return R::Err;
    out = rc.v;
    return R::Ok;
}

// ------------------------------------------------------- LZMADecompressor

extern const Type decomp_type;

struct DecompObj : Obj {
    lzma_stream s;
    Value unused;
    Value input;
    i64 check;
    bool eof;
    bool needs_input;
    bool started; // a step has been taken, so the dictionary is allocated
};

DecompObj *decomp_of(Value v)
{
    return static_cast<DecompObj *>(v.obj());
}

void decomp_trace(Obj *o)
{
    gc_mark(static_cast<DecompObj *>(o)->unused);
    gc_mark(static_cast<DecompObj *>(o)->input);
}

void decomp_fini(Obj *o)
{
    lzma_end(&static_cast<DecompObj *>(o)->s);
}

R decomp_repr(Value v, String &out)
{
    char buf[24];
    Buf<72> b;
    b.put("<_lzma.LZMADecompressor object at ").put(addr_text(buf, sizeof buf, v.obj())).put(">");
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
    if (name->str() == "check") {
        out = Value::of_int(i32(d->check));
        return R::Ok;
    }
    return R::NotImpl;
}

constexpr Type decomp_type{ .name    = "_lzma.LZMADecompressor",
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
        return err_set2("TypeError", "descriptor requires an LZMADecompressor", "decompress");
    Str data;
    if (!positional_only(a, "decompress", "data") || !meth_take(a, "decompress", NAMES, 1, v) ||
        !bytes_arg(v[0], "decompress", "data", data))
        return R::Err;
    i64 max = -1;
    if (given(v[1]) && !int_arg(v[1], "decompress", "max_length", max))
        return R::Err;
    DecompObj *d = decomp_of(self);
    if (d->eof)
        return err_set("EOFError", "Already at end of stream");
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
        // liblzma builds its dictionary on the first step, megabytes of it.
        // A coder another stream has dropped may not be swept yet, so this is
        // the moment to collect -- afterwards there is no retrying, because
        // lzma_code that has answered an error is finished.
        d->started = true;
        gc_collect();
        d = decomp_of(rs.v);
    }
    lzma_stream &s = decomp_of(rs.v)->s;
    s.next_in      = reinterpret_cast<const u8 *>(data.data());
    s.avail_in     = data.size();
    Growing g;
    bool end    = false;
    bool capped = false;
    for (;;) {
        if (max >= 0 && g.at >= usize(max)) {
            // The output limit ended this call, not the input: there may be
            // more waiting inside, so the caller should ask again with b"".
            capped = true;
            break;
        }
        if (!grow_for(g, s, max < 0 ? 0 : usize(max)))
            return oom();
        lzma_ret ret = lzma_code(&s, LZMA_RUN);
        took(g, s);
        if (ret == LZMA_BUF_ERROR && s.avail_in == 0 && s.avail_out)
            ret = LZMA_OK;
        if (ret == LZMA_STREAM_END) {
            end = true;
            break;
        }
        if (lzma_error(ret) != R::Ok)
            return R::Err;
        // Input used up is what says it wants more; a full buffer says grow.
        if (s.avail_in == 0) {
            // Both exhausted at once: there may still be output inside, so
            // the caller should ask again rather than read more input.
            capped = s.avail_out == 0;
            break;
        }
    }
    Str left(reinterpret_cast<const char *>(s.next_in), s.avail_in);
    Root made{ bytes_new(g.str()) };
    Root rest{ bytes_new(end ? Str() : left) };
    Root after{ bytes_new(end ? left : Str()) };
    if (made.v.is_nil() || rest.v.is_nil() || after.v.is_nil())
        return R::Err;
    d              = decomp_of(rs.v);
    d->input       = rest.v;
    d->needs_input = left.empty() && !capped;
    d->check       = i64(lzma_get_check(&d->s));
    if (end) {
        d->eof         = true;
        d->unused      = after.v;
        d->needs_input = false;
    }
    // next_in points into `data`, which goes when this call does.
    d->s.next_in  = nullptr;
    d->s.avail_in = 0;
    out           = made.v;
    return R::Ok;
}

constexpr Method DECOMP_METHODS[] = {
    { "decompress", dm_decompress },
};

lzma_ret decoder_init(lzma_stream &s, i64 format, Chain &chain, u64 memlimit, u32 flags)
{
    if (format == 1) // FORMAT_XZ
        return lzma_stream_decoder(&s, memlimit, flags);
    if (format == 2) // FORMAT_ALONE
        return lzma_alone_decoder(&s, memlimit);
    if (format == 3) // FORMAT_RAW
        return lzma_raw_decoder(&s, chain.f);
    return lzma_auto_decoder(&s, memlimit, flags); // FORMAT_AUTO
}

R b_decomp_new(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "format", "memlimit", "filters" };
    Value v[3];
    if (!fn_take(a, "LZMADecompressor", NAMES, 0, v))
        return R::Err;
    i64 format = 0; // FORMAT_AUTO
    if (given(v[0]) && !int_arg(v[0], "LZMADecompressor", "format", format))
        return R::Err;
    u64 memlimit = UINT64_MAX;
    if (given(v[1])) {
        if (format == 3)
            return err_set("ValueError", "Cannot specify memory limit with FORMAT_RAW");
        i64 m = 0;
        if (!int_arg(v[1], "LZMADecompressor", "memlimit", m))
            return R::Err;
        memlimit = u64(m);
    }
    if (format == 3 && !given(v[2]))
        return err_set("ValueError", "Must specify filters for FORMAT_RAW");
    if (format != 3 && given(v[2]))
        return err_set("ValueError", "Cannot specify filters except with FORMAT_RAW");
    Chain chain;
    if (given(v[2]) && !build_chain(v[2], chain))
        return R::Err;
    DecompObj *d = static_cast<DecompObj *>(obj_alloc(&decomp_type, sizeof(DecompObj)));
    if (!d)
        return oom();
    d->s           = lzma_stream LZMA_STREAM_INIT;
    d->unused      = Value();
    d->input       = Value();
    d->check       = LZMA_CHECK_ID_MAX + 1; // CHECK_UNKNOWN
    d->eof         = false;
    d->needs_input = true;
    d->started     = false;
    Root rd{ obj_value(d) };
    Root empty{ bytes_new(Str()) };
    if (empty.v.is_nil())
        return R::Err;
    decomp_of(rd.v)->unused = empty.v;
    decomp_of(rd.v)->input  = empty.v;
    lzma_stream &s          = decomp_of(rd.v)->s;
    // Not LZMA_CONCATENATED: with it the decoder waits for LZMA_FINISH before
    // it says End, and lzma.py takes one stream at a time through unused_data,
    // as bz2.py does. CPython's flags, exactly.
    constexpr u32 FLAGS = LZMA_TELL_ANY_CHECK | LZMA_TELL_NO_CHECK;
    if (format < 0 || format > 3)
        return err_set("ValueError", "Invalid container format");
    if (format == 2 || format == 3)
        decomp_of(rd.v)->check = LZMA_CHECK_NONE;
    lzma_ret ret = decoder_init(s, format, chain, memlimit, FLAGS);
    if (ret == LZMA_MEM_ERROR) {
        // The compressor that wrote this may still be holding its 94 MiB and
        // not be swept yet: collect and ask once more.
        gc_collect();
        ret = decoder_init(s, format, chain, memlimit, FLAGS);
    }
    if (lzma_error(ret) != R::Ok)
        return R::Err;
    out = rd.v;
    return R::Ok;
}

// ------------------------------------------------------------ module level

R z_is_check_supported(const CallArgs &a, Value &out)
{
    i64 id = 0;
    if (!args_only(a, "is_check_supported", 1, 1) ||
        !int_arg(a.args[0], "is_check_supported", "check_id", id))
        return R::Err;
    out = value_bool(lzma_check_is_supported(lzma_check(id)));
    return R::Ok;
}

R z_encode_props(const CallArgs &a, Value &out)
{
    if (!args_only(a, "_encode_filter_properties", 1, 1))
        return R::Err;
    lzma_filter f{};
    FilterOpts store{};
    if (!one_filter(a.args[0], f, store))
        return R::Err;
    u32 size     = 0;
    lzma_ret ret = lzma_properties_size(&size, &f);
    if (lzma_error(ret) != R::Ok)
        return R::Err;
    Vec<u8> buf;
    if (!buf.resize(size))
        return oom();
    ret = lzma_properties_encode(&f, buf.data());
    if (lzma_error(ret) != R::Ok)
        return R::Err;
    out = bytes_new(Str(reinterpret_cast<const char *>(buf.data()), size));
    return out.is_nil() ? R::Err : R::Ok;
}

R z_decode_props(const CallArgs &a, Value &out)
{
    i64 id = 0;
    Str props;
    if (!args_only(a, "_decode_filter_properties", 2, 2) ||
        !int_arg(a.args[0], "_decode_filter_properties", "filter_id", id) ||
        !bytes_arg(a.args[1], "_decode_filter_properties", "encoded_props", props))
        return R::Err;
    // A terminated array, because lzma_filters_free walks one until it finds
    // LZMA_VLI_UNKNOWN -- a lone lzma_filter would send it off the stack.
    lzma_filter f[2]{};
    f[0].id      = u64(id);
    f[1].id      = LZMA_VLI_UNKNOWN;
    lzma_ret ret = lzma_properties_decode(&f[0], nullptr,
                                          reinterpret_cast<const u8 *>(props.data()), props.size());
    if (lzma_error(ret) != R::Ok)
        return R::Err;
    // liblzma allocated the options; build the dict and give them back.
    Root d{ obj_value(dict_new()) };
    if (d.v.is_nil())
        return oom();
    bool ok  = true;
    auto put = [&](Str name, i64 v) {
        if (!ok)
            return;
        Root key{ str_new(name) };
        Root val{ int_from_i64(v) };
        ok = !key.v.is_nil() && !val.v.is_nil() &&
             dict_set(static_cast<DictObj *>(d.v.obj()), key.v, val.v) == R::Ok;
    };
    put("id", i64(f[0].id));
    switch (f[0].id) {
    case LZMA_FILTER_LZMA1:
    case LZMA_FILTER_LZMA2: {
        const lzma_options_lzma *o = static_cast<const lzma_options_lzma *>(f[0].options);
        put("dict_size", i64(o->dict_size));
        put("lc", i64(o->lc));
        put("lp", i64(o->lp));
        put("pb", i64(o->pb));
        break;
    }
    case LZMA_FILTER_DELTA: {
        const lzma_options_delta *o = static_cast<const lzma_options_delta *>(f[0].options);
        put("dist", i64(o->dist));
        break;
    }
    default: {
        const lzma_options_bcj *o = static_cast<const lzma_options_bcj *>(f[0].options);
        if (o)
            put("start_offset", i64(o->start_offset));
        break;
    }
    }
    lzma_filters_free(f, nullptr);
    if (!ok)
        return R::Err;
    out = d.v;
    return R::Ok;
}

constexpr ModDef DEFS[] = {
    { "is_check_supported", z_is_check_supported },
    { "_encode_filter_properties", z_encode_props },
    { "_decode_filter_properties", z_decode_props },
};

INFO_TYPE(version_type, "_lzma.lzma_version_info");

constexpr Str VERSION_NAMES[] = { "major", "minor", "patch", "stability" };

Value version_info()
{
    u32 v         = LZMA_VERSION;
    u32 stability = v % 10u;
    Str words[]   = { "alpha", "beta", "stable" };
    Root word{ str_new(words[stability < 3 ? stability : 2]) };
    if (word.v.is_nil())
        return Value();
    Value n[4] = { Value::of_int(i32(v / 10000000u)), Value::of_int(i32(v / 10000u % 1000u)),
                   Value::of_int(i32(v / 10u % 1000u)), word.v };
    return info_new(&version_type, n, VERSION_NAMES, 4, 4);
}

} // namespace

bool lzma_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    if (!home) {
        home = heap_new<Home>();
        if (!home)
            return false;
        gc_root_hook(home_mark);
    }
    if (home->error.is_nil()) {
        home->error = mod_exc_class("_lzma", "LZMAError", "Exception");
        if (home->error.is_nil())
            return false;
    }
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    if (!mod_put(d, "LZMAError", home->error) || !mod_defs(d, DEFS))
        return false;
    if (!method_install(&comp_type, COMP_METHODS) || !mod_type(d, &comp_type, b_comp_new) ||
        !method_install(&decomp_type, DECOMP_METHODS) || !mod_type(d, &decomp_type, b_decomp_new))
        return false;
    constexpr struct {
        Str name;
        i64 v;
    } NUMBERS[] = {
        { "FORMAT_AUTO", 0 },
        { "FORMAT_XZ", 1 },
        { "FORMAT_ALONE", 2 },
        { "FORMAT_RAW", 3 },
        { "CHECK_NONE", LZMA_CHECK_NONE },
        { "CHECK_CRC32", LZMA_CHECK_CRC32 },
        { "CHECK_CRC64", LZMA_CHECK_CRC64 },
        { "CHECK_SHA256", LZMA_CHECK_SHA256 },
        { "CHECK_ID_MAX", LZMA_CHECK_ID_MAX },
        { "CHECK_UNKNOWN", LZMA_CHECK_ID_MAX + 1 },
        { "FILTER_LZMA1", i64(LZMA_FILTER_LZMA1) },
        { "FILTER_LZMA2", i64(LZMA_FILTER_LZMA2) },
        { "FILTER_DELTA", i64(LZMA_FILTER_DELTA) },
        { "FILTER_X86", i64(LZMA_FILTER_X86) },
        { "FILTER_IA64", i64(LZMA_FILTER_IA64) },
        { "FILTER_ARM", i64(LZMA_FILTER_ARM) },
        { "FILTER_ARMTHUMB", i64(LZMA_FILTER_ARMTHUMB) },
        { "FILTER_ARM64", i64(LZMA_FILTER_ARM64) },
        { "FILTER_RISCV", i64(LZMA_FILTER_RISCV) },
        { "FILTER_SPARC", i64(LZMA_FILTER_SPARC) },
        { "FILTER_POWERPC", i64(LZMA_FILTER_POWERPC) },
        { "MF_HC3", LZMA_MF_HC3 },
        { "MF_HC4", LZMA_MF_HC4 },
        { "MF_BT2", LZMA_MF_BT2 },
        { "MF_BT3", LZMA_MF_BT3 },
        { "MF_BT4", LZMA_MF_BT4 },
        { "MODE_FAST", LZMA_MODE_FAST },
        { "MODE_NORMAL", LZMA_MODE_NORMAL },
        { "PRESET_DEFAULT", LZMA_PRESET_DEFAULT },
        { "PRESET_EXTREME", i64(i32(LZMA_PRESET_EXTREME)) },
    };
    for (const auto &n : NUMBERS)
        if (!mod_int(d, n.name, n.v))
            return false;
    Root ver{ version_info() };
    Root text{ str_new(LZMA_VERSION_STRING) };
    if (ver.v.is_nil() || text.v.is_nil())
        return false;
    d = static_cast<DictObj *>(rd.v.obj());
    return mod_put(d, "lzma_version_info", ver.v) && mod_put(d, "LZMA_VERSION_INFO", ver.v) &&
           mod_put(d, "lzma_version", text.v) && mod_put(d, "LZMA_VERSION", text.v);
}
