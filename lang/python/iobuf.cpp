// BufferedReader, BufferedWriter, BufferedRandom and BufferedRWPair.
//
// CPython's bufferedio.c in behaviour: the same checks in the same order, the
// same fast paths, and a raw stream reached only through its methods, so a
// class of the program's own is a raw stream like any other. What differs is
// the shape: a request that needs the raw stream is a state machine, and the
// part of it several requests share -- flushing, refilling, a raw read -- is
// a subroutine whose return state the machine keeps in x[3].
#include "io.h"

#include "builtin.h"
#include "exc.h"
#include "gc.h"
#include "intern.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "type.h"
#include "vm.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

constexpr i64 DEFAULT_BUFFER_SIZE = 128 * 1024;

struct BufObj : IoObj {
    Value raw;
    Vec<u8> rbuf; // [rpos, size) is unread
    Vec<u8> wbuf; // not written yet
    usize rpos;
    usize size;   // buffer_size
    i64 abs;      // where the raw stream is, or -1 when that is not known
    bool readable, writable;
    bool ok, detached;
    bool through; // writes go straight to the raw stream: sys.stdout's
};

BufObj *buf_of(Value v)
{
    return static_cast<BufObj *>(v.obj());
}

usize unread(const BufObj *b)
{
    return b->rbuf.size() - b->rpos;
}

void buf_trace(Obj *o)
{
    io_trace(o);
    gc_mark(static_cast<BufObj *>(o)->raw);
}

void buf_fini(Obj *o)
{
    io_untrack(o);
    BufObj *b = static_cast<BufObj *>(o);
    b->rbuf.~Vec();
    b->wbuf.~Vec();
}

bool raw_plain(const BufObj *b)
{
    return io_is_plain(b->raw, IO_FILE);
}

// Uninitialised or detached: the error every method raises first.
R not_ready(const BufObj *b)
{
    return err_set("ValueError", b->detached ? Str("raw stream has been detached")
                                             : Str("I/O operation on uninitialized object"));
}

// Is the raw stream closed, when that needs no call? -1 when it does.
int raw_closed_now(const BufObj *b)
{
    if (raw_plain(b))
        return file_facts(b->raw).closed ? 1 : 0;
    IoObj *io = nullptr;
    if (io_of(b->raw, io) && !is_inst(b->raw))
        return io->closed ? 1 : 0;
    return -1;
}

Value bytes_of(const Vec<u8> &v, usize from, usize n)
{
    return bytes_new(Str(reinterpret_cast<const char *>(v.data() + from), n));
}

bool put_bytes(Vec<u8> &v, Str s)
{
    if (!v.reserve(v.size() + s.size()))
        return false;
    for (usize i = 0; i < s.size(); i++)
        v.push(u8(s[i]));
    return true;
}

void reset_read(BufObj *b)
{
    b->rbuf.clear();
    b->rpos = 0;
}

Str type_label(Value self)
{
    return type_name(self);
}

// ------------------------------------------------------------ the machine

enum Op : u32 {
    B_READ,
    B_READALL,
    B_READ1,
    B_PEEK,
    B_READINTO,
    B_READINTO1,
    B_READLINE,
    B_NEXT,
    B_WRITE,
    B_FLUSH,
    B_CLOSE,
    B_DETACH,
    B_SEEK,
    B_TELL,
    B_TRUNCATE,
    B_INIT,
};

// States. The first few are subroutines, which go to x[3] when done.
enum : u32 {
    S_FLUSH = 1,    // write out wbuf
    S_FLUSHED,      // raw.write answered
    S_REWIND,       // after a flush: put the raw stream where the reader is
    S_REWOUND,      // raw.seek answered
    S_RAWREAD,      // read x[2] bytes into s[6]
    S_RAWREAD_A,    // raw.readinto answered
    S_RAWREAD_B,    // raw.read answered
    S_CLOSED_ASK,   // raw.closed answered, for the op's first check

    O_START = 20, // each op's own states follow
    O_1,
    O_2,
    O_3,
    O_4,
    O_5,
    O_6,
};

// s[0] self as called, s[1] the native, s[2] the op's argument, s[3] the
// answer being built (a bytearray), s[6] what a raw read gave, s[7] a saved
// exception. x[0] n, x[1] whence or written, x[2] the raw read's size, x[3]
// the subroutine's return state. j the op.

R finish_bytes(ContObj *k)
{
    Value b = bytes_new(array_of(k->s[3])->str());
    return b.is_nil() ? R::Err : cont_done(k, b);
}

bool acc_append(ContObj *k, Str s)
{
    return put_bytes(array_of(k->s[3])->data, s) || oom() == R::Ok;
}

// The op's first check: closed, unless there is still something to read.
// `msg` is what CPython says.
Str closed_msg(u32 op)
{
    switch (op) {
    case B_READ:
    case B_READALL:
    case B_READ1:
    case B_READINTO:
    case B_READINTO1:
        return "read of closed file";
    case B_PEEK:
        return "peek of closed file";
    case B_READLINE:
    case B_NEXT:
        return "readline of closed file";
    case B_WRITE:
        return "write to closed file";
    case B_FLUSH:
        return "flush of closed file";
    case B_SEEK:
        return "seek of closed file";
    case B_TRUNCATE:
        return "truncate of closed file";
    default:
        return "I/O operation on closed file.";
    }
}

Str raw_len_error(Str what, i64 n, usize most, Buf<160> &b)
{
    char t1[24], t2[24];
    b.put("raw ").put(what).put("() returned invalid length ").put(int_text(t1, sizeof t1, n));
    b.put(" (should have been between 0 and ").put(int_text(t2, sizeof t2, i64(most))).put(")");
    return b.str();
}

R ret(ContObj *k, Value in);

// BlockingIOError(EAGAIN, msg, written), pending.
R blocking(Str msg, i64 written)
{
    Root m{ str_new(msg) };
    Root w{ int_from_i64(written) };
    TupleObj *t = m.v.is_nil() || w.v.is_nil() ? nullptr : tuple_new(3);
    if (!t)
        return err_pending() ? R::Err : oom();
    t->items()[0] = Value::of_int(11);
    t->items()[1] = m.v;
    t->items()[2] = w.v;
    Root rt{ obj_value(t) };
    Root cls{ exc_type_value(exc_find("BlockingIOError")) };
    Value e = cls.v.is_nil() ? Value() : exc_construct(cls.v, rt.v);
    return e.is_nil() ? R::Err : err_set_value(e);
}

