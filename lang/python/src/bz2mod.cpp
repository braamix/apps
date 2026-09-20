// `_bz2`: the floor under bz2 and compression.bz2, over braam::bzip2, which
// is libbzip2 1.0.8 rewritten in C++ and whose output is libbzip2's byte for
// byte.
//
// Two objects and four methods, which is the whole of what bz2.py stands on.
// Nothing blocks, so every call is an ordinary native one.
//
// The state is large: a compressor is 7.6 MB at block size 9, which is
// BZ2Compressor's default and BZ2File's, and a decompressor 3.7 MB. Against
// the 100 MB cap that is a handful of open files at once.
#include "bzip2/bzip2.h"
#include "exc.h"
#include "gc.h"
#include "info.h"
#include "kernel/fmt.h"
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

// What a step fills, growing by doubling.
struct Growing {
    Vec<u8> out;
    usize at = 0;

    bool room(Span<u8> &span, usize want)
    {
        if (out.size() < at + want && !out.resize(at + want))
            return false;
        span = Span<u8>(out.data() + at, out.size() - at);
        return true;
    }

    void took(const Span<u8> &span) { at = out.size() - span.size(); }

    Str str() const { return Str(reinterpret_cast<const char *>(out.data()), at); }
};

bool grow_once(Growing &g, Span<u8> &span, usize limit)
{
    usize want = g.out.size() ? g.out.size() : CHUNK;
    if (limit && g.at + want > limit)
        want = limit - g.at;
    return g.room(span, want);
}

// A buffer that will not grow is usually one a dropped codec object is
// still holding megabytes from: collect and try once more.
bool grow_for(Growing &g, Span<u8> &span, usize limit)
{
    return grow_once(g, span, limit) || (gc_collect(), grow_once(g, span, limit));
}

struct Home {
    Value error; // _bz2 raises OSError for a stream error, as CPython does
};

Home *home;

void home_mark()
{
    if (home)
        gc_mark(home->error);
}

R bz_error(BzStatus st, Str what)
{
    switch (st) {
    case BzStatus::NoMemory:
        return oom();
    case BzStatus::Corrupt:
    case BzStatus::NotBzip2:
        // CPython says this for BZ_DATA_ERROR and BZ_DATA_ERROR_MAGIC alike,
        // whatever libbzip2's own message was.
        return err_set("OSError", "Invalid data stream");
    default:
        break;
    }
    Buf<128> b;
    b.put("Internal error - ").put(what);
    return err_set("OSError", b.str());
}

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

// ---------------------------------------------------------- BZ2Compressor

extern const Type comp_type;

struct CompObj : Obj {
    BzCompressor c;
    bool ended;
};

CompObj *comp_of(Value v)
{
    return static_cast<CompObj *>(v.obj());
}

void comp_fini(Obj *o)
{
    static_cast<CompObj *>(o)->c.~BzCompressor();
}

R comp_repr(Value v, String &out)
{
    char buf[24];
    Buf<64> b;
    b.put("<_bz2.BZ2Compressor object at ").put(addr_text(buf, sizeof buf, v.obj())).put(">");
    return out.append(b.str()) ? R::Ok : oom();
}

constexpr Type comp_type{ .name = "_bz2.BZ2Compressor", .fini = comp_fini, .repr = comp_repr };

CompObj *self_comp(const CallArgs &a, Str who)
{
    Value s = method_self(a.args[0]);
    if (!s.is_obj() || s.obj()->type != &comp_type)
        return err_set2("TypeError", "descriptor requires a BZ2Compressor", who), nullptr;
    return comp_of(s);
}

