// FileIO: the raw layer over a descriptor.
//
// A write to 1 or 2 goes into the buffer the VM already keeps for print and
// the traceback, which the driver writes out before it parks for anything; so
// sys.stdout's layers write through, the three streams stay in order, and a
// program printing a line at a time still costs one system call per four
// kilobytes. Everything else is a system call the driver makes.
#include "io.h"

#include "exc.h"
#include "gc.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "kernel/sysabi.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "type.h"
#include "vm.h"

namespace {

R oom();

} // namespace

Str file_mode(Value v);

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

constexpr i64 DEFAULT_BUFFER_SIZE = 128 * 1024;

struct FileObj : IoObj {
    Value name;   // what open() was given, or Nil
    i32 fd;       // -1 once closed
    i8 seekable;  // -1 unknown
    bool readable, writable, appending, created, truncate, closefd;
    bool sink;    // 1 or 2: the VM's own buffer
    bool regular; // a file in the store: a short read is only ever its end
    i64 blksize;
    i64 size;     // at open, or -1
};

FileObj *file_of(Value v)
{
    return static_cast<FileObj *>(v.obj());
}

void file_trace(Obj *o)
{
    io_trace(o);
    gc_mark(static_cast<FileObj *>(o)->name);
}

void file_fini(Obj *o)
{
    io_untrack(o);
}

R file_repr(Value v, String &out)
{
    FileObj *f = file_of(v);
    if (!out.append("<_io.FileIO "))
        return oom();
    if (f->fd < 0)
        return out.append("[closed]>") ? R::Ok : oom();
    Value name;
    StrObj *n = str_intern("name");
    R had     = n ? io_dict_get(v, n, name) : R::NotImpl;
    if (had == R::Err)
        return R::Err;
    if (had == R::Ok) {
        if (!out.append("name="))
            return oom();
        if (!repr_enter(v))
            return err_set("RuntimeError", "reentrant call inside FileIO.__repr__");
        R r = py_repr(name, out);
        repr_leave();
        if (r != R::Ok)
            return R::Err;
    } else {
        char tmp[24];
        if (!out.append("fd=") || !out.append(int_text(tmp, sizeof tmp, f->fd)))
            return oom();
    }
    return out.append(" mode='") && out.append(file_mode(v)) && out.append("' closefd=") &&
                   out.append(f->closefd ? Str("True") : Str("False")) && out.push('>')
               ? R::Ok
               : oom();
}

} // namespace

Str file_mode(Value v)
{
    FileObj *f = file_of(v);
    if (f->created)
        return f->readable ? Str("xb+") : Str("xb");
    if (f->appending)
        return f->readable ? Str("ab+") : Str("ab");
    if (f->readable)
        return f->writable ? (f->truncate ? Str("wb+") : Str("rb+")) : Str("rb");
    return Str("wb");
}

