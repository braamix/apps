// `_io`'s abstract layers -- _IOBase, _RawIOBase, _BufferedIOBase and
// _TextIOBase -- the module itself, open(), and the closing a program's end
// owes whatever it left open.
//
// The abstract layers are what a class of the program's own derives from, so
// every default here reaches the other methods through the instance, as
// CPython's do: IOBase.readline() calls self.peek() and self.read(), which
// may be Python. Each is therefore a continuation.
#include "io.h"

#include "binfmt.h"
#include "builtin.h"
#include "codec.h"
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
#include "vm.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

constexpr i64 DEFAULT_BUFFER_SIZE = 128 * 1024;

struct Home {
    Value unsupported; // the class
    Vec<Obj *> tracked;
};

Home *home;

void home_mark()
{
    if (home)
        gc_mark(home->unsupported);
}

Home *here()
{
    if (!home) {
        home = heap_new<Home>();
        if (home)
            gc_root_hook(home_mark);
    }
    return home;
}

bool in_family(const Type *t)
{
    for (; t; t = t->base)
        if (t == &iobase_type)
            return true;
    return false;
}

} // namespace

// ------------------------------------------------------------------ helpers

bool io_of(Value v, IoObj *&io)
{
    if (!v.is_obj())
        return false;
    if (in_family(v.obj()->type)) {
        io = io_cast(v);
        return true;
    }
    if (is_inst(v)) {
        Value n = inst_of(v)->native;
        if (n.is_obj() && in_family(n.obj()->type)) {
            io = io_cast(n);
            return true;
        }
    }
    return false;
}

bool io_is_plain(Value v, u8 kind)
{
    return v.is_obj() && in_family(v.obj()->type) && io_cast(v)->kind == kind;
}

IoObj *io_self(const CallArgs &a, u8 kind, Str who)
{
    IoObj *io = nullptr;
    if (!a.nargs || !io_of(a.args[0], io) || (kind != IO_BASE && io->kind != kind)) {
        Buf<128> b;
        b.put("descriptor '").put(who).put("' requires an _io object");
        err_set2("TypeError", b.str(), a.nargs ? type_name(a.args[0]) : Str("nothing"));
        return nullptr;
    }
    if (is_inst(a.args[0]))
        io->inside = true;
    return io;
}

void io_trace(Obj *o)
{
    gc_mark(static_cast<IoObj *>(o)->dict);
}

R io_dict_get(Value v, StrObj *name, Value &out)
{
    IoObj *io = io_cast(v);
    if (name->str() == "__dict__") {
        if (io->dict.is_nil()) {
            DictObj *d = dict_new();
            if (!d)
                return oom();
            io->dict = obj_value(d);
        }
        out = io->dict;
        return R::Ok;
    }
    if (io->dict.is_nil())
        return R::NotImpl;
    return dict_get(static_cast<DictObj *>(io->dict.obj()), obj_value(name), out);
}

R io_dict_set(Value v, StrObj *name, Value x)
{
    Root rv{ v }, rx{ x };
    IoObj *io = io_cast(rv.v);
    if (x.is_nil()) {
        if (io->dict.is_nil())
            return err_set2("AttributeError", "no such attribute", name->str());
        R r = dict_del(static_cast<DictObj *>(io->dict.obj()), obj_value(name));
        return r == R::NotImpl ? err_set2("AttributeError", "no such attribute", name->str()) : r;
    }
    if (io->dict.is_nil()) {
        DictObj *d = dict_new();
        if (!d)
            return oom();
        io_cast(rv.v)->dict = obj_value(d);
    }
    return dict_set(static_cast<DictObj *>(io_cast(rv.v)->dict.obj()), obj_value(name), rx.v);
}

R io_set_attr(Value v, StrObj *name, Value x, const Str *readonly, usize n, Str tname)
{
    Str nm = name->str();
    for (usize i = 0; i < n; i++)
        if (readonly[i] == nm) {
            Buf<128> m;
            m.put("attribute '").put(nm).put("' of '").put(tname).put("' objects is not writable");
            return err_set("AttributeError", m.str());
        }
    if (nm == "_finalizing") {
        if (x.is_nil())
            return err_set("TypeError", "can't delete numeric/char attribute");
        if (!is_bool(x))
            return err_set("TypeError", "attribute value type must be bool");
        io_cast(v)->finalizing = is_true(x);
        return R::Ok;
    }
    return io_dict_set(v, name, x);
}

Value io_unsupported_type()
{
    Home *h = here();
    if (!h)
        return oom(), Value();
    if (!h->unsupported.is_nil())
        return h->unsupported;
    Root os{ exc_type_value(exc_find("OSError")) };
    Root ve{ exc_type_value(exc_find("ValueError")) };
    TupleObj *bases = os.v.is_nil() || ve.v.is_nil() ? nullptr : tuple_new(2);
    if (!bases)
        return err_pending() ? Value() : (oom(), Value());
    bases->items()[0] = os.v;
    bases->items()[1] = ve.v;
    Root rb{ obj_value(bases) };
    DictObj *d = dict_new();
    if (!d)
        return oom(), Value();
    Root rd{ obj_value(d) };
    Root mod{ str_new("io") };
    Root name{ str_new("UnsupportedOperation") };
    StrObj *key = str_intern("__module__");
    if (mod.v.is_nil() || name.v.is_nil() || !key ||
        dict_set(static_cast<DictObj *>(rd.v.obj()), obj_value(key), mod.v) != R::Ok)
        return err_pending() ? Value() : (oom(), Value());
    h->unsupported = type_new(name.v, rb.v, rd.v);
    return h->unsupported;
}

R io_unsupported(Str msg)
{
    Root cls{ io_unsupported_type() };
    if (cls.v.is_nil())
        return R::Err;
    Root m{ str_new(msg) };
    TupleObj *t = m.v.is_nil() ? nullptr : tuple_new(1);
    if (!t)
        return err_pending() ? R::Err : oom();
    t->items()[0] = m.v;
    Root rt{ obj_value(t) };
    Value e = exc_construct(cls.v, rt.v);
    return e.is_nil() ? R::Err : err_set_value(e);
}

R io_closed_err(Str msg)
{
    return err_set("ValueError", msg.empty() ? Str("I/O operation on closed file.") : msg);
}

void io_track(IoObj *o)
{
    Home *h = here();
    if (!h)
        return;
    for (Obj *t : h->tracked)
        if (t == o)
            return;
    h->tracked.push(o);
    o->flags |= OBJ_FINAL;
}