// A Flush or Finish is repeated until it is done, with the same action and
// the input untouched: that is bzip2's rule, not zlib's.
R compress_run(Value self, Str in, BzAction action, Growing &g)
{
    Span<const u8> input(reinterpret_cast<const u8 *>(in.data()), in.size());
    for (;;) {
        Span<u8> span;
        if (!grow_for(g, span, 0))
            return oom();
        BzStatus st = comp_of(self)->c.step(input, span, action);
        g.took(span);
        if (st == BzStatus::End)
            return R::Ok;
        if (st == BzStatus::More)
            continue;
        if (st != BzStatus::Ok && st != BzStatus::Stuck)
            return bz_error(st, "while compressing data");
        // Room left over is what says the compressor had nothing more.
        if (span.size())
            return R::Ok;
    }
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
    if (compress_run(rs.v, data, BzAction::Run, g) != R::Ok)
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
    if (compress_run(rs.v, Str(), BzAction::Finish, g) != R::Ok)
        return R::Err;
    comp_of(rs.v)->ended = true;
    out                  = bytes_new(g.str());
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr Method COMP_METHODS[] = {
    { "compress", cm_compress },
    { "flush", cm_flush },
};

R b_comp_new(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "compresslevel" };
    Value v[1];
    if (!fn_take(a, "BZ2Compressor", NAMES, 0, v))
        return R::Err;
    i64 level = 9;
    if (!v[0].is_nil() && !int_arg(v[0], "BZ2Compressor", "compresslevel", level))
        return R::Err;
    if (level < 1 || level > 9)
        return err_set("ValueError", "compresslevel must be between 1 and 9");
    CompObj *c = static_cast<CompObj *>(obj_alloc(&comp_type, sizeof(CompObj)));
    if (!c)
        return oom();
    new (&c->c) BzCompressor();
    c->ended = false;
    Root rc{ obj_value(c) };
    Result<void> r = comp_of(rc.v)->c.init(int(level));
    if (r.is_err() && r.error() == Error::NoMemory) {
        // 7.6 MB a compressor at the default block size, and the last one may
        // not be swept yet: collect and ask once more.
        gc_collect();
        r = comp_of(rc.v)->c.init(int(level));
    }
    if (r.is_err())
        return r.error() == Error::NoMemory
                   ? oom()
                   : err_set("ValueError", "compresslevel must be between 1 and 9");
    out = rc.v;
    return R::Ok;
}

// -------------------------------------------------------- BZ2Decompressor

extern const Type decomp_type;

struct DecompObj : Obj {
    BzDecompressor d;
    Value unused; // bytes: what followed the end of the stream
    Value input;  // bytes: what a max_length left unread
    bool eof;
    bool needs_input;
    bool failed; // a data error: stepping again could write out of bounds
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
    static_cast<DecompObj *>(o)->d.~BzDecompressor();
}

R decomp_repr(Value v, String &out)
{
    char buf[24];
    Buf<72> b;
    b.put("<_bz2.BZ2Decompressor object at ").put(addr_text(buf, sizeof buf, v.obj())).put(">");
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

constexpr Type decomp_type{ .name    = "_bz2.BZ2Decompressor",
                            .trace   = decomp_trace,
                            .fini    = decomp_fini,
                            .repr    = decomp_repr,
                            .getattr = decomp_getattr };

R dm_decompress(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "data", "max_length" };
    Value v[2];
    Value s = method_self(a.args[0]);
    if (!s.is_obj() || s.obj()->type != &decomp_type)
        return err_set2("TypeError", "descriptor requires a BZ2Decompressor", "decompress");
    Str data;
    if (!positional_only(a, "decompress", "data") || !meth_take(a, "decompress", NAMES, 1, v) ||
        !bytes_arg(v[0], "decompress", "data", data))
        return R::Err;
    i64 max = -1;
    if (!v[1].is_nil() && !int_arg(v[1], "decompress", "max_length", max))
        return R::Err;
    DecompObj *d = decomp_of(s);
    if (d->eof)
        return err_set("EOFError", "End of stream already reached");
    if (d->failed)
        return err_set("ValueError", "Decompressor is unusable after a previous error");
    Root rs{ s };
    // What a max_length left behind comes first, so the caller hands each
    // block over once and this object keeps the remainder.
    Str held;
    if (!buffer_like(d->input, held))
        return R::Err;
    String all;
    if (!held.empty()) {
        if (!all.append(held) || !all.append(data))
            return oom();
        data = all.str();
    }
    Span<const u8> input(reinterpret_cast<const u8 *>(data.data()), data.size());
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
        Span<u8> span;
        if (!grow_for(g, span, max < 0 ? 0 : usize(max)))
            return oom();
        BzStatus st = decomp_of(rs.v)->d.step(input, span);
        g.took(span);
        if (st == BzStatus::End) {
            end = true;
            break;
        }
        if (st != BzStatus::Ok && st != BzStatus::Stuck) {
            DecompObj *bad   = decomp_of(rs.v);
            bad->failed      = true;
            bad->needs_input = false;
            return bz_error(st, "while decompressing data");
        }
        if (span.size())
            break;
    }
    Str left(reinterpret_cast<const char *>(input.data()), input.size());
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