R buf_step(ContObj *k, Value in)
{
    BufObj *b = buf_of(k->s[1]);
    u32 op    = k->j;
    for (;;) {
        switch (k->i) {
        // ---------------------------------------------------- subroutines
        case S_FLUSH:
            if (b->wbuf.empty())
                return ret(k, Value());
            {
                Value data = bytes_of(b->wbuf, 0, b->wbuf.size());
                if (data.is_nil())
                    return R::Err;
                k->i = S_FLUSHED;
                return cont_method(k, b->raw, "write", 1, data);
            }
        case S_FLUSHED: {
            if (is_none(in))
                return blocking("write could not complete without blocking", 0);
            i64 n = 0;
            if (!as_int_arg(in, n))
                return err_set2("TypeError", "raw write() should return an integer",
                                type_name(in));
            b = buf_of(k->s[1]);
            if (n < 0 || usize(n) > b->wbuf.size()) {
                Buf<160> m;
                return err_set("OSError", raw_len_error("write", n, b->wbuf.size(), m));
            }
            usize left = b->wbuf.size() - usize(n);
            for (usize i = 0; i < left; i++)
                b->wbuf[i] = b->wbuf[usize(n) + i];
            b->wbuf.resize(left);
            if (b->abs >= 0)
                b->abs += n;
            k->i = S_FLUSH;
            continue;
        }
        case S_REWIND:
            if (!b->readable || unread(b) == 0) {
                reset_read(b);
                return ret(k, Value());
            }
            {
                Value off = int_from_i64(-i64(unread(b)));
                if (off.is_nil())
                    return R::Err;
                k->i = S_REWOUND;
                return cont_method(k, b->raw, "seek", 2, off, Value::of_int(1));
            }
        case S_REWOUND: {
            i64 at = -1;
            as_int_arg(in, at);
            b->abs = at;
            reset_read(b);
            return ret(k, Value());
        }
        case S_RAWREAD: {
            Value want = int_from_i64(k->x[2]);
            if (want.is_nil())
                return R::Err;
            if (raw_plain(b)) {
                k->i = S_RAWREAD_B;
                return cont_method(k, b->raw, "read", 1, want);
            }
            String zeros;
            if (!zeros.reserve(usize(k->x[2])))
                return oom();
            for (i64 i = 0; i < k->x[2]; i++)
                zeros.push(0);
            Value scratch = bytearray_new(zeros.str());
            if (scratch.is_nil())
                return R::Err;
            k->s[6] = scratch;
            k->i    = S_RAWREAD_A;
            return cont_method(k, b->raw, "readinto", 1, scratch);
        }
        case S_RAWREAD_A: {
            if (is_none(in)) {
                k->s[6] = value_none();
                return ret(k, Value());
            }
            i64 n = 0;
            if (!as_int_arg(in, n))
                return err_set2("TypeError", "raw readinto() should return an integer",
                                type_name(in));
            usize most = array_of(k->s[6])->data.size();
            if (n < 0 || usize(n) > most) {
                Buf<160> m;
                return err_set("OSError", raw_len_error("readinto", n, most, m));
            }
            Value got = bytes_new(array_of(k->s[6])->str().substr(0, usize(n)));
            if (got.is_nil())
                return R::Err;
            k->s[6] = got;
            b       = buf_of(k->s[1]);
            if (b->abs >= 0)
                b->abs += n;
            return ret(k, Value());
        }
        case S_RAWREAD_B:
            if (!is_none(in) && !is_bytes(in))
                return err_set2("TypeError", "read() should return bytes", type_name(in));
            k->s[6] = in;
            if (is_bytes(in) && b->abs >= 0)
                b->abs += static_cast<BytesObj *>(in.obj())->len;
            return ret(k, Value());
        case S_CLOSED_ASK:
            if (py_truth(in))
                return io_closed_err(closed_msg(op));
            k->i = O_START + 1;
            continue;

        default:
            break;
        }

        // -------------------------------------------------------- the ops
        u32 st = k->i;
        if (st == 0) {
            // Every op but these checks the stream first.
            bool check = op != B_CLOSE && op != B_DETACH && op != B_TELL && op != B_INIT &&
                         !(op == B_FLUSH && !b->writable);
            bool data  = unread(b) > 0;
            if (check && !(data && op != B_WRITE && op != B_FLUSH && op != B_TRUNCATE)) {
                int c = raw_closed_now(b);
                if (c == 1)
                    return io_closed_err(closed_msg(op));
                if (c < 0) {
                    k->i = S_CLOSED_ASK;
                    return cont_attr(k, b->raw, "closed");
                }
            }
            k->i = O_START + 1;
            continue;
        }

        switch (op) {
        case B_READ: {
            // x[0] n, s[3] the answer so far.
            if (st == O_START + 1) {
                Value acc = bytearray_new(Str());
                if (acc.is_nil())
                    return R::Err;
                k->s[3] = acc;
                usize u = unread(b);
                if (!acc_append(k, Str(reinterpret_cast<const char *>(b->rbuf.data() + b->rpos),
                                       u)))
                    return R::Err;
                reset_read(b);
                k->x[1] = 0;
                k->x[3] = O_START + 2;
                k->i    = b->writable ? S_FLUSH : O_START + 2;
                continue;
            }
            if (st == O_START + 2) {
                i64 have      = i64(array_of(k->s[3])->data.size());
                i64 remaining = k->x[0] - have;
                if (remaining <= 0)
                    return finish_bytes(k);
                // Whole blocks go straight into the answer; the rest fills
                // the buffer from where the last fill ended (x[1]).
                i64 whole = remaining - remaining % i64(b->size);
                if (whole > 0) {
                    k->x[2] = whole;
                    k->x[1] = -1;
                } else {
                    if (k->x[1] < 0)
                        k->x[1] = 0;
                    k->x[2] = i64(b->size) - k->x[1];
                    if (k->x[2] <= 0)
                        return finish_bytes(k);
                }
                k->x[3] = O_START + 3;
                k->i    = S_RAWREAD;
                continue;
            }
            // O_START + 3: what the raw read gave.
            Value got = k->s[6];
            if (is_none(got))
                return array_of(k->s[3])->data.empty() ? cont_done(k, value_none())
                                                       : finish_bytes(k);
            Str data = static_cast<BytesObj *>(got.obj())->str();
            if (data.empty())
                return finish_bytes(k);
            b             = buf_of(k->s[1]);
            i64 have      = i64(array_of(k->s[3])->data.size());
            i64 remaining = k->x[0] - have;
            usize take    = usize(remaining) < data.size() ? usize(remaining) : data.size();
            if (!acc_append(k, data.substr(0, take)))
                return R::Err;
            if (k->x[1] >= 0)
                k->x[1] += i64(data.size());
            if (take < data.size()) {
                reset_read(b);
                if (!put_bytes(b->rbuf, data.substr(take)))
                    return oom();
            }
            k->i = O_START + 2;
            continue;
        }

        case B_READALL: {
            if (st == O_START + 1) {
                Value acc = bytearray_new(Str());
                if (acc.is_nil())
                    return R::Err;
                k->s[3] = acc;
                if (!acc_append(k, Str(reinterpret_cast<const char *>(b->rbuf.data() + b->rpos),
                                       unread(b))))
                    return R::Err;
                reset_read(b);
                k->x[3] = O_START + 2;
                k->i    = b->writable ? S_FLUSH : O_START + 2;
                continue;
            }
            if (st == O_START + 2) {
                StrObj *ra = str_intern("readall");
                Value m;
                Got g = ra ? py_attr(b->raw, ra, m) : Got::Error;
                if (g == Got::Error)
                    return R::Err;
                if (g == Got::Missing) {
                    k->i = O_START + 4;
                    return cont_method(k, b->raw, "read");
                }
                k->i = O_START + 3;
                return cont_method(k, b->raw, "readall");
            }
            if (st == O_START + 3) {
                if (is_none(in))
                    return array_of(k->s[3])->data.empty() ? cont_done(k, in) : finish_bytes(k);
                if (!is_bytes(in))
                    return err_set2("TypeError", "readall() should return bytes", type_name(in));
                if (!acc_append(k, static_cast<BytesObj *>(in.obj())->str()))
                    return R::Err;
                b = buf_of(k->s[1]);
                if (b->abs >= 0)
                    b->abs += static_cast<BytesObj *>(in.obj())->len;
                return finish_bytes(k);
            }
            // O_START + 4: raw.read() answered.
            if (is_none(in) || (is_bytes(in) && static_cast<BytesObj *>(in.obj())->len == 0)) {
                if (is_none(in) && array_of(k->s[3])->data.empty())
                    return cont_done(k, in);
                return finish_bytes(k);
            }
            if (!is_bytes(in))
                return err_set2("TypeError", "read() should return bytes", type_name(in));
            if (!acc_append(k, static_cast<BytesObj *>(in.obj())->str()))
                return R::Err;
            return cont_method(k, b->raw, "read");
        }

        case B_READ1:
        case B_PEEK: {
            if (st == O_START + 1) {
                reset_read(b);
                k->x[3] = O_START + 2;
                k->i    = b->writable ? S_FLUSH : O_START + 2;
                continue;
            }
            if (st == O_START + 2) {
                k->x[2] = op == B_READ1 ? k->x[0] : i64(b->size);
                k->x[3] = O_START + 3;
                k->i    = S_RAWREAD;
                continue;
            }
            Value got = k->s[6];
            if (is_none(got) || static_cast<BytesObj *>(got.obj())->len == 0) {
                Value e = bytes_new(Str());
                return e.is_nil() ? R::Err : cont_done(k, e);
            }
            if (op == B_READ1)
                return cont_done(k, got);
            b = buf_of(k->s[1]);
            reset_read(b);
            if (!put_bytes(b->rbuf, static_cast<BytesObj *>(got.obj())->str()))
                return oom();
            return cont_done(k, got);
        }

        case B_READINTO:
        case B_READINTO1: {
            // s[2] the buffer; x[1] written; x[0] where the last fill of
            // the buffer ended, or -1 for a read straight into s[2].
            bool one = op == B_READINTO1;
            u8 *p    = nullptr;
            usize n  = 0;
            if (!io_writable_span(k->s[2], p, n))
                return R::Err;
            if (st == O_START + 1) {
                usize u = unread(b);
                if (u > n)
                    u = n;
                for (usize i = 0; i < u; i++)
                    p[i] = b->rbuf[b->rpos + i];
                k->x[1] = i64(u);
                k->x[0] = 0;
                reset_read(b);
                k->x[3] = O_START + 2;
                k->i    = b->writable ? S_FLUSH : O_START + 2;
                continue;
            }
            if (st == O_START + 2) {
                i64 remaining = i64(n) - k->x[1];
                if (remaining <= 0)
                    return cont_done(k, int_from_i64(k->x[1]));
                if (remaining > i64(b->size)) {
                    k->x[2] = remaining;
                    k->x[0] = -1;
                } else if (!(one && k->x[1])) {
                    if (k->x[0] < 0)
                        k->x[0] = 0;
                    k->x[2] = i64(b->size) - k->x[0];
                    if (k->x[2] <= 0)
                        return cont_done(k, int_from_i64(k->x[1]));
                } else {
                    return cont_done(k, int_from_i64(k->x[1]));
                }
                k->x[3] = O_START + 3;
                k->i    = S_RAWREAD;
                continue;
            }
            Value got = k->s[6];
            if (is_none(got))
                return k->x[1] ? cont_done(k, int_from_i64(k->x[1])) : cont_done(k, got);
            Str data = static_cast<BytesObj *>(got.obj())->str();
            if (data.empty())
                return cont_done(k, int_from_i64(k->x[1]));
            b             = buf_of(k->s[1]);
            usize at      = usize(k->x[1]);
            usize room    = n > at ? n - at : 0;
            usize take    = data.size() < room ? data.size() : room;
            for (usize i = 0; i < take; i++)
                p[at + i] = u8(data[i]);
            k->x[1] += i64(take);
            if (k->x[0] >= 0 && take < data.size()) {
                reset_read(b);
                if (!put_bytes(b->rbuf, data.substr(take)))
                    return oom();
            }
            if (k->x[0] >= 0)
                k->x[0] += i64(data.size());
            if (one && k->x[0] < 0)
                return cont_done(k, int_from_i64(k->x[1]));
            k->i = O_START + 2;
            continue;
        }

        case B_READLINE:
        case B_NEXT: {
            // x[0] the limit, s[3] the line so far.
            if (st == O_START + 1) {
                Value acc = bytearray_new(Str());
                if (acc.is_nil())
                    return R::Err;
                k->s[3] = acc;
                usize u = unread(b);
                if (k->x[0] >= 0 && i64(u) > k->x[0])
                    u = usize(k->x[0]);
                if (!acc_append(k, Str(reinterpret_cast<const char *>(b->rbuf.data() + b->rpos),
                                       u)))
                    return R::Err;
                b->rpos += u;
                if (k->x[0] >= 0)
                    k->x[0] -= i64(u);
                k->x[3] = O_START + 2;
                k->i    = b->writable ? S_FLUSH : O_START + 2;
                continue;
            }
            if (st == O_START + 2) {
                // The flush may have left a read buffer; it goes, the raw
                // stream having been put back.
                if (unread(b)) {
                    k->x[3] = O_START + 3;
                    k->i    = S_REWIND;
                    continue;
                }
                k->i = O_START + 3;
                continue;
            }
            if (st == O_START + 3) {
                reset_read(b);
                k->x[2] = i64(b->size);
                k->x[3] = O_START + 4;
                k->i    = S_RAWREAD;
                continue;
            }
            Value got = k->s[6];
            bool eof  = is_none(got) || static_cast<BytesObj *>(got.obj())->len == 0;
            if (!eof) {
                Str data = static_cast<BytesObj *>(got.obj())->str();
                b        = buf_of(k->s[1]);
                reset_read(b);
                if (!put_bytes(b->rbuf, data))
                    return oom();
                usize n = data.size();
                if (k->x[0] >= 0 && i64(n) > k->x[0])
                    n = usize(k->x[0]);
                usize cut = n;
                bool nl   = false;
                for (usize i = 0; i < n; i++)
                    if (data[i] == '\n') {
                        cut = i + 1;
                        nl  = true;
                        break;
                    }
                if (!acc_append(k, data.substr(0, cut)))
                    return R::Err;
                b->rpos = cut;
                if (!nl && !(k->x[0] >= 0 && i64(n) == k->x[0])) {
                    if (k->x[0] >= 0)
                        k->x[0] -= i64(n);
                    k->i = O_START + 3;
                    continue;
                }
            }
            if (op == B_NEXT && array_of(k->s[3])->data.empty()) {
                Value e = exc_new(exc_find("StopIteration"), Value());
                return e.is_nil() ? R::Err : err_set_value(e);
            }
            return finish_bytes(k);
        }

        case B_WRITE: {
            // s[2] the data (bytes); x[1] written.
            Str data = static_cast<BytesObj *>(k->s[2].obj())->str();
            if (st == O_START + 1) {
                if (b->readable && unread(b)) {
                    k->x[3] = O_START + 2;
                    k->i    = S_REWIND;
                    continue;
                }
                k->i = O_START + 2;
                continue;
            }
            if (st == O_START + 2) {
                if (!b->through && b->wbuf.size() + data.size() <= b->size) {
                    if (!put_bytes(b->wbuf, data))
                        return oom();
                    return cont_done(k, int_from_i64(i64(data.size())));
                }
                k->x[3] = O_START + 3;
                k->i    = S_FLUSH;
                continue;
            }
            if (st == O_START + 3 || st == O_START + 4) {
                if (st == O_START + 4) {
                    i64 n = 0;
                    if (is_none(in))
                        return blocking("write could not complete without blocking",
                                        k->x[1]);
                    if (!as_int_arg(in, n))
                        return err_set2("TypeError", "raw write() should return an integer",
                                        type_name(in));
                    usize left = data.size() - usize(k->x[1]);
                    if (n < 0 || usize(n) > left) {
                        Buf<160> m;
                        return err_set("OSError", raw_len_error("write", n, left, m));
                    }
                    k->x[1] += n;
                    if (b->abs >= 0)
                        b->abs += n;
                }
                usize remaining = data.size() - usize(k->x[1]);
                if (remaining > (b->through ? 0 : b->size)) {
                    Value rest = bytes_new(data.substr(usize(k->x[1])));
                    if (rest.is_nil())
                        return R::Err;
                    k->i = O_START + 4;
                    return cont_method(k, b->raw, "write", 1, rest);
                }
                b = buf_of(k->s[1]);
                if (!put_bytes(b->wbuf, data.substr(usize(k->x[1]))))
                    return oom();
                return cont_done(k, int_from_i64(i64(data.size())));
            }
            break;
        }

        case B_FLUSH:
            if (st == O_START + 1) {
                if (!b->writable) {
                    k->i = O_START + 3;
                    return cont_method(k, b->raw, "flush");
                }
                k->x[3] = O_START + 2;
                k->i    = S_FLUSH;
                continue;
            }
            if (st == O_START + 2) {
                k->x[3] = O_START + 3;
                k->i    = S_REWIND;
                continue;
            }
            return cont_done(k, st == O_START + 3 && !b->writable ? in : value_none());

        case B_CLOSE:
            // s[7] what flush raised.
            if (st == O_START + 1) {
                int c = raw_closed_now(b);
                if (c == 1)
                    return cont_done(k, value_none());
                if (c < 0) {
                    k->i = O_START + 2;
                    return cont_attr(k, b->raw, "closed");
                }
                in = value_bool(false);
                st = O_START + 2;
            }
            if (st == O_START + 2) {
                if (py_truth(in))
                    return cont_done(k, value_none());
                k->i        = O_START + 3;
                k->catching = CATCH_ANY;
                return cont_method(k, k->s[0], "flush");
            }
            if (st == O_START + 3) {
                if (in.is_nil()) {
                    k->s[7]   = k->caught;
                    k->caught = Value();
                }
                k->i        = O_START + 4;
                k->catching = CATCH_ANY;
                return cont_method(k, b->raw, "close");
            }
            {
                k->catching = CATCH_NONE;
                Root late{ in.is_nil() ? k->caught : Value() };
                k->caught = Value();
                b         = buf_of(k->s[1]);
                b->wbuf.clear();
                reset_read(b);
                io_untrack(b);
                if (!late.v.is_nil()) {
                    if (!k->s[7].is_nil() && is_exc(late.v))
                        static_cast<ExcObj *>(late.v.obj())->context = k->s[7];
                    return err_set_value(late.v);
                }
                if (!k->s[7].is_nil())
                    return err_set_value(k->s[7]);
                return cont_done(k, value_none());
            }

        case B_DETACH:
            if (st == O_START + 1) {
                k->i = O_START + 2;
                return cont_method(k, k->s[0], "flush");
            }
            {
                b             = buf_of(k->s[1]);
                Value raw     = b->raw;
                b->raw        = Value();
                b->detached   = true;
                b->ok         = false;
                io_untrack(b);
                return cont_done(k, raw);
            }

        case B_SEEK: {
            // x[0] target, x[1] whence.
            if (st == O_START + 1) {
                if (raw_plain(b)) {
                    FileFacts f = file_facts(b->raw);
                    if (f.seekable == 0)
                        return io_unsupported("File or stream is not seekable.");
                    if (f.seekable == 1) {
                        k->i = O_START + 3;
                        continue;
                    }
                }
                k->i = O_START + 2;
                return cont_method(k, b->raw, "seekable");
            }
            if (st == O_START + 2) {
                if (!is_true(in))
                    return io_unsupported("File or stream is not seekable.");
                k->i = O_START + 3;
                continue;
            }
            if (st == O_START + 3) {
                // Inside what is buffered, no call at all.
                i64 avail = i64(unread(b));
                if ((k->x[1] == 0 || k->x[1] == 1) && b->readable && avail > 0) {
                    if (b->abs < 0) {
                        k->i = O_START + 4;
                        return cont_method(k, b->raw, "tell");
                    }
                    k->i = O_START + 5;
                    continue;
                }
                k->i = O_START + 6;
                continue;
            }
            if (st == O_START + 4) {
                i64 at = -1;
                if (!as_int_arg(in, at))
                    return err_set2("TypeError", "tell() should return an integer",
                                    type_name(in));
                b->abs = at;
                k->i   = O_START + 5;
                continue;
            }
            if (st == O_START + 5) {
                i64 avail   = i64(unread(b));
                i64 current = b->abs;
                i64 offset  = k->x[1] == 0 ? k->x[0] - (current - avail) : k->x[0];
                if (offset >= -i64(b->rpos) && offset <= avail) {
                    b->rpos = usize(i64(b->rpos) + offset);
                    return cont_done(k, int_from_i64(current - avail + offset));
                }
                k->i = O_START + 6;
                continue;
            }
            if (st == O_START + 6) {
                k->x[3] = O_START + 7;
                k->i    = S_FLUSH;
                continue;
            }
            if (st == O_START + 7) {
                i64 target = k->x[0];
                if (k->x[1] == 1)
                    target -= i64(unread(b));
                Value t = int_from_i64(target);
                if (t.is_nil())
                    return R::Err;
                k->i = O_START + 8;
                return cont_method(k, b->raw, "seek", 2, t, Value::of_int(i32(k->x[1])));
            }
            i64 at = 0;
            if (!as_int_arg(in, at))
                return err_set2("TypeError", "seek() should return an integer", type_name(in));
            if (at < 0) {
                char tmp[24];
                Buf<96> m;
                m.put("Raw stream returned invalid position ").put(int_text(tmp, sizeof tmp, at));
                return err_set("OSError", m.str());
            }
            b      = buf_of(k->s[1]);
            b->abs = at;
            if (b->readable)
                reset_read(b);
            return cont_done(k, in);
        }

        case B_TELL:
            if (st == O_START + 1) {
                k->i = O_START + 2;
                return cont_method(k, b->raw, "tell");
            }
            {
                i64 at = 0;
                if (!as_int_arg(in, at))
                    return err_set2("TypeError", "tell() should return an integer",
                                    type_name(in));
                if (at < 0) {
                    char tmp[24];
                    Buf<96> m;
                    m.put("Raw stream returned invalid position ");
                    m.put(int_text(tmp, sizeof tmp, at));
                    return err_set("OSError", m.str());
                }
                b      = buf_of(k->s[1]);
                b->abs = at;
                i64 pos = at - i64(unread(b)) + i64(b->wbuf.size());
                return cont_done(k, int_from_i64(pos < 0 ? 0 : pos));
            }

        case B_TRUNCATE:
            // s[2] the position, or None.
            if (st == O_START + 1) {
                if (!b->writable)
                    return io_unsupported("truncate");
                k->x[3] = O_START + 2;
                k->i    = S_FLUSH;
                continue;
            }
            if (st == O_START + 2) {
                k->x[3] = O_START + 3;
                k->i    = S_REWIND;
                continue;
            }
            if (st == O_START + 3) {
                k->i = O_START + 4;
                return cont_method(k, b->raw, "truncate", 1, k->s[2]);
            }
            if (st == O_START + 4) {
                k->s[3]     = in;
                k->i        = O_START + 5;
                k->catching = CATCH_ANY;
                return cont_method(k, b->raw, "tell");
            }
            k->catching = CATCH_NONE;
            k->caught   = Value();
            {
                i64 at = -1;
                if (!in.is_nil())
                    as_int_arg(in, at);
                b->abs = at;
            }
            return cont_done(k, k->s[3]);

        case B_INIT:
            // x[0] which checks are owed: 1 readable, 2 writable, 4 seekable.
            if (st == O_START + 1) {
                if (k->x[0] & 4) {
                    k->x[0] &= ~4;
                    k->i = O_START + 2;
                    return cont_method(k, b->raw, "seekable");
                }
                if (k->x[0] & 1) {
                    k->x[0] &= ~1;
                    k->i = O_START + 3;
                    return cont_method(k, b->raw, "readable");
                }
                if (k->x[0] & 2) {
                    k->x[0] &= ~2;
                    k->i = O_START + 4;
                    return cont_method(k, b->raw, "writable");
                }
                b->ok = true;
                io_track(b);
                return cont_done(k, k->s[0]);
            }
            if (!is_true(in))
                return io_unsupported(st == O_START + 2   ? Str("File or stream is not seekable.")
                                      : st == O_START + 3 ? Str("File or stream is not readable.")
                                                          : Str("File or stream is not writable."));
            k->i = O_START + 1;
            continue;

        default:
            break;
        }
        return err_set("SystemError", "buffered: lost its place");
    }
}