namespace {

R file_getattr(Value v, StrObj *name, Value &out)
{
    FileObj *f = file_of(v);
    Str n      = name->str();
    if (n == "closed")
        out = value_bool(f->fd < 0);
    else if (n == "closefd")
        out = value_bool(f->closefd);
    else if (n == "mode")
        out = str_new(file_mode(v));
    else if (n == "_blksize")
        out = int_from_i64(f->blksize);
    else if (n == "_finalizing")
        out = value_bool(f->finalizing);
    else
        return io_dict_get(v, name, out);
    return out.is_nil() ? R::Err : R::Ok;
}

FileObj *self_file(const CallArgs &a, Str who)
{
    return static_cast<FileObj *>(io_self(a, IO_FILE, who));
}

// fileio.c's own words, which have no full stop.
R file_closed()
{
    return err_set("ValueError", "I/O operation on closed file");
}

R not_open_for(Str what)
{
    Buf<48> b;
    b.put("File not open for ").put(what);
    return io_unsupported(b.str());
}

// ------------------------------------------------------------- making one

// s[0] self, s[1] the path's bytes, s[2] the name, s[3] the opener; j the
// open flags for the store; x[0] the flags as Linux spells them.
enum : u32 { FI_START, FI_OPENER, FI_OPENED, FI_STAT, FI_STATED, FI_SEEKED, FI_FAILED };

// A failure after the descriptor was ours closes it, then raises.
R init_fail(ContObj *k)
{
    FileObj *f = file_of(k->s[0]);
    if (f->fd >= 0 && k->x[1]) {
        k->caught = exc_pending();
        err_clear();
        k->x[2] = f->fd;
        f->fd   = -1;
        k->i    = FI_FAILED;
        SysReq q;
        q.op = SysOp::Close;
        q.fd = i32(k->x[2]);
        return cont_sys(k, q);
    }
    f->fd = -1;
    return R::Err;
}

R init_step(ContObj *k, Value in)
{
    FileObj *f = file_of(k->s[0]);
    R r;
    for (;;) {
        switch (k->i & ~SYS_TURN_BITS) {
        case FI_START:
            if (!k->s[4].is_nil()) {
                Root w{ k->s[4] };
                k->s[4] = Value();
                return cont_await(k, w.v);
            }
            if (f->fd >= 0) {
                k->i = FI_OPENED;
                continue;
            }
            if (!is_none(k->s[3])) {
                k->i = FI_OPENER;
                return cont_call(k, k->s[3], k->s[2], 2, Value::of_int(i32(k->x[0])));
            }
            {
                SysReq q;
                q.op    = SysOp::Open;
                q.path  = static_cast<BytesObj *>(k->s[1].obj())->str();
                q.flags = k->j;
                if (!sys_turn(k, q, r, k->s[2])) {
                    sys_open_retry(k, r, FI_START, &k->x[3]);
                    return r;
                }
                f->fd   = i32(vm_sys_answer().n);
                k->x[1] = 1; // ours to close
                k->i    = FI_STAT;
            }
            continue;
        case FI_OPENER: {
            i64 fd = 0;
            if (!is_fd_value(in) || !as_int_arg(in, fd))
                return err_set("TypeError", "expected integer from opener");
            if (fd < 0) {
                char tmp[24];
                Buf<48> b;
                b.put("opener returned ").put(int_text(tmp, sizeof tmp, fd));
                return err_set("ValueError", b.str());
            }
            f->fd   = i32(fd);
            k->x[1] = 1;
            k->i    = FI_STAT;
            continue;
        }
        case FI_OPENED:
            k->i = FI_STAT;
            continue;
        case FI_STAT: {
            // The store refuses to describe 0, 1 and 2; they are streams.
            if (f->fd < i32(SYS_FD_MIN)) {
                f->seekable = 0;
                k->i        = FI_STATED;
                continue;
            }
            SysReq q;
            q.op = SysOp::FStat;
            q.fd = f->fd;
            if (!(k->i & SYS_TURN_BITS)) {
                sys_turn(k, q, r);
                return r;
            }
            k->i &= ~SYS_TURN_BITS;
            SysAns &a = vm_sys_answer();
            if (!a.ok) {
                // A pipe or a fetched body has no size; a descriptor that is
                // not open at all is the error.
                k->i = FI_STATED;
                if (a.err == Error::Unsupported)
                    continue;
                err_errno(9);
                return init_fail(k);
            }
            if (a.kind == SYS_KIND_DIR) {
                err_errno(21, k->s[2]);
                return init_fail(k);
            }
            f->size    = i64(a.size);
            f->regular = true;
            f->blksize = 4096;
            k->i       = FI_STATED;
            continue;
        }
        case FI_STATED: {
            if (!k->s[2].is_nil()) {
                StrObj *n = str_intern("name");
                if (!n || io_dict_set(k->s[0], n, k->s[2]) != R::Ok)
                    return init_fail(k);
            }
            if (!f->appending || f->fd < i32(SYS_FD_MIN)) {
                k->i = FI_SEEKED;
                continue;
            }
            SysReq q;
            q.op     = SysOp::Seek;
            q.fd     = f->fd;
            q.whence = SYS_SEEK_END;
            if (!sys_turn(k, q, r)) {
                if (r == R::Err)
                    err_clear();
                else
                    return r;
            }
            k->i = FI_SEEKED;
            continue;
        }
        case FI_SEEKED:
            if (f->closefd && f->fd >= i32(SYS_FD_MIN))
                io_track(f);
            return cont_done(k, k->s[0]);
        case FI_FAILED:
        default: {
            Root e{ k->caught };
            k->caught = Value();
            return e.v.is_nil() ? R::Err : err_set_value(e.v);
        }
        }
    }
}

// FileIO.__init__(file, mode='r', closefd=True, opener=None), on an object
// that already exists.
R file_init(Value self, const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "file", "mode", "closefd", "opener" };
    Value v[4];
    if (!fn_take(a, "FileIO", NAMES, 1, v))
        return R::Err;
    Root rs{ self };
    FileObj *f = file_of(rs.v);
    if (f->fd >= 0) {
        // Initialised again: the old descriptor is forgotten.
        io_untrack(f);
        f->fd = -1;
    }
    Value file = v[0];
    if (is_float(file))
        return err_set("TypeError", "integer argument expected, got float");
    i64 fd = -1;
    Root path;
    Root warning;
    if (is_bool(file)) {
        warning = warn_cont("RuntimeWarning", "bool is used as a file descriptor", 1);
        if (warning.v.is_nil())
            return R::Err;
    }
    if (is_fd_value(file)) {
        if (!as_int_arg(file, fd))
            return R::Err;
        if (fd < 0)
            return err_set("ValueError", "negative file descriptor");
        if (fd > 2147483647)
            return err_set("OverflowError", "fd is greater than maximum");
    } else {
        String b;
        Value p = fs_path_of(file);
        if (p.is_nil()) {
            if (!err_pending())
                err_set2("TypeError", "expected str, bytes or os.PathLike object, not",
                         type_name(file));
            return R::Err;
        }
        if (!fs_bytes(p, "FileIO", b))
            return R::Err;
        path = bytes_new(b.str());
        if (path.v.is_nil())
            return R::Err;
    }
    Str mode = "r";
    if (!v[1].is_nil()) {
        if (!is_str(v[1]))
            return err_set2("TypeError", "FileIO() argument 'mode' must be str, not",
                            type_name(v[1]));
        mode = str_of(v[1])->str();
    }
    bool rwa = false, plus = false;
    bool readable = false, writable = false, appending = false, created = false, trunc = false;
    for (usize i = 0; i < mode.size(); i++) {
        switch (mode[i]) {
        case 'x':
        case 'r':
        case 'w':
        case 'a':
            if (rwa)
                goto bad_mode;
            rwa       = true;
            created   = mode[i] == 'x';
            readable  = mode[i] == 'r';
            writable  = mode[i] != 'r';
            appending = mode[i] == 'a';
            trunc     = mode[i] == 'w';
            break;
        case '+':
            if (plus)
                goto bad_mode;
            readable = writable = plus = true;
            break;
        case 'b':
            break;
        default: {
            Buf<96> b;
            b.put("invalid mode: ").put(mode);
            return err_set("ValueError", b.str());
        }
        }
    }
    if (!rwa) {
    bad_mode:
        return err_set("ValueError", "Must have exactly one of create/read/write/append mode and "
                                     "at most one plus");
    }
    bool closefd = v[2].is_nil() || py_truth(v[2]);
    if (fd < 0 && !closefd)
        return err_set("ValueError", "Cannot use closefd=False with file name");

