// TextIOWrapper and IncrementalNewlineDecoder.
//
// CPython's textio.c in behaviour, down to the tell() cookie's layout. Two
// things are this port's own. A codec written here -- UTF-8, Latin-1, ASCII,
// UTF-16 and UTF-32 -- is run directly, with its incremental state kept in the
// wrapper, so reading a file costs no call per chunk; any other codec is the
// registry's incremental decoder and encoder, called like everything else.
// And the newline layer is always native: IncrementalNewlineDecoder's logic,
// over whichever decoder there is.
//
// Everything that may call -- the buffer below, a codec written in Python, an
// error handler the program registered -- is a state in one machine, and what
// several operations share (decode, getstate, read a chunk) is a subroutine
// in it.
#include "io.h"

#include "bigint.h"
#include "builtin.h"
#include "codec.h"
#include "exc.h"
#include "gc.h"
#include "intern.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "type.h"
#include "ustr.h"
#include "vm.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

constexpr u8 SEEN_LF   = 1;
constexpr u8 SEEN_CR   = 2;
constexpr u8 SEEN_CRLF = 4;

struct TextObj : IoObj {
    Value buffer;
    Value encoding, errors;
    Value pydec;      // the registry's incremental decoder, for a codec not written here
    Value pyenc;
    Value readnl;     // the newline argument, or Nil for None
    Value decoded;    // Nil: nothing decoded (CPython's NULL)
    Value snap_input; // Nil: no snapshot
    Value raw;        // the FileIO under a plain buffer, for `closed`
    Value info;       // the CodecInfo, while it is needed
    String pending;   // encoded, not yet handed to the buffer
    String dpend;     // bytes the native decoder holds back
    Codec codec;      // Codec::None for the registry's
    char writenl[3];  // empty: no translation
    u32 used, used_b; // characters of `decoded` handed out, and their bytes
    i64 snap_flags;
    i64 chunk_size;
    f64 b2cratio;
    i32 dorder;       // UTF-16/32 while reading: 0 not known yet, -1 little, 1 big
    bool enc_started; // UTF-16/32 while writing: past the byte order mark
    bool pendingcr;
    u8 seennl;
    bool has_dec, has_enc;
    bool readuniversal, readtranslate, writetranslate;
    bool line_buffering, write_through;
    bool telling, seekable, has_read1;
    bool ok, detached;
    bool isstd; // one of the three: every write reaches the buffer at once
};

TextObj *text_of(Value v)
{
    return static_cast<TextObj *>(v.obj());
}

void text_trace(Obj *o)
{
    io_trace(o);
    TextObj *t = static_cast<TextObj *>(o);
    gc_mark(t->buffer);
    gc_mark(t->encoding);
    gc_mark(t->errors);
    gc_mark(t->pydec);
    gc_mark(t->pyenc);
    gc_mark(t->readnl);
    gc_mark(t->decoded);
    gc_mark(t->snap_input);
    gc_mark(t->raw);
    gc_mark(t->info);
}

void text_fini(Obj *o)
{
    io_untrack(o);
    TextObj *t = static_cast<TextObj *>(o);
    t->pending.~String();
    t->dpend.~String();
}

Str writenl_of(const TextObj *t)
{
    usize n = 0;
    while (n < 2 && t->writenl[n])
        n++;
    return Str(t->writenl, n);
}

bool bom_codec(Codec c)
{
    return c == Codec::Utf16 || c == Codec::Utf32;
}

bool native_codec(Codec c)
{
    switch (c) {
    case Codec::Utf8:
    case Codec::Latin1:
    case Codec::Ascii:
    case Codec::Utf16:
    case Codec::Utf16Le:
    case Codec::Utf16Be:
    case Codec::Utf32:
    case Codec::Utf32Le:
    case Codec::Utf32Be:
        return true;
    default:
        return false;
    }
}

R not_ready(const TextObj *t)
{
    return err_set("ValueError", t->detached ? Str("underlying buffer has been detached")
                                             : Str("I/O operation on uninitialized object"));
}

TextObj *self_text(const CallArgs &a, Str who, bool ready = true)
{
    IoObj *io = io_self(a, IO_TEXT, who);
    if (!io)
        return nullptr;
    TextObj *t = static_cast<TextObj *>(io);
    if (ready && !t->ok) {
        not_ready(t);
        return nullptr;
    }
    return t;
}

// Closed, when that needs no call: 1, 0, or -1 to ask the buffer.
int closed_now(const TextObj *t)
{
    if (!t->raw.is_nil())
        return file_facts(t->raw).closed ? 1 : 0;
    IoObj *io = nullptr;
    if (io_of(t->buffer, io) && !is_inst(t->buffer) && io->kind != IO_BUFFERED &&
        io->kind != IO_PAIR && io->kind != IO_TEXT)
        return io->closed ? 1 : 0;
    return -1;
}

// --------------------------------------------------------------- strings

// The byte offset `n` characters past `from`, or the end.
usize skip_chars(Str s, usize from, i64 n)
{
    usize i = from;
    while (i < s.size() && n > 0) {
        i += cp_width(u8(s[i]));
        n--;
    }
    return i < s.size() ? i : s.size();
}

usize chars_between(Str s, usize from, usize to)
{
    usize n = 0;
    for (usize i = from; i < to; i++)
        n += (u8(s[i]) & 0xc0) != 0x80;
    return n;
}

Value str_ok(Str s)
{
    StrObj *o = str_raw(s);
    return o ? obj_value(o) : (oom(), Value());
}

u32 chars_of(Value s)
{
    return s.is_nil() ? 0 : str_of(s)->chars;
}

Str decoded_str(const TextObj *t)
{
    return t->decoded.is_nil() ? Str() : str_of(t->decoded)->str();
}

void set_decoded(TextObj *t, Value s)
{
    t->decoded = s;
    t->used    = 0;
    t->used_b  = 0;
}

// Up to `n` characters of what is decoded, or all of it for n < 0.
Value get_decoded(Value tv, i64 n)
{
    TextObj *t = text_of(tv);
    Str all    = decoded_str(t);
    usize end  = n < 0 ? all.size() : skip_chars(all, t->used_b, n);
    Value v    = str_ok(all.substr(t->used_b, end - t->used_b));
    if (v.is_nil())
        return v;
    t = text_of(tv);
    t->used += chars_of(v);
    t->used_b = u32(end);
    return v;
}

void set_used(TextObj *t, u32 chars)
{
    t->used   = chars;
    t->used_b = u32(skip_chars(decoded_str(t), 0, chars));
}

Value join_strs(ListObj *l)
{
    String out;
    for (Value v : l->items)
        if (!out.append(str_of(v)->str()))
            return oom(), Value();
    return str_ok(out.str());
}

// ------------------------------------------------------ the newline layer

// CPython's IncrementalNewlineDecoder.decode, over text already decoded.
Value newline_apply(Str in, bool final, bool translate, bool &pendingcr, u8 &seennl)
{
    String s;
    if (pendingcr && (final || !in.empty())) {
        if (!s.push('\r'))
            return oom(), Value();
        pendingcr = false;
    }
    if (!s.append(in))
        return oom(), Value();
    if (!final && s.size() && s[s.size() - 1] == '\r') {
        s.pop();
        pendingcr = true;
    }
    Str t    = s.str();
    bool any = false;
    for (usize i = 0; i < t.size(); i++) {
        char c = t[i];
        if (c == '\n') {
            seennl |= SEEN_LF;
        } else if (c == '\r') {
            any = true;
            if (i + 1 < t.size() && t[i + 1] == '\n') {
                seennl |= SEEN_CRLF;
                i++;
            } else {
                seennl |= SEEN_CR;
            }
        }
    }
    if (!translate || !any)
        return str_ok(t);
    String o;
    if (!o.reserve(t.size()))
        return oom(), Value();
    for (usize i = 0; i < t.size(); i++) {
        if (t[i] == '\r') {
            o.push('\n');
            if (i + 1 < t.size() && t[i + 1] == '\n')
                i++;
        } else {
            o.push(t[i]);
        }
    }
    return str_ok(o.str());
}

// (None, "\n", "\r", ("\r", "\n"), "\r\n", ("\n", "\r\n"), ("\r", "\r\n"),
// ("\r", "\n", "\r\n"))[seen]
Value newlines_value(u8 seen)
{
    if (seen == 0)
        return value_none();
    if (seen == SEEN_LF)
        return str_new("\n");
    if (seen == SEEN_CR)
        return str_new("\r");
    if (seen == SEEN_CRLF)
        return str_new("\r\n");
    Str parts[3];
    usize n = 0;
    if (seen & SEEN_CR)
        parts[n++] = "\r";
    if (seen & SEEN_LF)
        parts[n++] = "\n";
    if (seen & SEEN_CRLF)
        parts[n++] = "\r\n";
    TupleObj *tp = tuple_new(n);
    if (!tp)
        return oom(), Value();
    Root rt{ obj_value(tp) };
    for (usize i = 0; i < n; i++) {
        Value v = str_new(parts[i]);
        if (v.is_nil())
            return Value();
        static_cast<TupleObj *>(rt.v.obj())->items()[i] = v;
    }
    return rt.v;
}

// Where a line ends in `s` from byte `from`: true with `end` the byte after
// it, or false with `consumed` the bytes that can be set aside. CPython's
// _PyIO_find_line_ending, over UTF-8: every line ending is ASCII.
bool find_line_ending(const TextObj *t, Str s, usize from, usize &end, usize &consumed)
{
    if (t->readtranslate) {
        for (usize i = from; i < s.size(); i++)
            if (s[i] == '\n') {
                end = i + 1;
                return true;
            }
        consumed = s.size();
        return false;
    }
    if (t->readuniversal) {
        for (usize i = from; i < s.size(); i++) {
            if (s[i] == '\n') {
                end = i + 1;
                return true;
            }
            if (s[i] == '\r') {
                end = i + 1 < s.size() && s[i + 1] == '\n' ? i + 2 : i + 1;
                return true;
            }
        }
        consumed = s.size();
        return false;
    }
    Str nl = str_of(t->readnl)->str();
    if (nl.size() == 1) {
        for (usize i = from; i < s.size(); i++)
            if (s[i] == nl[0]) {
                end = i + 1;
                return true;
            }
        consumed = s.size();
        return false;
    }
    usize e = s.size() >= nl.size() - 1 ? s.size() - (nl.size() - 1) : 0;
    if (e < from)
        e = from;
    for (usize i = from; i < e; i++)
        if (s[i] == nl[0] && s.substr(i, nl.size()) == nl) {
            end = i + nl.size();
            return true;
        }
    consumed = s.size();
    for (usize i = e; i < s.size(); i++)
        if (s[i] == nl[0]) {
            consumed = i;
            break;
        }
    return false;
}

// ------------------------------------------------------- the native codec

// Decode `data` (a bytes object): text and how much was taken, as a tuple,
// or a ContObj when an error handler of the program's own runs first.
R native_decode(TextObj *t, Value data, bool final, Value &out)
{
    CodecCall c;
    c.codec = t->codec;
    if (bom_codec(t->codec) && t->dorder != 0)
        c.codec = t->codec == Codec::Utf16 ? (t->dorder < 0 ? Codec::Utf16Le : Codec::Utf16Be)
                                           : (t->dorder < 0 ? Codec::Utf32Le : Codec::Utf32Be);
    c.final  = final;
    c.wrap   = WRAP_BYTEORDER;
    c.input  = data;
    c.errors = t->errors;
    return codec_run(c, out);
}

R bom_error(Codec codec, Value data)
{
    usize n       = codec == Codec::Utf16 ? 2 : 4;
    Value args[5] = { str_new(codec == Codec::Utf16 ? Str("utf-16") : Str("utf-32")), data,
                      Value::of_int(0), Value::of_int(i32(n)),
                      str_new("Stream does not start with BOM") };
    Roots pin{ args, 5 };
    for (Value v : args)
        if (v.is_nil())
            return R::Err;
    TupleObj *a = tuple_new(5);
    if (!a)
        return oom();
    for (u32 i = 0; i < 5; i++)
        a->items()[i] = args[i];
    Root ra{ obj_value(a) };
    Value e = exc_construct(exc_type_value(exc_find("UnicodeDecodeError")), ra.v);
    return e.is_nil() ? R::Err : err_set_value(e);
}