// A subroutine is done: back to where the op said.
R ret(ContObj *k, Value in)
{
    k->i = u32(k->x[3]);
    return buf_step(k, in);
}

R start(Value self, u32 op, Value &out, Value arg = Value(), i64 n = 0, i64 m = 0)
{
    Root rs{ self }, ra{ arg };
    Root kv{ cont_new(buf_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = rs.v;
    k->s[1]    = method_self(rs.v);
    k->s[2]    = ra.v;
    k->x[0]    = n;
    k->x[1]    = m;
    k->j       = op;
    out        = kv.v;
    return R::Ok;
}

BufObj *self_buf(const CallArgs &a, Str who, bool ready = true)
{
    IoObj *io = io_self(a, IO_BUFFERED, who);
    if (!io)
        return nullptr;
    BufObj *b = static_cast<BufObj *>(io);
    if (ready && !b->ok) {
        not_ready(b);
        return nullptr;
    }
    return b;
}

// ------------------------------------------------------------- the methods

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

bool closed_now_or_ask(BufObj *b, bool &closed)
{
    int c = raw_closed_now(b);
    closed = c == 1;
    return c >= 0;
}

R bm_read(const CallArgs &a, Value &out)
{
    BufObj *b = self_buf(a, "read");
    if (!b || !meth_args(a, "read", 0, 1))
        return R::Err;
    if (!b->readable)
        return io_unsupported("read");
    i64 n = -1;
    if (a.nargs > 1 && !size_arg(a.args[1], n))
        return R::Err;
    if (n < -1)
        return err_set("ValueError", "read length must be non-negative or -1");
    if (n >= 0 && usize(n) <= unread(b)) {
        out = bytes_of(b->rbuf, b->rpos, usize(n));
        b->rpos += usize(n);
        return out.is_nil() ? R::Err : R::Ok;
    }
    return start(a.args[0], n < 0 ? B_READALL : B_READ, out, Value(), n);
}

R bm_read1(const CallArgs &a, Value &out)
{
    BufObj *b = self_buf(a, "read1");
    if (!b || !meth_args(a, "read1", 0, 1))
        return R::Err;
    if (!b->readable)
        return io_unsupported("read1");
    i64 n = -1;
    if (a.nargs > 1 && !size_arg(a.args[1], n))
        return R::Err;
    if (n < 0)
        n = i64(b->size);
    bool closed = false;
    if (unread(b) == 0 && closed_now_or_ask(b, closed) && closed)
        return io_closed_err("read of closed file");
    if (n == 0) {
        out = bytes_new(Str());
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (unread(b)) {
        usize take = usize(n) < unread(b) ? usize(n) : unread(b);
        out        = bytes_of(b->rbuf, b->rpos, take);
        b->rpos += take;
        return out.is_nil() ? R::Err : R::Ok;
    }
    return start(a.args[0], B_READ1, out, Value(), n);
}

R bm_peek(const CallArgs &a, Value &out)
{
    BufObj *b = self_buf(a, "peek");
    if (!b || !meth_args(a, "peek", 0, 1))
        return R::Err;
    if (!b->readable)
        return io_unsupported("peek");
    if (unread(b)) {
        out = bytes_of(b->rbuf, b->rpos, unread(b));
        return out.is_nil() ? R::Err : R::Ok;
    }
    return start(a.args[0], B_PEEK, out);
}

R readinto_start(const CallArgs &a, Value &out, bool one)
{
    Str who   = one ? Str("readinto1") : Str("readinto");
    BufObj *b = self_buf(a, who);
    if (!b || !meth_args(a, who, 1, 1))
        return R::Err;
    if (!b->readable)
        return io_unsupported(who);
    u8 *p   = nullptr;
    usize n = 0;
    if (!io_writable_span(a.args[1], p, n))
        return R::Err;
    if (unread(b) >= n) {
        for (usize i = 0; i < n; i++)
            p[i] = b->rbuf[b->rpos + i];
        b->rpos += n;
        out = int_from_i64(i64(n));
        return out.is_nil() ? R::Err : R::Ok;
    }
    return start(a.args[0], one ? B_READINTO1 : B_READINTO, out, a.args[1]);
}

R bm_readinto(const CallArgs &a, Value &out)
{
    return readinto_start(a, out, false);
}

R bm_readinto1(const CallArgs &a, Value &out)
{
    return readinto_start(a, out, true);
}

// A whole line out of what is buffered, when there is one: Ok, or NotImpl.
R line_now(BufObj *b, i64 limit, Value &out)
{
    usize n = unread(b);
    if (limit >= 0 && i64(n) > limit)
        n = usize(limit);
    const u8 *at = b->rbuf.data() + b->rpos;
    for (usize i = 0; i < n; i++)
        if (at[i] == '\n') {
            out = bytes_of(b->rbuf, b->rpos, i + 1);
            b->rpos += i + 1;
            return out.is_nil() ? R::Err : R::Ok;
        }
    if (limit >= 0 && i64(n) == limit) {
        out = bytes_of(b->rbuf, b->rpos, n);
        b->rpos += n;
        return out.is_nil() ? R::Err : R::Ok;
    }
    return R::NotImpl;
}

R bm_readline(const CallArgs &a, Value &out)
{
    BufObj *b = self_buf(a, "readline");
    if (!b || !meth_args(a, "readline", 0, 1))
        return R::Err;
    i64 limit = -1;
    if (a.nargs > 1 && !size_arg(a.args[1], limit))
        return R::Err;
    R r = line_now(b, limit, out);
    if (r != R::NotImpl)
        return r;
    return start(a.args[0], B_READLINE, out, Value(), limit);
}

R bm_next(const CallArgs &a, Value &out)
{
    BufObj *b = self_buf(a, "__next__");
    if (!b || !meth_args(a, "__next__", 0, 0))
        return R::Err;
    R r = line_now(b, -1, out);
    if (r != R::NotImpl)
        return r;
    return start(a.args[0], B_NEXT, out, Value(), -1);
}

R bm_write(const CallArgs &a, Value &out)
{
    BufObj *b = self_buf(a, "write");
    if (!b || !meth_args(a, "write", 1, 1))
        return R::Err;
    if (!b->writable)
        return io_unsupported("write");
    Str data;
    if (!bytes_like(a.args[1], data))
        return err_not("a bytes-like object is required", a.args[1], true);
    bool closed = false;
    if (closed_now_or_ask(b, closed)) {
        if (closed)
            return io_closed_err("write to closed file");
        // Fits, and nothing to take back from the read side: no call.
        if (!(b->readable && unread(b))) {
            if (b->through && file_is_sink(b->raw))
                return file_write(b->raw, data, out);
            if (!b->through && b->wbuf.size() + data.size() <= b->size) {
                if (!put_bytes(b->wbuf, data))
                    return oom();
                out = int_from_i64(i64(data.size()));
                return R::Ok;
            }
        }
    }
    Root copy{ bytes_new(data) };
    if (copy.v.is_nil())
        return R::Err;
    return start(a.args[0], B_WRITE, out, copy.v);
}

R bm_flush(const CallArgs &a, Value &out)
{
    BufObj *b = self_buf(a, "flush");
    if (!b || !meth_args(a, "flush", 0, 0))
        return R::Err;
    bool closed = false;
    if (b->writable && b->wbuf.empty() && !(b->readable && unread(b)) &&
        closed_now_or_ask(b, closed)) {
        if (closed)
            return io_closed_err("flush of closed file");
        out = value_none();
        return R::Ok;
    }
    return start(a.args[0], B_FLUSH, out);
}

R bm_close(const CallArgs &a, Value &out)
{
    BufObj *b = self_buf(a, "close");
    if (!b || !meth_args(a, "close", 0, 0))
        return R::Err;
    return start(a.args[0], B_CLOSE, out);
}

R bm_detach(const CallArgs &a, Value &out)
{
    BufObj *b = self_buf(a, "detach");
    if (!b || !meth_args(a, "detach", 0, 0))
        return R::Err;
    return start(a.args[0], B_DETACH, out);
}

R bm_seek(const CallArgs &a, Value &out)
{
    BufObj *b = self_buf(a, "seek");
    if (!b || !meth_args(a, "seek", 1, 2))
        return R::Err;
    i64 target = 0, whence = 0;
    if (is_float(a.args[1]) || !as_int_arg(a.args[1], target))
        return err_set2("TypeError", "an integer is required", type_name(a.args[1]));
    if (a.nargs > 2 && !as_int_arg(a.args[2], whence))
        return err_set2("TypeError", "an integer is required", type_name(a.args[2]));
    if (whence < 0 || whence > 2) {
        char tmp[24];
        Buf<64> m;
        m.put("whence value ").put(int_text(tmp, sizeof tmp, whence)).put(" unsupported");
        return err_set("ValueError", m.str());
    }
    return start(a.args[0], B_SEEK, out, Value(), target, whence);
}

R bm_tell(const CallArgs &a, Value &out)
{
    BufObj *b = self_buf(a, "tell");
    if (!b || !meth_args(a, "tell", 0, 0))
        return R::Err;
    return start(a.args[0], B_TELL, out);
}

R bm_truncate(const CallArgs &a, Value &out)
{
    BufObj *b = self_buf(a, "truncate");
    if (!b || !meth_args(a, "truncate", 0, 1))
        return R::Err;
    Value pos = a.nargs > 1 ? a.args[1] : value_none();
    return start(a.args[0], B_TRUNCATE, out, pos);
}

// seekable, readable, writable, fileno and isatty: the raw stream's.
R forward(const CallArgs &a, Value &out, Str who)
{
    BufObj *b = self_buf(a, who);
    if (!b || !meth_args(a, who, 0, 0))
        return R::Err;
    if (raw_plain(b)) {
        FileFacts f = file_facts(b->raw);
        if (who == "readable" && !f.closed) {
            out = value_bool(f.readable);
            return R::Ok;
        }
        if (who == "writable" && !f.closed) {
            out = value_bool(f.writable);
            return R::Ok;
        }
    }
    Root rr{ b->raw };
    Root kv{ cont_new([](ContObj *k, Value in) -> R {
        if (k->i++ == 0)
            return cont_method(k, k->s[0], str_of(k->s[1])->str());
        return cont_done(k, in);
    }) };
    if (kv.v.is_nil())
        return R::Err;
    StrObj *name = str_intern(who);
    if (!name)
        return oom();
    cont_of(kv.v)->s[0] = rr.v;
    cont_of(kv.v)->s[1] = obj_value(name);
    out                 = kv.v;
    return R::Ok;
}

R bm_seekable(const CallArgs &a, Value &out)
{
    return forward(a, out, "seekable");
}

R bm_readable(const CallArgs &a, Value &out)
{
    return forward(a, out, "readable");
}

R bm_writable(const CallArgs &a, Value &out)
{
    return forward(a, out, "writable");
}

R bm_fileno(const CallArgs &a, Value &out)
{
    return forward(a, out, "fileno");
}

R bm_isatty(const CallArgs &a, Value &out)
{
    return forward(a, out, "isatty");
}

R bm_getstate(const CallArgs &a, Value &out)
{
    (void)out;
    Buf<96> b;
    b.put("cannot pickle '").put(a.nargs ? type_label(a.args[0]) : Str("?")).put("' instances");
    return err_set("TypeError", b.str());
}

R bm_sizeof(const CallArgs &a, Value &out)
{
    BufObj *b = self_buf(a, "__sizeof__", false);
    if (!b)
        return R::Err;
    out = int_from_i64(i64(sizeof(BufObj) + b->size));
    return R::Ok;
}

R bm_dealloc_warn(const CallArgs &a, Value &out)
{
    if (!self_buf(a, "_dealloc_warn", false))
        return R::Err;
    out = value_none();
    return R::Ok;
}

// __init__(raw, buffer_size=DEFAULT_BUFFER_SIZE), and the three constructors.
R buf_init(Value self, const CallArgs &a, bool readable, bool writable, Str who, Value &out)
{
    constexpr Str NAMES[] = { "raw", "buffer_size" };
    Value v[2];
    if (!fn_take(a, who, NAMES, 1, v))
        return R::Err;
    i64 size = DEFAULT_BUFFER_SIZE;
    if (!v[1].is_nil() && !as_int_arg(v[1], size))
        return err_set2("TypeError", "an integer is required", type_name(v[1]));
    if (size <= 0)
        return err_set("ValueError", "buffer size must be strictly positive");
    Root rs{ self }, raw{ v[0] };
    BufObj *b   = buf_of(method_self(rs.v));
    b->raw      = raw.v;
    b->size     = usize(size);
    b->readable = readable;
    b->writable = writable;
    b->detached = false;
    b->ok       = false;
    b->abs      = -1;
    b->closed   = false;
    reset_read(b);
    b->wbuf.clear();
    u32 owed = (readable ? 1 : 0) | (writable ? 2 : 0) | (readable && writable ? 4 : 0);
    if (raw_plain(b)) {
        FileFacts f = file_facts(raw.v);
        if (f.closed)
            return io_closed_err();
        if (readable && !f.readable)
            return io_unsupported("File or stream is not readable.");
        if (writable && !f.writable)
            return io_unsupported("File or stream is not writable.");
        owed &= 4;
        if (f.seekable == 0 && (owed & 4))
            return io_unsupported("File or stream is not seekable.");
        if (f.seekable == 1)
            owed = 0;
        if (!owed) {
            b->ok = true;
            io_track(b);
            out = rs.v;
            return R::Ok;
        }
    }
    if (start(rs.v, B_INIT, out) != R::Ok)
        return R::Err;
    ContObj *k = cont_of(out);
    k->x[0]    = owed;
    k->i       = O_START + 1;
    return R::Ok;
}

Value buf_alloc(const Type *t)
{
    BufObj *b = static_cast<BufObj *>(obj_alloc(t, sizeof(BufObj)));
    if (!b)
        return oom(), Value();
    b->dict       = Value();
    b->kind       = IO_BUFFERED;
    b->closed     = false;
    b->inside     = false;
    b->finalizing = false;
    b->raw        = Value();
    new (&b->rbuf) Vec<u8>();
    new (&b->wbuf) Vec<u8>();
    b->rpos     = 0;
    b->size     = usize(DEFAULT_BUFFER_SIZE);
    b->abs      = -1;
    b->readable = b->writable = false;
    b->ok = b->detached = false;
    b->through          = false;
    return obj_value(b);
}

R make(const Type *t, const CallArgs &a, bool rd, bool wr, Str who, Value &out)
{
    Root self{ buf_alloc(t) };
    if (self.v.is_nil())
        return R::Err;
    if (!a.nargs && !a.nkw) {
        out = self.v;
        return R::Ok;
    }
    return buf_init(self.v, a, rd, wr, who, out);
}

R bm_init(const CallArgs &a, Value &out)
{
    BufObj *b = self_buf(a, "__init__", false);
    if (!b)
        return R::Err;
    const Type *t = b->type;
    bool rd       = t != &bufwriter_type;
    bool wr       = t != &bufreader_type;
    CallArgs rest = a;
    rest.args++;
    rest.nargs--;
    Root r;
    if (buf_init(a.args[0], rest, rd, wr, "__init__", r.v) != R::Ok)
        return R::Err;
    if (!is_cont(r.v)) {
        out = value_none();
        return R::Ok;
    }
    Root wrap{ cont_new([](ContObj *k, Value) -> R {
        if (k->i++ == 0)
            return cont_await(k, k->s[0]);
        return cont_done(k, value_none());
    }) };
    if (wrap.v.is_nil())
        return R::Err;
    cont_of(wrap.v)->s[0] = r.v;
    out                   = wrap.v;
    return R::Ok;
}

constexpr Method BUF_METHODS[] = {
    { "__init__", bm_init },
    { "read", bm_read },
    { "read1", bm_read1 },
    { "peek", bm_peek },
    { "readinto", bm_readinto },
    { "readinto1", bm_readinto1 },
    { "readline", bm_readline },
    { "__next__", bm_next },
    { "write", bm_write },
    { "flush", bm_flush },
    { "close", bm_close },
    { "detach", bm_detach },
    { "seek", bm_seek },
    { "tell", bm_tell },
    { "truncate", bm_truncate },
    { "seekable", bm_seekable },
    { "readable", bm_readable },
    { "writable", bm_writable },
    { "fileno", bm_fileno },
    { "isatty", bm_isatty },
    { "__getstate__", bm_getstate },
    { "__reduce_ex__", bm_getstate },
    { "__sizeof__", bm_sizeof },
    { "_dealloc_warn", bm_dealloc_warn },
};

// raw, and closed, name and mode through it. The raw stream's own answer
// may be Python, which the lazy slot hands back as a call.
R buf_getattr(Value v, StrObj *name, Value &out)
{
    BufObj *b = buf_of(v);
    Str n     = name->str();
    if (n == "raw") {
        if (!b->ok && b->detached)
            return not_ready(b);
        out = b->raw.is_nil() ? value_none() : b->raw;
        return R::Ok;
    }
    if (n == "_finalizing") {
        out = value_bool(b->finalizing);
        return R::Ok;
    }
    if (n == "closed" || n == "name" || n == "mode") {
        if (!b->ok)
            return not_ready(b);
        StrObj *nm = name;
        Value got;
        R r = py_getattr(b->raw, nm, got);
        if (r == R::Ok)
            out = got;
        return r;
    }
    return io_dict_get(v, name, out);
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

Got buf_lazy(Value v, StrObj *name, Value &out, Value &args)
{
    BufObj *b = buf_of(v);
    Str n     = name->str();
    if (!(n == "closed" || n == "name" || n == "mode") || !b->ok)
        return Got::Missing;
    if (raw_plain(b))
        return Got::Missing;
    Value probe;
    Got g = py_attr(b->raw, name, probe);
    if (g != Got::Call)
        return Got::Missing;
    Root pr{ probe };
    out = builtin_getattr();
    if (out.is_nil())
        return err_pending() ? Got::Error : Got::Missing;
    TupleObj *t = tuple_new(2);
    if (!t)
        return oom(), Got::Error;
    t->items()[0] = b->raw;
    t->items()[1] = obj_value(name);
    args          = obj_value(t);
    return Got::Call;
}

R buf_repr(Value v, String &out)
{
    BufObj *b = buf_of(v);
    Str label = b->type == &bufreader_type   ? Str("BufferedReader")
                : b->type == &bufwriter_type ? Str("BufferedWriter")
                                             : Str("BufferedRandom");
    if (!out.append("<_io.") || !out.append(label))
        return oom();
    if (b->ok) {
        StrObj *n = str_intern("name");
        Value name;
        if (n && py_attr(b->raw, n, name) == Got::Ok) {
            if (!out.append(" name="))
                return oom();
            if (!repr_enter(v))
                return err_set("RuntimeError", "reentrant call inside repr");
            R r = py_repr(name, out);
            repr_leave();
            if (r != R::Ok)
                return R::Err;
        } else {
            err_clear();
        }
    }
    return out.push('>') ? R::Ok : oom();
}

Value self_iter(Value v)
{
    return v;
}

// ------------------------------------------------------------ BufferedRWPair

struct PairObj : IoObj {
    Value reader, writer;
};

PairObj *pair_of(Value v)
{
    return static_cast<PairObj *>(v.obj());
}

void pair_trace(Obj *o)
{
    io_trace(o);
    gc_mark(static_cast<PairObj *>(o)->reader);
    gc_mark(static_cast<PairObj *>(o)->writer);
}

PairObj *self_pair(const CallArgs &a, Str who)
{
    PairObj *p = static_cast<PairObj *>(io_self(a, IO_PAIR, who));
    if (p && (p->reader.is_nil() || p->writer.is_nil())) {
        err_set("ValueError", "I/O operation on uninitialized object");
        return nullptr;
    }
    return p;
}

// Calls `name` on one side with the call's own arguments.
R pair_forward(const CallArgs &a, Value &out, Str name, bool writer)
{
    PairObj *p = self_pair(a, name);
    if (!p)
        return R::Err;
    Value side = writer ? p->writer : p->reader;
    Root rs{ side };
    TupleObj *rest = tuple_new(a.nargs - 1);
    if (!rest)
        return oom();
    for (u32 i = 1; i < a.nargs; i++)
        rest->items()[i - 1] = a.args[i];
    Root rr{ obj_value(rest) };
    StrObj *nm = str_intern(name);
    if (!nm)
        return oom();
    Root kv{ cont_new([](ContObj *k, Value in) -> R {
        if (k->i++)
            return cont_done(k, in);
        TupleObj *t = static_cast<TupleObj *>(k->s[2].obj());
        return cont_method(k, k->s[0], str_of(k->s[1])->str(), t->len,
                           t->len > 0 ? t->items()[0] : Value(),
                           t->len > 1 ? t->items()[1] : Value());
    }) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = rs.v;
    cont_of(kv.v)->s[1] = obj_value(nm);
    cont_of(kv.v)->s[2] = rr.v;
    out                 = kv.v;
    return R::Ok;
}

R pm_read(const CallArgs &a, Value &out)
{
    return pair_forward(a, out, "read", false);
}

R pm_peek(const CallArgs &a, Value &out)
{
    return pair_forward(a, out, "peek", false);
}

R pm_read1(const CallArgs &a, Value &out)
{
    return pair_forward(a, out, "read1", false);
}

R pm_readinto(const CallArgs &a, Value &out)
{
    return pair_forward(a, out, "readinto", false);
}

R pm_readinto1(const CallArgs &a, Value &out)
{
    return pair_forward(a, out, "readinto1", false);
}

R pm_readable(const CallArgs &a, Value &out)
{
    return pair_forward(a, out, "readable", false);
}

R pm_write(const CallArgs &a, Value &out)
{
    return pair_forward(a, out, "write", true);
}

R pm_flush(const CallArgs &a, Value &out)
{
    return pair_forward(a, out, "flush", true);
}

R pm_writable(const CallArgs &a, Value &out)
{
    return pair_forward(a, out, "writable", true);
}

// s[0] the writer, s[1] the reader, s[2] what the first raised.
R pair_close_step(ContObj *k, Value in)
{
    switch (k->i++) {
    case 0:
        k->catching = CATCH_ANY;
        return cont_method(k, k->s[0], "close");
    case 1:
        if (in.is_nil()) {
            k->s[2]   = k->caught;
            k->caught = Value();
        }
        return cont_method(k, k->s[1], "close");
    default:
        k->catching = CATCH_NONE;
        if (in.is_nil()) {
            Root late{ k->caught };
            k->caught = Value();
            if (!k->s[2].is_nil() && is_exc(late.v))
                static_cast<ExcObj *>(late.v.obj())->context = k->s[2];
            return err_set_value(late.v);
        }
        if (!k->s[2].is_nil())
            return err_set_value(k->s[2]);
        return cont_done(k, value_none());
    }
}

R pm_close(const CallArgs &a, Value &out)
{
    PairObj *p = self_pair(a, "close");
    if (!p)
        return R::Err;
    Root rw{ p->writer }, rr{ p->reader };
    Root kv{ cont_new(pair_close_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = rw.v;
    cont_of(kv.v)->s[1] = rr.v;
    out                 = kv.v;
    return R::Ok;
}

// isatty: the writer's, and the reader's when that is False.
R pair_tty_step(ContObj *k, Value in)
{
    switch (k->i++) {
    case 0:
        return cont_method(k, k->s[0], "isatty");
    case 1:
        if (!is_bool(in) || is_true(in))
            return cont_done(k, in);
        return cont_method(k, k->s[1], "isatty");
    default:
        return cont_done(k, in);
    }
}

R pm_isatty(const CallArgs &a, Value &out)
{
    PairObj *p = self_pair(a, "isatty");
    if (!p)
        return R::Err;
    Root rw{ p->writer }, rr{ p->reader };
    Root kv{ cont_new(pair_tty_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = rw.v;
    cont_of(kv.v)->s[1] = rr.v;
    out                 = kv.v;
    return R::Ok;
}

// __init__(reader, writer, buffer_size=DEFAULT_BUFFER_SIZE): s[0] self,
// s[1] and s[2] the two raw streams, s[3] the size.
R pair_init_step(ContObj *k, Value in)
{
    switch (k->i++) {
    case 0:
        return cont_method(k, k->s[1], "readable");
    case 1:
        if (!is_true(in))
            return io_unsupported("File or stream is not readable.");
        return cont_method(k, k->s[2], "writable");
    case 2: {
        if (!is_true(in))
            return io_unsupported("File or stream is not writable.");
        Value args[2] = { k->s[1], k->s[3] };
        CallArgs ca;
        ca.args  = args;
        ca.nargs = 2;
        Value r;
        if (bufreader_new(ca, r) != R::Ok)
            return R::Err;
        return cont_await(k, r);
    }
    case 3: {
        pair_of(k->s[0])->reader = in;
        Value args[2]            = { k->s[2], k->s[3] };
        CallArgs ca;
        ca.args  = args;
        ca.nargs = 2;
        Value w;
        if (bufwriter_new(ca, w) != R::Ok)
            return R::Err;
        return cont_await(k, w);
    }
    default:
        pair_of(k->s[0])->writer = in;
        io_track(pair_of(k->s[0]));
        return cont_done(k, k->s[4].is_nil() ? k->s[0] : value_none());
    }
}

R pair_init(Value self, const CallArgs &a, Value &out, bool init)
{
    constexpr Str NAMES[] = { "reader", "writer", "buffer_size" };
    Value v[3];
    if (!fn_take(a, "BufferedRWPair", NAMES, 2, v))
        return R::Err;
    i64 size = DEFAULT_BUFFER_SIZE;
    if (!v[2].is_nil() && !as_int_arg(v[2], size))
        return err_set2("TypeError", "an integer is required", type_name(v[2]));
    if (size <= 0)
        return err_set("ValueError", "buffer size must be strictly positive");
    Root rs{ self };
    Root sz{ int_from_i64(size) };
    Root kv{ cont_new(pair_init_step) };
    if (kv.v.is_nil() || sz.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = method_self(rs.v);
    k->s[1]    = v[0];
    k->s[2]    = v[1];
    k->s[3]    = sz.v;
    k->s[4]    = init ? value_none() : Value();
    out        = kv.v;
    return R::Ok;
}

R pm_init(const CallArgs &a, Value &out)
{
    PairObj *p = static_cast<PairObj *>(io_self(a, IO_PAIR, "__init__"));
    if (!p)
        return R::Err;
    CallArgs rest = a;
    rest.args++;
    rest.nargs--;
    return pair_init(a.args[0], rest, out, true);
}

constexpr Method PAIR_METHODS[] = {
    { "__init__", pm_init },   { "read", pm_read },         { "peek", pm_peek },
    { "read1", pm_read1 },     { "readinto", pm_readinto }, { "readinto1", pm_readinto1 },
    { "readable", pm_readable }, { "write", pm_write },     { "flush", pm_flush },
    { "writable", pm_writable }, { "close", pm_close },     { "isatty", pm_isatty },
};

R pair_getattr(Value v, StrObj *name, Value &out)
{
    PairObj *p = pair_of(v);
    if (name->str() == "closed") {
        if (p->writer.is_nil())
            return err_set("RuntimeError", "the BufferedRWPair object is being garbage-collected");
        return py_getattr(p->writer, name, out);
    }
    return io_dict_get(v, name, out);
}

void pair_fini(Obj *o)
{
    io_untrack(o);
}

} // namespace

R buf_setattr(Value v, StrObj *name, Value x)
{
    constexpr Str READONLY[] = { "raw", "closed", "name", "mode" };
    return io_set_attr(v, name, x, READONLY, v.obj()->type->name);
}

constexpr Type bufreader_type{ .name     = "_io.BufferedReader",
                               .trace    = buf_trace,
                               .fini     = buf_fini,
                               .repr     = buf_repr,
                               .iter     = self_iter,
                               .getattr  = buf_getattr,
                               .setattr  = buf_setattr,
                               .lazyattr = buf_lazy,
                               .del      = io_del,
                               .base     = &bufbase_type,
                               .vmnext   = true };

constexpr Type bufwriter_type{ .name     = "_io.BufferedWriter",
                               .trace    = buf_trace,
                               .fini     = buf_fini,
                               .repr     = buf_repr,
                               .iter     = self_iter,
                               .getattr  = buf_getattr,
                               .setattr  = buf_setattr,
                               .lazyattr = buf_lazy,
                               .del      = io_del,
                               .base     = &bufbase_type,
                               .vmnext   = true };

constexpr Type bufrandom_type{ .name     = "_io.BufferedRandom",
                               .trace    = buf_trace,
                               .fini     = buf_fini,
                               .repr     = buf_repr,
                               .iter     = self_iter,
                               .getattr  = buf_getattr,
                               .setattr  = buf_setattr,
                               .lazyattr = buf_lazy,
                               .del      = io_del,
                               .base     = &bufbase_type,
                               .vmnext   = true };

constexpr Type bufpair_type{ .name    = "_io.BufferedRWPair",
                             .trace   = pair_trace,
                             .fini    = pair_fini,
                             .iter    = self_iter,
                             .getattr = pair_getattr,
                             .setattr = io_dict_set,
                             .del     = io_del,
                             .base    = &bufbase_type,
                             .vmnext  = true };

bool buffered_methods()
{
    return method_install(&bufreader_type, BUF_METHODS) &&
           method_install(&bufwriter_type, BUF_METHODS) &&
           method_install(&bufrandom_type, BUF_METHODS) &&
           method_install(&bufpair_type, PAIR_METHODS);
}

R bufreader_new(const CallArgs &a, Value &out)
{
    return make(&bufreader_type, a, true, false, "BufferedReader", out);
}

R bufwriter_new(const CallArgs &a, Value &out)
{
    return make(&bufwriter_type, a, false, true, "BufferedWriter", out);
}

R bufrandom_new(const CallArgs &a, Value &out)
{
    return make(&bufrandom_type, a, true, true, "BufferedRandom", out);
}

R bufpair_new(const CallArgs &a, Value &out)
{
    PairObj *p = static_cast<PairObj *>(obj_alloc(&bufpair_type, sizeof(PairObj)));
    if (!p)
        return oom();
    p->dict       = Value();
    p->kind       = IO_PAIR;
    p->closed     = false;
    p->inside     = false;
    p->finalizing = false;
    p->reader     = Value();
    p->writer     = Value();
    Root rp{ obj_value(p) };
    if (!a.nargs && !a.nkw) {
        out = rp.v;
        return R::Ok;
    }
    return pair_init(rp.v, a, out, false);
}

Value buffered_std(Value raw, bool writable)
{
    Root rr{ raw };
    Root self{ buf_alloc(writable ? &bufwriter_type : &bufreader_type) };
    if (self.v.is_nil())
        return Value();
    BufObj *b   = buf_of(self.v);
    b->raw      = rr.v;
    b->readable = !writable;
    b->writable = writable;
    b->through  = writable;
    b->ok       = true;
    b->size     = usize(writable ? 8192 : DEFAULT_BUFFER_SIZE);
    return self.v;
}

R buffered_put(Value w, Str data)
{
    if (!io_is_plain(w, IO_BUFFERED))
        return R::NotImpl;
    BufObj *b = buf_of(w);
    if (!b->ok || !b->writable || (b->readable && unread(b)))
        return R::NotImpl;
    int c = raw_closed_now(b);
    if (c != 0)
        return R::NotImpl;
    if (b->through) {
        if (!file_is_sink(b->raw))
            return R::NotImpl;
        Value n;
        R r = file_write(b->raw, data, n);
        return r == R::Ok && is_cont(n) ? R::NotImpl : r;
    }
    if (b->wbuf.size() + data.size() > b->size)
        return R::NotImpl;
    return put_bytes(b->wbuf, data) ? R::Ok : oom();
}
