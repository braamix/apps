// `zlib`: deflate and inflate over braam::zlib, which is zlib 1.3.2.1
// rewritten in C++ and whose output is zlib's byte for byte.
//
// Nothing here blocks -- a step computes and returns -- so every call is an
// ordinary native one and no continuation is needed. What each object holds
// is a Deflater or an Inflater, which is one heap block the collector frees
// through `fini`; a Deflater is 262 KiB of it at the defaults, so an object
// dropped without being finished is worth collecting.
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
#include "zlib/zlib.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// zlib's own numbers, which are what the module's constants are and what an
// error message quotes.
enum : int {
    Z_OK            = 0,
    Z_STREAM_END    = 1,
    Z_NEED_DICT     = 2,
    Z_ERRNO         = -1,
    Z_STREAM_ERROR  = -2,
    Z_DATA_ERROR    = -3,
    Z_MEM_ERROR     = -4,
    Z_BUF_ERROR     = -5,
    Z_VERSION_ERROR = -6,
};

enum : int {
    Z_NO_FLUSH      = 0,
    Z_PARTIAL_FLUSH = 1,
    Z_SYNC_FLUSH    = 2,
    Z_FULL_FLUSH    = 3,
    Z_FINISH        = 4,
    Z_BLOCK         = 5,
    Z_TREES         = 6,
};

constexpr int MAX_WBITS      = 15;
constexpr int DEF_MEM_LEVEL  = 8;
constexpr i64 DEF_BUF_SIZE   = 16384;
constexpr usize CHUNK        = 8192; // what one step fills before it grows
constexpr Str ZLIB_VERSION_S = "1.3.2.1";

int code_of(ZStatus s)
{
    switch (s) {
    case ZStatus::Ok:
        return Z_OK;
    case ZStatus::End:
        return Z_STREAM_END;
    case ZStatus::NeedDict:
        return Z_NEED_DICT;
    case ZStatus::Stuck:
        return Z_BUF_ERROR;
    case ZStatus::Corrupt:
        return Z_DATA_ERROR;
    case ZStatus::NoMemory:
        return Z_MEM_ERROR;
    default:
        return Z_STREAM_ERROR;
    }
}

struct Home {
    Value error; // zlib.error
};

Home *home;

void home_mark()
{
    if (home)
        gc_mark(home->error);
}

// zlibmodule.c's zlib_error, word for word: the stream's own message when it
// has one, and the standing text for the three codes that often do not.
R zlib_error(int err, Str what, Str why)
{
    Str zmsg = why;
    if (zmsg.empty()) {
        if (err == Z_BUF_ERROR)
            zmsg = "incomplete or truncated stream";
        else if (err == Z_STREAM_ERROR)
            zmsg = "inconsistent stream state";
        else if (err == Z_DATA_ERROR)
            zmsg = "invalid input data";
    }
    char num[24];
    Buf<256> b;
    b.put("Error ").put(int_text(num, sizeof num, i64(err))).put(" ").put(what);
    if (!zmsg.empty())
        b.put(": ").put(zmsg.size() > 200 ? zmsg.substr(0, 200) : zmsg);
    if (home && !home->error.is_nil())
        return mod_raise(home->error, b.str());
    return err_set("Exception", b.str());
}

// ---------------------------------------------------------------- arguments

// The whole of a bytes-like argument.
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

// `data` is positional-only in every one of these signatures, so a data=
// keyword is not that argument and CPython says so.
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

