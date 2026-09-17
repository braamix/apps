// BytesIO and StringIO: streams over memory, which never call anything.
//
// CPython's bytesio.c and stringio.c, messages and all -- including the one
// place they differ, StringIO's "closed file" with no full stop. A StringIO
// holds codepoints, so a seek is an index; its newline handling is done as the
// text is written, as CPython's is.
#include "io.h"

#include "exc.h"
#include "gc.h"
#include "intern.h"
#include "iter.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "type.h"
#include "ustr.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// ------------------------------------------------------------------ BytesIO

struct BytesIOObj : IoObj {
    Value buf; // a bytearray: getbuffer() hands out a view of it
    usize pos;
};

BytesIOObj *bio_of(Value v)
{
    return static_cast<BytesIOObj *>(v.obj());
}

void bio_trace(Obj *o)
{
    io_trace(o);
    gc_mark(static_cast<BytesIOObj *>(o)->buf);
}

Vec<u8> &bio_data(BytesIOObj *b)
{
    return array_of(b->buf)->data;
}

BytesIOObj *self_bio(const CallArgs &a, Str who, bool open = true)
{
    BytesIOObj *b = static_cast<BytesIOObj *>(io_self(a, IO_BYTES, who));
    if (b && open && b->closed) {
        io_closed_err();
        return nullptr;
    }
    return b;
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

Value bytes_span(const Vec<u8> &v, usize from, usize n)
{
    return bytes_new(Str(reinterpret_cast<const char *>(v.data() + from), n));
}

R bio_write_data(BytesIOObj *b, Str data, usize &n)
{
    n = data.size();
    if (!n)
        return R::Ok;
    Vec<u8> &v = bio_data(b);
    usize end  = b->pos + n;
    if (end > v.size()) {
        usize old = v.size();
        if (!v.resize(end))
            return oom();
        for (usize i = old; i < b->pos; i++)
            v[i] = 0;
    }
    for (usize i = 0; i < n; i++)
        v[b->pos + i] = u8(data[i]);
    b->pos = end;
    return R::Ok;
}

R bio_init_value(Value self, Value initial)
{
    Root rs{ self };
    Root fresh{ bytearray_new(Str()) };
    if (fresh.v.is_nil())
        return R::Err;
    BytesIOObj *b = bio_of(rs.v);
    b->buf        = fresh.v;
    b->pos        = 0;
    b->closed     = false;
    if (initial.is_nil() || is_none(initial))
        return R::Ok;
    Str data;
    if (!bytes_like(initial, data))
        return err_not("a bytes-like object is required", initial, true);
    usize n = 0;
    if (bio_write_data(b, data, n) != R::Ok)
        return R::Err;
    bio_of(rs.v)->pos = 0;
    return R::Ok;
}

R bm_init(const CallArgs &a, Value &out)
{
    if (!self_bio(a, "__init__", false))
        return R::Err;
    constexpr Str NAMES[] = { "initial_bytes" };
    Value v[1];
    if (!meth_take(a, "BytesIO", NAMES, 0, v))
        return R::Err;
    if (bio_init_value(method_self(a.args[0]), v[0]) != R::Ok)
        return R::Err;
    out = value_none();
    return R::Ok;
}

R bm_true(const CallArgs &a, Value &out, Str who)
{
    if (!self_bio(a, who) || !meth_args(a, who, 0, 0))
        return R::Err;
    out = value_bool(true);
    return R::Ok;
}

R bm_readable(const CallArgs &a, Value &out)
{
    return bm_true(a, out, "readable");
}

R bm_writable(const CallArgs &a, Value &out)
{
    return bm_true(a, out, "writable");
}

R bm_seekable(const CallArgs &a, Value &out)
{
    return bm_true(a, out, "seekable");
}

R bm_flush(const CallArgs &a, Value &out)
{
    if (!self_bio(a, "flush") || !meth_args(a, "flush", 0, 0))
        return R::Err;
    out = value_none();
    return R::Ok;
}

R bm_isatty(const CallArgs &a, Value &out)
{
    if (!self_bio(a, "isatty") || !meth_args(a, "isatty", 0, 0))
        return R::Err;
    out = value_bool(false);
    return R::Ok;
}

R bm_tell(const CallArgs &a, Value &out)
{
    BytesIOObj *b = self_bio(a, "tell");
    if (!b || !meth_args(a, "tell", 0, 0))
        return R::Err;
    out = int_from_i64(i64(b->pos));
    return out.is_nil() ? R::Err : R::Ok;
}

R bm_getvalue(const CallArgs &a, Value &out)
{
    BytesIOObj *b = self_bio(a, "getvalue");
    if (!b || !meth_args(a, "getvalue", 0, 0))
        return R::Err;
    out = bytes_span(bio_data(b), 0, bio_data(b).size());
    return out.is_nil() ? R::Err : R::Ok;
}

R bm_getbuffer(const CallArgs &a, Value &out)
{
    BytesIOObj *b = self_bio(a, "getbuffer");
    if (!b || !meth_args(a, "getbuffer", 0, 0))
        return R::Err;
    out = memview_new(b->buf);
    return out.is_nil() ? R::Err : R::Ok;
}

R bm_read(const CallArgs &a, Value &out)
{
    BytesIOObj *b = self_bio(a, "read");
    if (!b || !meth_args(a, "read", 0, 1))
        return R::Err;
    i64 n = -1;
    if (a.nargs > 1 && !size_arg(a.args[1], n))
        return R::Err;
    usize len   = bio_data(b).size();
    usize avail = b->pos < len ? len - b->pos : 0;
    usize take  = n < 0 || usize(n) > avail ? avail : usize(n);
    out         = bytes_span(bio_data(b), b->pos < len ? b->pos : len, take);
    if (out.is_nil())
        return R::Err;
    b->pos += take;
    return R::Ok;
}

R bm_readline_n(BytesIOObj *b, i64 limit, Value &out)
{
    Vec<u8> &v  = bio_data(b);
    usize len   = v.size();
    usize avail = b->pos < len ? len - b->pos : 0;
    usize n     = avail;
    for (usize i = 0; i < avail; i++)
        if (v[b->pos + i] == '\n') {
            n = i + 1;
            break;
        }
    if (limit >= 0 && usize(limit) < n)
        n = usize(limit);
    out = bytes_span(v, b->pos < len ? b->pos : len, n);
    if (out.is_nil())
        return R::Err;
    b->pos += n;
    return R::Ok;
}

R bm_readline(const CallArgs &a, Value &out)
{
    BytesIOObj *b = self_bio(a, "readline");
    if (!b || !meth_args(a, "readline", 0, 1))
        return R::Err;
    i64 n = -1;
    if (a.nargs > 1 && !size_arg(a.args[1], n))
        return R::Err;
    return bm_readline_n(b, n, out);
}

R bm_next(const CallArgs &a, Value &out)
{
    BytesIOObj *b = self_bio(a, "__next__");
    if (!b || !meth_args(a, "__next__", 0, 0))
        return R::Err;
    if (bm_readline_n(b, -1, out) != R::Ok)
        return R::Err;
    if (static_cast<BytesObj *>(out.obj())->len == 0) {
        Value e = exc_new(exc_find("StopIteration"), Value());
        return e.is_nil() ? R::Err : err_set_value(e);
    }
    return R::Ok;
}

R bm_readlines(const CallArgs &a, Value &out)
{
    BytesIOObj *b = self_bio(a, "readlines");
    if (!b || !meth_args(a, "readlines", 0, 1))
        return R::Err;
    i64 hint = -1;
    if (a.nargs > 1 && !size_arg(a.args[1], hint))
        return R::Err;
    ListObj *l = list_new();
    if (!l)
        return oom();
    Root rl{ obj_value(l) };
    Root self{ method_self(a.args[0]) };
    i64 total = 0;
    for (;;) {
        Value line;
        if (bm_readline_n(bio_of(self.v), -1, line) != R::Ok)
            return R::Err;
        usize n = static_cast<BytesObj *>(line.obj())->len;
        if (!n)
            break;
        if (!list_push(list_of(rl.v), line))
            return oom();
        total += i64(n);
        if (hint > 0 && total >= hint)
            break;
    }
    out = rl.v;
    return R::Ok;
}

R bm_readinto(const CallArgs &a, Value &out)
{
    BytesIOObj *b = self_bio(a, "readinto");
    if (!b || !meth_args(a, "readinto", 1, 1))
        return R::Err;
    u8 *p   = nullptr;
    usize n = 0;
    if (!io_writable_span(a.args[1], p, n))
        return R::Err;
    Vec<u8> &v  = bio_data(b);
    usize avail = b->pos < v.size() ? v.size() - b->pos : 0;
    usize take  = n < avail ? n : avail;
    for (usize i = 0; i < take; i++)
        p[i] = v[b->pos + i];
    b->pos += take;
    out = int_from_i64(i64(take));
    return R::Ok;
}

R bm_write(const CallArgs &a, Value &out)
{
    BytesIOObj *b = self_bio(a, "write", false);
    if (!b || !meth_args(a, "write", 1, 1))
        return R::Err;
    Str data;
    if (!bytes_like(a.args[1], data))
        return err_not("a bytes-like object is required", a.args[1], true);
    if (b->closed)
        return io_closed_err();
    // A view of the buffer itself is copied before the buffer grows.
    String copy;
    if (!copy.assign(data))
        return oom();
    usize n = 0;
    if (bio_write_data(b, copy.str(), n) != R::Ok)
        return R::Err;
    out = int_from_i64(i64(n));
    return R::Ok;
}

R bm_writelines(const CallArgs &a, Value &out)
{
    BytesIOObj *b = self_bio(a, "writelines");
    if (!b || !meth_args(a, "writelines", 1, 1))
        return R::Err;
    if (iter_needs_vm(a.args[1]))
        return iter_park(a, 1, bm_writelines, out);
    Root self{ method_self(a.args[0]) };
    Root lines{ obj_value(py_list_of(a.args[1])) };
    if (lines.v.is_nil())
        return R::Err;
    for (Value line : list_of(lines.v)->items) {
        Str data;
        if (!bytes_like(line, data))
            return err_not("a bytes-like object is required", line, true);
        String copy;
        usize n = 0;
        if (!copy.assign(data))
            return oom();
        if (bio_write_data(bio_of(self.v), copy.str(), n) != R::Ok)
            return R::Err;
    }
    out = value_none();
    return R::Ok;
}

R bm_seek(const CallArgs &a, Value &out)
{
    BytesIOObj *b = self_bio(a, "seek");
    if (!b || !meth_args(a, "seek", 1, 2))
        return R::Err;
    i64 pos = 0, whence = 0;
    if (!as_int_arg(a.args[1], pos))
        return err_set2("TypeError", "an integer is required", type_name(a.args[1]));
    if (a.nargs > 2 && !as_int_arg(a.args[2], whence))
        return err_set2("TypeError", "an integer is required", type_name(a.args[2]));
    char tmp[24];
    if (whence == 0) {
        if (pos < 0) {
            Buf<64> m;
            m.put("negative seek value ").put(int_text(tmp, sizeof tmp, pos));
            return err_set("ValueError", m.str());
        }
    } else if (whence == 1) {
        pos += i64(b->pos);
    } else if (whence == 2) {
        pos += i64(bio_data(b).size());
    } else {
        Buf<64> m;
        m.put("invalid whence (").put(int_text(tmp, sizeof tmp, whence));
        m.put(", should be 0, 1 or 2)");
        return err_set("ValueError", m.str());
    }
    if (pos < 0)
        pos = 0;
    b->pos = usize(pos);
    out    = int_from_i64(pos);
    return R::Ok;
}

R bm_truncate(const CallArgs &a, Value &out)
{
    BytesIOObj *b = self_bio(a, "truncate");
    if (!b || !meth_args(a, "truncate", 0, 1))
        return R::Err;
    i64 size = i64(b->pos);
    if (a.nargs > 1 && !is_none(a.args[1]) && !as_int_arg(a.args[1], size))
        return err_set2("TypeError", "an integer is required", type_name(a.args[1]));
    if (size < 0) {
        char tmp[24];
        Buf<64> m;
        m.put("negative size value ").put(int_text(tmp, sizeof tmp, size));
        return err_set("ValueError", m.str());
    }
    if (usize(size) < bio_data(b).size())
        bio_data(b).resize(usize(size));
    out = int_from_i64(size);
    return R::Ok;
}

R bm_close(const CallArgs &a, Value &out)
{
    BytesIOObj *b = self_bio(a, "close", false);
    if (!b || !meth_args(a, "close", 0, 0))
        return R::Err;
    b->closed = true;
    bio_data(b).clear();
    out = value_none();
    return R::Ok;
}

R bm_getstate(const CallArgs &a, Value &out)
{
    BytesIOObj *b = self_bio(a, "__getstate__");
    if (!b || !meth_args(a, "__getstate__", 0, 0))
        return R::Err;
    Root self{ method_self(a.args[0]) };
    Root value{ bytes_span(bio_data(b), 0, bio_data(b).size()) };
    Root pos{ int_from_i64(i64(b->pos)) };
    Root dict{ bio_of(self.v)->dict };
    if (!dict.v.is_nil()) {
        Root copy{ obj_value(dict_new()) };
        if (copy.v.is_nil())
            return oom();
        usize at = 0;
        Value k, x;
        while (table_next(static_cast<DictObj *>(dict.v.obj())->t, at, k, x))
            if (dict_set(static_cast<DictObj *>(copy.v.obj()), k, x) != R::Ok)
                return R::Err;
        dict = copy.v;
    }
    TupleObj *t = value.v.is_nil() || pos.v.is_nil() ? nullptr : tuple_new(3);
    if (!t)
        return err_pending() ? R::Err : oom();
    t->items()[0] = value.v;
    t->items()[1] = pos.v;
    t->items()[2] = dict.v.is_nil() ? value_none() : dict.v;
    out           = obj_value(t);
    return R::Ok;
}

R bm_setstate(const CallArgs &a, Value &out)
{
    BytesIOObj *b = self_bio(a, "__setstate__", false);
    if (!b || !meth_args(a, "__setstate__", 1, 1))
        return R::Err;
    Value st = a.args[1];
    if (!is_tuple(st) || static_cast<TupleObj *>(st.obj())->len < 3) {
        Buf<128> m;
        m.put(type_name(a.args[0])).put(".__setstate__ argument should be 3-tuple, got ");
        m.put(type_name(st));
        return err_set("TypeError", m.str());
    }
    Value *it = static_cast<TupleObj *>(st.obj())->items();
    Root self{ method_self(a.args[0]) }, rst{ st };
    if (bio_init_value(self.v, it[0]) != R::Ok)
        return R::Err;
    it = static_cast<TupleObj *>(rst.v.obj())->items();
    i64 pos = 0;
    if (!as_int_arg(it[1], pos))
        return err_set2("TypeError", "second item of state must be an integer, not",
                        type_name(it[1]));
    if (pos < 0)
        return err_set("ValueError", "position value cannot be negative");
    bio_of(self.v)->pos = usize(pos);
    if (!is_none(it[2])) {
        if (!is_dict(it[2]))
            return err_set2("TypeError", "third item of state should be a dict, got a",
                            type_name(it[2]));
        bio_of(self.v)->dict = it[2];
    }
    out = value_none();
    return R::Ok;
}

R bm_sizeof(const CallArgs &a, Value &out)
{
    BytesIOObj *b = self_bio(a, "__sizeof__", false);
    if (!b)
        return R::Err;
    out = int_from_i64(i64(sizeof(BytesIOObj) + bio_data(b).size()));
    return R::Ok;
}

R bm_read1(const CallArgs &a, Value &out)
{
    return bm_read(a, out);
}

R bm_peek(const CallArgs &a, Value &out)
{
    BytesIOObj *b = self_bio(a, "peek");
    if (!b || !meth_args(a, "peek", 0, 1))
        return R::Err;
    i64 n = 0;
    if (a.nargs > 1 && !size_arg(a.args[1], n))
        return R::Err;
    if (n < 1)
        n = 128 * 1024;
    usize len   = bio_data(b).size();
    usize avail = b->pos < len ? len - b->pos : 0;
    usize take  = usize(n) > avail ? avail : usize(n);
    out         = bytes_span(bio_data(b), b->pos < len ? b->pos : len, take);
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr Method BIO_METHODS[] = {
    { "__init__", bm_init },         { "readable", bm_readable },
    { "writable", bm_writable },     { "seekable", bm_seekable },
    { "flush", bm_flush },           { "isatty", bm_isatty },
    { "tell", bm_tell },             { "getvalue", bm_getvalue },
    { "getbuffer", bm_getbuffer },   { "read", bm_read },
    { "read1", bm_read1 },           { "readline", bm_readline },
    { "readlines", bm_readlines },   { "readinto", bm_readinto },
    { "__next__", bm_next },         { "write", bm_write },
    { "writelines", bm_writelines }, { "seek", bm_seek },
    { "truncate", bm_truncate },     { "close", bm_close },
    { "__getstate__", bm_getstate }, { "__setstate__", bm_setstate },
    { "__sizeof__", bm_sizeof },     { "peek", bm_peek },
};

R bio_getattr(Value v, StrObj *name, Value &out)
{
    if (name->str() == "closed") {
        out = value_bool(bio_of(v)->closed);
        return R::Ok;
    }
    return io_dict_get(v, name, out);
}

R bio_repr(Value v, String &out)
{
    char tmp[24];
    Buf<96> b;
    b.put("<_io.BytesIO object at ").put(addr_text(tmp, sizeof tmp, v.obj())).put('>');
    return out.append(b.str()) ? R::Ok : oom();
}

Value self_iter(Value v)
{
    return v;
}

// ----------------------------------------------------------------- StringIO

struct StringIOObj : IoObj {
    Vec<u32> buf; // codepoints
    usize pos;
    Value readnl;   // the newline argument, or Nil for None
    Str writenl;    // "" for none
    bool ok, readuniversal, readtranslate, has_decoder;
    u8 seennl;
};

StringIOObj *sio_of(Value v)
{
    return static_cast<StringIOObj *>(v.obj());
}

void sio_trace(Obj *o)
{
    io_trace(o);
    gc_mark(static_cast<StringIOObj *>(o)->readnl);
}

void sio_fini(Obj *o)
{
    static_cast<StringIOObj *>(o)->buf.~Vec();
}

R sio_closed_err()
{
    return err_set("ValueError", "I/O operation on closed file");
}

StringIOObj *self_sio(const CallArgs &a, Str who, bool open = true)
{
    StringIOObj *s = static_cast<StringIOObj *>(io_self(a, IO_STRING, who));
    if (!s)
        return nullptr;
    if (!s->ok) {
        err_set("ValueError", "I/O operation on uninitialized object");
        return nullptr;
    }
    if (open && s->closed) {
        sio_closed_err();
        return nullptr;
    }
    return s;
}

Value cps_str(const Vec<u32> &v, usize from, usize n)
{
    String out;
    if (!out.reserve(n))
        return oom(), Value();
    for (usize i = 0; i < n; i++)
        if (!cp_append(out, v[from + i]))
            return oom(), Value();
    StrObj *s = str_raw(out.str());
    return s ? obj_value(s) : (oom(), Value());
}

// What CPython's write_str does to the text before it goes in.
R sio_write_str(StringIOObj *s, Str text)
{
    String decoded;
    // The newline decoder, always final: \r\n and \r become \n under
    // translation, and what is seen is recorded either way.
    if (s->has_decoder) {
        for (usize i = 0; i < text.size(); i++) {
            char c = text[i];
            if (c == '\n') {
                s->seennl |= 1;
            } else if (c == '\r') {
                bool crlf = i + 1 < text.size() && text[i + 1] == '\n';
                s->seennl |= crlf ? 4 : 2;
                if (s->readtranslate) {
                    if (crlf)
                        i++;
                    if (!decoded.push('\n'))
                        return oom();
                    continue;
                }
            }
            if (!decoded.push(c))
                return oom();
        }
        text = decoded.str();
    }
    String translated;
    if (!s->writenl.empty()) {
        for (usize i = 0; i < text.size(); i++)
            if (!(text[i] == '\n' ? translated.append(s->writenl) : translated.push(text[i])))
                return oom();
        text = translated.str();
    }
    // Into codepoints, at pos, padding with NUL past the end.
    usize n = 0;
    for (usize i = 0; i < text.size(); i++)
        n += (u8(text[i]) & 0xc0) != 0x80;
    if (!n)
        return R::Ok;
    usize end = s->pos + n;
    if (end > s->buf.size()) {
        usize old = s->buf.size();
        if (!s->buf.resize(end))
            return oom();
        for (usize i = old; i < end; i++)
            s->buf[i] = 0;
    }
    usize at = 0, k = s->pos;
    while (at < text.size()) {
        u32 cp = 0;
        at += cp_decode(text, at, cp);
        s->buf[k++] = cp;
    }
    s->pos = end;
    return R::Ok;
}

bool newline_ok(Value v)
{
    if (v.is_nil() || is_none(v))
        return true;
    if (!is_str(v)) {
        err_set2("TypeError", "newline must be str or None, not", type_name(v));
        return false;
    }
    Str s = str_of(v)->str();
    if (s == "" || s == "\n" || s == "\r" || s == "\r\n")
        return true;
    String r;
    if (py_repr(v, r) != R::Ok)
        return false;
    Buf<96> m;
    m.put("illegal newline value: ").put(r.str());
    err_set("ValueError", m.str());
    return false;
}

R sio_init(Value self, Value value, Value newline)
{
    Root rs{ self }, rv{ value }, rn{ newline };
    if (!newline_ok(newline))
        return R::Err;
    if (!value.is_nil() && !is_none(value) && !is_str(value)) {
        Buf<96> m;
        m.put("initial_value must be str or None, not ").put(type_name(value));
        return err_set("TypeError", m.str());
    }
    StringIOObj *s = sio_of(rs.v);
    s->ok          = false;
    s->buf.clear();
    Str nl           = is_str(newline) ? str_of(newline)->str() : Str();
    bool none        = newline.is_nil() || is_none(newline);
    s->readnl        = none ? Value() : newline;
    s->writenl       = !none && !nl.empty() && nl[0] == '\r' ? nl : Str();
    s->readuniversal = none || nl.empty();
    s->readtranslate = none;
    s->has_decoder   = s->readuniversal;
    s->seennl        = 0;
    s->pos           = 0;
    if (!value.is_nil() && !is_none(value) && str_of(value)->len) {
        if (sio_write_str(s, str_of(rv.v)->str()) != R::Ok)
            return R::Err;
    }
    s         = sio_of(rs.v);
    s->pos    = 0;
    s->closed = false;
    s->ok     = true;
    return R::Ok;
}

R sm_init(const CallArgs &a, Value &out)
{
    IoObj *io = io_self(a, IO_STRING, "__init__");
    if (!io)
        return R::Err;
    constexpr Str NAMES[] = { "initial_value", "newline" };
    Value v[2];
    if (!meth_take(a, "StringIO", NAMES, 0, v))
        return R::Err;
    Root nl{ v[1].is_nil() ? str_new("\n") : v[1] };
    if (nl.v.is_nil() || sio_init(method_self(a.args[0]), v[0], nl.v) != R::Ok)
        return R::Err;
    out = value_none();
    return R::Ok;
}

R sm_true(const CallArgs &a, Value &out, Str who)
{
    if (!self_sio(a, who) || !meth_args(a, who, 0, 0))
        return R::Err;
    out = value_bool(true);
    return R::Ok;
}

R sm_readable(const CallArgs &a, Value &out)
{
    return sm_true(a, out, "readable");
}

R sm_writable(const CallArgs &a, Value &out)
{
    return sm_true(a, out, "writable");
}

R sm_seekable(const CallArgs &a, Value &out)
{
    return sm_true(a, out, "seekable");
}

R sm_tell(const CallArgs &a, Value &out)
{
    StringIOObj *s = self_sio(a, "tell");
    if (!s || !meth_args(a, "tell", 0, 0))
        return R::Err;
    out = int_from_i64(i64(s->pos));
    return R::Ok;
}

R sm_getvalue(const CallArgs &a, Value &out)
{
    StringIOObj *s = self_sio(a, "getvalue");
    if (!s || !meth_args(a, "getvalue", 0, 0))
        return R::Err;
    out = cps_str(s->buf, 0, s->buf.size());
    return out.is_nil() ? R::Err : R::Ok;
}

R sm_read(const CallArgs &a, Value &out)
{
    StringIOObj *s = self_sio(a, "read");
    if (!s || !meth_args(a, "read", 0, 1))
        return R::Err;
    i64 n = -1;
    if (a.nargs > 1 && !size_arg(a.args[1], n))
        return R::Err;
    i64 avail = i64(s->buf.size()) - i64(s->pos);
    if (avail < 0)
        avail = 0;
    if (n < 0 || n > avail)
        n = avail;
    out = cps_str(s->buf, usize(avail ? s->pos : s->buf.size()), usize(n));
    if (out.is_nil())
        return R::Err;
    s->pos += usize(n);
    return R::Ok;
}

// CPython's _PyIO_find_line_ending over codepoints: the length through the
// line ending, or -1.
i64 find_ending(const StringIOObj *s, const u32 *p, usize n)
{
    if (s->readtranslate) {
        for (usize i = 0; i < n; i++)
            if (p[i] == '\n')
                return i64(i + 1);
        return -1;
    }
    if (s->readuniversal) {
        for (usize i = 0; i < n; i++) {
            if (p[i] == '\n')
                return i64(i + 1);
            if (p[i] == '\r')
                return i + 1 < n && p[i + 1] == '\n' ? i64(i + 2) : i64(i + 1);
        }
        return -1;
    }
    Str nl = str_of(s->readnl)->str();
    for (usize i = 0; i + nl.size() <= n; i++) {
        bool hit = true;
        for (usize k = 0; k < nl.size() && hit; k++)
            hit = p[i + k] == u32(u8(nl[k]));
        if (hit)
            return i64(i + nl.size());
    }
    return -1;
}

R sio_readline(StringIOObj *s, i64 limit, Value &out)
{
    if (s->pos >= s->buf.size()) {
        out = str_new(Str());
        return out.is_nil() ? R::Err : R::Ok;
    }
    usize avail = s->buf.size() - s->pos;
    if (limit < 0 || usize(limit) > avail)
        limit = i64(avail);
    i64 len = find_ending(s, s->buf.data() + s->pos, usize(limit));
    if (len < 0)
        len = limit;
    out = cps_str(s->buf, s->pos, usize(len));
    if (out.is_nil())
        return R::Err;
    s->pos += usize(len);
    return R::Ok;
}

R sm_readline(const CallArgs &a, Value &out)
{
    StringIOObj *s = self_sio(a, "readline");
    if (!s || !meth_args(a, "readline", 0, 1))
        return R::Err;
    i64 n = -1;
    if (a.nargs > 1 && !size_arg(a.args[1], n))
        return R::Err;
    return sio_readline(s, n, out);
}

R sio_next_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_method(k, k->s[0], "readline");
    if (!is_str(in)) {
        Buf<128> m;
        m.put("readline() should have returned a str object, not '").put(type_name(in)).put("'");
        return err_set("OSError", m.str());
    }
    if (str_of(in)->len == 0) {
        Value e = exc_new(exc_find("StopIteration"), Value());
        return e.is_nil() ? R::Err : err_set_value(e);
    }
    return cont_done(k, in);
}

R sm_next(const CallArgs &a, Value &out)
{
    StringIOObj *s = self_sio(a, "__next__");
    if (!s || !meth_args(a, "__next__", 0, 0))
        return R::Err;
    if (is_inst(a.args[0]) && type_has_py_special(a.args[0], "readline")) {
        Root rs{ a.args[0] };
        Root kv{ cont_new(sio_next_step) };
        if (kv.v.is_nil())
            return R::Err;
        cont_of(kv.v)->s[0] = rs.v;
        out                 = kv.v;
        return R::Ok;
    }
    if (sio_readline(s, -1, out) != R::Ok)
        return R::Err;
    if (str_of(out)->len == 0) {
        Value e = exc_new(exc_find("StopIteration"), Value());
        return e.is_nil() ? R::Err : err_set_value(e);
    }
    return R::Ok;
}

R sm_write(const CallArgs &a, Value &out)
{
    StringIOObj *s = self_sio(a, "write", false);
    if (!s || !meth_args(a, "write", 1, 1))
        return R::Err;
    if (!is_str(a.args[1])) {
        Buf<96> m;
        m.put("string argument expected, got '").put(type_name(a.args[1])).put("'");
        return err_set("TypeError", m.str());
    }
    if (s->closed)
        return sio_closed_err();
    Root text{ a.args[1] };
    i64 n = str_of(text.v)->chars;
    if (n && sio_write_str(s, str_of(text.v)->str()) != R::Ok)
        return R::Err;
    out = int_from_i64(n);
    return R::Ok;
}

R sm_seek(const CallArgs &a, Value &out)
{
    StringIOObj *s = self_sio(a, "seek");
    if (!s || !meth_args(a, "seek", 1, 2))
        return R::Err;
    i64 pos = 0, whence = 0;
    if (!as_int_arg(a.args[1], pos))
        return err_set2("TypeError", "an integer is required", type_name(a.args[1]));
    if (a.nargs > 2 && !as_int_arg(a.args[2], whence))
        return err_set2("TypeError", "an integer is required", type_name(a.args[2]));
    char tmp[24];
    if (whence != 0 && whence != 1 && whence != 2) {
        Buf<64> m;
        m.put("Invalid whence (").put(int_text(tmp, sizeof tmp, whence));
        m.put(", should be 0, 1 or 2)");
        return err_set("ValueError", m.str());
    }
    if (pos < 0 && whence == 0) {
        Buf<64> m;
        m.put("Negative seek position ").put(int_text(tmp, sizeof tmp, pos));
        return err_set("ValueError", m.str());
    }
    if (whence != 0 && pos != 0)
        return err_set("OSError", "Can't do nonzero cur-relative seeks");
    if (whence == 1)
        pos = i64(s->pos);
    else if (whence == 2)
        pos = i64(s->buf.size());
    s->pos = usize(pos);
    out    = int_from_i64(pos);
    return R::Ok;
}

R sm_truncate(const CallArgs &a, Value &out)
{
    StringIOObj *s = self_sio(a, "truncate");
    if (!s || !meth_args(a, "truncate", 0, 1))
        return R::Err;
    i64 size = i64(s->pos);
    if (a.nargs > 1 && !is_none(a.args[1]) && !as_int_arg(a.args[1], size))
        return err_set2("TypeError", "an integer is required", type_name(a.args[1]));
    if (size < 0) {
        char tmp[24];
        Buf<64> m;
        m.put("Negative size value ").put(int_text(tmp, sizeof tmp, size));
        return err_set("ValueError", m.str());
    }
    if (usize(size) < s->buf.size())
        s->buf.resize(usize(size));
    out = int_from_i64(size);
    return R::Ok;
}

R sm_close(const CallArgs &a, Value &out)
{
    IoObj *io = io_self(a, IO_STRING, "close");
    if (!io || !meth_args(a, "close", 0, 0))
        return R::Err;
    StringIOObj *s = static_cast<StringIOObj *>(io);
    s->closed      = true;
    s->buf.clear();
    out = value_none();
    return R::Ok;
}

R sm_getstate(const CallArgs &a, Value &out)
{
    StringIOObj *s = self_sio(a, "__getstate__");
    if (!s || !meth_args(a, "__getstate__", 0, 0))
        return R::Err;
    Root self{ method_self(a.args[0]) };
    Root value{ cps_str(s->buf, 0, s->buf.size()) };
    Root pos{ int_from_i64(i64(s->pos)) };
    Root dict{ sio_of(self.v)->dict };
    if (!dict.v.is_nil()) {
        Root copy{ obj_value(dict_new()) };
        if (copy.v.is_nil())
            return oom();
        usize at = 0;
        Value k, x;
        while (table_next(static_cast<DictObj *>(dict.v.obj())->t, at, k, x))
            if (dict_set(static_cast<DictObj *>(copy.v.obj()), k, x) != R::Ok)
                return R::Err;
        dict = copy.v;
    }
    TupleObj *t = value.v.is_nil() || pos.v.is_nil() ? nullptr : tuple_new(4);
    if (!t)
        return err_pending() ? R::Err : oom();
    s              = sio_of(self.v);
    t->items()[0] = value.v;
    t->items()[1] = s->readnl.is_nil() ? value_none() : s->readnl;
    t->items()[2] = pos.v;
    t->items()[3] = dict.v.is_nil() ? value_none() : dict.v;
    out           = obj_value(t);
    return R::Ok;
}

R sm_setstate(const CallArgs &a, Value &out)
{
    IoObj *io = io_self(a, IO_STRING, "__setstate__");
    if (!io || !meth_args(a, "__setstate__", 1, 1))
        return R::Err;
    Value st = a.args[1];
    if (!is_tuple(st) || static_cast<TupleObj *>(st.obj())->len < 4) {
        Buf<128> m;
        m.put(type_name(a.args[0])).put(".__setstate__ argument should be 4-tuple, got ");
        m.put(type_name(st));
        return err_set("TypeError", m.str());
    }
    Root self{ method_self(a.args[0]) }, rst{ st };
    Value *it = static_cast<TupleObj *>(st.obj())->items();
    // The newlines were translated when the state was taken; the value goes
    // back in as it is.
    if (sio_init(self.v, Value(), it[1]) != R::Ok)
        return R::Err;
    it = static_cast<TupleObj *>(rst.v.obj())->items();
    if (!is_str(it[0]))
        return err_set2("TypeError", "initial_value must be str, not", type_name(it[0]));
    StringIOObj *s = sio_of(self.v);
    Str text       = str_of(it[0])->str();
    usize at       = 0;
    while (at < text.size()) {
        u32 cp = 0;
        at += cp_decode(text, at, cp);
        if (!s->buf.push(cp))
            return oom();
    }
    i64 pos = 0;
    if (!as_int_arg(it[2], pos))
        return err_set2("TypeError", "third item of state must be an integer, got",
                        type_name(it[2]));
    if (pos < 0)
        return err_set("ValueError", "position value cannot be negative");
    s->pos = usize(pos);
    if (!is_none(it[3])) {
        if (!is_dict(it[3]))
            return err_set2("TypeError", "fourth item of state should be a dict, got a",
                            type_name(it[3]));
        s->dict = it[3];
    }
    out = value_none();
    return R::Ok;
}

constexpr Method SIO_METHODS[] = {
    { "__init__", sm_init },         { "readable", sm_readable },
    { "writable", sm_writable },     { "seekable", sm_seekable },
    { "tell", sm_tell },             { "getvalue", sm_getvalue },
    { "read", sm_read },             { "readline", sm_readline },
    { "__next__", sm_next },         { "write", sm_write },
    { "seek", sm_seek },             { "truncate", sm_truncate },
    { "close", sm_close },           { "__getstate__", sm_getstate },
    { "__setstate__", sm_setstate },
};

R sio_getattr(Value v, StrObj *name, Value &out)
{
    StringIOObj *s = sio_of(v);
    Str n          = name->str();
    if (n == "closed" || n == "line_buffering" || n == "newlines") {
        if (!s->ok)
            return err_set("ValueError", "I/O operation on uninitialized object");
        if (n == "closed") {
            out = value_bool(s->closed);
            return R::Ok;
        }
        if (s->closed)
            return sio_closed_err();
        if (n == "line_buffering") {
            out = value_bool(false);
            return R::Ok;
        }
        if (!s->has_decoder) {
            out = value_none();
            return R::Ok;
        }
        u8 seen = s->seennl;
        if (seen == 0) {
            out = value_none();
            return R::Ok;
        }
        // The same answer IncrementalNewlineDecoder.newlines gives.
        Value probe;
        Root kv{ Value() };
        Value args[2] = { value_none(), value_bool(s->readtranslate) };
        CallArgs ca;
        ca.args  = args;
        ca.nargs = 2;
        if (nldecoder_new(ca, probe) != R::Ok)
            return R::Err;
        Root rp{ probe };
        nldecoder_set_seen(rp.v, seen);
        StrObj *nl = str_intern("newlines");
        return nl ? py_getattr(rp.v, nl, out) : oom();
    }
    if (n == "encoding" || n == "errors") {
        out = value_none();
        return R::Ok;
    }
    return io_dict_get(v, name, out);
}

R sio_repr(Value v, String &out)
{
    char tmp[24];
    Buf<96> b;
    b.put("<_io.StringIO object at ").put(addr_text(tmp, sizeof tmp, v.obj())).put('>');
    return out.append(b.str()) ? R::Ok : oom();
}

} // namespace