void io_untrack(Obj *o)
{
    if (!home)
        return;
    Vec<Obj *> &t = home->tracked;
    for (usize i = 0; i < t.size(); i++)
        if (t[i] == o) {
            t[i] = t[t.size() - 1];
            t.pop();
            return;
        }
}

bool io_writable_span(Value v, u8 *&p, usize &n)
{
    if (is_bytearray(v)) {
        p = array_of(v)->data.data();
        n = array_of(v)->data.size();
        return true;
    }
    if (u8 *d = array_data(v)) {
        Str s;
        char code;
        array_bytes(v, s, code);
        p = d;
        n = s.size();
        return true;
    }
    if (is_memview(v)) {
        Str s;
        bool w = false;
        if (!memview_bytes(v, s, &w))
            return false;
        if (!w)
            return err_set("TypeError", "readinto() argument must be read-write bytes-like object"),
                   false;
        p = const_cast<u8 *>(reinterpret_cast<const u8 *>(s.data()));
        n = s.size();
        return true;
    }
    Buf<128> b;
    b.put("readinto() argument must be read-write bytes-like object, not ").put(type_name(v));
    return err_set("TypeError", b.str()), false;
}

namespace {

// ------------------------------------------------------------- closed, async

// What `self.closed` says. Yes or No when that needed no Python; Ask when
// `call` is a ContObj whose answer is the attribute.
enum class Ask : u8 { No, Yes, Call, Err };

Ask closed_of(Value self, Value &call)
{
    IoObj *io = nullptr;
    StrObj *n = str_intern("closed");
    if (!n)
        return oom(), Ask::Err;
    Value got;
    Got g = py_attr(self, n, got);
    if (g == Got::Error)
        return Ask::Err;
    if (g == Got::Call) {
        call = got;
        return Ask::Call;
    }
    if (g == Got::Missing) {
        if (io_of(self, io))
            return io->closed ? Ask::Yes : Ask::No;
        return err_set("AttributeError", "closed"), Ask::Err;
    }
    return py_truth(got) ? Ask::Yes : Ask::No;
}

// The state machines below share their first states: ask whether the stream
// is closed, and raise if so. k->s[0] is self.
enum : u32 { CK_START = 0, CK_ANSWER = 1, CK_OPEN = 2 };

// True when the step is past the check; otherwise `r` is what to return.
bool check_open(ContObj *k, Value in, R &r, Str msg = Str())
{
    if (k->i == CK_START) {
        Value call;
        switch (closed_of(k->s[0], call)) {
        case Ask::Err:
            r = R::Err;
            return false;
        case Ask::Yes:
            r = io_closed_err(msg);
            return false;
        case Ask::No:
            k->i = CK_OPEN;
            return true;
        case Ask::Call:
            k->i = CK_ANSWER;
            r    = cont_await(k, call);
            return false;
        }
    }
    if (k->i == CK_ANSWER) {
        if (py_truth(in)) {
            r = io_closed_err(msg);
            return false;
        }
        k->i = CK_OPEN;
    }
    return true;
}

Value new_cont(ContStep step, Value self)
{
    Root rs{ self };
    Root kv{ cont_new(step) };
    if (!kv.v.is_nil())
        cont_of(kv.v)->s[0] = rs.v;
    return kv.v;
}

R start_cont(ContStep step, Value self, Value &out)
{
    out = new_cont(step, self);
    return out.is_nil() ? R::Err : R::Ok;
}

// ------------------------------------------------------------------ _IOBase

R ib_seek(const CallArgs &a, Value &out)
{
    (void)out;
    if (!io_self(a, IO_BASE, "seek"))
        return R::Err;
    return io_unsupported("seek");
}

R ib_truncate(const CallArgs &a, Value &out)
{
    (void)out;
    if (!io_self(a, IO_BASE, "truncate"))
        return R::Err;
    return io_unsupported("truncate");
}

R ib_fileno(const CallArgs &a, Value &out)
{
    (void)out;
    if (!io_self(a, IO_BASE, "fileno"))
        return R::Err;
    return io_unsupported("fileno");
}

R tell_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_method(k, k->s[0], "seek", 2, Value::of_int(0), Value::of_int(1));
    return cont_done(k, in);
}

R ib_tell(const CallArgs &a, Value &out)
{
    if (!io_self(a, IO_BASE, "tell") || !meth_args(a, "tell", 0, 0))
        return R::Err;
    return start_cont(tell_step, a.args[0], out);
}

R flush_step(ContObj *k, Value in)
{
    R r;
    if (!check_open(k, in, r))
        return r;
    return cont_done(k, value_none());
}

R ib_flush(const CallArgs &a, Value &out)
{
    IoObj *io = io_self(a, IO_BASE, "flush");
    if (!io || !meth_args(a, "flush", 0, 0))
        return R::Err;
    if (!is_inst(a.args[0])) {
        if (io->closed)
            return io_closed_err();
        out = value_none();
        return R::Ok;
    }
    return start_cont(flush_step, a.args[0], out);
}

// close(): flush, and the flag set whatever the flush did.
void close_fail(ContObj *k)
{
    IoObj *io = nullptr;
    if (io_of(k->s[0], io))
        io->closed = true;
}

R close_step(ContObj *k, Value)
{
    IoObj *io = nullptr;
    io_of(k->s[0], io);
    if (k->i++ == 0) {
        k->fail = close_fail;
        return cont_method(k, k->s[0], "flush");
    }
    io->closed = true;
    return cont_done(k, value_none());
}

R ib_close(const CallArgs &a, Value &out)
{
    IoObj *io = io_self(a, IO_BASE, "close");
    if (!io || !meth_args(a, "close", 0, 0))
        return R::Err;
    if (io->closed) {
        out = value_none();
        return R::Ok;
    }
    if (!is_inst(a.args[0])) {
        io->closed = true;
        out        = value_none();
        return R::Ok;
    }
    return start_cont(close_step, a.args[0], out);
}

R ib_false(const CallArgs &a, Value &out, Str who)
{
    if (!io_self(a, IO_BASE, who) || !meth_args(a, who, 0, 0))
        return R::Err;
    out = value_bool(false);
    return R::Ok;
}

R ib_seekable(const CallArgs &a, Value &out)
{
    return ib_false(a, out, "seekable");
}

R ib_readable(const CallArgs &a, Value &out)
{
    return ib_false(a, out, "readable");
}

R ib_writable(const CallArgs &a, Value &out)
{
    return ib_false(a, out, "writable");
}

// _checkClosed(msg=None) and the other three: s[1] the message; j which.
enum : u32 { CHECK_CLOSED, CHECK_SEEKABLE, CHECK_READABLE, CHECK_WRITABLE };