// The answer native_decode's run gave, taken in; `data` is what it ran on.
R native_take(TextObj *t, Value data, Value got, Value &text)
{
    TupleObj *tp = static_cast<TupleObj *>(got.obj());
    i64 used     = 0;
    as_int_arg(tp->items()[1], used);
    text = tp->items()[0];
    if (bom_codec(t->codec) && t->dorder == 0) {
        i64 order = 0;
        as_int_arg(tp->items()[2], order);
        usize bom = t->codec == Codec::Utf16 ? 2 : 4;
        if (order != 0)
            t->dorder = i32(order);
        else if (usize(used) >= bom)
            return bom_error(t->codec, data);
    }
    Str all = static_cast<BytesObj *>(data.obj())->str();
    if (!t->dpend.assign(all.substr(usize(used))))
        return oom();
    return R::Ok;
}

i64 native_flags(const TextObj *t)
{
    if (!bom_codec(t->codec))
        return 0;
    return t->dorder == 0 ? 2 : t->dorder < 0 ? 0 : 1;
}

// Encoded bytes, or a ContObj.
R native_encode(TextObj *t, Value s, Value &out)
{
    Str text   = str_of(s)->str();
    bool ascii = (s.obj()->flags & OBJ_ASCII) != 0;
    if ((t->codec == Codec::Utf8 && !has_surrogate(text)) ||
        ((t->codec == Codec::Latin1 || t->codec == Codec::Ascii) && ascii)) {
        out = bytes_new(text);
        return out.is_nil() ? R::Err : R::Ok;
    }
    CodecCall c;
    c.codec = t->codec;
    if (bom_codec(t->codec)) {
        if (t->enc_started)
            c.codec = t->codec == Codec::Utf16 ? Codec::Utf16Le : Codec::Utf32Le;
        if (!text.empty())
            t->enc_started = true;
    }
    c.encode = true;
    c.input  = s;
    c.errors = t->errors;
    return codec_run(c, out);
}

// Text as it is written: "\n" become the newline the stream writes.
Value translate_out(const TextObj *t, Value s, bool &haslf)
{
    Str text = str_of(s)->str();
    haslf    = false;
    if (t->writetranslate || t->line_buffering)
        for (usize i = 0; i < text.size() && !haslf; i++)
            haslf = text[i] == '\n';
    Str nl = writenl_of(t);
    if (!(haslf && t->writetranslate && !nl.empty()))
        return s;
    String o;
    for (usize i = 0; i < text.size(); i++)
        if (!(text[i] == '\n' ? o.append(nl) : o.push(text[i])))
            return oom(), Value();
    return str_ok(o.str());
}

bool has_cr(Value s)
{
    Str text = str_of(s)->str();
    for (usize i = 0; i < text.size(); i++)
        if (text[i] == '\r')
            return true;
    return false;
}

// ------------------------------------------------------------ the cookie

// start_pos | dec_flags << 64 | bytes_to_feed << 96 | chars_to_skip << 128 |
// need_eof << 160, as CPython packs it.
struct Cookie {
    i64 start_pos     = 0;
    i64 dec_flags     = 0;
    i64 bytes_to_feed = 0;
    i64 chars_to_skip = 0;
    bool need_eof     = false;
};

Value cookie_build(const Cookie &c)
{
    if (!c.dec_flags && !c.bytes_to_feed && !c.chars_to_skip && !c.need_eof)
        return int_from_i64(c.start_pos);
    char raw[21] = {};
    u64 p        = u64(c.start_pos);
    for (u32 i = 0; i < 8; i++)
        raw[i] = char(p >> (8 * i));
    u32 fields[3] = { u32(c.dec_flags), u32(c.bytes_to_feed), u32(c.chars_to_skip) };
    for (u32 f = 0; f < 3; f++)
        for (u32 i = 0; i < 4; i++)
            raw[8 + 4 * f + i] = char(fields[f] >> (8 * i));
    raw[20] = c.need_eof;
    return int_from_octets(Str(raw, sizeof raw), true, false);
}

bool cookie_parse(Value v, Cookie &c)
{
    String raw;
    if (int_to_octets(v, 21, true, false, raw) != R::Ok)
        return false;
    auto byte = [&](u32 i) { return u8(raw[i]); };
    u64 p     = 0;
    for (u32 i = 0; i < 8; i++)
        p |= u64(byte(i)) << (8 * i);
    c.start_pos   = i64(p);
    u32 fields[3] = {};
    for (u32 f = 0; f < 3; f++)
        for (u32 i = 0; i < 4; i++)
            fields[f] |= u32(byte(8 + 4 * f + i)) << (8 * i);
    c.dec_flags     = i32(fields[0]);
    c.bytes_to_feed = i32(fields[1]);
    c.chars_to_skip = i32(fields[2]);
    c.need_eof      = byte(20) != 0;
    return true;
}

// ------------------------------------------------------------ the machine

// The numbers a job keeps, in a bytearray: the first of s[7]'s items.
struct Job {
    i64 size; // read's n, readline's limit, seek's whence
    i64 remaining;
    i64 chunked;
    i64 start, endpos, offset;
    i64 rc_ret, rc_hint; // read_chunk's return state and size hint
    i64 dflags;          // what getstate said, packed with the newline bit
    i64 set_flags;       // what setstate is to be told
    i64 snap_flags;      // read_chunk's, while it reads
    i64 len;             // write's answer
    Cookie ck;
    i64 skip_bytes, skip_back, chars_decoded, at, chars_to_skip;
    bool final, eof, res, set_reset, enc_reset, needflush, reconf;
};

// s[7] is a list: the job, then Values the machine needs besides s[].
enum : u32 { X_JOB, X_SETBUF, X_SNAPBUF, X_CHUNK, X_ERROR, X_SAVED, X_NEXTIN, X_COUNT };

Job *job_of(ContObj *k)
{
    Value blob = list_of(k->s[7])->items[X_JOB];
    return reinterpret_cast<Job *>(array_of(blob)->data.data());
}

Value &extra(ContObj *k, u32 at)
{
    return list_of(k->s[7])->items[at];
}

enum Op : u32 {
    T_INIT,
    T_WRITE,
    T_READ,
    T_READLINE,
    T_NEXT,
    T_FLUSH,
    T_CLOSE,
    T_DETACH,
    T_TELL,
    T_SEEK,
    T_TRUNCATE,
    T_RECONF,
    T_FORWARD,
    T_RESET,
};

enum : u32 {
    // Leaf subroutines, which go back to x[3].
    U_DECODE = 1, // s[4] bytes, job.final -> s[5] str
    U_DECODE_PY,
    U_DECODE_NAT,
    U_GETSTATE, // -> s[6] bytes, job.dflags
    U_GETSTATE_PY,
    U_SETSTATE, // X_SETBUF (Nil for b"") and job.set_flags, or job.set_reset
    U_SETSTATE_PY,
    U_ENCODE, // s[4] str -> pending
    U_ENCODE_2,
    U_ENCSTATE, // job.enc_reset: reset(), else setstate(0)
    U_ENCSTATE_PY,
    U_WRITEFLUSH, // pending -> buffer.write
    U_WRITEFLUSH_2,
    // read_chunk goes back to job.rc_ret, and calls the leaves.
    RC_START = 20,
    RC_READ,
    RC_READ_2,
    RC_DECODED,
    CL_ASK, // the op's closed check, answered

    O_START = 40, // the ops' own states from here
};

constexpr u32 O(u32 n)
{
    return O_START + n;
}

enum class Go : u8 { Again, Stop };

#define STOP(x)                                                                                    \
    do {                                                                                           \
        res_ = (x);                                                                                \
        return Go::Stop;                                                                           \
    } while (0)

Go op_state(ContObj *k, Value &in, R &res_);
R text_step(ContObj *k, Value in);
struct TextObj;
void text_set_newline(TextObj *t, Value newline);
R text_set_codec(Value tv, Value encoding, Value errors);

// Go to `state`, and come back to `back`.
void leaf(ContObj *k, u32 state, u32 back)
{
    k->x[3] = back;
    k->i    = state;
}

void read_chunk(ContObj *k, i64 hint, u32 back)
{
    Job *j     = job_of(k);
    j->rc_ret  = back;
    j->rc_hint = hint;
    k->i       = RC_START;
}

// setstate as tell() and seek() use it: reset() at the very start.
void setstate_cookie(ContObj *k, const Cookie &c, u32 back)
{
    Job *j          = job_of(k);
    j->set_reset    = c.start_pos == 0 && c.dec_flags == 0;
    j->set_flags    = c.dec_flags;
    extra(k, X_SETBUF) = Value();
    leaf(k, U_SETSTATE, back);
}

Str read_name(const TextObj *t)
{
    return t->has_read1 ? Str("read1") : Str("read");
}