// An argument written as a class with __index__: call it, put the answer in
// its place and enter the builtin again. Positional only, which is where
// CPython's own converters meet one.
bool redo_index(const CallArgs &a, R (*again)(const CallArgs &, Value &out), u32 from, u32 to,
                Value &out, R &r)
{
    for (u32 i = from; i <= to; i++)
        if (redo_converted(a, i, "__index__", again, out, r))
            return true;
    return false;
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

// wbits, as zlib spells it: 8..15 is a zlib wrapper, -8..-15 raw, 24..31
// gzip, and 40..47 either of the last two by the header. `bits` takes the
// window size, zero meaning "whatever the header says".
bool wbits_of(i64 wbits, bool inflating, ZFormat &fmt, u8 &bits)
{
    i64 w = wbits;
    if (w >= 40 && w <= 47 && inflating) {
        fmt = ZFormat::Auto;
        w -= 32;
    } else if (w >= 24 && w <= 31) {
        fmt = ZFormat::Gzip;
        w -= 16;
    } else if (w >= 8 && w <= 15) {
        fmt = ZFormat::Zlib;
    } else if (w <= -8 && w >= -15) {
        fmt = ZFormat::Raw;
        w   = -w;
    } else if (w == 0 && inflating) {
        // zlib takes 0 as "the header decides", for the zlib wrapper alone.
        fmt  = ZFormat::Zlib;
        bits = 0;
        return true;
    } else {
        err_set("ValueError", "Invalid initialization option");
        return false;
    }
    bits = u8(w);
    return true;
}

// -------------------------------------------------------------- the buffers

// What a step fills. The output grows by doubling, as CPython's
// _BlocksOutputBuffer does, so a long stream is not a write per chunk.
struct Growing {
    Vec<u8> out;
    usize at = 0; // how much of `out` holds real output

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

// `limit` is how much output is wanted at most, or 0 for all of it.
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

// --------------------------------------------------------------- compressobj

extern const Type comp_type;

struct CompObj : Obj {
    Deflater d;
    // What copy() needs, since a Deflater does not report its own parameters.
    int level, wbits, memlevel;
    ZStrategy strategy;
    bool ended; // flush(Z_FINISH) has run and the stream is over
};

CompObj *comp_of(Value v)
{
    return static_cast<CompObj *>(v.obj());
}

void comp_fini(Obj *o)
{
    static_cast<CompObj *>(o)->d.~Deflater();
}

R comp_repr(Value v, String &out)
{
    char buf[24];
    Buf<64> b;
    b.put("<zlib.Compress object at ").put(addr_text(buf, sizeof buf, v.obj())).put(">");
    return out.append(b.str()) ? R::Ok : oom();
}

constexpr Type comp_type{ .name = "zlib.Compress", .fini = comp_fini, .repr = comp_repr };

CompObj *self_comp(const CallArgs &a, Str who)
{
    Value s = method_self(a.args[0]);
    if (!s.is_obj() || s.obj()->type != &comp_type)
        return err_set2("TypeError", "descriptor requires a zlib.Compress object", who), nullptr;
    return comp_of(s);
}

// One deflate loop: steps until the output buffer came back with room to
// spare, which is zlib's own "do ... while (avail_out == 0)".
R deflate_run(CompObj *c, Str in, ZFlush flush, Growing &g)
{
    Span<const u8> input(reinterpret_cast<const u8 *>(in.data()), in.size());
    for (;;) {
        Span<u8> span;
        if (!grow_for(g, span, 0))
            return oom();
        ZStatus st = c->d.step(input, span, flush);
        g.took(span);
        if (st == ZStatus::End)
            return R::Ok;
        if (st != ZStatus::Ok && st != ZStatus::Stuck)
            return zlib_error(code_of(st), "while compressing data", c->d.why());
        // Room left over is what says deflate had nothing more to put out. A
        // buffer filled to the brim says the opposite, whatever the status --
        // a Finish that did not fit is Stuck with the buffer full.
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
        return zlib_error(Z_STREAM_ERROR, "while compressing data", Str());
    Growing g;
    if (deflate_run(comp_of(method_self(a.args[0])), data, ZFlush::None, g) != R::Ok)
        return R::Err;
    out = bytes_new(g.str());
    return out.is_nil() ? R::Err : R::Ok;
}

R cm_flush(const CallArgs &a, Value &out)
{
    R r;
    if (redo_index(a, cm_flush, 1, 1, out, r))
        return r;
    static const Str NAMES[] = { "mode" };
    Value v[1];
    CompObj *c = self_comp(a, "flush");
    if (!c || !meth_take(a, "flush", NAMES, 0, v))
        return R::Err;
    i64 mode = Z_FINISH;
    if (!v[0].is_nil() && !int_arg(v[0], "flush", "mode", mode))
        return R::Err;
    if (mode == Z_NO_FLUSH) {
        out = bytes_new(Str());
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (mode < 0 || mode > Z_TREES)
        return err_set("ValueError", "Invalid flush option");
    if (c->ended)
        return zlib_error(Z_STREAM_ERROR, "while flushing", Str());
    Growing g;
    if (deflate_run(c, Str(), ZFlush(u8(mode)), g) != R::Ok)
        return R::Err;
    if (mode == Z_FINISH)
        comp_of(method_self(a.args[0]))->ended = true;
    out = bytes_new(g.str());
    return out.is_nil() ? R::Err : R::Ok;
}

Value comp_new(int level, ZFormat fmt, u8 bits, int memlevel, ZStrategy strategy, Str zdict)
{
    CompObj *c = static_cast<CompObj *>(obj_alloc(&comp_type, sizeof(CompObj)));
    if (!c)
        return oom(), Value();
    new (&c->d) Deflater();
    c->level    = level;
    c->wbits    = bits;
    c->memlevel = memlevel;
    c->strategy = strategy;
    c->ended    = false;
    Root rc{ obj_value(c) };
    Result<void> r = comp_of(rc.v)->d.init(level, fmt, bits, u8(memlevel), strategy);
    if (r.is_err() && r.error() == Error::NoMemory) {
        // 262 KiB a deflater at the defaults; the last one may not be swept.
        gc_collect();
        r = comp_of(rc.v)->d.init(level, fmt, bits, u8(memlevel), strategy);
    }
    if (r.is_err()) {
        if (r.error() == Error::NoMemory)
            return oom(), Value();
        err_set("ValueError", "Invalid initialization option");
        return Value();
    }
    if (!zdict.empty()) {
        ZStatus st = comp_of(rc.v)->d.set_dictionary(
            Bytes(reinterpret_cast<const u8 *>(zdict.data()), zdict.size()));
        if (st != ZStatus::Ok)
            return zlib_error(code_of(st), "while setting zdict", Str()), Value();
    }
    return rc.v;
}

R cm_copy(const CallArgs &a, Value &out)
{
    CompObj *c = self_comp(a, "copy");
    if (!c || !meth_args(a, "copy", 0, 1)) // __deepcopy__ takes a memo
        return R::Err;
    // flush(Z_FINISH) ended the stream, and zlib has nothing left to copy.
    if (c->ended)
        return err_set("ValueError", "Inconsistent stream state");
    Root rs{ method_self(a.args[0]) };
    CompObj *n = static_cast<CompObj *>(obj_alloc(&comp_type, sizeof(CompObj)));
    if (!n)
        return oom();
    new (&n->d) Deflater();
    c           = comp_of(rs.v);
    n->level    = c->level;
    n->wbits    = c->wbits;
    n->memlevel = c->memlevel;
    n->strategy = c->strategy;
    n->ended    = c->ended;
    Root rn{ obj_value(n) };
    ZStatus st = comp_of(rn.v)->d.copy_from(comp_of(rs.v)->d);
    if (st != ZStatus::Ok)
        return zlib_error(code_of(st), "while copying compression object", Str());
    out = rn.v;
    return R::Ok;
}

constexpr Method COMP_METHODS[] = {
    { "compress", cm_compress }, { "flush", cm_flush },       { "copy", cm_copy },
    { "__copy__", cm_copy },     { "__deepcopy__", cm_copy },
};

// ------------------------------------------------------------- decompressobj

extern const Type decomp_type;

struct DecompObj : Obj {
    Inflater z;
    Value unused;     // bytes: what followed the end of the stream
    Value unconsumed; // bytes: what max_length left behind
    Value zdict;      // bytes or Nil, for a Raw stream and for copy()
    int wbits;
    bool eof;
    bool ended;      // flush() has run
    bool dict_given; // the dictionary has been handed to the stream
};

DecompObj *decomp_of(Value v)
{
    return static_cast<DecompObj *>(v.obj());
}

void decomp_trace(Obj *o)
{
    DecompObj *d = static_cast<DecompObj *>(o);
    gc_mark(d->unused);
    gc_mark(d->unconsumed);
    gc_mark(d->zdict);
}

void decomp_fini(Obj *o)
{
    static_cast<DecompObj *>(o)->z.~Inflater();
}

R decomp_repr(Value v, String &out)
{
    char buf[24];
    Buf<64> b;
    b.put("<zlib.Decompress object at ").put(addr_text(buf, sizeof buf, v.obj())).put(">");
    return out.append(b.str()) ? R::Ok : oom();
}

R decomp_getattr(Value v, StrObj *name, Value &out)
{
    DecompObj *d = decomp_of(v);
    if (name->str() == "unused_data")
        return out = d->unused, R::Ok;
    if (name->str() == "unconsumed_tail")
        return out = d->unconsumed, R::Ok;
    if (name->str() == "eof")
        return out = value_bool(d->eof), R::Ok;
    return R::NotImpl;
}

constexpr Type decomp_type{ .name    = "zlib.Decompress",
                            .trace   = decomp_trace,
                            .fini    = decomp_fini,
                            .repr    = decomp_repr,
                            .getattr = decomp_getattr };

DecompObj *self_decomp(const CallArgs &a, Str who)
{
    Value s = method_self(a.args[0]);
    if (!s.is_obj() || s.obj()->type != &decomp_type)
        return err_set2("TypeError", "descriptor requires a zlib.Decompress object", who), nullptr;
    return decomp_of(s);
}

// A Raw stream takes its dictionary before the first step, a Zlib one only
// after NeedDict says which it wants.
R dict_if_wanted(DecompObj *d, bool now)
{
    if (d->dict_given || d->zdict.is_nil())
        return R::Ok;
    Str z;
    if (!buffer_like(d->zdict, z))
        return R::Err;
    if (!now)
        return R::Ok;
    ZStatus st = d->z.set_dictionary(Bytes(reinterpret_cast<const u8 *>(z.data()), z.size()));
    if (st != ZStatus::Ok)
        return zlib_error(code_of(st), "while decompressing data", d->z.why());
    d->dict_given = true;
    return R::Ok;
}

// One inflate loop over `in`, into `g`, stopping at `limit` bytes of output
// (0 for all of it). `left` takes what of `in` was not consumed.
R inflate_run(Value self, Str in, usize limit, ZFlush flush, Growing &g, Str &left, bool &end)
{
    DecompObj *d = decomp_of(self);
    Span<const u8> input(reinterpret_cast<const u8 *>(in.data()), in.size());
    end = false;
    for (;;) {
        if (limit && g.at >= limit)
            break;
        Span<u8> span;
        if (!grow_for(g, span, limit))
            return oom();
        ZStatus st = d->z.step(input, span, flush);
        g.took(span);
        if (st == ZStatus::NeedDict) {
            if (dict_if_wanted(d, true) != R::Ok)
                return R::Err;
            if (!d->dict_given)
                return zlib_error(Z_NEED_DICT, "while decompressing data", d->z.why());
            continue;
        }
        if (st == ZStatus::End) {
            end = true;
            break;
        }
        if (st != ZStatus::Ok && st != ZStatus::Stuck)
            return zlib_error(code_of(st), "while decompressing data", d->z.why());
        // Room to spare means inflate wants more input, not more room.
        if (span.size())
            break;
    }
    left = Str(reinterpret_cast<const char *>(input.data()), input.size());
    return R::Ok;
}

R dm_decompress(const CallArgs &a, Value &out)
{
    R r;
    if (redo_index(a, dm_decompress, 2, 2, out, r))
        return r;
    static const Str NAMES[] = { "data", "max_length" };
    Value v[2];
    DecompObj *d = self_decomp(a, "decompress");
    Str data;
    if (!d || !positional_only(a, "decompress", "data") ||
        !meth_take(a, "decompress", NAMES, 1, v) || !bytes_arg(v[0], "decompress", "data", data))
        return R::Err;
    i64 max = 0;
    if (!v[1].is_nil() && !int_arg(v[1], "decompress", "max_length", max))
        return R::Err;
    if (max < 0)
        return err_set("ValueError", "max_length must be non-negative");
    Root rs{ method_self(a.args[0]) };
    if (dict_if_wanted(d, d->wbits < 0) != R::Ok)
        return R::Err;
    Growing g;
    Str left;
    bool end = false;
    if (inflate_run(rs.v, data, usize(max), ZFlush::None, g, left, end) != R::Ok)
        return R::Err;
    Root made{ bytes_new(g.str()) };
    if (made.v.is_nil())
        return R::Err;
    // What is left over is the tail when a max_length cut the output short,
    // and what followed the stream once it has ended.
    Root tail{ bytes_new(end ? Str() : left) };
    if (tail.v.is_nil())
        return R::Err;
    Root after{ bytes_new(end ? left : Str()) };
    if (after.v.is_nil())
        return R::Err;
    d             = decomp_of(rs.v);
    d->unconsumed = tail.v;
    if (end) {
        d->eof = true;
        Str had;
        if (buffer_like(d->unused, had) && !had.empty()) {
            // A second call after the end appends, as zlib's own does.
            String both;
            if (!both.append(had) || !both.append(left))
                return oom();
            Root more{ bytes_new(both.str()) };
            if (more.v.is_nil())
                return R::Err;
            decomp_of(rs.v)->unused = more.v;
        } else {
            d->unused = after.v;
        }
    }
    out = made.v;
    return R::Ok;
}

R dm_flush(const CallArgs &a, Value &out)
{
    R r;
    if (redo_index(a, dm_flush, 1, 1, out, r))
        return r;
    static const Str NAMES[] = { "length" };
    Value v[1];
    DecompObj *d = self_decomp(a, "flush");
    if (!d || !meth_take(a, "flush", NAMES, 0, v))
        return R::Err;
    i64 length = DEF_BUF_SIZE;
    if (!v[0].is_nil() && !int_arg(v[0], "flush", "length", length))
        return R::Err;
    if (length <= 0)
        return err_set("ValueError", "length must be greater than zero");
    Root rs{ method_self(a.args[0]) };
    Str had;
    if (!buffer_like(d->unconsumed, had))
        return R::Err;
    Growing g;
    Str left;
    bool end = false;
    if (inflate_run(rs.v, had, 0, ZFlush::Finish, g, left, end) != R::Ok)
        return R::Err;
    Root made{ bytes_new(g.str()) };
    if (made.v.is_nil())
        return R::Err;
    Root empty{ bytes_new(Str()) };
    if (empty.v.is_nil())
        return R::Err;
    d             = decomp_of(rs.v);
    d->unconsumed = empty.v;
    d->ended      = true;
    if (end)
        d->eof = true;
    out = made.v;
    return R::Ok;
}

Value decomp_new(ZFormat fmt, u8 bits, int wbits, Value zdict)
{
    Root rz{ zdict };
    DecompObj *d = static_cast<DecompObj *>(obj_alloc(&decomp_type, sizeof(DecompObj)));
    if (!d)
        return oom(), Value();
    new (&d->z) Inflater();
    d->unused     = Value();
    d->unconsumed = Value();
    d->zdict      = rz.v;
    d->wbits      = wbits;
    d->eof        = false;
    d->ended      = false;
    d->dict_given = false;
    Root rd{ obj_value(d) };
    Root empty{ bytes_new(Str()) };
    if (empty.v.is_nil())
        return Value();
    decomp_of(rd.v)->unused     = empty.v;
    decomp_of(rd.v)->unconsumed = empty.v;
    Result<void> r              = decomp_of(rd.v)->z.init(fmt, bits);
    if (r.is_err()) {
        if (r.error() == Error::NoMemory)
            return oom(), Value();
        err_set("ValueError", "Invalid initialization option");
        return Value();
    }
    return rd.v;
}

R dm_copy(const CallArgs &a, Value &out)
{
    DecompObj *d = self_decomp(a, "copy");
    if (!d || !meth_args(a, "copy", 0, 1))
        return R::Err;
    if (d->ended)
        return err_set("ValueError", "Inconsistent stream state");
    Root rs{ method_self(a.args[0]) };
    DecompObj *n = static_cast<DecompObj *>(obj_alloc(&decomp_type, sizeof(DecompObj)));
    if (!n)
        return oom();
    new (&n->z) Inflater();
    d             = decomp_of(rs.v);
    n->unused     = d->unused;
    n->unconsumed = d->unconsumed;
    n->zdict      = d->zdict;
    n->wbits      = d->wbits;
    n->eof        = d->eof;
    n->ended      = d->ended;
    n->dict_given = d->dict_given;
    Root rn{ obj_value(n) };
    ZStatus st = decomp_of(rn.v)->z.copy_from(decomp_of(rs.v)->z);
    if (st != ZStatus::Ok)
        return zlib_error(code_of(st), "while copying decompression object", Str());
    out = rn.v;
    return R::Ok;
}

constexpr Method DECOMP_METHODS[] = {
    { "decompress", dm_decompress }, { "flush", dm_flush },       { "copy", dm_copy },
    { "__copy__", dm_copy },         { "__deepcopy__", dm_copy },
};

// ------------------------------------------------------- _ZlibDecompressor
//
// The 3.14 decompressor, which gzip's reader and compression._common's
// DecompressReader drive: one stream, and `needs_input` rather than a
// tail the caller carries back.

extern const Type zdec_type;

struct ZDecObj : Obj {
    Inflater z;
    Value unused; // bytes after the end of the stream
    Value input;  // bytes: what a max_length left unread
    Value zdict;
    int wbits;
    bool eof;
    bool needs_input;
    bool dict_given;
};

ZDecObj *zdec_of(Value v)
{
    return static_cast<ZDecObj *>(v.obj());
}

void zdec_trace(Obj *o)
{
    ZDecObj *d = static_cast<ZDecObj *>(o);
    gc_mark(d->unused);
    gc_mark(d->input);
    gc_mark(d->zdict);
}

void zdec_fini(Obj *o)
{
    static_cast<ZDecObj *>(o)->z.~Inflater();
}

R zdec_repr(Value v, String &out)
{
    char buf[24];
    Buf<72> b;
    b.put("<zlib._ZlibDecompressor object at ").put(addr_text(buf, sizeof buf, v.obj())).put(">");
    return out.append(b.str()) ? R::Ok : oom();
}

R zdec_getattr(Value v, StrObj *name, Value &out)
{
    ZDecObj *d = zdec_of(v);
    if (name->str() == "unused_data")
        return out = d->unused, R::Ok;
    if (name->str() == "eof")
        return out = value_bool(d->eof), R::Ok;
    if (name->str() == "needs_input")
        return out = value_bool(d->needs_input), R::Ok;
    return R::NotImpl;
}

constexpr Type zdec_type{ .name    = "zlib._ZlibDecompressor",
                          .trace   = zdec_trace,
                          .fini    = zdec_fini,
                          .repr    = zdec_repr,
                          .getattr = zdec_getattr };

R zm_decompress(const CallArgs &a, Value &out)
{
    R r;
    if (redo_index(a, zm_decompress, 2, 2, out, r))
        return r;
    Value s = method_self(a.args[0]);
    if (!s.is_obj() || s.obj()->type != &zdec_type)
        return err_set2("TypeError", "descriptor requires a _ZlibDecompressor", "decompress");
    static const Str NAMES[] = { "data", "max_length" };
    Value v[2];
    ZDecObj *d = zdec_of(s);
    Str data;
    if (!positional_only(a, "decompress", "data") || !meth_take(a, "decompress", NAMES, 1, v) ||
        !bytes_arg(v[0], "decompress", "data", data))
        return R::Err;
    i64 max = -1;
    if (!v[1].is_nil() && !int_arg(v[1], "decompress", "max_length", max))
        return R::Err;
    if (d->eof)
        return err_set("EOFError", "End of stream already reached");
    Root rs{ s };
    // What was left over last time comes first, so the caller hands each
    // block once and the object keeps the remainder.
    Str held;
    if (!buffer_like(d->input, held))
        return R::Err;
    String all;
    if (!held.empty()) {
        if (!all.append(held) || !all.append(data))
            return oom();
        data = all.str();
    }
    if (d->zdict.is_nil() || d->dict_given) {
        // nothing to hand over
    } else if (d->wbits < 0) {
        Str z;
        if (!buffer_like(d->zdict, z))
            return R::Err;
        ZStatus st = d->z.set_dictionary(Bytes(reinterpret_cast<const u8 *>(z.data()), z.size()));
        if (st != ZStatus::Ok)
            return zlib_error(code_of(st), "while decompressing data", d->z.why());
        d->dict_given = true;
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
        ZStatus st = zdec_of(rs.v)->z.step(input, span, ZFlush::None);
        g.took(span);
        if (st == ZStatus::NeedDict) {
            ZDecObj *o = zdec_of(rs.v);
            Str z;
            if (o->zdict.is_nil() || !buffer_like(o->zdict, z))
                return zlib_error(Z_NEED_DICT, "while decompressing data", o->z.why());
            ZStatus s2 =
                o->z.set_dictionary(Bytes(reinterpret_cast<const u8 *>(z.data()), z.size()));
            if (s2 != ZStatus::Ok)
                return zlib_error(code_of(s2), "while decompressing data", o->z.why());
            o->dict_given = true;
            continue;
        }
        if (st == ZStatus::End) {
            end = true;
            break;
        }
        if (st != ZStatus::Ok && st != ZStatus::Stuck)
            return zlib_error(code_of(st), "while decompressing data", zdec_of(rs.v)->z.why());
        if (span.size())
            break;
    }
    Str left(reinterpret_cast<const char *>(input.data()), input.size());
    Root made{ bytes_new(g.str()) };
    if (made.v.is_nil())
        return R::Err;
    Root rest{ bytes_new(end ? Str() : left) };
    if (rest.v.is_nil())
        return R::Err;
    Root after{ bytes_new(end ? left : Str()) };
    if (after.v.is_nil())
        return R::Err;
    d              = zdec_of(rs.v);
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

constexpr Method ZDEC_METHODS[] = {
    { "decompress", zm_decompress },
};

R z_zdec_new(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "wbits", "zdict" };
    Value v[2];
    if (!fn_take(a, "_ZlibDecompressor", NAMES, 0, v))
        return R::Err;
    i64 wbits = MAX_WBITS;
    if (!v[0].is_nil() && !int_arg(v[0], "_ZlibDecompressor", "wbits", wbits))
        return R::Err;
    ZFormat fmt = ZFormat::Zlib;
    u8 bits     = MAX_WBITS;
    if (!wbits_of(wbits, true, fmt, bits))
        return R::Err;
    Root rz{ v[1] };
    if (!rz.v.is_nil()) {
        Str z;
        if (!bytes_arg(rz.v, "_ZlibDecompressor", "zdict", z))
            return R::Err;
    }
    ZDecObj *d = static_cast<ZDecObj *>(obj_alloc(&zdec_type, sizeof(ZDecObj)));
    if (!d)
        return oom();
    new (&d->z) Inflater();
    d->unused      = Value();
    d->input       = Value();
    d->zdict       = rz.v;
    d->wbits       = int(wbits);
    d->eof         = false;
    d->needs_input = true;
    d->dict_given  = false;
    Root rd{ obj_value(d) };
    Root empty{ bytes_new(Str()) };
    if (empty.v.is_nil())
        return R::Err;
    zdec_of(rd.v)->unused = empty.v;
    zdec_of(rd.v)->input  = empty.v;
    Result<void> r        = zdec_of(rd.v)->z.init(fmt, bits);
    if (r.is_err())
        return r.error() == Error::NoMemory
                   ? oom()
                   : err_set("ValueError", "Invalid initialization option");
    out = rd.v;
    return R::Ok;
}

// ------------------------------------------------------------ module level

R z_adler32(const CallArgs &a, Value &out)
{
    Str d;
    i64 start = 1;
    if (!args_only(a, "adler32", 1, 2) || !bytes_arg(a.args[0], "adler32", "data", d) ||
        (a.nargs > 1 && !int_arg(a.args[1], "adler32", "value", start)))
        return R::Err;
    u32 v = adler32_update(u32(start), d);
    out   = int_from_i64(i64(v));
    return out.is_nil() ? R::Err : R::Ok;
}

R z_crc32(const CallArgs &a, Value &out)
{
    Str d;
    i64 start = 0;
    if (!args_only(a, "crc32", 1, 2) || !bytes_arg(a.args[0], "crc32", "data", d) ||
        (a.nargs > 1 && !int_arg(a.args[1], "crc32", "value", start)))
        return R::Err;
    u32 v = crc32_update(u32(start), d);
    out   = int_from_i64(i64(v));
    return out.is_nil() ? R::Err : R::Ok;
}

// A length for a combine: an int, as CPython's unsigned long long argument.
bool len_arg(Value v, Str who, u64 &out)
{
    if (!is_intval(v)) {
        Buf<128> b;
        b.put(who).put("() argument 3 must be int, not '").put(type_name(v)).put("'");
        return err_set("TypeError", b.str()) == R::Ok;
    }
    i64 n = 0;
    if (!as_index(v, n))
        return err_set("OverflowError",
                       "Python int too large to convert to C unsigned long long") == R::Ok;
    if (n < 0)
        return err_set("ValueError", "length must be non-negative") == R::Ok;
    out = u64(n);
    return true;
}

R z_crc32_combine(const CallArgs &a, Value &out)
{
    i64 x = 0, y = 0;
    u64 len = 0;
    if (!args_only(a, "crc32_combine", 3, 3) || !int_arg(a.args[0], "crc32_combine", "crc1", x) ||
        !int_arg(a.args[1], "crc32_combine", "crc2", y) ||
        !len_arg(a.args[2], "crc32_combine", len))
        return R::Err;
    out = int_from_i64(i64(crc32_combine(u32(x), u32(y), len)));
    return out.is_nil() ? R::Err : R::Ok;
}

R z_adler32_combine(const CallArgs &a, Value &out)
{
    i64 x = 0, y = 0;
    u64 len = 0;
    if (!args_only(a, "adler32_combine", 3, 3) ||
        !int_arg(a.args[0], "adler32_combine", "adler1", x) ||
        !int_arg(a.args[1], "adler32_combine", "adler2", y) ||
        !len_arg(a.args[2], "adler32_combine", len))
        return R::Err;
    out = int_from_i64(i64(adler32_combine(u32(x), u32(y), len)));
    return out.is_nil() ? R::Err : R::Ok;
}

// PEP 562: zlib.__version__ is the module's own version, which CPython has
// been saying it will remove since 3.14.
R z_getattr(const CallArgs &a, Value &out)
{
    if (!args_only(a, "__getattr__", 1, 1))
        return R::Err;
    Str name = is_str(a.args[0]) ? str_of(a.args[0])->str() : Str();
    if (name == "__version__") {
        Root v{ str_new("1.0") };
        if (v.v.is_nil())
            return R::Err;
        return warn_then("DeprecationWarning",
                         "'__version__' is deprecated and slated for removal in Python 3.20", 1,
                         v.v, out);
    }
    return err_set2("AttributeError", "module 'zlib' has no attribute", name);
}

R z_compress(const CallArgs &a, Value &out)
{
    R rr;
    if (redo_index(a, z_compress, 1, 2, out, rr))
        return rr;
    static const Str NAMES[] = { "data", "level", "wbits" };
    Value v[3];
    Str d;
    if (!positional_only(a, "compress", "data") || !fn_take(a, "compress", NAMES, 1, v) ||
        !bytes_arg(v[0], "compress", "data", d))
        return R::Err;
    i64 level = -1, wbits = MAX_WBITS;
    if (!v[1].is_nil() && !int_arg(v[1], "compress", "level", level))
        return R::Err;
    if (!v[2].is_nil() && !int_arg(v[2], "compress", "wbits", wbits))
        return R::Err;
    ZFormat fmt = ZFormat::Zlib;
    u8 bits     = MAX_WBITS;
    if (!wbits_of(wbits, false, fmt, bits))
        return R::Err;
    if (level < -1 || level > 9)
        return zlib_error(Z_STREAM_ERROR, "while compressing data", Str());
    Result<String> r = zlib_compress(d, fmt, int(level));
    if (r.is_err())
        return r.error() == Error::NoMemory
                   ? oom()
                   : zlib_error(Z_STREAM_ERROR, "while compressing data", Str());
    out = bytes_new(r.value().str());
    return out.is_nil() ? R::Err : R::Ok;
}

R z_decompress(const CallArgs &a, Value &out)
{
    R rr;
    if (redo_index(a, z_decompress, 1, 2, out, rr))
        return rr;
    static const Str NAMES[] = { "data", "wbits", "bufsize" };
    Value v[3];
    Str d;
    if (!positional_only(a, "decompress", "data") || !fn_take(a, "decompress", NAMES, 1, v) ||
        !bytes_arg(v[0], "decompress", "data", d))
        return R::Err;
    i64 wbits = MAX_WBITS, bufsize = DEF_BUF_SIZE;
    if (!v[1].is_nil() && !int_arg(v[1], "decompress", "wbits", wbits))
        return R::Err;
    if (!v[2].is_nil() && !int_arg(v[2], "decompress", "bufsize", bufsize))
        return R::Err;
    if (bufsize < 0)
        return err_set("ValueError", "bufsize must be non-negative");
    ZFormat fmt = ZFormat::Zlib;
    u8 bits     = MAX_WBITS;
    if (!wbits_of(wbits, true, fmt, bits))
        return R::Err;
    // No limit of our own: the cap is the process's, and a MemoryError says
    // so more usefully than a short answer would.
    Inflater z;
    Result<void> ir = z.init(fmt, bits);
    if (ir.is_err())
        return ir.error() == Error::NoMemory
                   ? oom()
                   : err_set("ValueError", "Invalid initialization option");
    Span<const u8> input(reinterpret_cast<const u8 *>(d.data()), d.size());
    Growing g;
    for (;;) {
        Span<u8> span;
        if (!grow_for(g, span, 0))
            return oom();
        ZStatus st = z.step(input, span, ZFlush::None);
        g.took(span);
        if (st == ZStatus::End)
            break;
        if (st != ZStatus::Ok && st != ZStatus::Stuck)
            return zlib_error(code_of(st), "while decompressing data", z.why());
        // Room left and no End: the stream was cut short.
        if (span.size())
            return zlib_error(Z_BUF_ERROR, "while decompressing data", Str());
    }
    out = bytes_new(g.str());
    return out.is_nil() ? R::Err : R::Ok;
}

R z_compressobj(const CallArgs &a, Value &out)
{
    R rr;
    if (redo_index(a, z_compressobj, 0, 4, out, rr))
        return rr;
    static const Str NAMES[] = { "level", "method", "wbits", "memLevel", "strategy", "zdict" };
    Value v[6];
    if (!fn_take(a, "compressobj", NAMES, 0, v))
        return R::Err;
    i64 level = -1, method = 8, wbits = MAX_WBITS, memlevel = DEF_MEM_LEVEL, strategy = 0;
    if ((!v[0].is_nil() && !int_arg(v[0], "compressobj", "level", level)) ||
        (!v[1].is_nil() && !int_arg(v[1], "compressobj", "method", method)) ||
        (!v[2].is_nil() && !int_arg(v[2], "compressobj", "wbits", wbits)) ||
        (!v[3].is_nil() && !int_arg(v[3], "compressobj", "memLevel", memlevel)) ||
        (!v[4].is_nil() && !int_arg(v[4], "compressobj", "strategy", strategy)))
        return R::Err;
    Str zdict;
    if (!v[5].is_nil() && !bytes_arg(v[5], "compressobj", "zdict", zdict))
        return R::Err;
    ZFormat fmt = ZFormat::Zlib;
    u8 bits     = MAX_WBITS;
    if (!wbits_of(wbits, false, fmt, bits))
        return R::Err;
    if (method != 8 || level < -1 || level > 9 || memlevel < 1 || memlevel > 9 || strategy < 0 ||
        strategy > 4)
        return err_set("ValueError", "Invalid initialization option");
    out = comp_new(int(level), fmt, bits, int(memlevel), ZStrategy(u8(strategy)), zdict);
    return out.is_nil() ? R::Err : R::Ok;
}

R z_decompressobj(const CallArgs &a, Value &out)
{
    R rr;
    if (redo_index(a, z_decompressobj, 0, 0, out, rr))
        return rr;
    static const Str NAMES[] = { "wbits", "zdict" };
    Value v[2];
    if (!fn_take(a, "decompressobj", NAMES, 0, v))
        return R::Err;
    i64 wbits = MAX_WBITS;
    if (!v[0].is_nil() && !int_arg(v[0], "decompressobj", "wbits", wbits))
        return R::Err;
    Root rz{ v[1] };
    if (!rz.v.is_nil()) {
        Str z;
        if (!bytes_arg(rz.v, "decompressobj", "zdict", z))
            return R::Err;
        if (z.empty())
            rz = Value();
    }
    ZFormat fmt = ZFormat::Zlib;
    u8 bits     = MAX_WBITS;
    if (!wbits_of(wbits, true, fmt, bits))
        return R::Err;
    out = decomp_new(fmt, bits, int(wbits), rz.v);
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr ModDef DEFS[] = {
    { "adler32", z_adler32 },
    { "crc32", z_crc32 },
    { "adler32_combine", z_adler32_combine },
    { "crc32_combine", z_crc32_combine },
    { "compress", z_compress },
    { "decompress", z_decompress },
    { "compressobj", z_compressobj },
    { "decompressobj", z_decompressobj },
    { "__getattr__", z_getattr },
};

INFO_TYPE(version_type, "zlib.zlib_version_info");

constexpr Str VERSION_NAMES[] = { "major", "minor", "revision", "subversion" };

Value version_info()
{
    // "1.3.2.1", as four numbers.
    Value n[4];
    usize at = 0;
    for (u32 i = 0; i < 4; i++) {
        i64 part = 0;
        while (at < ZLIB_VERSION_S.size() && ZLIB_VERSION_S[at] >= '0' && ZLIB_VERSION_S[at] <= '9')
            part = part * 10 + (ZLIB_VERSION_S[at++] - '0');
        if (at < ZLIB_VERSION_S.size() && ZLIB_VERSION_S[at] == '.')
            at++;
        n[i] = Value::of_int(i32(part));
    }
    return info_new(&version_type, n, VERSION_NAMES, 4, 4);
}

} // namespace

bool zlib_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    if (!home) {
        home = heap_new<Home>();
        if (!home)
            return false;
        gc_root_hook(home_mark);
    }
    if (home->error.is_nil()) {
        home->error = mod_exc_class("zlib", "error", "Exception");
        if (home->error.is_nil())
            return false;
    }
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    if (!mod_put(d, "error", home->error) || !mod_defs(d, DEFS))
        return false;
    if (!method_install(&comp_type, COMP_METHODS) || !mod_type(d, &comp_type) ||
        !method_install(&decomp_type, DECOMP_METHODS) || !mod_type(d, &decomp_type) ||
        !method_install(&zdec_type, ZDEC_METHODS) || !mod_type(d, &zdec_type, z_zdec_new))
        return false;
    constexpr struct {
        Str name;
        i64 v;
    } NUMBERS[] = {
        { "MAX_WBITS", MAX_WBITS },
        { "DEFLATED", 8 },
        { "DEF_MEM_LEVEL", DEF_MEM_LEVEL },
        { "DEF_BUF_SIZE", DEF_BUF_SIZE },
        { "Z_NO_COMPRESSION", 0 },
        { "Z_BEST_SPEED", 1 },
        { "Z_BEST_COMPRESSION", 9 },
        { "Z_DEFAULT_COMPRESSION", -1 },
        { "Z_DEFAULT_STRATEGY", 0 },
        { "Z_FILTERED", 1 },
        { "Z_HUFFMAN_ONLY", 2 },
        { "Z_RLE", 3 },
        { "Z_FIXED", 4 },
        { "Z_NO_FLUSH", Z_NO_FLUSH },
        { "Z_PARTIAL_FLUSH", Z_PARTIAL_FLUSH },
        { "Z_SYNC_FLUSH", Z_SYNC_FLUSH },
        { "Z_FULL_FLUSH", Z_FULL_FLUSH },
        { "Z_FINISH", Z_FINISH },
        { "Z_BLOCK", Z_BLOCK },
        { "Z_TREES", Z_TREES },
    };
    for (const auto &n : NUMBERS)
        if (!mod_int(d, n.name, n.v))
            return false;
    Root ver{ version_info() };
    if (ver.v.is_nil())
        return false;
    constexpr Str INFO_NAMES[] = { "ZLIB_VERSION_INFO", "zlib_version_info" };
    for (Str name : INFO_NAMES)
        if (!mod_put(static_cast<DictObj *>(rd.v.obj()), name, ver.v))
            return false;
    constexpr Str TEXT_NAMES[] = { "ZLIB_VERSION", "ZLIB_RUNTIME_VERSION", "zlib_version" };
    for (Str name : TEXT_NAMES) {
        Root s{ str_new(ZLIB_VERSION_S) };
        if (s.v.is_nil() || !mod_put(static_cast<DictObj *>(rd.v.obj()), name, s.v))
            return false;
    }
    return true;
}