R check_step(ContObj *k, Value in)
{
    Str msg = is_str(k->s[1]) ? str_of(k->s[1])->str() : Str();
    if (k->j == CHECK_CLOSED) {
        R r;
        if (!check_open(k, in, r, msg))
            return r;
        return cont_done(k, value_none());
    }
    constexpr Str METHOD[] = { "", "seekable", "readable", "writable" };
    constexpr Str DEFAULT[] = { "", "File or stream is not seekable.",
                                "File or stream is not readable.",
                                "File or stream is not writable." };
    if (k->i++ == 0)
        return cont_method(k, k->s[0], METHOD[k->j]);
    if (!is_true(in))
        return io_unsupported(msg.empty() ? DEFAULT[k->j] : msg);
    return cont_done(k, value_none());
}

R check_start(const CallArgs &a, Value &out, u32 which, Str who)
{
    if (!io_self(a, IO_BASE, who) || !meth_args(a, who, 0, 1))
        return R::Err;
    Root rm{ a.nargs > 1 && !is_none(a.args[1]) ? a.args[1] : Value() };
    if (start_cont(check_step, a.args[0], out) != R::Ok)
        return R::Err;
    cont_of(out)->s[1] = rm.v;
    cont_of(out)->j    = which;
    return R::Ok;
}

R ib_check_closed(const CallArgs &a, Value &out)
{
    return check_start(a, out, CHECK_CLOSED, "_checkClosed");
}

R ib_check_seekable(const CallArgs &a, Value &out)
{
    return check_start(a, out, CHECK_SEEKABLE, "_checkSeekable");
}

R ib_check_readable(const CallArgs &a, Value &out)
{
    return check_start(a, out, CHECK_READABLE, "_checkReadable");
}

R ib_check_writable(const CallArgs &a, Value &out)
{
    return check_start(a, out, CHECK_WRITABLE, "_checkWritable");
}

R self_step(ContObj *k, Value in)
{
    R r;
    if (!check_open(k, in, r))
        return r;
    return cont_done(k, k->s[0]);
}

R ib_enter(const CallArgs &a, Value &out)
{
    IoObj *io = io_self(a, IO_BASE, "__enter__");
    if (!io || !meth_args(a, "__enter__", 0, 0))
        return R::Err;
    Value call;
    switch (closed_of(a.args[0], call)) {
    case Ask::Err:
        return R::Err;
    case Ask::Yes:
        return io_closed_err();
    case Ask::No:
        out = a.args[0];
        return R::Ok;
    case Ask::Call:
        break;
    }
    return start_cont(self_step, a.args[0], out);
}

R ib_iter(const CallArgs &a, Value &out)
{
    if (!io_self(a, IO_BASE, "__iter__") || !meth_args(a, "__iter__", 0, 0))
        return R::Err;
    return ib_enter(a, out);
}

R exit_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_method(k, k->s[0], "close");
    return cont_done(k, in);
}

R ib_exit(const CallArgs &a, Value &out)
{
    if (!io_self(a, IO_BASE, "__exit__"))
        return R::Err;
    return start_cont(exit_step, a.args[0], out);
}

R isatty_step(ContObj *k, Value in)
{
    R r;
    if (!check_open(k, in, r))
        return r;
    return cont_done(k, value_bool(false));
}

R ib_isatty(const CallArgs &a, Value &out)
{
    if (!io_self(a, IO_BASE, "isatty") || !meth_args(a, "isatty", 0, 0))
        return R::Err;
    return start_cont(isatty_step, a.args[0], out);
}

// A size argument: None or an index. -1 for None.
bool size_arg(Value v, i64 &n, Str who)
{
    n = -1;
    if (v.is_nil() || is_none(v))
        return true;
    if (as_int_arg(v, n))
        return true;
    Buf<128> b;
    b.put("argument should be integer or None, not '").put(type_name(v)).put("'");
    (void)who;
    return err_set("TypeError", b.str()) == R::Ok;
}

// readline(size=-1): s[1] the line so far (a bytearray); x[0] the size;
// j whether self has a peek.
enum : u32 { RL_LOOP = 10, RL_PEEKED, RL_READ };

R bytes_so_far(ContObj *k)
{
    Value b = bytes_new(array_of(k->s[1])->str());
    return b.is_nil() ? R::Err : cont_done(k, b);
}

R readline_step(ContObj *k, Value in)
{
    for (;;) {
        switch (k->i) {
        case RL_LOOP: {
            i64 size = k->x[0];
            usize have = array_of(k->s[1])->data.size();
            if (size >= 0 && i64(have) >= size)
                return bytes_so_far(k);
            if (k->j) {
                k->i = RL_PEEKED;
                return cont_method(k, k->s[0], "peek", 1, Value::of_int(1));
            }
            k->i = RL_READ;
            return cont_method(k, k->s[0], "read", 1, Value::of_int(1));
        }
        case RL_PEEKED: {
            Str ahead;
            if (!is_bytes(in)) {
                Buf<128> b;
                b.put("peek() should have returned a bytes object, not '").put(type_name(in));
                b.put("'");
                return err_set("OSError", b.str());
            }
            ahead   = static_cast<BytesObj *>(in.obj())->str();
            i64 n   = 1;
            if (!ahead.empty()) {
                n = i64(ahead.size());
                for (usize i = 0; i < ahead.size(); i++)
                    if (ahead[i] == '\n') {
                        n = i64(i + 1);
                        break;
                    }
            }
            i64 size = k->x[0];
            i64 have = i64(array_of(k->s[1])->data.size());
            if (size >= 0 && n > size - have)
                n = size - have;
            k->i = RL_READ;
            Value nv = int_from_i64(n);
            if (nv.is_nil())
                return R::Err;
            return cont_method(k, k->s[0], "read", 1, nv);
        }
        case RL_READ: {
            if (!is_bytes(in)) {
                Buf<128> b;
                b.put("read() should have returned a bytes object, not '").put(type_name(in));
                b.put("'");
                return err_set("OSError", b.str());
            }
            Str got = static_cast<BytesObj *>(in.obj())->str();
            if (got.empty())
                return bytes_so_far(k);
            ArrayObj *acc = array_of(k->s[1]);
            for (usize i = 0; i < got.size(); i++)
                if (!acc->data.push(u8(got[i])))
                    return oom();
            if (got[got.size() - 1] == '\n')
                return bytes_so_far(k);
            k->i = RL_LOOP;
            continue;
        }
        default: {
            R r;
            if (!check_open(k, in, r))
                return r;
            k->i = RL_LOOP;
            continue;
        }
        }
    }
}