constexpr Type bytesio_type{ .name    = "_io.BytesIO",
                             .trace   = bio_trace,
                             .repr    = bio_repr,
                             .iter    = self_iter,
                             .getattr = bio_getattr,
                             .setattr = io_dict_set,
                             .base    = &bufbase_type,
                             .vmnext  = true };

constexpr Type stringio_type{ .name    = "_io.StringIO",
                              .trace   = sio_trace,
                              .fini    = sio_fini,
                              .repr    = sio_repr,
                              .iter    = self_iter,
                              .getattr = sio_getattr,
                              .setattr = io_dict_set,
                              .base    = &textbase_type,
                              .vmnext  = true };

bool mem_methods()
{
    return method_install(&bytesio_type, BIO_METHODS) &&
           method_install(&stringio_type, SIO_METHODS);
}

R bytesio_new(const CallArgs &a, Value &out)
{
    BytesIOObj *b = static_cast<BytesIOObj *>(obj_alloc(&bytesio_type, sizeof(BytesIOObj)));
    if (!b)
        return oom();
    b->dict       = Value();
    b->kind       = IO_BYTES;
    b->closed     = false;
    b->inside     = false;
    b->finalizing = false;
    b->buf        = Value();
    b->pos        = 0;
    Root rb{ obj_value(b) };
    constexpr Str NAMES[] = { "initial_bytes" };
    Value v[1];
    if (!fn_take(a, "BytesIO", NAMES, 0, v))
        return R::Err;
    if (bio_init_value(rb.v, v[0]) != R::Ok)
        return R::Err;
    out = rb.v;
    return R::Ok;
}

R stringio_new(const CallArgs &a, Value &out)
{
    StringIOObj *s = static_cast<StringIOObj *>(obj_alloc(&stringio_type, sizeof(StringIOObj)));
    if (!s)
        return oom();
    s->dict       = Value();
    s->kind       = IO_STRING;
    s->closed     = false;
    s->inside     = false;
    s->finalizing = false;
    new (&s->buf) Vec<u32>();
    s->pos           = 0;
    s->readnl        = Value();
    s->writenl       = Str();
    s->ok            = false;
    s->readuniversal = s->readtranslate = s->has_decoder = false;
    s->seennl                                            = 0;
    Root rs{ obj_value(s) };
    constexpr Str NAMES[] = { "initial_value", "newline" };
    Value v[2];
    if (!fn_take(a, "StringIO", NAMES, 0, v))
        return R::Err;
    Root nl{ v[1].is_nil() ? str_new("\n") : v[1] };
    if (nl.v.is_nil() || sio_init(rs.v, v[0], nl.v) != R::Ok)
        return R::Err;
    out = rs.v;
    return R::Ok;
}