Go leaf_state(ContObj *k, Value &in, R &res_)
{
    {
        TextObj *t = text_of(k->s[1]);
        Job *j     = job_of(k);
        switch (k->i) {
        // ------------------------------------------------------- decode
        case U_DECODE:
            if (t->codec == Codec::None) {
                k->i = U_DECODE_PY;
                STOP(cont_method(k, t->pydec, "decode", 2, k->s[4], value_bool(j->final)));
            }
            {
                Str data = static_cast<BytesObj *>(k->s[4].obj())->str();
                Value joined = k->s[4];
                if (!t->dpend.empty()) {
                    String all;
                    if (!all.append(t->dpend.str()) || !all.append(data))
                        STOP(oom());
                    joined = bytes_new(all.str());
                    if (joined.is_nil())
                        STOP(R::Err);
                }
                k->s[5] = joined;
                Value got;
                if (native_decode(text_of(k->s[1]), joined, job_of(k)->final, got) != R::Ok)
                    STOP(R::Err);
                k->i = U_DECODE_NAT;
                if (is_cont(got))
                    STOP(cont_await(k, got));
                in = got;
                return Go::Again;
            }
        case U_DECODE_NAT: {
            Root text;
            if (native_take(t, k->s[5], in, text.v) != R::Ok)
                STOP(R::Err);
            in = text.v;
            [[fallthrough]];
        }
        case U_DECODE_PY: {
            if (!is_str(in)) {
                Buf<128> m;
                m.put("decoder should return a string result, not '").put(type_name(in)).put("'");
                STOP(err_set("TypeError", m.str()));
            }
            Value out = in;
            if (t->readuniversal) {
                out = newline_apply(str_of(in)->str(), j->final, t->readtranslate, t->pendingcr,
                                    t->seennl);
                if (out.is_nil())
                    STOP(R::Err);
            }
            k->s[5] = out;
            k->i    = u32(k->x[3]);
            return Go::Again;
        }

        // ----------------------------------------------------- getstate
        case U_GETSTATE:
            if (t->codec == Codec::None) {
                k->i = U_GETSTATE_PY;
                STOP(cont_method(k, t->pydec, "getstate"));
            }
            {
                Value b = bytes_new(t->dpend.str());
                if (b.is_nil())
                    STOP(R::Err);
                k->s[6]             = b;
                job_of(k)->dflags   = native_flags(text_of(k->s[1]));
            }
            goto packed;
        case U_GETSTATE_PY: {
            TupleObj *tp = is_tuple(in) ? static_cast<TupleObj *>(in.obj()) : nullptr;
            if (!tp || tp->len != 2)
                STOP(err_set("TypeError", "illegal decoder state"));
            if (!is_bytes(tp->items()[0])) {
                Buf<128> m;
                m.put("illegal decoder state: the first item must be a bytes object, not '");
                m.put(type_name(tp->items()[0])).put("'");
                STOP(err_set("TypeError", m.str()));
            }
            i64 f = 0;
            if (!as_int_arg(tp->items()[1], f))
                STOP(err_set("TypeError", "illegal decoder state"));
            k->s[6]   = tp->items()[0];
            j->dflags = f;
        }
        packed:
            t = text_of(k->s[1]);
            j = job_of(k);
            if (t->readuniversal)
                j->dflags = (j->dflags << 1) | (t->pendingcr ? 1 : 0);
            k->i = u32(k->x[3]);
            return Go::Again;

        // ----------------------------------------------------- setstate
        case U_SETSTATE: {
            i64 f = j->set_flags;
            if (t->readuniversal) {
                if (j->set_reset) {
                    t->seennl    = 0;
                    t->pendingcr = false;
                } else {
                    t->pendingcr = (f & 1) != 0;
                    f >>= 1;
                }
            }
            Value buf = extra(k, X_SETBUF);
            if (t->codec != Codec::None) {
                if (j->set_reset) {
                    t->dpend.clear();
                    t->dorder = 0;
                } else {
                    Str b = buf.is_nil() ? Str() : static_cast<BytesObj *>(buf.obj())->str();
                    if (!t->dpend.assign(b))
                        STOP(oom());
                    if (bom_codec(t->codec))
                        t->dorder = f == 2 ? 0 : f == 0 ? -1 : 1;
                }
                k->i = u32(k->x[3]);
                return Go::Again;
            }
            k->i = U_SETSTATE_PY;
            if (j->set_reset)
                STOP(cont_method(k, t->pydec, "reset"));
            Root rb{ buf.is_nil() ? bytes_new(Str()) : buf };
            Root rn{ int_from_i64(f) };
            TupleObj *tp = rb.v.is_nil() || rn.v.is_nil() ? nullptr : tuple_new(2);
            if (!tp)
                STOP(err_pending() ? R::Err : oom());
            tp->items()[0] = rb.v;
            tp->items()[1] = rn.v;
            Root rt{ obj_value(tp) };
            STOP(cont_method(k, text_of(k->s[1])->pydec, "setstate", 1, rt.v));
        }
        case U_SETSTATE_PY:
            k->i = u32(k->x[3]);
            return Go::Again;

        // ------------------------------------------------------- encode
        case U_ENCODE:
            k->i = U_ENCODE_2;
            if (t->codec != Codec::None) {
                Value got;
                if (native_encode(t, k->s[4], got) != R::Ok)
                    STOP(R::Err);
                if (is_cont(got))
                    STOP(cont_await(k, got));
                in = got;
                return Go::Again;
            }
            STOP(cont_method(k, t->pyenc, "encode", 1, k->s[4]));
        case U_ENCODE_2: {
            Str b;
            if (!bytes_like(in, b)) {
                Buf<128> m;
                m.put("encoder should return a bytes object, not '").put(type_name(in)).put("'");
                STOP(err_set("TypeError", m.str()));
            }
            if (!t->pending.append(b))
                STOP(oom());
            k->i = u32(k->x[3]);
            return Go::Again;
        }
        case U_ENCSTATE:
            if (!t->has_enc) {
                k->i = u32(k->x[3]);
                return Go::Again;
            }
            if (t->codec != Codec::None) {
                t->enc_started = !j->enc_reset;
                k->i           = u32(k->x[3]);
                return Go::Again;
            }
            k->i = U_ENCSTATE_PY;
            if (j->enc_reset)
                STOP(cont_method(k, t->pyenc, "reset"));
            STOP(cont_method(k, t->pyenc, "setstate", 1, Value::of_int(0)));
        case U_ENCSTATE_PY:
            k->i = u32(k->x[3]);
            return Go::Again;

        // --------------------------------------------------- writeflush
        case U_WRITEFLUSH: {
            if (t->pending.empty()) {
                k->i = u32(k->x[3]);
                return Go::Again;
            }
            Value b = bytes_new(t->pending.str());
            if (b.is_nil())
                STOP(R::Err);
            text_of(k->s[1])->pending.clear();
            k->i = U_WRITEFLUSH_2;
            STOP(cont_method(k, text_of(k->s[1])->buffer, "write", 1, b));
        }
        case U_WRITEFLUSH_2:
            k->i = u32(k->x[3]);
            return Go::Again;

        // --------------------------------------------------- read_chunk
        case RC_START:
            if (!t->has_dec)
                STOP(io_unsupported("not readable"));
            extra(k, X_SNAPBUF) = Value();
            if (t->telling) {
                leaf(k, U_GETSTATE, RC_READ);
                return Go::Again;
            }
            k->i = RC_READ;
            return Go::Again;
        case RC_READ: {
            if (t->telling) {
                extra(k, X_SNAPBUF) = k->s[6];
                j->snap_flags       = j->dflags;
            }
            i64 hint = j->rc_hint;
            if (hint > 0) {
                f64 r = t->b2cratio > 1.0 ? t->b2cratio : 1.0;
                hint  = i64(r * f64(hint));
            }
            i64 size = hint > t->chunk_size ? hint : t->chunk_size;
            Value n  = int_from_i64(size);
            if (n.is_nil())
                STOP(R::Err);
            k->i = RC_READ_2;
            STOP(cont_method(k, t->buffer, read_name(t), 1, n));
        }
        case RC_READ_2: {
            Str chunk;
            if (!bytes_like(in, chunk)) {
                Buf<160> m;
                m.put("underlying ").put(read_name(t));
                m.put("() should have returned a bytes-like object, not '").put(type_name(in));
                m.put("'");
                STOP(err_set("TypeError", m.str()));
            }
            Value b = is_bytes(in) ? in : bytes_new(chunk);
            if (b.is_nil())
                STOP(R::Err);
            extra(k, X_CHUNK) = b;
            k->s[4]           = b;
            j                 = job_of(k);
            j->eof            = chunk.empty();
            j->final          = j->eof;
            leaf(k, U_DECODE, RC_DECODED);
            return Go::Again;
        }
        case RC_DECODED: {
            set_decoded(t, k->s[5]);
            u32 nchars   = chars_of(k->s[5]);
            Value chunkv = extra(k, X_CHUNK);
            Str chunk    = static_cast<BytesObj *>(chunkv.obj())->str();
            t->b2cratio  = nchars ? f64(chunk.size()) / f64(nchars) : 0.0;
            if (nchars)
                j->eof = false;
            if (t->telling) {
                // At the snapshot point, what comes next is what the decoder
                // held, then this chunk.
                Value held = extra(k, X_SNAPBUF);
                String next;
                if (!held.is_nil() && !next.append(static_cast<BytesObj *>(held.obj())->str()))
                    STOP(oom());
                if (!next.append(chunk))
                    STOP(oom());
                Value sn = bytes_new(next.str());
                if (sn.is_nil())
                    STOP(R::Err);
                t             = text_of(k->s[1]);
                j             = job_of(k);
                t->snap_input = sn;
                t->snap_flags = j->snap_flags;
            }
            j->res = !j->eof;
            k->i   = u32(j->rc_ret);
            return Go::Again;
        }

        case CL_ASK:
            if (py_truth(in))
                STOP(io_closed_err());
            k->i = O(1);
            return Go::Again;

        default:
            break;
        }
    }
    STOP(err_set("SystemError", "textio: a state with no handler"));
}

R finish_error(ContObj *k)
{
    Root e{ extra(k, X_ERROR) };
    return err_set_value(e.v);
}