R ib_readline(const CallArgs &a, Value &out)
{
    if (!io_self(a, IO_BASE, "readline") || !meth_args(a, "readline", 0, 1))
        return R::Err;
    i64 size = -1;
    if (a.nargs > 1 && !size_arg(a.args[1], size, "readline"))
        return R::Err;
    // Whether self has a peek, asked before the continuation exists: the
    // lookup may allocate, and `out` is no root.
    StrObj *pk = str_intern("peek");
    Value m;
    Got g = pk ? py_attr(a.args[0], pk, m) : Got::Error;
    if (g == Got::Error) {
        if (!err_pending())
            return oom();
        err_clear();
    }
    Root acc{ bytearray_new(Str()) };
    if (acc.v.is_nil() || start_cont(readline_step, a.args[0], out) != R::Ok)
        return R::Err;
    ContObj *k = cont_of(out);
    k->s[1]    = acc.v;
    k->x[0]    = size;
    k->j       = g == Got::Ok || g == Got::Call;
    k->i       = RL_LOOP;
    return R::Ok;
}

R next_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_method(k, k->s[0], "readline");
    usize n = 0;
    if (py_len(in, n) != R::Ok)
        return R::Err;
    if (n == 0) {
        Value e = exc_new(exc_find("StopIteration"), Value());
        return e.is_nil() ? R::Err : err_set_value(e);
    }
    return cont_done(k, in);
}

R ib_next(const CallArgs &a, Value &out)
{
    if (!io_self(a, IO_BASE, "__next__") || !meth_args(a, "__next__", 0, 0))
        return R::Err;
    return start_cont(next_step, a.args[0], out);
}

// readlines(hint=-1): s[1] the list; x[0] the hint, x[1] the length so far.
R readlines_step(ContObj *k, Value in)
{
    if (k->i == 0) {
        k->i        = 1;
        k->catching = CATCH_STOP;
        return cont_method(k, k->s[0], "__next__");
    }
    if (in.is_nil()) {
        k->caught   = Value();
        k->catching = CATCH_NONE;
        return cont_done(k, k->s[1]);
    }
    if (!list_push(list_of(k->s[1]), in))
        return oom();
    usize n = 0;
    if (k->x[0] > 0) {
        if (py_len(in, n) != R::Ok)
            return R::Err;
        k->x[1] += i64(n);
        if (k->x[1] >= k->x[0]) {
            k->catching = CATCH_NONE;
            return cont_done(k, k->s[1]);
        }
    }
    return cont_method(k, k->s[0], "__next__");
}

R ib_readlines(const CallArgs &a, Value &out)
{
    if (!io_self(a, IO_BASE, "readlines") || !meth_args(a, "readlines", 0, 1))
        return R::Err;
    i64 hint = -1;
    if (a.nargs > 1 && !size_arg(a.args[1], hint, "readlines"))
        return R::Err;
    ListObj *l = list_new();
    if (!l)
        return oom();
    Root rl{ obj_value(l) };
    if (start_cont(readlines_step, a.args[0], out) != R::Ok)
        return R::Err;
    cont_of(out)->s[1] = rl.v;
    cont_of(out)->x[0] = hint;
    return R::Ok;
}

// writelines(lines): s[1] the lines, a list; j the next.
R writelines_step(ContObj *k, Value in)
{
    if (k->i < CK_OPEN) {
        R r;
        if (!check_open(k, in, r))
            return r;
    }
    ListObj *l = list_of(k->s[1]);
    if (k->j >= l->items.size())
        return cont_done(k, value_none());
    return cont_method(k, k->s[0], "write", 1, l->items[k->j++]);
}

R ib_writelines(const CallArgs &a, Value &out)
{
    if (!io_self(a, IO_BASE, "writelines") || !meth_args(a, "writelines", 1, 1))
        return R::Err;
    if (iter_needs_vm(a.args[1]))
        return iter_park(a, 1, ib_writelines, out);
    Root lines{ obj_value(py_list_of(a.args[1])) };
    if (lines.v.is_nil())
        return R::Err;
    if (start_cont(writelines_step, a.args[0], out) != R::Ok)
        return R::Err;
    cont_of(out)->s[1] = lines.v;
    return R::Ok;
}

// __del__: close() unless closed; what close() raises is the finalizer's.
R del_step(ContObj *k, Value in)
{
    IoObj *io = nullptr;
    io_of(k->s[0], io);
    if (k->i == CK_START || k->i == CK_ANSWER) {
        R r;
        bool open = check_open(k, in, r);
        if (!open) {
            // Closed, or `closed` itself failed: nothing to do.
            err_clear();
            return cont_done(k, value_none());
        }
    }
    if (k->i == CK_OPEN) {
        k->i = CK_OPEN + 1;
        if (io)
            io->finalizing = true;
        return cont_method(k, k->s[0], "close");
    }
    return cont_done(k, value_none());
}

R ib_del(const CallArgs &a, Value &out)
{
    if (!io_self(a, IO_BASE, "__del__"))
        return R::Err;
    return start_cont(del_step, a.args[0], out);
}

constexpr Method IOBASE_METHODS[] = {
    { "seek", ib_seek },
    { "tell", ib_tell },
    { "truncate", ib_truncate },
    { "flush", ib_flush },
    { "close", ib_close },
    { "seekable", ib_seekable },
    { "readable", ib_readable },
    { "writable", ib_writable },
    { "_checkClosed", ib_check_closed },
    { "_checkSeekable", ib_check_seekable },
    { "_checkReadable", ib_check_readable },
    { "_checkWritable", ib_check_writable },
    { "__enter__", ib_enter },
    { "__exit__", ib_exit },
    { "fileno", ib_fileno },
    { "isatty", ib_isatty },
    { "readline", ib_readline },
    { "__iter__", ib_iter },
    { "__next__", ib_next },
    { "readlines", ib_readlines },
    { "writelines", ib_writelines },
    { "__del__", ib_del },
};

R base_getattr(Value v, StrObj *name, Value &out)
{
    if (name->str() == "closed") {
        out = value_bool(io_cast(v)->closed);
        return R::Ok;
    }
    return io_dict_get(v, name, out);
}

R base_repr(Value v, String &out)
{
    char tmp[24];
    Buf<96> b;
    b.put("<_io.").put(type_name(v)).put(" object at ").put(addr_text(tmp, sizeof tmp, v.obj()));
    b.put('>');
    return out.append(b.str()) ? R::Ok : oom();
}

Value base_iter(Value v)
{
    return v;
}

// --------------------------------------------------------------- _RawIOBase