R b_decomp_new(const CallArgs &a, Value &out)
{
    if (a.nkw || a.nargs)
        return err_set("TypeError", "BZ2Decompressor() takes no arguments");
    DecompObj *d = static_cast<DecompObj *>(obj_alloc(&decomp_type, sizeof(DecompObj)));
    if (!d)
        return oom();
    new (&d->d) BzDecompressor();
    d->unused      = Value();
    d->input       = Value();
    d->eof         = false;
    d->needs_input = true;
    d->failed      = false;
    Root rd{ obj_value(d) };
    Root empty{ bytes_new(Str()) };
    if (empty.v.is_nil())
        return R::Err;
    decomp_of(rd.v)->unused = empty.v;
    decomp_of(rd.v)->input  = empty.v;
    Result<void> r          = decomp_of(rd.v)->d.init();
    if (r.is_err() && r.error() == Error::NoMemory) {
        gc_collect();
        r = decomp_of(rd.v)->d.init();
    }
    if (r.is_err())
        return oom();
    out = rd.v;
    return R::Ok;
}

INFO_TYPE(version_type, "_bz2.bzlib_version_info");

constexpr Str VERSION_NAMES[] = { "major", "minor", "patch" };

// What BZ2_bzlibVersion() answers for the 1.0.8 this is a rewrite of; the
// date after the number is upstream's own suffix.
constexpr Str BZLIB_VERSION = "1.0.8, 13-Jul-2019";

Value version_info()
{
    Value n[3];
    usize at = 0;
    for (u32 i = 0; i < 3; i++) {
        i64 part = 0;
        while (at < BZLIB_VERSION.size() && BZLIB_VERSION[at] >= '0' && BZLIB_VERSION[at] <= '9')
            part = part * 10 + (BZLIB_VERSION[at++] - '0');
        if (at < BZLIB_VERSION.size() && BZLIB_VERSION[at] == '.')
            at++;
        n[i] = Value::of_int(i32(part));
    }
    return info_new(&version_type, n, VERSION_NAMES, 3, 3);
}

} // namespace

bool bz2_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    if (!home) {
        home = heap_new<Home>();
        if (!home)
            return false;
        gc_root_hook(home_mark);
    }
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    if (!method_install(&comp_type, COMP_METHODS) || !mod_type(d, &comp_type, b_comp_new) ||
        !method_install(&decomp_type, DECOMP_METHODS) || !mod_type(d, &decomp_type, b_decomp_new))
        return false;
    Root ver{ version_info() };
    Root text{ str_new(BZLIB_VERSION) };
    if (ver.v.is_nil() || text.v.is_nil())
        return false;
    d = static_cast<DictObj *>(rd.v.obj());
    return mod_put(d, "bzlib_version", text.v) && mod_put(d, "bzlib_version_info", ver.v);
}