Go op_state(ContObj *k, Value &in, R &res_)
{
    {
        TextObj *t = text_of(k->s[1]);
        Job *j     = job_of(k);
        u32 op     = k->j;
        u32 st     = k->i;

        if (st == 0) {
            bool check = op == T_WRITE || op == T_READ || op == T_READLINE || op == T_NEXT ||
                         op == T_FLUSH || op == T_TELL || op == T_SEEK;
            k->i = O(1);
            if (check) {
                int c = closed_now(t);
                if (c == 1)
                    STOP(io_closed_err());
                if (c < 0) {
                    k->i = CL_ASK;
                    STOP(cont_attr(k, t->buffer, "closed"));
                }
            }
            return Go::Again;
        }

        switch (op) {
        // ---------------------------------------------------------- init
        case T_INIT:
            switch (st) {
            case O(1):
                if (t->codec != Codec::None) {
                    k->i = O(3);
                    break;
                }
                {
                    Value mod = builtin_module("_codecs");
                    if (mod.is_nil())
                        STOP(err_pending() ? R::Err : err_set("ImportError", "_codecs"));
                    k->i = O(2);
                    STOP(cont_method(k, mod, "lookup", 1, t->encoding));
                }
            case O(2): {
                Root info{ in };
                StrObj *n = str_intern("_is_text_encoding");
                Value is;
                Got g = n ? py_attr(info.v, n, is) : Got::Error;
                if (g == Got::Error)
                    STOP(R::Err);
                if (g == Got::Ok && !py_truth(is)) {
                    Buf<160> m;
                    Value e = text_of(k->s[1])->encoding;
                    m.put("'").put(is_str(e) ? str_of(e)->str() : Str()).put("' is not a text encoding");
                    STOP(err_set("LookupError", m.str()));
                }
                text_of(k->s[1])->info = info.v;
                k->i                   = O(3);
                break;
            }
            case O(3):
                k->i = O(4);
                STOP(cont_method(k, t->buffer, "readable"));
            case O(4):
                t->has_dec = is_true(in);
                if (t->has_dec && t->codec == Codec::None) {
                    k->i = O(5);
                    StrObj *n = str_intern("incrementaldecoder");
                    Value mk;
                    if (!n || py_getattr(t->info, n, mk) != R::Ok)
                        STOP(R::Err);
                    STOP(cont_call(k, mk, t->errors));
                }
                k->i = O(6);
                break;
            case O(5):
                t->pydec = in;
                k->i     = O(6);
                break;
            case O(6):
                k->i = O(7);
                STOP(cont_method(k, t->buffer, "writable"));
            case O(7):
                t->has_enc = is_true(in);
                if (t->has_enc && t->codec == Codec::None) {
                    k->i = O(8);
                    StrObj *n = str_intern("incrementalencoder");
                    Value mk;
                    if (!n || py_getattr(t->info, n, mk) != R::Ok)
                        STOP(R::Err);
                    STOP(cont_call(k, mk, t->errors));
                }
                k->i = O(9);
                break;
            case O(8):
                t->pyenc = in;
                k->i     = O(9);
                break;
            case O(9):
                t->info = Value();
                k->i    = O(10);
                STOP(cont_method(k, t->buffer, "seekable"));
            case O(10): {
                t->seekable = t->telling = py_truth(in);
                StrObj *n                = str_intern("read1");
                Value m;
                Got g = n ? py_attr(t->buffer, n, m) : Got::Error;
                if (g == Got::Error)
                    err_clear();
                t->has_read1 = g == Got::Ok || g == Got::Call;
                t->raw       = Value();
                if (io_is_plain(t->buffer, IO_BUFFERED)) {
                    StrObj *rn = str_intern("raw");
                    Value r;
                    if (rn && py_attr(t->buffer, rn, r) == Got::Ok && io_is_plain(r, IO_FILE))
                        t->raw = r;
                    err_clear();
                }
                if (t->seekable && t->has_enc) {
                    k->i = O(11);
                    STOP(cont_method(k, t->buffer, "tell"));
                }
                k->i = O(12);
                break;
            }
            case O(11): {
                i64 at = -1;
                as_int_arg(in, at);
                k->i = O(12);
                if (at != 0) {
                    j->enc_reset = false;
                    leaf(k, U_ENCSTATE, O(12));
                }
                break;
            }
            default:
                t->ok = true;
                io_track(t);
                STOP(cont_done(k, j->reconf ? value_none() : k->s[0]));
            }
            return Go::Again;

        // --------------------------------------------------------- write
        case T_WRITE:
            switch (st) {
            case O(1):
                if (!t->has_enc)
                    STOP(io_unsupported("not writable"));
                k->s[4] = k->s[2];
                leaf(k, U_ENCODE, O(2));
                break;
            case O(2):
                k->i = O(3);
                if (t->pending.size() >= usize(t->chunk_size) || j->needflush ||
                    t->write_through || t->isstd)
                    leaf(k, U_WRITEFLUSH, O(3));
                break;
            case O(3):
                k->i = O(4);
                if (j->needflush && !t->isstd)
                    STOP(cont_method(k, t->buffer, "flush"));
                break;
            case O(4):
                set_decoded(t, Value());
                t->snap_input = Value();
                k->i          = O(5);
                if (t->has_dec) {
                    j->set_reset = true;
                    leaf(k, U_SETSTATE, O(5));
                }
                break;
            default:
                STOP(cont_done(k, int_from_i64(j->len)));
            }
            return Go::Again;

        // ---------------------------------------------------------- read
        case T_READ:
            switch (st) {
            case O(1):
                if (!t->has_dec)
                    STOP(io_unsupported("not readable"));
                leaf(k, U_WRITEFLUSH, O(2));
                break;
            case O(2):
                if (j->size < 0) {
                    k->i = O(3);
                    STOP(cont_method(k, t->buffer, "read"));
                }
                {
                    Root rf{ get_decoded(k->s[1], j->size) };
                    ListObj *l = rf.v.is_nil() ? nullptr : list_new();
                    if (!l)
                        STOP(err_pending() ? R::Err : oom());
                    k->s[3] = obj_value(l);
                    if (!list_push(l, rf.v))
                        STOP(oom());
                    j            = job_of(k);
                    j->remaining = j->size - chars_of(rf.v);
                    k->i         = O(5);
                }
                break;
            case O(3):
                if (is_none(in))
                    STOP(err_set_value(exc_make("BlockingIOError", "Read returned None.")));
                if (!is_bytes(in)) {
                    Str b;
                    if (!bytes_like(in, b))
                        STOP(err_set2("TypeError", "read() should return bytes",
                                      type_name(in)));
                    in = bytes_new(b);
                    if (in.is_nil())
                        STOP(R::Err);
                }
                k->s[4]  = in;
                j->final = true;
                leaf(k, U_DECODE, O(4));
                break;
            case O(4): {
                Root head{ get_decoded(k->s[1], -1) };
                if (head.v.is_nil())
                    STOP(R::Err);
                String all;
                if (!all.append(str_of(head.v)->str()) || !all.append(str_of(k->s[5])->str()))
                    STOP(oom());
                t = text_of(k->s[1]);
                set_decoded(t, Value());
                t->snap_input = Value();
                Value r       = str_ok(all.str());
                STOP(r.is_nil() ? R::Err : cont_done(k, r));
            }
            case O(5):
                if (j->remaining <= 0) {
                    Value r = join_strs(list_of(k->s[3]));
                    STOP(r.is_nil() ? R::Err : cont_done(k, r));
                }
                read_chunk(k, j->remaining, O(6));
                break;
            default: { // O(6)
                if (!j->res) {
                    Value r = join_strs(list_of(k->s[3]));
                    STOP(r.is_nil() ? R::Err : cont_done(k, r));
                }
                Root more{ get_decoded(k->s[1], j->remaining) };
                if (more.v.is_nil() || !list_push(list_of(k->s[3]), more.v))
                    STOP(err_pending() ? R::Err : oom());
                j = job_of(k);
                j->remaining -= chars_of(more);
                k->i = O(5);
                break;
            }
            }
            return Go::Again;

        // ------------------------------------------------------ readline
        case T_READLINE:
        case T_NEXT:
            // s[2] what is left over from the last chunk, s[3] the chunks put
            // aside, s[5] the line being searched.
            switch (st) {
            case O(1):
                if (op == T_NEXT)
                    t->telling = false;
                if (!t->has_dec)
                    STOP(io_unsupported("not readable"));
                {
                    ListObj *l = list_new();
                    if (!l)
                        STOP(oom());
                    k->s[3] = obj_value(l);
                }
                leaf(k, U_WRITEFLUSH, O(2));
                break;
            case O(2):
                j->res = true;
                if (t->decoded.is_nil() || chars_of(t->decoded) == 0) {
                    read_chunk(k, 0, O(3));
                    break;
                }
                k->i = O(4);
                break;
            case O(3):
                if (!j->res) {
                    // The end of the file.
                    set_decoded(t, Value());
                    t->snap_input = Value();
                    k->s[5]       = Value();
                    j->start = j->endpos = j->offset = 0;
                    k->i                             = O(6);
                    break;
                }
                if (t->decoded.is_nil() || chars_of(t->decoded) == 0) {
                    read_chunk(k, 0, O(3));
                    break;
                }
                k->i = O(4);
                break;
            case O(4): {
                Root line;
                usize start_b = 0;
                if (k->s[2].is_nil()) {
                    line      = t->decoded;
                    j->start  = t->used;
                    start_b   = t->used_b;
                    j->offset = 0;
                } else {
                    String cat;
                    if (!cat.append(str_of(k->s[2])->str()) ||
                        !cat.append(str_of(t->decoded)->str()))
                        STOP(oom());
                    line = str_ok(cat.str());
                    if (line.v.is_nil())
                        STOP(R::Err);
                    t         = text_of(k->s[1]);
                    j         = job_of(k);
                    j->start  = 0;
                    j->offset = chars_of(k->s[2]);
                    k->s[2]   = Value();
                }
                k->s[5]   = line.v;
                Str s     = str_of(line.v)->str();
                usize end = 0, consumed = 0;
                bool hit  = find_line_ending(t, s, start_b, end, consumed);
                usize endb = hit ? end : consumed;
                i64 endpos = j->start + i64(chars_between(s, start_b, endb));
                i64 limit  = j->size;
                if (limit >= 0 && (endpos - j->start) + j->chunked >= limit) {
                    j->endpos = j->start + limit - j->chunked;
                    k->i      = O(5);
                    break;
                }
                if (hit) {
                    j->endpos = endpos;
                    k->i      = O(5);
                    break;
                }
                if (endb > start_b) {
                    Value part = str_ok(s.substr(start_b, endb - start_b));
                    if (part.is_nil() || !list_push(list_of(k->s[3]), part))
                        STOP(err_pending() ? R::Err : oom());
                    j = job_of(k);
                    j->chunked += chars_of(part);
                }
                s = str_of(k->s[5])->str();
                if (endb < s.size()) {
                    Value rest = str_ok(s.substr(endb));
                    if (rest.is_nil())
                        STOP(R::Err);
                    k->s[2] = rest;
                }
                k->s[5] = Value();
                set_decoded(text_of(k->s[1]), Value());
                k->i = O(2);
                break;
            }
            case O(5): {
                // The line ends in what is decoded now.
                Value line  = k->s[5];
                Str s       = str_of(line)->str();
                i64 used    = j->endpos - j->offset;
                set_used(t, u32(used));
                usize len   = chars_of(line);
                if (j->start > 0 || j->endpos < i64(len)) {
                    usize a = skip_chars(s, 0, j->start);
                    usize b = skip_chars(s, a, j->endpos - j->start);
                    Value p = str_ok(s.substr(a, b - a));
                    if (p.is_nil())
                        STOP(R::Err);
                    k->s[5] = p;
                }
                k->i = O(6);
                break;
            }
            default: { // O(6): put it together
                ListObj *l = list_of(k->s[3]);
                if (!k->s[2].is_nil() && !list_push(l, k->s[2]))
                    STOP(oom());
                Root line{ k->s[5] };
                if (l->items.size()) {
                    if (!line.v.is_nil() && !list_push(l, line.v))
                        STOP(oom());
                    line = join_strs(l);
                    if (line.v.is_nil())
                        STOP(R::Err);
                }
                if (line.v.is_nil()) {
                    line = str_ok(Str());
                    if (line.v.is_nil())
                        STOP(R::Err);
                }
                if (op == T_NEXT && chars_of(line.v) == 0) {
                    t             = text_of(k->s[1]);
                    t->snap_input = Value();
                    t->telling    = t->seekable;
                    Value e       = exc_new(exc_find("StopIteration"), Value());
                    STOP(e.is_nil() ? R::Err : err_set_value(e));
                }
                STOP(cont_done(k, line.v));
            }
            }
            return Go::Again;

        // --------------------------------------------------------- flush
        case T_FLUSH:
            switch (st) {
            case O(1):
                t->telling = t->seekable;
                leaf(k, U_WRITEFLUSH, O(2));
                break;
            case O(2):
                k->i = O(3);
                STOP(cont_method(k, t->buffer, "flush"));
            default:
                STOP(cont_done(k, in));
            }
            return Go::Again;

        // --------------------------------------------------------- close
        case T_CLOSE:
            switch (st) {
            case O(1): {
                int c = closed_now(t);
                if (c == 1)
                    STOP(cont_done(k, value_none()));
                k->i = O(2);
                if (c < 0)
                    STOP(cont_attr(k, t->buffer, "closed"));
                in = value_bool(false);
                break;
            }
            case O(2):
                if (py_truth(in))
                    STOP(cont_done(k, value_none()));
                k->i        = O(3);
                k->catching = CATCH_ANY;
                STOP(cont_method(k, k->s[0], "flush"));
            case O(3):
                if (in.is_nil()) {
                    extra(k, X_ERROR) = k->caught;
                    k->caught         = Value();
                }
                k->i        = O(4);
                k->catching = CATCH_ANY;
                STOP(cont_method(k, t->buffer, "close"));
            default: {
                k->catching = CATCH_NONE;
                Root first{ extra(k, X_ERROR) };
                if (in.is_nil()) {
                    Root late{ k->caught };
                    k->caught = Value();
                    if (!first.v.is_nil() && is_exc(late.v))
                        static_cast<ExcObj *>(late.v.obj())->context = first.v;
                    STOP(err_set_value(late.v));
                }
                if (!first.v.is_nil())
                    STOP(err_set_value(first.v));
                io_untrack(t);
                STOP(cont_done(k, in));
            }
            }
            return Go::Again;

        // -------------------------------------------------------- detach
        case T_DETACH:
            if (st == O(1)) {
                k->i = O(2);
                STOP(cont_method(k, k->s[0], "flush"));
            }
            {
                Value b     = t->buffer;
                t->buffer   = Value();
                t->raw      = Value();
                t->detached = true;
                t->ok       = false;
                io_untrack(t);
                STOP(cont_done(k, b));
            }

        // ---------------------------------------------------------- tell
        case T_TELL:
            switch (st) {
            case O(1):
                if (!t->seekable)
                    STOP(io_unsupported("underlying stream is not seekable"));
                if (!t->telling)
                    STOP(err_set("OSError", "telling position disabled by next() call"));
                leaf(k, U_WRITEFLUSH, O(2));
                break;
            case O(2):
                k->i = O(3);
                STOP(cont_method(k, k->s[0], "flush"));
            case O(3):
                k->i = O(4);
                STOP(cont_method(k, t->buffer, "tell"));
            case O(4): {
                i64 pos = 0;
                if (!as_int_arg(in, pos))
                    STOP(err_set2("TypeError", "tell() should return an integer",
                                  type_name(in)));
                if (!t->has_dec || t->snap_input.is_nil())
                    STOP(cont_done(k, in));
                extra(k, X_NEXTIN) = t->snap_input;
                j                  = job_of(k);
                j->ck              = Cookie();
                j->ck.start_pos =
                    pos - i64(static_cast<BytesObj *>(t->snap_input.obj())->len);
                j->ck.dec_flags = t->snap_flags;
                if (t->used == 0) {
                    Value c = cookie_build(j->ck);
                    STOP(c.is_nil() ? R::Err : cont_done(k, c));
                }
                j->chars_to_skip = t->used;
                leaf(k, U_GETSTATE, O(5));
                break;
            }
            case O(5):
                // What to put back at the end.
                extra(k, X_SAVED) = k->s[6];
                j->set_flags      = j->dflags;
                j->snap_flags     = j->dflags;
                j->skip_bytes     = i64(t->b2cratio * f64(j->chars_to_skip));
                j->skip_back      = 1;
                k->i              = O(6);
                break;
            case O(6):
                if (j->skip_bytes <= 0) {
                    j->skip_bytes = 0;
                    setstate_cookie(k, j->ck, O(9));
                    break;
                }
                setstate_cookie(k, j->ck, O(7));
                break;
            case O(7): {
                Str next = static_cast<BytesObj *>(extra(k, X_NEXTIN).obj())->str();
                usize n  = usize(j->skip_bytes) < next.size() ? usize(j->skip_bytes) : next.size();
                Value b  = bytes_new(next.substr(0, n));
                if (b.is_nil())
                    STOP(R::Err);
                k->s[4]         = b;
                job_of(k)->final = false;
                leaf(k, U_DECODE, O(8));
                break;
            }
            case O(8):
                j->chars_decoded = chars_of(k->s[5]);
                if (j->chars_decoded <= j->chars_to_skip) {
                    leaf(k, U_GETSTATE, O(9) + 100);
                    break;
                }
                j->skip_bytes -= j->skip_back;
                j->skip_back *= 2;
                k->i = O(6);
                break;
            case O(9) + 100: {
                usize held = static_cast<BytesObj *>(k->s[6].obj())->len;
                if (held == 0) {
                    j->ck.dec_flags = j->dflags;
                    j->chars_to_skip -= j->chars_decoded;
                    k->i = O(9);
                    break;
                }
                j->skip_bytes -= i64(held);
                j->skip_back = 1;
                k->i         = O(6);
                break;
            }
            case O(9):
                j->ck.start_pos += j->skip_bytes;
                if (j->chars_to_skip == 0) {
                    k->i = O(13);
                    break;
                }
                j->chars_decoded = 0;
                j->at            = j->skip_bytes;
                k->i             = O(10);
                break;
            case O(10): {
                Str next = static_cast<BytesObj *>(extra(k, X_NEXTIN).obj())->str();
                if (usize(j->at) >= next.size()) {
                    Value e = bytes_new(Str());
                    if (e.is_nil())
                        STOP(R::Err);
                    k->s[4]          = e;
                    job_of(k)->final = true;
                    leaf(k, U_DECODE, O(12));
                    break;
                }
                Value b = bytes_new(next.substr(usize(j->at), 1));
                if (b.is_nil())
                    STOP(R::Err);
                k->s[4]          = b;
                job_of(k)->final = false;
                leaf(k, U_DECODE, O(11));
                break;
            }
            case O(11):
                j->chars_decoded += chars_of(k->s[5]);
                j->ck.bytes_to_feed += 1;
                leaf(k, U_GETSTATE, O(11) + 100);
                break;
            case O(11) + 100: {
                usize held = static_cast<BytesObj *>(k->s[6].obj())->len;
                if (held == 0 && j->chars_decoded <= j->chars_to_skip) {
                    j->ck.start_pos += j->ck.bytes_to_feed;
                    j->chars_to_skip -= j->chars_decoded;
                    j->ck.dec_flags     = j->dflags;
                    j->ck.bytes_to_feed = 0;
                    j->chars_decoded    = 0;
                }
                if (j->chars_decoded >= j->chars_to_skip) {
                    k->i = O(13);
                    break;
                }
                j->at++;
                k->i = O(10);
                break;
            }
            case O(12):
                j->chars_decoded += chars_of(k->s[5]);
                j->ck.need_eof = true;
                if (j->chars_decoded < j->chars_to_skip) {
                    err_set("OSError", "can't reconstruct logical file position");
                    extra(k, X_ERROR) = exc_pending();
                    err_clear();
                }
                k->i = O(13);
                break;
            case O(13):
                // Put the decoder back as it was.
                extra(k, X_SETBUF) = extra(k, X_SAVED);
                j->set_reset       = false;
                j->set_flags       = j->snap_flags;
                leaf(k, U_SETSTATE, O(14));
                break;
            default: { // O(14)
                if (!extra(k, X_ERROR).is_nil())
                    STOP(finish_error(k));
                j->ck.chars_to_skip = j->chars_to_skip;
                Value c             = cookie_build(j->ck);
                STOP(c.is_nil() ? R::Err : cont_done(k, c));
            }
            }
            return Go::Again;

        // ---------------------------------------------------------- seek
        case T_SEEK:
            // s[2] the cookie; job.size the whence.
            switch (st) {
            case O(1): {
                if (!t->seekable)
                    STOP(io_unsupported("underlying stream is not seekable"));
                i64 whence = j->size;
                bool zero  = false;
                if (whence == 1 || whence == 2) {
                    Value z = Value::of_int(0);
                    if (py_eq(k->s[2], z, zero) != R::Ok)
                        STOP(R::Err);
                }
                if (whence == 1) {
                    if (!zero)
                        STOP(io_unsupported("can't do nonzero cur-relative seeks"));
                    k->i = O(2);
                    STOP(cont_method(k, k->s[0], "tell"));
                }
                if (whence == 2) {
                    if (!zero)
                        STOP(io_unsupported("can't do nonzero end-relative seeks"));
                    k->i = O(3);
                    STOP(cont_method(k, k->s[0], "flush"));
                }
                if (whence != 0) {
                    char tmp[24];
                    Buf<96> m;
                    m.put("invalid whence (").put(int_text(tmp, sizeof tmp, whence));
                    m.put(", should be 0, 1 or 2)");
                    STOP(err_set("ValueError", m.str()));
                }
                k->i = O(5);
                break;
            }
            case O(2):
                k->s[2] = in;
                k->i    = O(5);
                break;
            case O(3):
                set_decoded(t, Value());
                t->snap_input = Value();
                k->i          = O(4);
                if (t->has_dec) {
                    j->set_reset = true;
                    leaf(k, U_SETSTATE, O(4));
                }
                break;
            case O(4):
                k->i = O(4) + 100;
                STOP(cont_method(k, t->buffer, "seek", 2, Value::of_int(0), Value::of_int(2)));
            case O(4) + 100: {
                k->s[2]   = in;
                bool zero = false;
                if (py_eq(in, Value::of_int(0), zero) != R::Ok)
                    STOP(R::Err);
                j            = job_of(k);
                j->enc_reset = zero;
                leaf(k, U_ENCSTATE, O(4) + 101);
                break;
            }
            case O(4) + 101:
                STOP(cont_done(k, k->s[2]));
            case O(5): {
                bool neg = false;
                if (py_cmp(k->s[2], Value::of_int(0), Cmp::Lt, neg) != R::Ok)
                    STOP(R::Err);
                if (neg) {
                    String r;
                    if (py_repr(k->s[2], r) != R::Ok)
                        STOP(R::Err);
                    Buf<96> m;
                    m.put("negative seek position ").put(r.str());
                    STOP(err_set("ValueError", m.str()));
                }
                k->i = O(6);
                STOP(cont_method(k, k->s[0], "flush"));
            }
            case O(6): {
                if (!cookie_parse(k->s[2], j->ck))
                    STOP(R::Err);
                Value p = int_from_i64(j->ck.start_pos);
                if (p.is_nil())
                    STOP(R::Err);
                k->i = O(7);
                STOP(cont_method(k, t->buffer, "seek", 1, p));
            }
            case O(7):
                set_decoded(t, Value());
                t->snap_input = Value();
                k->i          = O(8);
                if (t->has_dec)
                    setstate_cookie(k, j->ck, O(8));
                break;
            case O(8):
                if (j->ck.chars_to_skip) {
                    Value n = int_from_i64(j->ck.bytes_to_feed);
                    if (n.is_nil())
                        STOP(R::Err);
                    k->i = O(9);
                    STOP(cont_method(k, t->buffer, "read", 1, n));
                }
                t->snap_input = bytes_new(Str());
                if (t->snap_input.is_nil())
                    STOP(R::Err);
                t->snap_flags = j->ck.dec_flags;
                k->i          = O(11);
                break;
            case O(9):
                if (!is_bytes(in)) {
                    Buf<128> m;
                    m.put("underlying read() should have returned a bytes object, not '");
                    m.put(type_name(in)).put("'");
                    STOP(err_set("TypeError", m.str()));
                }
                t->snap_input = in;
                t->snap_flags = j->ck.dec_flags;
                k->s[4]       = in;
                j->final      = j->ck.need_eof;
                leaf(k, U_DECODE, O(10));
                break;
            case O(10):
                set_decoded(t, k->s[5]);
                if (chars_of(t->decoded) < j->ck.chars_to_skip)
                    STOP(err_set("OSError", "can't restore logical file position"));
                set_used(t, u32(j->ck.chars_to_skip));
                k->i = O(11);
                break;
            case O(11):
                k->i = O(12);
                if (t->has_enc) {
                    j->enc_reset = j->ck.start_pos == 0 && j->ck.dec_flags == 0;
                    leaf(k, U_ENCSTATE, O(12));
                }
                break;
            default:
                STOP(cont_done(k, k->s[2]));
            }
            return Go::Again;

        // ------------------------------------------------------ truncate
        case T_TRUNCATE:
            if (st == O(1)) {
                k->i = O(2);
                STOP(cont_method(k, k->s[0], "flush"));
            }
            if (st == O(2)) {
                k->i = O(3);
                STOP(cont_method(k, t->buffer, "truncate", 1, k->s[2]));
            }
            STOP(cont_done(k, in));

        // --------------------------------------------------- reconfigure
        case T_RECONF:
            // s[2] the encoding, s[3] the errors, s[4] the newline (Nil: left
            // alone); x[0] and x[1] line_buffering and write_through.
            if (st == O(1)) {
                k->i = O(2);
                STOP(cont_method(k, k->s[0], "flush"));
            }
            {
                t->b2cratio = 0;
                if (!k->s[4].is_nil()) {
                    Value nl = is_none(k->s[4]) ? Value() : k->s[4];
                    text_set_newline(t, nl);
                }
                t->line_buffering = k->x[0] != 0;
                t->write_through  = k->x[1] != 0;
                bool enc = !is_none(k->s[2]), errs = !is_none(k->s[3]);
                if (!enc && !errs && k->s[4].is_nil())
                    STOP(cont_done(k, value_none()));
                if (!enc) {
                    if (!errs)
                        k->s[3] = t->errors;
                    k->s[2] = t->encoding;
                } else if (!errs) {
                    Value s = str_new("strict");
                    if (s.is_nil())
                        STOP(R::Err);
                    k->s[3] = s;
                }
                if (text_set_codec(k->s[1], k->s[2], k->s[3]) != R::Ok)
                    STOP(R::Err);
                t          = text_of(k->s[1]);
                t->has_dec = t->has_enc = false;
                t->pydec = t->pyenc = Value();
                j                   = job_of(k);
                j->reconf           = true;
                k->j                = T_INIT;
                k->i                = O(1);
                return Go::Again;
            }

        case T_FORWARD:
            if (st == O(1)) {
                k->i = O(2);
                STOP(cont_method(k, t->buffer, str_of(k->s[2])->str()));
            }
            STOP(cont_done(k, in));

        case T_RESET:
            // What a write owes a decoder written in Python: its reset.
            if (st == O(1)) {
                j->set_reset = true;
                leaf(k, U_SETSTATE, O(2));
                return Go::Again;
            }
            STOP(cont_done(k, k->s[2]));

        default:
            STOP(err_set("SystemError", "textio: lost its place"));
        }
    }
}