// read(size=-1): x[0] the size; s[1] the bytearray.
R rawread_step(ContObj *k, Value in)
{
    if (k->i++ == 0) {
        if (k->x[0] < 0)
            return cont_method(k, k->s[0], "readall");
        String zeros;
        if (!zeros.reserve(usize(k->x[0])))
            return oom();
        for (i64 i = 0; i < k->x[0]; i++)
            zeros.push(0);
        Value b = bytearray_new(zeros.str());
        if (b.is_nil())
            return R::Err;
        k->s[1] = b;
        return cont_method(k, k->s[0], "readinto", 1, b);
    }
    if (k->x[0] < 0 || is_none(in))
        return cont_done(k, in);
    i64 n = 0;
    if (!as_int_arg(in, n))
        return err_set2("TypeError", "readinto() should return an integer", type_name(in));
    usize len = array_of(k->s[1])->data.size();
    if (n < 0 || usize(n) > len) {
        char t1[24], t2[24];
        Buf<96> b;
        b.put("readinto returned ").put(int_text(t1, sizeof t1, n));
        b.put(" outside buffer size ").put(int_text(t2, sizeof t2, i64(len)));
        return err_set("ValueError", b.str());
    }
    Value out = bytes_new(array_of(k->s[1])->str().substr(0, usize(n)));
    return out.is_nil() ? R::Err : cont_done(k, out);
}

R rb_read(const CallArgs &a, Value &out)
{
    if (!io_self(a, IO_BASE, "read") || !meth_args(a, "read", 0, 1))
        return R::Err;
    i64 size = -1;
    if (a.nargs > 1 && !size_arg(a.args[1], size, "read"))
        return R::Err;
    if (start_cont(rawread_step, a.args[0], out) != R::Ok)
        return R::Err;
    cont_of(out)->x[0] = size;
    return R::Ok;
}

// readall(): s[1] a bytearray of what came.
R readall_step(ContObj *k, Value in)
{
    if (k->i++ == 0) {
        Value b = bytearray_new(Str());
        if (b.is_nil())
            return R::Err;
        k->s[1] = b;
    } else {
        if (is_none(in)) {
            if (array_of(k->s[1])->data.empty())
                return cont_done(k, in);
            in = Value();
        } else if (!is_bytes(in)) {
            return err_set("TypeError", "read() should return bytes");
        }
        Str got = in.is_nil() ? Str() : static_cast<BytesObj *>(in.obj())->str();
        if (got.empty()) {
            Value out = bytes_new(array_of(k->s[1])->str());
            return out.is_nil() ? R::Err : cont_done(k, out);
        }
        ArrayObj *acc = array_of(k->s[1]);
        if (!acc->data.reserve(acc->data.size() + got.size()))
            return oom();
        for (usize i = 0; i < got.size(); i++)
            acc->data.push(u8(got[i]));
    }
    Value n = int_from_i64(DEFAULT_BUFFER_SIZE);
    return cont_method(k, k->s[0], "read", 1, n);
}

R rb_readall(const CallArgs &a, Value &out)
{
    if (!io_self(a, IO_BASE, "readall") || !meth_args(a, "readall", 0, 0))
        return R::Err;
    return start_cont(readall_step, a.args[0], out);
}

R rb_readinto(const CallArgs &a, Value &out)
{
    (void)out;
    if (!io_self(a, IO_BASE, "readinto"))
        return R::Err;
    return err_set("NotImplementedError", Str());
}

R rb_write(const CallArgs &a, Value &out)
{
    (void)out;
    if (!io_self(a, IO_BASE, "write"))
        return R::Err;
    return err_set("NotImplementedError", Str());
}

constexpr Method RAWBASE_METHODS[] = {
    { "read", rb_read },
    { "readall", rb_readall },
    { "readinto", rb_readinto },
    { "write", rb_write },
};

// ---------------------------------------------------------- _BufferedIOBase

R bb_unsupported(const CallArgs &a, Str who)
{
    if (!io_self(a, IO_BASE, who))
        return R::Err;
    return io_unsupported(who);
}

R bb_read(const CallArgs &a, Value &)
{
    return bb_unsupported(a, "read");
}

R bb_read1(const CallArgs &a, Value &)
{
    return bb_unsupported(a, "read1");
}

R bb_write(const CallArgs &a, Value &)
{
    return bb_unsupported(a, "write");
}

R bb_detach(const CallArgs &a, Value &)
{
    return bb_unsupported(a, "detach");
}

// readinto(b) through read(): s[1] the buffer; j 1 for read1.
R bufinto_step(ContObj *k, Value in)
{
    u8 *p   = nullptr;
    usize n = 0;
    if (!io_writable_span(k->s[1], p, n))
        return R::Err;
    if (k->i++ == 0) {
        Value len = int_from_i64(i64(n));
        return cont_method(k, k->s[0], k->j ? Str("read1") : Str("read"), 1, len);
    }
    if (!is_bytes(in))
        return err_set("TypeError", "read() should return bytes");
    Str got = static_cast<BytesObj *>(in.obj())->str();
    if (got.size() > n) {
        char t1[24], t2[24];
        Buf<128> b;
        b.put("read() returned too much data: ").put(int_text(t1, sizeof t1, i64(n)));
        b.put(" bytes requested, ").put(int_text(t2, sizeof t2, i64(got.size())));
        b.put(" returned");
        return err_set("ValueError", b.str());
    }
    for (usize i = 0; i < got.size(); i++)
        p[i] = u8(got[i]);
    return cont_done(k, int_from_i64(i64(got.size())));
}

R bufinto_start(const CallArgs &a, Value &out, bool one)
{
    Str who = one ? Str("readinto1") : Str("readinto");
    if (!io_self(a, IO_BASE, who) || !meth_args(a, who, 1, 1))
        return R::Err;
    u8 *p   = nullptr;
    usize n = 0;
    if (!io_writable_span(a.args[1], p, n))
        return R::Err;
    if (start_cont(bufinto_step, a.args[0], out) != R::Ok)
        return R::Err;
    cont_of(out)->s[1] = a.args[1];
    cont_of(out)->j    = one;
    return R::Ok;
}

R bb_readinto(const CallArgs &a, Value &out)
{
    return bufinto_start(a, out, false);
}

R bb_readinto1(const CallArgs &a, Value &out)
{
    return bufinto_start(a, out, true);
}

constexpr Method BUFBASE_METHODS[] = {
    { "read", bb_read },         { "read1", bb_read1 },         { "readinto", bb_readinto },
    { "readinto1", bb_readinto1 }, { "write", bb_write },       { "detach", bb_detach },
};

// ------------------------------------------------------------ _TextIOBase