    i64 lflags = (readable && writable) ? 2 : writable ? 1 : 0;
    u32 sflags = (readable ? SYS_O_READ : 0) | (writable ? SYS_O_WRITE : 0);
    if (created) {
        lflags |= 0300;
        sflags |= SYS_O_CREATE | SYS_O_EXCL;
    } else if (trunc) {
        lflags |= 01100;
        sflags |= SYS_O_CREATE | SYS_O_TRUNC;
    } else if (appending) {
        lflags |= 02100;
        sflags |= SYS_O_CREATE | SYS_O_APPEND;
    }
    lflags |= 02000000; // O_CLOEXEC

    f            = file_of(rs.v);
    f->fd        = i32(fd);
    f->readable  = readable;
    f->writable  = writable;
    f->appending = appending;
    f->created   = created;
    f->truncate  = trunc;
    f->closefd   = closefd;
    f->closed    = false;
    f->seekable  = -1;
    f->size      = -1;
    f->blksize   = DEFAULT_BUFFER_SIZE;
    f->sink      = writable && (fd == SYS_STDOUT || fd == SYS_STDERR);
    f->name      = file;

    Root kv{ cont_new(init_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = rs.v;
    k->s[1]    = path.v;
    k->s[2]    = file;
    k->s[3]    = v[3].is_nil() ? value_none() : v[3];
    k->s[4]    = warning.v;
    k->j       = sflags;
    k->x[0]    = lflags;
    out        = kv.v;
    return R::Ok;
}

Value file_alloc()
{
    FileObj *f = static_cast<FileObj *>(obj_alloc(&fileio_type, sizeof(FileObj)));
    if (!f)
        return oom(), Value();
    f->dict       = Value();
    f->kind       = IO_FILE;
    f->closed     = false;
    f->inside     = false;
    f->finalizing = false;
    f->name       = Value();
    f->fd         = -1;
    f->seekable   = -1;
    f->readable = f->writable = f->appending = f->created = f->truncate = false;
    f->closefd  = true;
    f->sink     = false;
    f->regular  = false;
    f->blksize  = DEFAULT_BUFFER_SIZE;
    f->size     = -1;
    return obj_value(f);
}

R fm_init(const CallArgs &a, Value &out)
{
    FileObj *f = self_file(a, "__init__");
    if (!f)
        return R::Err;
    CallArgs rest = a;
    rest.args++;
    rest.nargs--;
    Value self = method_self(a.args[0]);
    Root kv;
    if (file_init(self, rest, kv.v) != R::Ok)
        return R::Err;
    // __init__ answers None: the continuation's answer is dropped.
    Root wrap{ cont_new([](ContObj *k, Value) -> R {
        if (k->i++ == 0)
            return cont_await(k, k->s[0]);
        return cont_done(k, value_none());
    }) };
    if (wrap.v.is_nil())
        return R::Err;
    cont_of(wrap.v)->s[0] = kv.v;
    out                   = wrap.v;
    return R::Ok;
}

// ------------------------------------------------------------- reading

// s[0] self; x[0] how many; s[1] the bytearray readall fills, or readinto's
// buffer; j what: 0 read, 1 readall, 2 readinto.
enum : u32 { RD_READ, RD_ALL, RD_INTO };

R read_step(ContObj *k, Value)
{
    FileObj *f = file_of(k->s[0]);
    if (f->fd < 0)
        return file_closed();
    SysReq q;
    q.op = SysOp::Read;
    q.fd = f->fd;
    u8 *p   = nullptr;
    usize n = 0;
    if (k->j == RD_INTO) {
        if (!io_writable_span(k->s[1], p, n))
            return R::Err;
        q.max = u32(n > SYS_READ_MAX ? SYS_READ_MAX : n);
    } else if (k->j == RD_READ) {
        i64 want = k->x[0];
        if (!k->s[1].is_nil())
            want -= i64(array_of(k->s[1])->data.size());
        q.max = u32(want > SYS_READ_MAX ? SYS_READ_MAX : want);
    } else {
        q.max = SYS_READ_MAX;
    }
    R r;
    if (!sys_turn(k, q, r))
        return r;
    SysAns &a = vm_sys_answer();
    Str got   = a.data.str();
    switch (k->j) {
    case RD_READ: {
        // A file in the store answers a read in full, as a disk file does:
        // one system call's worth at a time, until the end.
        bool more = f->regular && !got.empty() && i64(got.size()) < k->x[0];
        if (k->s[1].is_nil() && !more) {
            Value b = bytes_new(got);
            return b.is_nil() ? R::Err : cont_done(k, b);
        }
        if (k->s[1].is_nil()) {
            Value acc = bytearray_new(Str());
            if (acc.is_nil())
                return R::Err;
            k->s[1] = acc;
        }
        ArrayObj *acc = array_of(k->s[1]);
        if (!acc->data.reserve(acc->data.size() + got.size()))
            return oom();
        for (usize i = 0; i < got.size(); i++)
            acc->data.push(u8(got[i]));
        if (!got.empty() && i64(acc->data.size()) < k->x[0])
            return read_step(k, Value());
        Value b = bytes_new(acc->str());
        return b.is_nil() ? R::Err : cont_done(k, b);
    }
    case RD_INTO:
        // The buffer may have moved while this was parked.
        if (!io_writable_span(k->s[1], p, n))
            return R::Err;
        if (got.size() > n)
            got = got.substr(0, n);
        for (usize i = 0; i < got.size(); i++)
            p[i] = u8(got[i]);
        return cont_done(k, int_from_i64(i64(got.size())));
    default: {
        if (got.empty()) {
            Value b = bytes_new(array_of(k->s[1])->str());
            return b.is_nil() ? R::Err : cont_done(k, b);
        }
        ArrayObj *acc = array_of(k->s[1]);
        if (!acc->data.reserve(acc->data.size() + got.size()))
            return oom();
        for (usize i = 0; i < got.size(); i++)
            acc->data.push(u8(got[i]));
        return read_step(k, Value());
    }
    }
}

R read_start(Value self, u32 what, i64 n, Value buf, Value &out)
{
    Root rs{ self }, rb{ buf };
    if (what == RD_ALL && rb.v.is_nil()) {
        rb = bytearray_new(Str());
        if (rb.v.is_nil())
            return R::Err;
    }
    Root kv{ cont_new(read_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = rs.v;
    k->s[1]    = rb.v;
    k->x[0]    = n;
    k->j       = what;
    out        = kv.v;
    return R::Ok;
}

bool readable_check(FileObj *f, R &r)
{
    if (f->fd < 0) {
        r = file_closed();
        return false;
    }
    if (!f->readable) {
        r = not_open_for("reading");
        return false;
    }
    return true;
}

R fm_read(const CallArgs &a, Value &out)
{
    FileObj *f = self_file(a, "read");
    if (!f || !meth_args(a, "read", 0, 1))
        return R::Err;
    R r;
    if (!readable_check(f, r))
        return r;
    i64 n = -1;
    if (a.nargs > 1 && !is_none(a.args[1]) && !as_int_arg(a.args[1], n))
        return err_set2("TypeError", "argument should be integer or None, not",
                        type_name(a.args[1]));
    Value self = method_self(a.args[0]);
    if (n < 0)
        return read_start(self, RD_ALL, 0, Value(), out);
    if (n == 0) {
        out = bytes_new(Str());
        return out.is_nil() ? R::Err : R::Ok;
    }
    return read_start(self, RD_READ, n, Value(), out);
}

R fm_readall(const CallArgs &a, Value &out)
{
    FileObj *f = self_file(a, "readall");
    if (!f || !meth_args(a, "readall", 0, 0))
        return R::Err;
    R r;
    if (!readable_check(f, r))
        return r;
    return read_start(method_self(a.args[0]), RD_ALL, 0, Value(), out);
}

R fm_readinto(const CallArgs &a, Value &out)
{
    FileObj *f = self_file(a, "readinto");
    if (!f || !meth_args(a, "readinto", 1, 1))
        return R::Err;
    R r;
    if (!readable_check(f, r))
        return r;
    u8 *p   = nullptr;
    usize n = 0;
    if (!io_writable_span(a.args[1], p, n))
        return R::Err;
    if (n == 0) {
        out = Value::of_int(0);
        return R::Ok;
    }
    return read_start(method_self(a.args[0]), RD_INTO, 0, a.args[1], out);
}

// ------------------------------------------------------------- writing

// s[0] self, s[1] the bytes.
R write_step(ContObj *k, Value)
{
    FileObj *f = file_of(k->s[0]);
    if (f->fd < 0)
        return file_closed();
    SysReq q;
    q.op   = SysOp::Write;
    q.fd   = f->fd;
    q.data = static_cast<BytesObj *>(k->s[1].obj())->str();
    R r;
    if (!sys_turn(k, q, r))
        return r;
    return cont_done(k, int_from_i64(vm_sys_answer().n));
}

} // namespace

// Bytes into a FileIO: the count written, or a ContObj.
R file_write(Value self, Str data, Value &out)
{
    FileObj *f = file_of(self);
    if (f->fd < 0)
        return file_closed();
    if (!f->writable)
        return not_open_for("writing");
    if (f->sink) {
        String *s = f->fd == SYS_STDOUT ? vm_out() : vm_errout();
        if (s && !s->append(data))
            return oom();
        out = int_from_i64(i64(data.size()));
        return out.is_nil() ? R::Err : R::Ok;
    }
    Root rs{ self };
    Root rb{ bytes_new(data) };
    if (rb.v.is_nil())
        return R::Err;
    Root kv{ cont_new(write_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = rs.v;
    cont_of(kv.v)->s[1] = rb.v;
    out                 = kv.v;
    return R::Ok;
}

FileFacts file_facts(Value v)
{
    FileObj *f = file_of(v);
    FileFacts r;
    r.closed   = f->fd < 0;
    r.readable = f->readable;
    r.writable = f->writable;
    r.seekable = f->fd >= 0 && f->fd < i32(SYS_FD_MIN) ? i8(0) : f->seekable;
    return r;
}

bool file_is_sink(Value v)
{
    return io_is_plain(v, IO_FILE) && file_of(v)->sink && file_of(v)->fd >= 0;
}

namespace {

R fm_write(const CallArgs &a, Value &out)
{
    FileObj *f = self_file(a, "write");
    if (!f || !meth_args(a, "write", 1, 1))
        return R::Err;
    Str data;
    if (!bytes_like(a.args[1], data))
        return err_not("a bytes-like object is required", a.args[1], true);
    return file_write(method_self(a.args[0]), data, out);
}

// ------------------------------------------------------------- positioning

// s[0] self; x[0] offset, x[1] whence; j what: 0 seek, 1 tell, 2 truncate
// (x[0] the size, or -1 to ask tell first), 3 seekable.
enum : u32 { PO_SEEK, PO_TELL, PO_TRUNC, PO_SEEKABLE };

R pos_step(ContObj *k, Value)
{
    FileObj *f = file_of(k->s[0]);
    if (f->fd < 0)
        return file_closed();
    SysReq q;
    q.fd = f->fd;
    R r;
    if (k->j == PO_TRUNC && k->x[0] >= 0) {
        q.op  = SysOp::Truncate;
        q.off = k->x[0];
        if (!sys_turn(k, q, r))
            return r;
        f->size = -1;
        return cont_done(k, int_from_i64(k->x[0]));
    }
    q.op     = SysOp::Seek;
    q.off    = k->j == PO_SEEK ? k->x[0] : 0;
    q.whence = k->j == PO_SEEK ? u32(k->x[1]) : SYS_SEEK_CUR;
    if (k->j == PO_SEEKABLE) {
        if (!(k->i & SYS_TURN_BITS)) {
            sys_turn(k, q, r);
            return r;
        }
        k->i &= ~SYS_TURN_BITS;
        f->seekable = vm_sys_answer().ok ? 1 : 0;
        return cont_done(k, value_bool(f->seekable));
    }
    if (!sys_turn(k, q, r)) {
        if (r == R::Err && vm_sys_answer().err == Error::Unsupported) {
            err_clear();
            return err_errno(29);
        }
        return r;
    }
    i64 at = vm_sys_answer().n;
    if (k->j == PO_TRUNC) {
        k->x[0] = at;
        return pos_step(k, Value());
    }
    return cont_done(k, int_from_i64(at));
}

R pos_start(Value self, u32 what, i64 off, i64 whence, Value &out)
{
    Root rs{ self };
    Root kv{ cont_new(pos_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = rs.v;
    k->x[0]    = off;
    k->x[1]    = whence;
    k->j       = what;
    out        = kv.v;
    return R::Ok;
}

R fm_seek(const CallArgs &a, Value &out)
{
    FileObj *f = self_file(a, "seek");
    if (!f || !meth_args(a, "seek", 1, 2))
        return R::Err;
    if (is_float(a.args[1]))
        return err_set("TypeError", "an integer is required");
    i64 off = 0, whence = 0;
    if (!as_int_arg(a.args[1], off))
        return err_set2("TypeError", "an integer is required", type_name(a.args[1]));
    if (a.nargs > 2 && !as_int_arg(a.args[2], whence))
        return err_set2("TypeError", "an integer is required", type_name(a.args[2]));
    if (f->fd < 0)
        return file_closed();
    if (whence < 0 || whence > 2)
        return err_errno(22);
    if (f->fd < i32(SYS_FD_MIN))
        return err_errno(29);
    return pos_start(method_self(a.args[0]), PO_SEEK, off, whence, out);
}

R fm_tell(const CallArgs &a, Value &out)
{
    FileObj *f = self_file(a, "tell");
    if (!f || !meth_args(a, "tell", 0, 0))
        return R::Err;
    if (f->fd < 0)
        return file_closed();
    if (f->fd < i32(SYS_FD_MIN))
        return err_errno(29);
    return pos_start(method_self(a.args[0]), PO_TELL, 0, 0, out);
}

R fm_truncate(const CallArgs &a, Value &out)
{
    FileObj *f = self_file(a, "truncate");
    if (!f || !meth_args(a, "truncate", 0, 1))
        return R::Err;
    if (f->fd < 0)
        return file_closed();
    if (!f->writable)
        return not_open_for("writing");
    i64 size = -1;
    if (a.nargs > 1 && !is_none(a.args[1])) {
        if (!as_int_arg(a.args[1], size))
            return err_set2("TypeError", "an integer is required", type_name(a.args[1]));
        if (size < 0)
            return err_errno(22);
    }
    if (f->fd < i32(SYS_FD_MIN))
        return err_errno(29);
    return pos_start(method_self(a.args[0]), PO_TRUNC, size, 0, out);
}

R fm_seekable(const CallArgs &a, Value &out)
{
    FileObj *f = self_file(a, "seekable");
    if (!f || !meth_args(a, "seekable", 0, 0))
        return R::Err;
    if (f->fd < 0)
        return file_closed();
    if (f->fd < i32(SYS_FD_MIN))
        f->seekable = 0;
    if (f->seekable >= 0) {
        out = value_bool(f->seekable);
        return R::Ok;
    }
    return pos_start(method_self(a.args[0]), PO_SEEKABLE, 0, 0, out);
}

R fm_readable(const CallArgs &a, Value &out)
{
    FileObj *f = self_file(a, "readable");
    if (!f || !meth_args(a, "readable", 0, 0))
        return R::Err;
    if (f->fd < 0)
        return file_closed();
    out = value_bool(f->readable);
    return R::Ok;
}

R fm_writable(const CallArgs &a, Value &out)
{
    FileObj *f = self_file(a, "writable");
    if (!f || !meth_args(a, "writable", 0, 0))
        return R::Err;
    if (f->fd < 0)
        return file_closed();
    out = value_bool(f->writable);
    return R::Ok;
}

R fm_fileno(const CallArgs &a, Value &out)
{
    FileObj *f = self_file(a, "fileno");
    if (!f || !meth_args(a, "fileno", 0, 0))
        return R::Err;
    if (f->fd < 0)
        return file_closed();
    out = Value::of_int(f->fd);
    return R::Ok;
}

R isatty_step(ContObj *k, Value)
{
    SysReq q;
    q.op = SysOp::Tty;
    q.fd = i32(k->x[0]);
    if (!(k->i & SYS_TURN_BITS)) {
        R r;
        sys_turn(k, q, r);
        return r;
    }
    k->i &= ~SYS_TURN_BITS;
    SysAns &a = vm_sys_answer();
    return cont_done(k, value_bool(a.ok && a.n));
}

R fm_isatty(const CallArgs &a, Value &out)
{
    FileObj *f = self_file(a, "isatty");
    if (!f || !meth_args(a, "isatty", 0, 0))
        return R::Err;
    if (f->fd < 0)
        return file_closed();
    bool known = false;
    bool tty   = sys_tty(f->fd, known);
    if (known) {
        out = value_bool(tty);
        return R::Ok;
    }
    Root kv{ cont_new(isatty_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->x[0] = f->fd;
    out                 = kv.v;
    return R::Ok;
}

// ------------------------------------------------------------- closing

// s[0] self, s[1] what flush raised; j the stage.
R close_step(ContObj *k, Value in)
{
    (void)in;
    FileObj *f = file_of(k->s[0]);
    if ((k->i & ~SYS_TURN_BITS) == 0) {
        k->i = 1;
        if (!k->s[2].is_nil()) {
            // A class of the program's own: its flush first.
            k->catching = CATCH_ANY;
            return cont_method(k, k->s[2], "flush");
        }
    }
    if ((k->i & ~SYS_TURN_BITS) == 1) {
        k->catching = CATCH_NONE;
        if (!k->caught.is_nil()) {
            k->s[1]   = k->caught;
            k->caught = Value();
        }
        f->closed = true;
        if (!f->closefd || f->fd < 0) {
            f->fd = -1;
            k->i  = 3;
        } else {
            k->x[0] = f->fd;
            f->fd   = -1;
            k->i    = 2;
        }
    }
    if ((k->i & ~SYS_TURN_BITS) == 2) {
        io_untrack(f);
        SysReq q;
        q.op = SysOp::Close;
        q.fd = i32(k->x[0]);
        R r;
        if (!sys_turn(k, q, r)) {
            if (r == R::Err && k->s[1].is_nil()) {
                k->s[1] = exc_pending();
                err_clear();
            } else if (r != R::Err) {
                return r;
            } else {
                err_clear();
            }
        }
        k->i = 3;
    }
    if (!k->s[1].is_nil())
        return err_set_value(k->s[1]);
    return cont_done(k, value_none());
}

R fm_close(const CallArgs &a, Value &out)
{
    FileObj *f = self_file(a, "close");
    if (!f || !meth_args(a, "close", 0, 0))
        return R::Err;
    if (f->fd < 0 && f->closed) {
        out = value_none();
        return R::Ok;
    }
    Value self = method_self(a.args[0]);
    // Nothing to flush and nothing to close: no call at all.
    if (!is_inst(a.args[0]) && (!f->closefd || f->fd < 0 || f->fd < i32(SYS_FD_MIN))) {
        f->closed = true;
        f->fd     = -1;
        io_untrack(f);
        out = value_none();
        return R::Ok;
    }
    Root rs{ self }, ri{ is_inst(a.args[0]) ? a.args[0] : Value() };
    Root kv{ cont_new(close_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = rs.v;
    cont_of(kv.v)->s[2] = ri.v;
    out                 = kv.v;
    return R::Ok;
}

R fm_dealloc_warn(const CallArgs &a, Value &out)
{
    if (!self_file(a, "_dealloc_warn"))
        return R::Err;
    out = value_none();
    return R::Ok;
}

R fm_getstate(const CallArgs &a, Value &out)
{
    (void)out;
    if (!a.nargs)
        return err_set("TypeError", "cannot pickle");
    Buf<96> b;
    b.put("cannot pickle '").put(type_name(a.args[0])).put("' instances");
    return err_set("TypeError", b.str());
}

constexpr Method FILE_METHODS[] = {
    { "__init__", fm_init },
    { "read", fm_read },
    { "readall", fm_readall },
    { "readinto", fm_readinto },
    { "write", fm_write },
    { "seek", fm_seek },
    { "tell", fm_tell },
    { "truncate", fm_truncate },
    { "close", fm_close },
    { "seekable", fm_seekable },
    { "readable", fm_readable },
    { "writable", fm_writable },
    { "fileno", fm_fileno },
    { "isatty", fm_isatty },
    { "_isatty_open_only", fm_isatty },
    { "_dealloc_warn", fm_dealloc_warn },
    { "__getstate__", fm_getstate },
    { "__reduce_ex__", fm_getstate },
};

} // namespace

R file_setattr(Value v, StrObj *name, Value x)
{
    constexpr Str READONLY[] = { "closed", "closefd", "mode", "_blksize" };
    return io_set_attr(v, name, x, READONLY, "_io.FileIO");
}

constexpr Type fileio_type{ .name    = "_io.FileIO",
                            .trace   = file_trace,
                            .fini    = file_fini,
                            .repr    = file_repr,
                            .iter    = [](Value v) { return v; },
                            .getattr = file_getattr,
                            .setattr = file_setattr,
                            .del     = io_del,
                            .base    = &rawbase_type,
                            .vmnext  = true };

bool fileio_methods()
{
    return method_install(&fileio_type, FILE_METHODS);
}

R fileio_new(const CallArgs &a, Value &out)
{
    Root self{ file_alloc() };
    if (self.v.is_nil())
        return R::Err;
    // Made empty, for a subclass whose own __init__ opens it.
    if (!a.nargs && !a.nkw) {
        out = self.v;
        return R::Ok;
    }
    return file_init(self.v, a, out);
}

Value fileio_std(i32 fd, bool writable)
{
    Root self{ file_alloc() };
    if (self.v.is_nil())
        return Value();
    FileObj *f  = file_of(self.v);
    f->fd       = fd;
    f->readable = !writable;
    f->writable = writable;
    f->truncate = writable;
    f->closefd  = false;
    f->seekable = 0;
    f->sink     = writable;
    return self.v;
}