R text_step(ContObj *k, Value in)
{
    // One loop drives both halves, so a thousand states cost no stack. What a
    // state hands the next is a root.
    Root rin{ in };
    for (;;) {
        R r  = R::Ok;
        Go g = k->i != 0 && k->i < O_START ? leaf_state(k, rin.v, r) : op_state(k, rin.v, r);
        if (g == Go::Stop)
            return r;
    }
}

// Set by init and by reconfigure.
void text_set_newline(TextObj *t, Value newline)
{
    t->readnl         = newline;
    Str nl            = newline.is_nil() ? Str() : str_of(newline)->str();
    t->readuniversal  = newline.is_nil() || nl.empty();
    t->readtranslate  = newline.is_nil();
    t->writetranslate = newline.is_nil() || !nl.empty();
    t->writenl[0]     = 0;
    if (!t->readuniversal && nl != "\n") {
        usize n = nl.size() < 2 ? nl.size() : 2;
        for (usize i = 0; i < n; i++)
            t->writenl[i] = nl[i];
        t->writenl[n] = 0;
    }
}

R text_set_codec(Value tv, Value encoding, Value errors)
{
    TextObj *t = text_of(tv);
    Str name   = str_of(encoding)->str();
    Codec c    = codec_shortcut(name);
    t->codec   = native_codec(c) ? c : Codec::None;
    // A registered search function may still answer for one of these; the
    // registry's own codecs module is what reads them, as here.
    t->encoding    = encoding;
    t->errors      = errors;
    t->dpend.clear();
    t->dorder      = 0;
    t->enc_started = false;
    t->pendingcr   = false;
    t->seennl      = 0;
    return R::Ok;
}