R tb_read(const CallArgs &a, Value &)
{
    return bb_unsupported(a, "read");
}

R tb_readline(const CallArgs &a, Value &)
{
    return bb_unsupported(a, "readline");
}

R tb_write(const CallArgs &a, Value &)
{
    return bb_unsupported(a, "write");
}

R tb_detach(const CallArgs &a, Value &)
{
    return bb_unsupported(a, "detach");
}

constexpr Method TEXTBASE_METHODS[] = {
    { "read", tb_read },
    { "readline", tb_readline },
    { "write", tb_write },
    { "detach", tb_detach },
};

R textbase_getattr(Value v, StrObj *name, Value &out)
{
    Str n = name->str();
    if (n == "encoding" || n == "newlines" || n == "errors") {
        out = value_none();
        return R::Ok;
    }
    return base_getattr(v, name, out);
}

// ---------------------------------------------------------- the constructors

R make_base(const Type *t, u8 kind, Value &out)
{
    IoObj *o = static_cast<IoObj *>(obj_alloc(t, sizeof(IoObj)));
    if (!o)
        return oom();
    o->dict       = Value();
    o->kind       = kind;
    o->closed     = false;
    o->inside     = false;
    o->finalizing = false;
    out           = obj_value(o);
    return R::Ok;
}

R new_iobase(const CallArgs &a, Value &out)
{
    (void)a;
    return make_base(&iobase_type, IO_BASE, out);
}

R new_rawbase(const CallArgs &a, Value &out)
{
    (void)a;
    return make_base(&rawbase_type, IO_RAWBASE, out);
}

R new_bufbase(const CallArgs &a, Value &out)
{
    (void)a;
    return make_base(&bufbase_type, IO_BUFBASE, out);
}

R new_textbase(const CallArgs &a, Value &out)
{
    (void)a;
    return make_base(&textbase_type, IO_TEXTBASE, out);
}

// ------------------------------------------------------------------ open()

// text_encoding(encoding, stacklevel=2)
R b_text_encoding(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "encoding", "stacklevel" };
    Value v[2];
    if (!fn_take(a, "text_encoding", NAMES, 1, v))
        return R::Err;
    if (!is_none(v[0])) {
        out = v[0];
        return R::Ok;
    }
    out = str_new("utf-8");
    return out.is_nil() ? R::Err : R::Ok;
}

// What open() decided, carried from the FileIO to the layers above it.
enum : u32 {
    OPEN_TEXT     = 1,
    OPEN_BINARY   = 2,
    OPEN_UPDATING = 4,
    OPEN_WRITING  = 8, // w, a or x
    OPEN_READING  = 16,
    OPEN_LINEBUF  = 32,
};

// s[0] the FileIO once made, s[1] the buffer, s[2] mode, s[3] encoding,
// s[4] errors, s[5] newline, s[6] the text; x[0] buffering, j the flags.
// s[7] the FileIO, or the continuation that makes one; s[6], until it is
// made, the warning owed first.
enum : u32 { OP_WARN, OP_START, OP_RAW, OP_ISATTY, OP_LAYERS, OP_TEXT, OP_FAILED };

R open_step(ContObj *k, Value in)
{
    switch (k->i) {
    case OP_WARN:
        k->i = OP_START;
        if (!k->s[6].is_nil()) {
            Root w{ k->s[6] };
            k->s[6] = Value();
            return cont_await(k, w.v);
        }
        [[fallthrough]];
    case OP_START:
        k->i = OP_RAW;
        return cont_await(k, k->s[7]);
    case OP_RAW:
        // The FileIO's answer.
        k->s[0] = in;
        k->i    = OP_ISATTY;
        if (k->x[0] == 1 || (k->x[0] < 0)) {
            // Line buffering is what a terminal gets; the descriptor knows.
            k->catching = CATCH_ANY;
            return cont_method(k, in, "isatty");
        }
        in = value_bool(false);
        [[fallthrough]];
    case OP_ISATTY: {
        k->catching = CATCH_NONE;
        if (in.is_nil()) {
            // isatty failed: close and raise.
            k->i = OP_FAILED;
            return cont_method(k, k->s[0], "close");
        }
        i64 buffering = k->x[0];
        bool linebuf  = false;
        if (buffering == 1 || (buffering < 0 && is_true(in))) {
            buffering = -1;
            linebuf   = true;
        }
        if (buffering < 0)
            buffering = DEFAULT_BUFFER_SIZE;
        u32 f = k->j;
        if (buffering == 0) {
            if (f & OPEN_BINARY)
                return cont_done(k, k->s[0]);
            k->caught = Value();
            err_set("ValueError", "can't have unbuffered text I/O");
            k->caught = exc_pending();
            err_clear();
            k->i = OP_FAILED;
            return cont_method(k, k->s[0], "close");
        }
        if (linebuf)
            k->j |= OPEN_LINEBUF;
        Value args[2] = { k->s[0], int_from_i64(buffering) };
        CallArgs ca;
        ca.args  = args;
        ca.nargs = 2;
        Value buf;
        R r = (f & OPEN_UPDATING) ? bufrandom_new(ca, buf)
              : (f & OPEN_WRITING) ? bufwriter_new(ca, buf)
                                   : bufreader_new(ca, buf);
        if (r != R::Ok) {
            k->caught = exc_pending();
            err_clear();
            k->i = OP_FAILED;
            return cont_method(k, k->s[0], "close");
        }
        k->i = OP_LAYERS;
        if (is_cont(buf))
            return cont_await(k, buf);
        in = buf;
        [[fallthrough]];
    }
    case OP_LAYERS: {
        k->s[1] = in;
        if (k->j & OPEN_BINARY)
            return cont_done(k, in);
        Value args[5] = { in, k->s[3], k->s[4], k->s[5], value_bool(k->j & OPEN_LINEBUF) };
        CallArgs ca;
        ca.args  = args;
        ca.nargs = 5;
        Value text;
        if (textio_new(ca, text) != R::Ok) {
            k->caught = exc_pending();
            err_clear();
            k->i = OP_FAILED;
            return cont_method(k, k->s[1], "close");
        }
        k->i = OP_TEXT;
        if (is_cont(text)) {
            k->catching = CATCH_ANY;
            return cont_await(k, text);
        }
        in = text;
        [[fallthrough]];
    }
    case OP_TEXT: {
        k->catching = CATCH_NONE;
        if (in.is_nil()) {
            k->i = OP_FAILED;
            return cont_method(k, k->s[1], "close");
        }
        k->s[6]   = in;
        StrObj *m = str_intern("mode");
        if (!m || io_dict_set(in, m, k->s[2]) != R::Ok)
            return err_pending() ? R::Err : err_set("MemoryError", "out of memory");
        return cont_done(k, in);
    }
    case OP_FAILED:
    default: {
        Root e{ k->caught };
        k->caught = Value();
        return e.v.is_nil() ? (err_pending() ? R::Err : err_set("OSError", "open failed"))
                            : err_set_value(e.v);
    }
    }
}

} // namespace

bool is_fd_value(Value v)
{
    i64 n = 0;
    return v.is_int() || is_bool(v) || (v.is_obj() && v.obj()->type == &int_type) ||
           (is_inst(v) && as_int_arg(v, n));
}

R io_open(const CallArgs &a, Value &out)
{
    // The builtin can be reached before `_io` is imported, and its types'
    // methods are put in place when it is.
    if (builtin_module("_io").is_nil())
        return err_pending() ? R::Err : err_set("ImportError", "_io");
    constexpr Str NAMES[] = { "file",    "mode",    "buffering", "encoding",
                              "errors",  "newline", "closefd",   "opener" };
    Value v[8];
    if (!fn_take(a, "open", NAMES, 1, v))
        return R::Err;
    R conv;
    if (!is_fd_value(v[0]) && fs_convert(v, 8, 1, io_open, out, conv))
        return conv;
    Value file = v[0];
    if (!is_fd_value(file)) {
        file = fs_path_of(file);
        if (file.is_nil()) {
            if (err_pending())
                err_clear();
            Buf<128> b;
            b.put("expected str, bytes or os.PathLike object, not ").put(type_name(v[0]));
            return err_set("TypeError", b.str());
        }
    }
    Str mode = "r";
    if (!v[1].is_nil()) {
        if (!is_str(v[1]))
            return err_set2("TypeError", "open() argument 'mode' must be str, not",
                            type_name(v[1]));
        mode = str_of(v[1])->str();
    }
    i64 buffering = -1;
    if (!v[2].is_nil() && !as_int_arg(v[2], buffering))
        return err_set2("TypeError", "'buffering' must be an integer, not", type_name(v[2]));
    Value encoding = v[3].is_nil() ? value_none() : v[3];
    Value errors   = v[4].is_nil() ? value_none() : v[4];
    Value newline  = v[5].is_nil() ? value_none() : v[5];
    if (!is_none(encoding) && !is_str(encoding))
        return err_set2("TypeError", "open() argument 'encoding' must be str or None, not",
                        type_name(encoding));
    if (!is_none(errors) && !is_str(errors))
        return err_set2("TypeError", "open() argument 'errors' must be str or None, not",
                        type_name(errors));

    bool creating = false, reading = false, writing = false, appending = false;
    bool updating = false, text = false, binary = false;
    u32 seen = 0;
    for (usize i = 0; i < mode.size(); i++) {
        const char c  = mode[i];
        constexpr Str ALL = "axrwb+t";
        usize at      = 0;
        while (at < ALL.size() && ALL[at] != c)
            at++;
        if (at == ALL.size() || (seen >> at) & 1) {
            Buf<96> b;
            b.put("invalid mode: '").put(mode).put("'");
            return err_set("ValueError", b.str());
        }
        seen |= 1u << at;
        creating |= c == 'x';
        reading |= c == 'r';
        writing |= c == 'w';
        appending |= c == 'a';
        updating |= c == '+';
        text |= c == 't';
        binary |= c == 'b';
    }
    if (text && binary)
        return err_set("ValueError", "can't have text and binary mode at once");
    if (int(creating) + int(reading) + int(writing) + int(appending) > 1)
        return err_set("ValueError", "must have exactly one of create/read/write/append mode");
    if (!(creating || reading || writing || appending))
        return err_set("ValueError", "Must have exactly one of create/read/write/append mode "
                                     "and at most one plus");
    if (binary && !is_none(encoding))
        return err_set("ValueError", "binary mode doesn't take an encoding argument");
    if (binary && !is_none(errors))
        return err_set("ValueError", "binary mode doesn't take an errors argument");
    if (binary && !is_none(newline))
        return err_set("ValueError", "binary mode doesn't take a newline argument");
    Root warning;
    if (binary && buffering == 1) {
        warning = warn_cont("RuntimeWarning",
                            "line buffering (buffering=1) isn't supported in binary mode, the "
                            "default buffer size will be used",
                            1);
        if (warning.v.is_nil())
            return R::Err;
        buffering = -1;
    }
    Buf<8> rawmode;
    if (creating)
        rawmode.put('x');
    if (reading)
        rawmode.put('r');
    if (writing)
        rawmode.put('w');
    if (appending)
        rawmode.put('a');
    if (updating)
        rawmode.put('+');

    Root rm{ str_new(rawmode.str()) };
    Root full{ v[1].is_nil() ? str_new("r") : v[1] };
    if (rm.v.is_nil() || full.v.is_nil())
        return R::Err;
    Value fargs[4] = { file, rm.v, v[6].is_nil() ? value_bool(true) : v[6],
                       v[7].is_nil() ? value_none() : v[7] };
    Roots pin{ fargs, 4 };
    CallArgs ca;
    ca.args  = fargs;
    ca.nargs = 4;
    Root raw;
    if (fileio_new(ca, raw.v) != R::Ok)
        return R::Err;

    Root kv{ cont_new(open_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[2]    = full.v;
    k->s[3]    = encoding;
    k->s[4]    = errors;
    k->s[5]    = newline;
    k->x[0]    = buffering;
    k->j       = (binary ? OPEN_BINARY : OPEN_TEXT) | (updating ? OPEN_UPDATING : 0) |
           ((writing || appending || creating) ? OPEN_WRITING : 0) | (reading ? OPEN_READING : 0);
    k->s[7]    = raw.v;
    k->s[6]    = warning.v;
    k->i       = OP_WARN;
    out        = kv.v;
    return R::Ok;
}

namespace {

// open_code(path): open(path, "rb").
R b_open_code(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "path" };
    Value v[1];
    if (!fn_take(a, "open_code", NAMES, 1, v))
        return R::Err;
    if (!is_str(v[0]))
        return err_set2("TypeError", "open_code() argument 'path' must be str, not",
                        type_name(v[0]));
    Root mode{ str_new("rb") };
    if (mode.v.is_nil())
        return R::Err;
    Value args[2] = { v[0], mode.v };
    CallArgs ca;
    ca.args  = args;
    ca.nargs = 2;
    return io_open(ca, out);
}

// ------------------------------------------------------------ the program's end

// s[0] the list of streams; j the next. What fails is reported, and the rest
// still closed.
R exit_step_io(ContObj *k, Value in)
{
    ListObj *l = list_of(k->s[0]);
    if (k->i && in.is_nil() && !k->caught.is_nil()) {
        Root e{ k->caught };
        k->caught = Value();
        String *err = vm_errout();
        if (err) {
            err->append("Exception ignored while finalizing file ");
            String r;
            if (py_repr(l->items[k->j - 1], r) == R::Ok)
                err->append(r.str());
            err_clear();
            err->append(":\n");
            exc_line(e.v, *err);
            err->push('\n');
        }
        err_clear();
    }
    k->i = 1;
    while (k->j < l->items.size()) {
        Value s    = l->items[k->j++];
        IoObj *io  = io_cast(s);
        k->catching = CATCH_ANY;
        if (io->kind == IO_TEXT && textio_is_std(s))
            return cont_method(k, s, "flush");
        if (io->closed)
            continue;
        return cont_method(k, s, "close");
    }
    k->catching = CATCH_NONE;
    return cont_done(k, value_none());
}

} // namespace

bool io_exit_pending()
{
    return home && !home->tracked.empty();
}

Value io_exit_runner()
{
    if (!io_exit_pending())
        return Value();
    ListObj *l = list_new();
    if (!l)
        return oom(), Value();
    Root rl{ obj_value(l) };
    // Text first, then the buffers under it, then what is raw: a layer's
    // close closes the one under it, and the order keeps that from mattering.
    constexpr u8 ORDER[] = { IO_TEXT, IO_PAIR, IO_BUFFERED, IO_FILE };
    for (u8 kind : ORDER)
        for (Obj *o : home->tracked) {
            IoObj *io = static_cast<IoObj *>(o);
            if (io->kind != kind || io->inside)
                continue;
            if (kind != IO_TEXT && io->closed)
                continue;
            if (!list_push(list_of(rl.v), obj_value(o)))
                return oom(), Value();
        }
    Root kv{ cont_new(exit_step_io) };
    if (kv.v.is_nil())
        return Value();
    cont_of(kv.v)->s[0] = rl.v;
    return kv.v;
}

Value io_del(Value v)
{
    IoObj *io = io_cast(v);
    if (io->inside || io->closed)
        return Value();
    Root rv{ v };
    Root fn{ native_new("__del__", ib_del) };
    return fn.v.is_nil() ? Value() : method_new(fn.v, rv.v);
}

// --------------------------------------------------------------- the types

constexpr Type iobase_type{ .name     = "_io._IOBase",
                            .trace    = io_trace,
                            .repr     = base_repr,
                            .iter     = base_iter,
                            .getattr  = base_getattr,
                            .setattr  = io_dict_set,
                            .vmnext   = true };

constexpr Type rawbase_type{ .name    = "_io._RawIOBase",
                             .trace   = io_trace,
                             .repr    = base_repr,
                             .iter    = base_iter,
                             .getattr = base_getattr,
                             .setattr = io_dict_set,
                             .base    = &iobase_type,
                             .vmnext  = true };

constexpr Type bufbase_type{ .name    = "_io._BufferedIOBase",
                             .trace   = io_trace,
                             .repr    = base_repr,
                             .iter    = base_iter,
                             .getattr = base_getattr,
                             .setattr = io_dict_set,
                             .base    = &iobase_type,
                             .vmnext  = true };

constexpr Type textbase_type{ .name    = "_io._TextIOBase",
                              .trace   = io_trace,
                              .repr    = base_repr,
                              .iter    = base_iter,
                              .getattr = textbase_getattr,
                              .setattr = io_dict_set,
                              .base    = &iobase_type,
                              .vmnext  = true };

bool io_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    if (!method_install(&iobase_type, IOBASE_METHODS) ||
        !method_install(&rawbase_type, RAWBASE_METHODS) ||
        !method_install(&bufbase_type, BUFBASE_METHODS) ||
        !method_install(&textbase_type, TEXTBASE_METHODS))
        return false;
    // __next__ and friends call Python, so a class that inherits them has to
    // be stepped through the VM.
    {
        Root w{ type_wrap(&iobase_type) };
        if (w.v.is_nil())
            return false;
        constexpr Str PYLIKE[] = { "__next__", "__iter__", "__enter__", "__exit__", "readline",
                                   "readlines", "writelines", "close", "flush", "tell",
                                   "__del__" };
        for (Str n : PYLIKE) {
            StrObj *k = str_intern(n);
            Value fn;
            if (!k || dict_get(static_cast<DictObj *>(type_obj(w.v)->dict.obj()), obj_value(k),
                               fn) != R::Ok)
                return false;
            fn.obj()->flags |= OBJ_PYLIKE;
        }
    }
    if (!fileio_methods() || !buffered_methods() || !text_methods() || !mem_methods())
        return false;
    if (!mod_type(d, &iobase_type, new_iobase) || !mod_type(d, &rawbase_type, new_rawbase) ||
        !mod_type(d, &bufbase_type, new_bufbase) || !mod_type(d, &textbase_type, new_textbase) ||
        !mod_type(d, &fileio_type, fileio_new) || !mod_type(d, &bytesio_type, bytesio_new) ||
        !mod_type(d, &stringio_type, stringio_new) ||
        !mod_type(d, &bufreader_type, bufreader_new) ||
        !mod_type(d, &bufwriter_type, bufwriter_new) ||
        !mod_type(d, &bufrandom_type, bufrandom_new) || !mod_type(d, &bufpair_type, bufpair_new) ||
        !mod_type(d, &textio_type, textio_new) || !mod_type(d, &nldecoder_type, nldecoder_new))
        return false;
    // open is the builtin itself, so `io.open is open`.
    {
        StrObj *n = str_intern("open");
        Value fn;
        DictObj *b = builtins_dict();
        if (!n || !b || dict_get(b, obj_value(n), fn) != R::Ok || !mod_put(d, "open", fn))
            return err_pending() ? false : oom() == R::Ok;
    }
    constexpr ModDef DEFS[] = {
        { "open_code", b_open_code },
        { "text_encoding", b_text_encoding },
    };
    if (!mod_defs(d, DEFS))
        return false;
    Root un{ io_unsupported_type() };
    Root blocking{ exc_type_value(exc_find("BlockingIOError")) };
    if (un.v.is_nil() || blocking.v.is_nil() || !mod_put(d, "UnsupportedOperation", un.v) ||
        !mod_put(d, "BlockingIOError", blocking.v) ||
        !mod_int(d, "DEFAULT_BUFFER_SIZE", DEFAULT_BUFFER_SIZE))
        return false;
    return true;
}