// ------------------------------------------------------------ making one

Value job_new()
{
    ListObj *l = list_new();
    if (!l)
        return oom(), Value();
    Root rl{ obj_value(l) };
    Job zero{};
    Value blob = bytearray_new(Str(reinterpret_cast<const char *>(&zero), sizeof zero));
    if (blob.is_nil() || !list_push(list_of(rl.v), blob))
        return blob.is_nil() ? Value() : (oom(), Value());
    for (u32 i = 1; i < X_COUNT; i++)
        if (!list_push(list_of(rl.v), Value()))
            return oom(), Value();
    return rl.v;
}

R start(Value self, u32 op, Value &out, Value arg = Value(), i64 size = 0)
{
    Root rs{ self }, ra{ arg };
    Root job{ job_new() };
    if (job.v.is_nil())
        return R::Err;
    Root kv{ cont_new(text_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = rs.v;
    k->s[1]    = method_self(rs.v);
    k->s[2]    = ra.v;
    k->s[7]    = job.v;
    k->j       = op;
    job_of(k)->size = size;
    out             = kv.v;
    return R::Ok;
}

Value text_alloc()
{
    TextObj *t = static_cast<TextObj *>(obj_alloc(&textio_type, sizeof(TextObj)));
    if (!t)
        return oom(), Value();
    t->dict       = Value();
    t->kind       = IO_TEXT;
    t->closed     = false;
    t->inside     = false;
    t->finalizing = false;
    t->buffer = t->encoding = t->errors = t->pydec = t->pyenc = Value();
    t->readnl = t->decoded = t->snap_input = t->raw = t->info = Value();
    new (&t->pending) String();
    new (&t->dpend) String();
    t->codec      = Codec::None;
    t->writenl[0] = 0;
    t->used = t->used_b = 0;
    t->snap_flags       = 0;
    t->chunk_size       = 8192;
    t->b2cratio         = 0;
    t->dorder           = 0;
    t->enc_started      = false;
    t->pendingcr        = false;
    t->seennl           = 0;
    t->has_dec = t->has_enc = false;
    t->readuniversal = t->readtranslate = t->writetranslate = false;
    t->line_buffering = t->write_through = false;
    t->telling = t->seekable = t->has_read1 = false;
    t->ok = t->detached = false;
    t->isstd            = false;
    return obj_value(t);
}

bool newline_ok(Value v)
{
    if (v.is_nil() || is_none(v))
        return true;
    if (!is_str(v)) {
        err_set2("TypeError", "TextIOWrapper() argument 'newline' must be str or None, not",
                 type_name(v));
        return false;
    }
    Str s = str_of(v)->str();
    if (s == "" || s == "\n" || s == "\r" || s == "\r\n")
        return true;
    Buf<96> m;
    m.put("illegal newline value: ").put(s);
    err_set("ValueError", m.str());
    return false;
}

// __init__(buffer, encoding=None, errors=None, newline=None,
//          line_buffering=False, write_through=False)
R text_init(Value self, const CallArgs &a, Value &out, bool init)
{
    constexpr Str NAMES[] = { "buffer",  "encoding",       "errors",
                              "newline", "line_buffering", "write_through" };
    Value v[6];
    if (!fn_take(a, "TextIOWrapper", NAMES, 1, v))
        return R::Err;
    Root rs{ self };
    Value encoding = v[1].is_nil() || is_none(v[1]) ? Value() : v[1];
    if (!encoding.is_nil() && !is_str(encoding))
        return err_set2("TypeError", "TextIOWrapper() argument 'encoding' must be str or None, not",
                        type_name(encoding));
    Root enc{ encoding.is_nil() || str_of(encoding)->str() == "locale" ? str_new("utf-8")
                                                                       : encoding };
    if (enc.v.is_nil())
        return R::Err;
    // Refuse an encoding that cannot be encoded, as CPython's does.
    for (usize i = 0; i < str_of(enc.v)->len; i++)
        if (str_of(enc.v)->bytes()[i] == 0)
            return err_set("ValueError", "embedded null character");
    Value errors = v[2].is_nil() ? value_none() : v[2];
    if (!is_none(errors) && !is_str(errors))
        return err_set2("TypeError", "TextIOWrapper() argument 'errors' must be str or None, not",
                        type_name(errors));
    Root errs{ is_none(errors) ? str_new("strict") : errors };
    if (errs.v.is_nil())
        return R::Err;
    if (!newline_ok(v[3]))
        return R::Err;

    TextObj *t        = text_of(method_self(rs.v));
    t->ok             = false;
    t->detached       = false;
    t->chunk_size     = 8192;
    t->line_buffering = !v[4].is_nil() && py_truth(v[4]);
    t->write_through  = !v[5].is_nil() && py_truth(v[5]);
    text_set_newline(t, v[3].is_nil() || is_none(v[3]) ? Value() : v[3]);
    t->buffer = v[0];
    set_decoded(t, Value());
    t->snap_input = Value();
    t->pending.clear();
    t->has_dec = t->has_enc = false;
    t->pydec = t->pyenc = Value();
    t->b2cratio         = 0;
    if (text_set_codec(method_self(rs.v), enc.v, errs.v) != R::Ok)
        return R::Err;
    if (start(rs.v, T_INIT, out) != R::Ok)
        return R::Err;
    job_of(cont_of(out))->reconf = init;
    cont_of(out)->i               = O(1);
    return R::Ok;
}

R tm_init(const CallArgs &a, Value &out)
{
    if (!self_text(a, "__init__", false))
        return R::Err;
    CallArgs rest = a;
    rest.args++;
    rest.nargs--;
    return text_init(a.args[0], rest, out, true);
}

// ---------------------------------------------------------------- methods

bool closed_or_ask(TextObj *t, bool &closed)
{
    int c  = closed_now(t);
    closed = c == 1;
    return c >= 0;
}

// write(s): answered here when it can be.
R write_now(Value self, TextObj *t, Value s, Value &out)
{
    Root rs{ self }, rt{ s };
    bool haslf = false;
    Root text{ translate_out(t, rt.v, haslf) };
    if (text.v.is_nil())
        return R::Err;
    t              = text_of(method_self(rs.v));
    bool needflush = t->line_buffering && (haslf || has_cr(text.v));
    i64 len        = chars_of(rt.v);
    bool closed    = false;
    bool sync      = t->has_enc && t->codec != Codec::None && closed_or_ask(t, closed);
    if (sync && closed)
        return io_closed_err();
    if (sync) {
        Value enc;
        if (native_encode(t, text.v, enc) != R::Ok)
            return R::Err;
        if (!is_cont(enc)) {
            t = text_of(method_self(rs.v));
            if (!t->pending.append(static_cast<BytesObj *>(enc.obj())->str()))
                return oom();
            bool flush = t->pending.size() >= usize(t->chunk_size) || needflush ||
                         t->write_through || t->isstd;
            bool done  = true;
            if (flush) {
                R put = buffered_put(t->buffer, t->pending.str());
                if (put == R::Err)
                    return R::Err;
                if (put == R::Ok)
                    t->pending.clear();
                else
                    done = false;
            }
            if (done && (!needflush || t->isstd)) {
                set_decoded(t, Value());
                t->snap_input = Value();
                if (t->has_dec) {
                    t->pendingcr = false;
                    t->seennl    = 0;
                    if (t->codec != Codec::None) {
                        t->dpend.clear();
                        t->dorder = 0;
                    } else {
                        // The registry's decoder is reset by a call.
                        Root n{ int_from_i64(len) };
                        if (start(rs.v, T_RESET, out, n.v) != R::Ok)
                            return R::Err;
                        cont_of(out)->i = O(1);
                        return R::Ok;
                    }
                }
                out = int_from_i64(len);
                return out.is_nil() ? R::Err : R::Ok;
            }
            // Encoded already: the machine takes it from the flush.
            if (start(rs.v, T_WRITE, out, text.v) != R::Ok)
                return R::Err;
            Job *j       = job_of(cont_of(out));
            j->needflush = needflush;
            j->len       = len;
            cont_of(out)->i = O(2);
            return R::Ok;
        }
    }
    if (start(rs.v, T_WRITE, out, text.v) != R::Ok)
        return R::Err;
    Job *j       = job_of(cont_of(out));
    j->needflush = needflush;
    j->len       = len;
    return R::Ok;
}

R tm_write(const CallArgs &a, Value &out)
{
    TextObj *t = self_text(a, "write");
    if (!t || !meth_args(a, "write", 1, 1))
        return R::Err;
    // A subclass of str is a str here, so the instance is unwrapped first.
    Value arg = method_self(a.args[1]);
    if (!is_str(arg)) {
        Buf<96> m;
        m.put("write() argument must be str, not ").put(type_name(a.args[1]));
        return err_set("TypeError", m.str());
    }
    return write_now(a.args[0], t, arg, out);
}

bool size_arg(Value v, i64 &n)
{
    n = -1;
    if (v.is_nil() || is_none(v))
        return true;
    if (as_int_arg(v, n))
        return true;
    Buf<128> b;
    b.put("argument should be integer or None, not '").put(type_name(v)).put("'");
    return err_set("TypeError", b.str()) == R::Ok;
}

R tm_read(const CallArgs &a, Value &out)
{
    TextObj *t = self_text(a, "read");
    if (!t || !meth_args(a, "read", 0, 1))
        return R::Err;
    i64 n = -1;
    if (a.nargs > 1 && !size_arg(a.args[1], n))
        return R::Err;
    bool closed = false;
    if (closed_or_ask(t, closed) && !closed && t->has_dec && t->pending.empty() && n >= 0) {
        Str rest = decoded_str(t).substr(t->used_b);
        usize at = skip_chars(rest, 0, n);
        if (chars_between(rest, 0, at) == usize(n)) {
            out = get_decoded(method_self(a.args[0]), n);
            return out.is_nil() ? R::Err : R::Ok;
        }
    }
    return start(a.args[0], T_READ, out, Value(), n);
}

// A line out of what is decoded, when one is there.
R line_now(Value tv, i64 limit, Value &out)
{
    TextObj *t = text_of(tv);
    if (!t->has_dec || !t->pending.empty() || t->decoded.is_nil())
        return R::NotImpl;
    bool closed = false;
    if (!closed_or_ask(t, closed) || closed)
        return R::NotImpl;
    Str s     = decoded_str(t);
    usize end = 0, consumed = 0;
    usize cut;
    if (find_line_ending(t, s, t->used_b, end, consumed))
        cut = end;
    else if (limit >= 0)
        cut = s.size();
    else
        return R::NotImpl;
    if (limit >= 0) {
        usize lim = skip_chars(s, t->used_b, limit);
        if (lim < cut)
            cut = lim;
        else if (cut == s.size() && chars_between(s, t->used_b, cut) < usize(limit))
            return R::NotImpl;
    }
    out = str_ok(s.substr(t->used_b, cut - t->used_b));
    if (out.is_nil())
        return R::Err;
    t = text_of(tv);
    t->used += chars_of(out);
    t->used_b = u32(cut);
    return R::Ok;
}

R tm_readline(const CallArgs &a, Value &out)
{
    TextObj *t = self_text(a, "readline");
    if (!t || !meth_args(a, "readline", 0, 1))
        return R::Err;
    i64 limit = -1;
    if (a.nargs > 1 && !size_arg(a.args[1], limit))
        return R::Err;
    R r = line_now(method_self(a.args[0]), limit, out);
    if (r != R::NotImpl)
        return r;
    return start(a.args[0], T_READLINE, out, Value(), limit);
}

R tm_next(const CallArgs &a, Value &out)
{
    TextObj *t = self_text(a, "__next__");
    if (!t || !meth_args(a, "__next__", 0, 0))
        return R::Err;
    t->telling = false;
    R r        = line_now(method_self(a.args[0]), -1, out);
    if (r != R::NotImpl)
        return r;
    return start(a.args[0], T_NEXT, out, Value(), -1);
}

R tm_flush(const CallArgs &a, Value &out)
{
    TextObj *t = self_text(a, "flush");
    if (!t || !meth_args(a, "flush", 0, 0))
        return R::Err;
    // The standard streams have nothing held back anywhere.
    bool closed = false;
    if (t->isstd && t->pending.empty() && closed_or_ask(t, closed)) {
        if (closed)
            return io_closed_err();
        t->telling = t->seekable;
        out        = value_none();
        return R::Ok;
    }
    return start(a.args[0], T_FLUSH, out);
}

R tm_close(const CallArgs &a, Value &out)
{
    TextObj *t = self_text(a, "close");
    if (!t || !meth_args(a, "close", 0, 0))
        return R::Err;
    if (closed_now(t) == 1) {
        out = value_none();
        return R::Ok;
    }
    if (start(a.args[0], T_CLOSE, out) != R::Ok)
        return R::Err;
    cont_of(out)->i = O(1);
    return R::Ok;
}

R tm_detach(const CallArgs &a, Value &out)
{
    TextObj *t = self_text(a, "detach");
    if (!t || !meth_args(a, "detach", 0, 0))
        return R::Err;
    if (start(a.args[0], T_DETACH, out) != R::Ok)
        return R::Err;
    cont_of(out)->i = O(1);
    return R::Ok;
}

R tm_tell(const CallArgs &a, Value &out)
{
    TextObj *t = self_text(a, "tell");
    if (!t || !meth_args(a, "tell", 0, 0))
        return R::Err;
    return start(a.args[0], T_TELL, out);
}

R tm_seek(const CallArgs &a, Value &out)
{
    TextObj *t = self_text(a, "seek");
    if (!t || !meth_args(a, "seek", 1, 2))
        return R::Err;
    i64 whence = 0;
    if (a.nargs > 2 && !as_int_arg(a.args[2], whence))
        return err_set2("TypeError", "an integer is required", type_name(a.args[2]));
    i64 probe = 0;
    if (!as_int_arg(a.args[1], probe) && !(a.args[1].is_obj() && a.args[1].obj()->type == &int_type))
        return err_set2("TypeError", "an integer is required", type_name(a.args[1]));
    return start(a.args[0], T_SEEK, out, a.args[1], whence);
}

R tm_truncate(const CallArgs &a, Value &out)
{
    TextObj *t = self_text(a, "truncate");
    if (!t || !meth_args(a, "truncate", 0, 1))
        return R::Err;
    if (start(a.args[0], T_TRUNCATE, out, a.nargs > 1 ? a.args[1] : value_none()) != R::Ok)
        return R::Err;
    cont_of(out)->i = O(1);
    return R::Ok;
}

R forward(const CallArgs &a, Value &out, Str who)
{
    TextObj *t = self_text(a, who);
    if (!t || !meth_args(a, who, 0, 0))
        return R::Err;
    Root name{ str_new(who) };
    if (name.v.is_nil() || start(a.args[0], T_FORWARD, out, name.v) != R::Ok)
        return R::Err;
    cont_of(out)->i = O(1);
    return R::Ok;
}

R tm_fileno(const CallArgs &a, Value &out)
{
    return forward(a, out, "fileno");
}

R tm_seekable(const CallArgs &a, Value &out)
{
    TextObj *t = self_text(a, "seekable");
    if (!t || !meth_args(a, "seekable", 0, 0))
        return R::Err;
    bool closed = false;
    if (closed_or_ask(t, closed)) {
        if (closed)
            return io_closed_err();
        out = value_bool(t->seekable);
        return R::Ok;
    }
    return forward(a, out, "seekable");
}

R tm_readable(const CallArgs &a, Value &out)
{
    return forward(a, out, "readable");
}

R tm_writable(const CallArgs &a, Value &out)
{
    return forward(a, out, "writable");
}

R tm_isatty(const CallArgs &a, Value &out)
{
    return forward(a, out, "isatty");
}

// reconfigure(*, encoding=None, errors=None, newline=..., line_buffering=None,
// write_through=None)
R tm_reconfigure(const CallArgs &a, Value &out)
{
    TextObj *t = self_text(a, "reconfigure");
    if (!t)
        return R::Err;
    if (a.nargs > 1)
        return err_set("TypeError", "reconfigure() takes no positional arguments");
    constexpr Str NAMES[] = { "encoding", "errors", "newline", "line_buffering",
                              "write_through" };
    Value v[5];
    if (!meth_take(a, "reconfigure", NAMES, 0, v))
        return R::Err;
    Value encoding = v[0].is_nil() ? value_none() : v[0];
    Value errors   = v[1].is_nil() ? value_none() : v[1];
    if (!t->decoded.is_nil() && (!is_none(encoding) || !is_none(errors) || !v[2].is_nil()))
        return io_unsupported("It is not possible to set the encoding or newline of stream after "
                              "the first read");
    if (!is_none(encoding) && !is_str(encoding))
        return err_set2("TypeError", "reconfigure() argument 'encoding' must be str or None, not",
                        type_name(encoding));
    if (!is_none(errors) && !is_str(errors))
        return err_set2("TypeError", "reconfigure() argument 'errors' must be str or None, not",
                        type_name(errors));
    if (!v[2].is_nil() && !newline_ok(v[2]))
        return R::Err;
    Root enc{ encoding };
    if (is_str(encoding) && str_of(encoding)->str() == "locale") {
        enc = str_new("utf-8");
        if (enc.v.is_nil())
            return R::Err;
    }
    if (start(a.args[0], T_RECONF, out, enc.v) != R::Ok)
        return R::Err;
    ContObj *k = cont_of(out);
    k->s[3]    = errors;
    k->s[4]    = v[2].is_nil() ? Value() : v[2];
    k->x[0]    = v[3].is_nil() || is_none(v[3]) ? t->line_buffering : py_truth(v[3]);
    k->x[1]    = v[4].is_nil() || is_none(v[4]) ? t->write_through : py_truth(v[4]);
    k->i       = O(1);
    return R::Ok;
}

R tm_getstate(const CallArgs &a, Value &out)
{
    (void)out;
    Buf<96> b;
    b.put("cannot pickle '").put(a.nargs ? type_name(a.args[0]) : Str("?")).put("' instances");
    return err_set("TypeError", b.str());
}

R tm_dealloc_warn(const CallArgs &a, Value &out)
{
    if (!self_text(a, "_dealloc_warn", false))
        return R::Err;
    out = value_none();
    return R::Ok;
}

constexpr Method TEXT_METHODS[] = {
    { "__init__", tm_init },
    { "write", tm_write },
    { "read", tm_read },
    { "readline", tm_readline },
    { "__next__", tm_next },
    { "flush", tm_flush },
    { "close", tm_close },
    { "detach", tm_detach },
    { "tell", tm_tell },
    { "seek", tm_seek },
    { "truncate", tm_truncate },
    { "fileno", tm_fileno },
    { "seekable", tm_seekable },
    { "readable", tm_readable },
    { "writable", tm_writable },
    { "isatty", tm_isatty },
    { "reconfigure", tm_reconfigure },
    { "__getstate__", tm_getstate },
    { "__reduce_ex__", tm_getstate },
    { "_dealloc_warn", tm_dealloc_warn },
};

R text_getattr(Value v, StrObj *name, Value &out)
{
    TextObj *t = text_of(v);
    Str n      = name->str();
    if (n == "_CHUNK_SIZE") {
        out = int_from_i64(t->chunk_size);
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (n == "_finalizing") {
        out = value_bool(t->finalizing);
        return R::Ok;
    }
    bool mine = n == "encoding" || n == "errors" || n == "line_buffering" ||
                n == "write_through" || n == "buffer" || n == "newlines" || n == "name" ||
                n == "closed";
    if (mine && !t->ok)
        return not_ready(t);
    if (n == "encoding")
        out = t->encoding;
    else if (n == "errors")
        out = t->errors;
    else if (n == "line_buffering")
        out = value_bool(t->line_buffering);
    else if (n == "write_through")
        out = value_bool(t->write_through);
    else if (n == "buffer")
        out = t->buffer;
    else if (n == "newlines") {
        if (!t->has_dec) {
            out = value_none();
        } else if (t->readuniversal) {
            out = newlines_value(t->seennl);
        } else if (t->codec == Codec::None) {
            StrObj *nl = str_intern("newlines");
            Got g      = nl ? py_attr(t->pydec, nl, out) : Got::Error;
            if (g == Got::Error)
                return R::Err;
            if (g != Got::Ok)
                out = value_none();
        } else {
            out = value_none();
        }
    } else if (n == "name" || n == "closed") {
        if (n == "closed" && !t->raw.is_nil()) {
            out = value_bool(file_facts(t->raw).closed);
            return R::Ok;
        }
        return py_getattr(t->buffer, name, out);
    } else {
        return io_dict_get(v, name, out);
    }
    return out.is_nil() ? R::Err : R::Ok;
}

R text_setattr(Value v, StrObj *name, Value x)
{
    if (name->str() == "_CHUNK_SIZE") {
        TextObj *t = text_of(v);
        if (!t->ok)
            return not_ready(t);
        if (x.is_nil())
            return err_set("AttributeError", "cannot delete attribute");
        i64 n = 0;
        if (!as_int_arg(x, n))
            return err_set2("TypeError", "an integer is required", type_name(x));
        if (n <= 0)
            return err_set("ValueError", "a strictly positive integer is required");
        t->chunk_size = n;
        return R::Ok;
    }
    constexpr Str READONLY[] = { "encoding",      "errors", "line_buffering", "write_through",
                                 "buffer",        "closed", "name",           "newlines" };
    return io_set_attr(v, name, x, READONLY, "_io.TextIOWrapper");
}

Value builtin_getattr()
{
    StrObj *n = str_intern("getattr");
    Value fn;
    DictObj *d = builtins_dict();
    if (!n || !d || dict_get(d, obj_value(n), fn) != R::Ok)
        return Value();
    return fn;
}

Got text_lazy(Value v, StrObj *name, Value &out, Value &args)
{
    TextObj *t = text_of(v);
    Str n      = name->str();
    if (!(n == "closed" || n == "name") || !t->ok)
        return Got::Missing;
    if (n == "closed" && !t->raw.is_nil())
        return Got::Missing;
    Value probe;
    Got g = py_attr(t->buffer, name, probe);
    if (g != Got::Call)
        return g == Got::Error ? (err_clear(), Got::Missing) : Got::Missing;
    Root pr{ probe };
    out = builtin_getattr();
    if (out.is_nil())
        return err_pending() ? Got::Error : Got::Missing;
    TupleObj *tp = tuple_new(2);
    if (!tp)
        return oom(), Got::Error;
    tp->items()[0] = t->buffer;
    tp->items()[1] = obj_value(name);
    args           = obj_value(tp);
    return Got::Call;
}

R text_repr(Value v, String &out)
{
    TextObj *t = text_of(v);
    if (!out.append("<_io.TextIOWrapper"))
        return oom();
    if (!repr_enter(v))
        return err_set("RuntimeError", "reentrant call inside _io.TextIOWrapper.__repr__");
    Root rv{ v };
    R bad = R::Ok;
    constexpr Str FIELDS[] = { "name", "mode" };
    for (Str f : FIELDS) {
        StrObj *n = str_intern(f);
        Value got;
        Got g = n ? py_attr(rv.v, n, got) : Got::Error;
        if (g != Got::Ok) {
            // ValueError or AttributeError: the field is left out.
            err_clear();
            continue;
        }
        Root rg{ got };
        if (!out.push(' ') || !out.append(f) || !out.push('=')) {
            bad = oom();
            break;
        }
        if (py_repr(rg.v, out) != R::Ok) {
            bad = R::Err;
            break;
        }
    }
    repr_leave();
    if (bad != R::Ok)
        return bad;
    t = text_of(rv.v);
    if (!out.append(" encoding="))
        return oom();
    if (t->encoding.is_nil()) {
        if (!out.append("None"))
            return oom();
    } else if (py_repr(t->encoding, out) != R::Ok) {
        return R::Err;
    }
    return out.push('>') ? R::Ok : oom();
}

Value self_iter(Value v)
{
    return v;
}

// --------------------------------------------- IncrementalNewlineDecoder

struct NlObj : Obj {
    Value decoder; // Nil until __init__
    Value errors;
    bool translate, pendingcr;
    u8 seennl;
};

NlObj *nl_of(Value v)
{
    return static_cast<NlObj *>(v.obj());
}

void nl_trace(Obj *o)
{
    gc_mark(static_cast<NlObj *>(o)->decoder);
    gc_mark(static_cast<NlObj *>(o)->errors);
}

NlObj *self_nl(const CallArgs &a, Str who)
{
    Value v = a.nargs ? method_self(a.args[0]) : Value();
    if (!v.is_obj() || v.obj()->type != &nldecoder_type) {
        err_set2("TypeError", "an IncrementalNewlineDecoder is required", who);
        return nullptr;
    }
    NlObj *n = nl_of(v);
    if (n->decoder.is_nil()) {
        err_set("ValueError", "IncrementalNewlineDecoder.__init__() not called");
        return nullptr;
    }
    return n;
}

R nl_init(Value self, const CallArgs &a)
{
    constexpr Str NAMES[] = { "decoder", "translate", "errors" };
    Value v[3];
    if (!fn_take(a, "IncrementalNewlineDecoder", NAMES, 2, v))
        return R::Err;
    NlObj *n     = nl_of(self);
    n->decoder   = v[0];
    n->translate = py_truth(v[1]);
    n->errors    = v[2].is_nil() ? str_new("strict") : v[2];
    n->pendingcr = false;
    n->seennl    = 0;
    return n->errors.is_nil() ? R::Err : R::Ok;
}

R nm_init(const CallArgs &a, Value &out)
{
    Value v = a.nargs ? method_self(a.args[0]) : Value();
    if (!v.is_obj() || v.obj()->type != &nldecoder_type)
        return err_set("TypeError", "an IncrementalNewlineDecoder is required");
    CallArgs rest = a;
    rest.args++;
    rest.nargs--;
    if (nl_init(v, rest) != R::Ok)
        return R::Err;
    out = value_none();
    return R::Ok;
}

// s[0] the native, s[1] input; j final.
R nl_decode_step(ContObj *k, Value in)
{
    NlObj *n = nl_of(k->s[0]);
    if (k->i++ == 0)
        return cont_method(k, n->decoder, "decode", 2, k->s[1], value_bool(k->j));
    if (!is_str(in)) {
        Buf<128> m;
        m.put("decoder should return a string result, not '").put(type_name(in)).put("'");
        return err_set("TypeError", m.str());
    }
    Value out = newline_apply(str_of(in)->str(), k->j, n->translate, n->pendingcr, n->seennl);
    return out.is_nil() ? R::Err : cont_done(k, out);
}

R nm_decode(const CallArgs &a, Value &out)
{
    NlObj *n = self_nl(a, "decode");
    if (!n)
        return R::Err;
    constexpr Str NAMES[] = { "input", "final" };
    Value v[2];
    if (!meth_take(a, "decode", NAMES, 1, v))
        return R::Err;
    bool final = !v[1].is_nil() && py_truth(v[1]);
    if (is_none(n->decoder)) {
        if (!is_str(v[0])) {
            Buf<128> m;
            m.put("decoder should return a string result, not '").put(type_name(v[0])).put("'");
            return err_set("TypeError", m.str());
        }
        out = newline_apply(str_of(v[0])->str(), final, n->translate, n->pendingcr, n->seennl);
        return out.is_nil() ? R::Err : R::Ok;
    }
    Root self{ method_self(a.args[0]) }, input{ v[0] };
    Root kv{ cont_new(nl_decode_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = self.v;
    cont_of(kv.v)->s[1] = input.v;
    cont_of(kv.v)->j    = final;
    out                 = kv.v;
    return R::Ok;
}

Value state_pair(Value buf, i64 flag)
{
    Root rb{ buf };
    Root rf{ int_from_i64(flag) };
    TupleObj *t = rf.v.is_nil() ? nullptr : tuple_new(2);
    if (!t)
        return err_pending() ? Value() : (oom(), Value());
    t->items()[0] = rb.v;
    t->items()[1] = rf.v;
    return obj_value(t);
}

R nl_getstate_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_method(k, nl_of(k->s[0])->decoder, "getstate");
    TupleObj *tp = is_tuple(in) ? static_cast<TupleObj *>(in.obj()) : nullptr;
    i64 flag     = 0;
    if (!tp || tp->len != 2 || !as_int_arg(tp->items()[1], flag))
        return err_set("TypeError", "illegal decoder state");
    flag <<= 1;
    if (nl_of(k->s[0])->pendingcr)
        flag |= 1;
    Value out = state_pair(tp->items()[0], flag);
    return out.is_nil() ? R::Err : cont_done(k, out);
}

R nm_getstate(const CallArgs &a, Value &out)
{
    NlObj *n = self_nl(a, "getstate");
    if (!n || !meth_args(a, "getstate", 0, 0))
        return R::Err;
    if (is_none(n->decoder)) {
        Root b{ bytes_new(Str()) };
        out = b.v.is_nil() ? Value() : state_pair(b.v, n->pendingcr ? 1 : 0);
        return out.is_nil() ? R::Err : R::Ok;
    }
    Root self{ method_self(a.args[0]) };
    Root kv{ cont_new(nl_getstate_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = self.v;
    out                 = kv.v;
    return R::Ok;
}

R nl_forward_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return k->s[2].is_nil() ? cont_method(k, k->s[0], str_of(k->s[1])->str())
                                : cont_method(k, k->s[0], str_of(k->s[1])->str(), 1, k->s[2]);
    (void)in;
    return cont_done(k, value_none());
}

R nl_forward(Value decoder, Str name, Value arg, Value &out)
{
    Root rd{ decoder }, ra{ arg };
    Root rn{ str_new(name) };
    Root kv{ rn.v.is_nil() ? Value() : cont_new(nl_forward_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = rd.v;
    cont_of(kv.v)->s[1] = rn.v;
    cont_of(kv.v)->s[2] = ra.v;
    out                 = kv.v;
    return R::Ok;
}

R nm_setstate(const CallArgs &a, Value &out)
{
    NlObj *n = self_nl(a, "setstate");
    if (!n || !meth_args(a, "setstate", 1, 1))
        return R::Err;
    TupleObj *tp = is_tuple(a.args[1]) ? static_cast<TupleObj *>(a.args[1].obj()) : nullptr;
    i64 flag     = 0;
    if (!tp)
        return err_set("TypeError", "state argument must be a tuple");
    if (tp->len != 2 || !as_int_arg(tp->items()[1], flag))
        return err_set("TypeError", "illegal state argument");
    n->pendingcr = (flag & 1) != 0;
    if (is_none(n->decoder)) {
        out = value_none();
        return R::Ok;
    }
    Root pair{ state_pair(tp->items()[0], flag >> 1) };
    if (pair.v.is_nil())
        return R::Err;
    return nl_forward(nl_of(method_self(a.args[0]))->decoder, "setstate", pair.v, out);
}

R nm_reset(const CallArgs &a, Value &out)
{
    NlObj *n = self_nl(a, "reset");
    if (!n || !meth_args(a, "reset", 0, 0))
        return R::Err;
    n->seennl    = 0;
    n->pendingcr = false;
    if (is_none(n->decoder)) {
        out = value_none();
        return R::Ok;
    }
    return nl_forward(n->decoder, "reset", Value(), out);
}

constexpr Method NL_METHODS[] = {
    { "__init__", nm_init },         { "decode", nm_decode }, { "getstate", nm_getstate },
    { "setstate", nm_setstate },     { "reset", nm_reset },
};

R nl_getattr(Value v, StrObj *name, Value &out)
{
    NlObj *n = nl_of(v);
    if (name->str() == "newlines") {
        if (n->decoder.is_nil())
            return err_set("ValueError", "IncrementalNewlineDecoder.__init__() not called");
        out = newlines_value(n->seennl);
        return out.is_nil() ? R::Err : R::Ok;
    }
    return R::NotImpl;
}

R nl_repr(Value v, String &out)
{
    char tmp[24];
    Buf<96> b;
    b.put("<_io.IncrementalNewlineDecoder object at ").put(addr_text(tmp, sizeof tmp, v.obj()));
    b.put('>');
    return out.append(b.str()) ? R::Ok : oom();
}

} // namespace

constexpr Type textio_type{ .name     = "_io.TextIOWrapper",
                            .trace    = text_trace,
                            .fini     = text_fini,
                            .repr     = text_repr,
                            .iter     = self_iter,
                            .getattr  = text_getattr,
                            .setattr  = text_setattr,
                            .lazyattr = text_lazy,
                            .del      = io_del,
                            .base     = &textbase_type,
                            .vmnext   = true };

constexpr Type nldecoder_type{ .name    = "_io.IncrementalNewlineDecoder",
                               .trace   = nl_trace,
                               .repr    = nl_repr,
                               .getattr = nl_getattr };

bool text_methods()
{
    return method_install(&textio_type, TEXT_METHODS) &&
           method_install(&nldecoder_type, NL_METHODS);
}

R textio_new(const CallArgs &a, Value &out)
{
    Root self{ text_alloc() };
    if (self.v.is_nil())
        return R::Err;
    if (!a.nargs && !a.nkw) {
        out = self.v;
        return R::Ok;
    }
    return text_init(self.v, a, out, false);
}

R nldecoder_new(const CallArgs &a, Value &out)
{
    NlObj *n = static_cast<NlObj *>(obj_alloc(&nldecoder_type, sizeof(NlObj)));
    if (!n)
        return oom();
    n->decoder   = Value();
    n->errors    = Value();
    n->translate = n->pendingcr = false;
    n->seennl                   = 0;
    Root rn{ obj_value(n) };
    if (a.nargs || a.nkw) {
        if (nl_init(rn.v, a) != R::Ok)
            return R::Err;
    }
    out = rn.v;
    return R::Ok;
}

void nldecoder_set_seen(Value v, u8 seen)
{
    nl_of(v)->seennl = seen;
}

void textio_set_line_buffering(Value v, bool on)
{
    if (io_is_plain(v, IO_TEXT) && text_of(v)->isstd)
        text_of(v)->line_buffering = on;
}

bool textio_is_std(Value v)
{
    return io_is_plain(v, IO_TEXT) && text_of(v)->isstd;
}

Value textio_std(Value buffer, i32 fd)
{
    Root rb{ buffer };
    Root self{ text_alloc() };
    if (self.v.is_nil())
        return Value();
    Root nl{ str_new("\n") };
    Root enc{ str_new("utf-8") };
    Root errs{ str_new(fd == 2 ? Str("backslashreplace") : Str("surrogateescape")) };
    if (nl.v.is_nil() || enc.v.is_nil() || errs.v.is_nil())
        return Value();
    TextObj *t = text_of(self.v);
    t->buffer  = rb.v;
    text_set_newline(t, nl.v);
    text_set_codec(self.v, enc.v, errs.v);
    t          = text_of(self.v);
    bool known = false;
    bool tty   = sys_tty(fd, known);
    t->line_buffering = fd == 2 || tty;
    t->has_dec        = fd == 0;
    t->has_enc        = fd != 0;
    t->seekable = t->telling = false;
    t->has_read1             = true;
    t->isstd                 = true;
    t->ok                    = true;
    StrObj *rn               = str_intern("raw");
    Value r;
    if (rn && py_attr(rb.v, rn, r) == Got::Ok && io_is_plain(r, IO_FILE))
        text_of(self.v)->raw = r;
    err_clear();
    return self.v;
}

R textio_print(Value file, Str text, Value &out)
{
    if (!io_is_plain(file, IO_TEXT) || !text_of(file)->ok)
        return R::NotImpl;
    Root rf{ file };
    Root s{ str_new(text) };
    if (s.v.is_nil())
        return R::Err;
    return write_now(rf.v, text_of(rf.v), s.v, out);
}
