// `posix`: the native floor under CPython's os.py.
//
// Every call that touches the file system is a system call, and nothing
// under vm_burst awaits one, so each of these answers a continuation that
// asks the driver (posix.h's sys_turn) and turns the answer into a value.
// What the platform does not have -- modes, owners, inodes, a second link to
// a file -- is answered the way README.md's Known differences say.
#include "builtin.h"
#include "codec.h"
#include "exc.h"
#include "gc.h"
#include "info.h"
#include "intern.h"
#include "io.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "kernel/sysabi.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "posix.h"
#include "proc/rt.h"
#include "type.h"
#include "ustr.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// Linux's numbers, the dialect errno already speaks.
constexpr i64 O_RDONLY    = 0;
constexpr i64 O_WRONLY    = 1;
constexpr i64 O_RDWR      = 2;
constexpr i64 O_ACCMODE   = 3;
constexpr i64 O_CREAT     = 0100;
constexpr i64 O_EXCL      = 0200;
constexpr i64 O_NOCTTY    = 0400;
constexpr i64 O_TRUNC     = 01000;
constexpr i64 O_APPEND    = 02000;
constexpr i64 O_NONBLOCK  = 04000;
constexpr i64 O_DIRECTORY = 0200000;
constexpr i64 O_NOFOLLOW  = 0400000;
constexpr i64 O_CLOEXEC   = 02000000;
constexpr i64 O_TMPFILE   = 020000000 | O_DIRECTORY;

constexpr i64 S_IFDIR = 0040000;
constexpr i64 S_IFREG = 0100000;
constexpr i64 S_IFLNK = 0120000;

struct Home {
    String cwd;
    Value environ; // posix.environ, a dict of bytes
    i64 umask = 022;
};

Home *home;

void home_mark()
{
    if (home)
        gc_mark(home->environ);
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

} // namespace

// ---------------------------------------------------------------- the floor

void sys_set_cwd(Str path)
{
    if (Home *h = here())
        h->cwd.assign(path);
}

Str sys_cwd()
{
    Home *h = here();
    return h && !h->cwd.empty() ? h->cwd.str() : Str("/");
}

R err_badfd()
{
    return err_errno(9);
}

Value posix_direntry_path(Value v);

bool fs_bytes(Value v, Str who, String &out)
{
    out.clear();
    Value d = posix_direntry_path(v);
    if (d.is_nil())
        d = is_inst(v) ? inst_of(v)->native : v;
    if (is_str(d)) {
        Str s = str_of(d)->str();
        if (has_surrogate(s)) {
            if (!std_encode(s, true, out))
                return false;
        } else if (!out.assign(s)) {
            return oom() == R::Ok;
        }
    } else if (is_bytes(d)) {
        if (!out.assign(static_cast<BytesObj *>(d.obj())->str()))
            return oom() == R::Ok;
    } else {
        Buf<128> b;
        b.put(who).put(": path should be string, bytes or os.PathLike, not ").put(type_name(v));
        return err_set("TypeError", b.str()) == R::Ok;
    }
    for (usize i = 0; i < out.size(); i++)
        if (out[i] == 0) {
            Buf<96> b;
            b.put(who).put(": embedded null character in path");
            return err_set("ValueError", b.str()) == R::Ok;
        }
    return true;
}

bool fs_needs_call(Value v)
{
    if (is_str(v) || is_bytes(v))
        return false;
    if (is_inst(v) && (is_str(inst_of(v)->native) || is_bytes(inst_of(v)->native)))
        return false;
    return type_has_py_special(v, "__fspath__");
}

Value fs_path_of(Value v)
{
    if (is_str(v) || is_bytes(v))
        return v;
    if (is_inst(v) && (is_str(inst_of(v)->native) || is_bytes(inst_of(v)->native)))
        return v;
    Value p = posix_direntry_path(v);
    if (!p.is_nil() || err_pending())
        return p;
    if (fs_needs_call(v))
        return Value();
    Buf<128> b;
    b.put("expected str, bytes or os.PathLike object, not ").put(type_name(v));
    return err_set("TypeError", b.str()), Value();
}

namespace {

// s[0] the arguments, a tuple with Nil where one was left out; j the one being
// converted; redo the builtin to enter again.
R fspath_step(ContObj *k, Value in)
{
    TupleObj *t = static_cast<TupleObj *>(k->s[0].obj());
    if (k->i++ == 0) {
        Root m{ type_special(t->items()[k->j], "__fspath__") };
        if (m.v.is_nil())
            return R::Err;
        return cont_call(k, m.v, Value(), 0);
    }
    if (!is_str(in) && !is_bytes(in)) {
        Buf<160> b;
        b.put("expected ").put(type_name(t->items()[k->j]));
        b.put(".__fspath__() to return str or bytes, not ").put(type_name(in));
        return err_set("TypeError", b.str());
    }
    t->items()[k->j] = in;
    CallArgs a;
    a.args  = t->items();
    a.nargs = u32(t->len);
    Value got;
    R r = k->redo(a, got);
    return r == R::Ok ? cont_done(k, got) : r;
}

} // namespace

bool fs_convert(const Value *vals, u32 n, u32 which, R (*again)(const CallArgs &, Value &out),
                Value &out, R &r)
{
    u32 at = 0;
    while (at < n && !((which >> at) & 1 && !vals[at].is_nil() && fs_needs_call(vals[at])))
        at++;
    if (at == n)
        return false;
    Roots pin{ const_cast<Value *>(vals), n };
    TupleObj *t = tuple_new(n);
    if (!t) {
        r = oom();
        return true;
    }
    for (u32 i = 0; i < n; i++)
        t->items()[i] = vals[i];
    Root rt{ obj_value(t) };
    Root kv{ cont_new(fspath_step) };
    if (kv.v.is_nil()) {
        r = R::Err;
        return true;
    }
    ContObj *k = cont_of(kv.v);
    k->s[0]    = rt.v;
    k->j       = at;
    k->redo    = again;
    out        = kv.v;
    r          = R::Ok;
    return true;
}

Value fs_decode(Str bytes)
{
    bool ascii = true;
    for (usize i = 0; i < bytes.size() && ascii; i++)
        ascii = u8(bytes[i]) < 0x80;
    if (ascii) {
        StrObj *s = str_raw(bytes);
        return s ? obj_value(s) : (oom(), Value());
    }
    Root in{ bytes_new(bytes) };
    Root errors{ str_new("surrogateescape") };
    if (in.v.is_nil() || errors.v.is_nil())
        return Value();
    CodecCall c;
    c.codec  = Codec::Utf8;
    c.input  = in.v;
    c.errors = errors.v;
    Value out;
    if (codec_run(c, out) != R::Ok || !is_str(out))
        return err_pending() ? Value() : (err_set("SystemError", "fs_decode"), Value());
    return out;
}

bool fn_take(const CallArgs &a, Str who, const Str *names, u32 n, u32 least, Value *out)
{
    for (u32 i = 0; i < n; i++)
        out[i] = Value();
    if (a.nargs > n) {
        char tmp[24];
        Buf<128> b;
        b.put(who).put("() takes at most ").put(int_text(tmp, sizeof tmp, i64(n)));
        b.put(" arguments (").put(int_text(tmp, sizeof tmp, i64(a.nargs))).put(" given)");
        return err_set("TypeError", b.str()) == R::Ok;
    }
    for (u32 i = 0; i < a.nargs; i++)
        out[i] = a.args[i];
    for (u32 k = 0; k < a.nkw; k++) {
        Str nm = is_str(a.kwnames[k]) ? str_of(a.kwnames[k])->str() : Str();
        u32 i  = 0;
        while (i < n && names[i] != nm)
            i++;
        if (i == n) {
            Buf<128> b;
            b.put(who).put("() got an unexpected keyword argument '").put(nm).put("'");
            return err_set("TypeError", b.str()) == R::Ok;
        }
        if (!out[i].is_nil()) {
            Buf<128> b;
            char t[16];
            b.put("argument for ").put(who).put("() given by name ('").put(nm);
            b.put("') and position (").put(int_text(t, sizeof t, i + 1)).put(')');
            return err_set("TypeError", b.str()) == R::Ok;
        }
        out[i] = a.kwvals[k];
    }
    for (u32 i = 0; i < least; i++)
        if (out[i].is_nil()) {
            Buf<128> b;
            b.put(who).put("() missing required argument '").put(names[i]).put("'");
            return err_set("TypeError", b.str()) == R::Ok;
        }
    return true;
}

namespace {

constexpr u32 SYS_WAITING  = 1u << 31;
constexpr u32 SYS_HANDLING = 1u << 30;

} // namespace

bool sys_turn(ContObj *k, const SysReq &q, R &r, Value f1, Value f2)
{
    if (k->i & SYS_HANDLING) {
        // The program's handler has run and raised nothing: the call again.
        k->i &= ~SYS_HANDLING;
    } else if (k->i & SYS_WAITING) {
        k->i &= ~SYS_WAITING;
        SysAns &a = vm_sys_answer();
        if (a.ok)
            return true;
        if (a.err != Error::Intr) {
            // A descriptor that is not open is what an fd operation's
            // Invalid means.
            bool fdop = q.op == SysOp::Read || q.op == SysOp::Write || q.op == SysOp::Close ||
                        q.op == SysOp::Seek || q.op == SysOp::Truncate || q.op == SysOp::FStat ||
                        q.op == SysOp::Dup || q.op == SysOp::Tty;
            r         = fdop && (a.err == Error::Invalid || a.err == Error::NotFound)
                            ? err_errno(9, f1, f2)
                            : err_os(a.err, f1, f2);
            return false;
        }
        if (u32 sig = vm_take_signal()) {
            Value h;
            r = sig_handler(sig, h);
            if (r != R::Ok)
                return false;
            if (!h.is_nil()) {
                k->i |= SYS_HANDLING;
                Value f = vm_frame();
                r = cont_call(k, h, Value::of_int(i32(sig)), 2, f.is_nil() ? value_none() : f);
                return false;
            }
        }
    }
    k->i |= SYS_WAITING;
    r = cont_sys(k, q);
    return false;
}

bool sys_open_retry(ContObj *k, R &r, u32 again, i64 *tried)
{
    if (*tried || r != R::Err || vm_sys_answer().err != Error::Perm)
        return false;
    *tried = 1;
    Root saved{ exc_pending() };
    err_clear();
    Root drain{ vm_collect_and_finalize() };
    if (drain.v.is_nil()) {
        if (err_pending())
            return r = R::Err, true;
        err_set_value(saved.v);
        return false;
    }
    k->i = again;
    r    = cont_await(k, drain.v);
    return true;
}

namespace {

// ------------------------------------------------------------- stat_result

INFO_TYPE(stat_type, "os.stat_result");
INFO_TYPE(tsize_type, "os.terminal_size");
INFO_TYPE(uname_type, "posix.uname_result");
INFO_TYPE(times_type, "posix.times_result");

// Index 7 to 9 are the integer times, shown under the names the float ones
// are reached by -- which is what CPython's repr prints too.
constexpr Str STAT_NAMES[] = {
    "st_mode",     "st_ino",      "st_dev",     "st_nlink",  "st_uid",   "st_gid",   "st_size",
    "st_atime",    "st_mtime",    "st_ctime",   "st_atime",  "st_mtime", "st_ctime", "st_atime_ns",
    "st_mtime_ns", "st_ctime_ns", "st_blksize", "st_blocks", "st_rdev",
};
constexpr usize STAT_SHOWN = 10;
constexpr usize STAT_ALL   = sizeof STAT_NAMES / sizeof STAT_NAMES[0];

// There are no inodes here. A path's hash stands in for one, so samefile
// agrees with itself; see README.md.
i64 fake_ino(Str path)
{
    String abs;
    if (path.empty() || path[0] != '/') {
        abs.assign(sys_cwd());
        if (!abs.str().ends_with("/"))
            abs.push('/');
    }
    abs.append(path);
    // Trailing slashes and "./" do not make another file.
    Str p = abs.str();
    while (p.size() > 1 && p[p.size() - 1] == '/')
        p = p.substr(0, p.size() - 1);
    u32 h = 2166136261u;
    for (usize i = 0; i < p.size(); i++) {
        if (p[i] == '.' && i > 0 && p[i - 1] == '/' && (i + 1 == p.size() || p[i + 1] == '/')) {
            i++;
            continue;
        }
        h = (h ^ u8(p[i])) * 16777619u;
    }
    return i64(h & 0x7fffffff) + 1;
}

Value stat_new(u32 kind, u64 size, u64 mtime_ms, i64 ino)
{
    i64 mode = kind == SYS_KIND_DIR    ? S_IFDIR | 0755
               : kind == SYS_KIND_LINK ? S_IFLNK | 0777
                                       : S_IFREG | 0644;
    i64 secs = i64(mtime_ms / 1000);
    f64 fsec = f64(mtime_ms) / 1000.0;
    i64 ns   = i64(mtime_ms) * 1000000;
    Value items[STAT_ALL];
    Roots pin{ items, STAT_ALL };
    items[0] = int_from_i64(mode);
    items[1] = int_from_i64(ino);
    items[2] = Value::of_int(1);
    items[3] = Value::of_int(kind == SYS_KIND_DIR ? 2 : 1);
    items[4] = Value::of_int(0);
    items[5] = Value::of_int(0);
    items[6] = int_from_i64(i64(size));
    for (usize i = 7; i < 10; i++)
        items[i] = int_from_i64(secs);
    for (usize i = 10; i < 13; i++)
        items[i] = float_new(fsec);
    for (usize i = 13; i < 16; i++)
        items[i] = int_from_i64(ns);
    items[16] = Value::of_int(4096);
    items[17] = int_from_i64(i64((size + 511) / 512));
    items[18] = Value::of_int(0);
    for (Value v : items)
        if (v.is_nil())
            return Value();
    return info_new(&stat_type, items, STAT_NAMES, STAT_ALL, STAT_SHOWN);
}

// os.stat_result(seq): the ten shown fields, and up to nine more.
R b_stat_result(const CallArgs &a, Value &out)
{
    if (!args_only(a, "stat_result", 1, 2))
        return R::Err;
    Root l{ obj_value(py_list_of(a.args[0])) };
    if (l.v.is_nil())
        return R::Err;
    usize n = list_of(l.v)->items.size();
    if (n < STAT_SHOWN || n > STAT_ALL) {
        char tmp[24];
        Buf<96> b;
        b.put("os.stat_result() takes a sequence of length ");
        b.put(n < STAT_SHOWN ? Str("at least 10") : Str("at most 19"));
        b.put(" (").put(int_text(tmp, sizeof tmp, i64(n))).put("-sequence given)");
        return err_set("TypeError", b.str());
    }
    Value items[STAT_ALL];
    for (usize i = 0; i < STAT_ALL; i++)
        items[i] = i < n ? list_of(l.v)->items[i] : value_none();
    // The float times default to the integer ones, as CPython fills them.
    for (usize i = n; i < 13 && i >= 10; i++)
        items[i] = list_of(l.v)->items[i - 3];
    out = info_new(&stat_type, items, STAT_NAMES, STAT_ALL, STAT_SHOWN);
    return out.is_nil() ? R::Err : R::Ok;
}

R b_terminal_size(const CallArgs &a, Value &out)
{
    if (!args_only(a, "terminal_size", 1, 2))
        return R::Err;
    Root l{ obj_value(py_list_of(a.args[0])) };
    if (l.v.is_nil())
        return R::Err;
    if (list_of(l.v)->items.size() != 2)
        return err_set("TypeError", "os.terminal_size() takes a 2-sequence");
    constexpr Str NAMES[] = { "columns", "lines" };
    out                   = info_new(&tsize_type, list_of(l.v)->items.data(), NAMES, 2);
    return out.is_nil() ? R::Err : R::Ok;
}

Value tsize_new(u64 cols, u64 rows)
{
    constexpr Str NAMES[] = { "columns", "lines" };
    Value items[2]        = { int_from_i64(i64(cols)), int_from_i64(i64(rows)) };
    return info_new(&tsize_type, items, NAMES, 2);
}

// --------------------------------------------------------- argument helpers

bool dirfd_none(Value v, Str who)
{
    if (v.is_nil() || is_none(v))
        return true;
    Buf<96> b;
    b.put(who).put(": dir_fd unavailable on this platform");
    return err_set("NotImplementedError", b.str()) == R::Ok;
}

bool fd_of(Value v, i32 &fd)
{
    i64 n = 0;
    if (!as_int_arg(v, n)) {
        err_set2("TypeError", "an integer is required", type_name(v));
        return false;
    }
    if (n < -2147483647 - 1 || n > 2147483647) {
        err_set("OverflowError", "fd is greater than maximum");
        return false;
    }
    fd = i32(n);
    return true;
}

bool is_fd_arg(Value v)
{
    i64 n = 0;
    return v.is_int() || is_bool(v) || (is_inst(v) && as_int_arg(v, n));
}

// A converted path in the form a step keeps: the bytes, as a bytes object.
Value path_bytes(Value v, Str who)
{
    String b;
    if (!fs_bytes(v, who, b))
        return Value();
    return bytes_new(b.str());
}

Str bytes_str(Value v)
{
    return static_cast<BytesObj *>(v.obj())->str();
}

// Did the caller hand over bytes? Then names come back as bytes.
bool wants_bytes(Value v)
{
    return is_bytes(v) || (is_inst(v) && is_bytes(inst_of(v)->native));
}

// ------------------------------------------------------------- one call
//
// s[0] the path as given, s[1] its bytes, s[2] the second path, s[3] its
// bytes, s[4] data, j the op and its detail.

enum Op : u32 {
    OP_STAT,
    OP_LSTAT,
    OP_FSTAT,
    OP_MKDIR,
    OP_UNLINK,
    OP_RMDIR,
    OP_RENAME,
    OP_SYMLINK,
    OP_READLINK,
    OP_TOUCH,
    OP_UTIME_CHECK,
    OP_CHDIR,
    OP_LISTDIR,
    OP_OPEN,
    OP_CLOSE,
    OP_READ,
    OP_WRITE,
    OP_LSEEK,
    OP_FTRUNCATE,
    OP_TRUNCATE,
    OP_DUP,
    OP_ISATTY,
    OP_TERMSIZE,
    OP_PIPE,
    OP_KILL,
    OP_ACCESS,
    OP_SCANDIR,
    OP_EXISTS,
    OP_READINTO,
};

struct OpState {
    i32 fd;
    u32 flags;
    i64 off;
    u32 whence;
    u32 max;
};

// The numbers a call carries, in s[5] as a bytes blob, so a step can be
// entered again without its CallArgs.
Value state_blob(const OpState &st)
{
    return bytes_new(Str(reinterpret_cast<const char *>(&st), sizeof st));
}

OpState state_of(ContObj *k)
{
    OpState st{};
    if (is_bytes(k->s[5]))
        __builtin_memcpy(&st, static_cast<BytesObj *>(k->s[5].obj())->data(), sizeof st);
    return st;
}

Value scandir_new(Value dir, Value ents, bool bytes);

R one_step(ContObj *k, Value)
{
    u32 op    = k->j;
    OpState s = state_of(k);
    u32 stage = k->i & ~SYS_TURN_BITS;
    SysReq q;
    q.fd     = s.fd;
    q.flags  = s.flags;
    q.off    = s.off;
    q.whence = s.whence;
    q.max    = s.max;
    if (is_bytes(k->s[1]))
        q.path = bytes_str(k->s[1]);
    if (is_bytes(k->s[3]))
        q.path2 = bytes_str(k->s[3]);
    if (is_bytes(k->s[4]))
        q.data = bytes_str(k->s[4]);
    R r;
    Value f1 = k->s[0], f2 = k->s[2];

    switch (op) {
    case OP_STAT:
    case OP_LSTAT:
        q.op     = SysOp::Stat;
        q.follow = op == OP_STAT;
        break;
    case OP_EXISTS:
    case OP_ACCESS:
        q.op = SysOp::Stat;
        break;
    case OP_FSTAT:
        q.op = SysOp::FStat;
        break;
    case OP_MKDIR:
        q.op = SysOp::MkDir;
        break;
    case OP_UNLINK:
    case OP_RMDIR:
        // What is there decides the error: unlink refuses a directory and
        // rmdir anything else, which the store's one Remove does not.
        if (stage == 0) {
            q.op     = SysOp::Stat;
            q.follow = false;
            break;
        }
        q.op = SysOp::Remove;
        break;
    case OP_RENAME:
        q.op = SysOp::Rename;
        break;
    case OP_SYMLINK:
        q.op   = SysOp::Symlink;
        q.path = q.path2;
        q.data = bytes_str(k->s[1]);
        f1     = k->s[0];
        break;
    case OP_READLINK:
        q.op = SysOp::ReadLink;
        break;
    case OP_TOUCH:
        q.op = SysOp::Touch;
        break;
    case OP_UTIME_CHECK:
        q.op = SysOp::Stat;
        break;
    case OP_CHDIR:
        q.op = SysOp::Chdir;
        break;
    case OP_LISTDIR:
    case OP_SCANDIR:
        q.op = SysOp::List;
        break;
    case OP_OPEN:
        q.op = SysOp::Open;
        break;
    case OP_CLOSE:
        q.op = SysOp::Close;
        break;
    case OP_READ:
    case OP_READINTO:
        // A request past one system call's worth is filled from a file, as
        // Linux fills it, and answered by one call from a stream.
        q.op = stage == 0 ? SysOp::FStat : SysOp::Read;
        if (stage >= 2) {
            usize have = is_bytearray(k->s[4]) ? array_of(k->s[4])->data.size() : 0;
            usize left = s.max - have;
            q.max      = u32(left > SYS_READ_MAX ? SYS_READ_MAX : left);
        } else if (q.max > SYS_READ_MAX) {
            q.max = SYS_READ_MAX;
        }
        break;
    case OP_WRITE:
        q.op = SysOp::Write;
        break;
    case OP_LSEEK:
        q.op = SysOp::Seek;
        break;
    case OP_FTRUNCATE:
        q.op = SysOp::Truncate;
        break;
    case OP_TRUNCATE:
        q.op    = stage == 0 ? SysOp::Open : stage == 1 ? SysOp::Truncate : SysOp::Close;
        q.flags = SYS_O_WRITE;
        q.fd    = i32(k->a[0].is_int() ? k->a[0].as_int() : -1);
        break;
    case OP_DUP:
        q.op = SysOp::Dup;
        break;
    case OP_ISATTY:
    case OP_TERMSIZE:
        q.op = SysOp::Tty;
        break;
    case OP_PIPE:
        q.op = SysOp::Pipe;
        break;
    case OP_KILL:
        q.op = SysOp::Kill;
        break;
    default:
        return err_set("SystemError", "posix: unknown call");
    }

    if ((op == OP_READ || op == OP_READINTO) && stage == 0) {
        if (!(k->i & SYS_TURN_BITS) && s.max > SYS_READ_MAX) {
            k->i |= 1u << 31;
            return cont_sys(k, q);
        }
        bool file = false;
        if (k->i & SYS_TURN_BITS) {
            k->i &= ~SYS_TURN_BITS;
            file = vm_sys_answer().ok;
        }
        k->i = file ? 2 : 1;
        if (file) {
            Value acc = bytearray_new(Str());
            if (acc.is_nil())
                return R::Err;
            k->s[4] = acc;
        }
        return one_step(k, Value());
    }

    if (op == OP_EXISTS || op == OP_ACCESS || op == OP_ISATTY) {
        // A failure is an answer here, not an error.
        if (!(k->i & SYS_TURN_BITS)) {
            k->i |= 1u << 31;
            return cont_sys(k, q);
        }
        k->i &= ~SYS_TURN_BITS;
        SysAns &a = vm_sys_answer();
        if (!a.ok && a.err == Error::Intr && vm_take_interrupt()) {
            Value h;
            if (sig_interrupted(h) != R::Ok)
                return R::Err;
        }
        if (op == OP_ISATTY)
            return cont_done(k, value_bool(a.ok && a.n));
        return cont_done(k, value_bool(a.ok));
    }

    // For truncate, the close is owed even when the truncate failed.
    if (op == OP_TRUNCATE && stage == 2) {
        if (!sys_turn(k, q, r))
            return r;
        if (!k->caught.is_nil())
            return err_set_value(k->caught);
        return cont_done(k, value_none());
    }

    if (op == OP_FSTAT && s.fd >= 0 && s.fd < i32(SYS_FD_MIN) && !(k->i & SYS_TURN_BITS)) {
        // The store will not describe the three streams; they are a terminal
        // or a pipe, which is what their mode says.
        bool known = false;
        bool tty   = sys_tty(s.fd, known);
        Value st   = stat_new(SYS_KIND_FILE, 0, 0, 0x40000000 + s.fd);
        if (st.is_nil())
            return R::Err;
        Root rs{ st };
        Value items[STAT_ALL];
        InfoObj *io      = static_cast<InfoObj *>(rs.v.obj());
        TupleObj *shown  = static_cast<TupleObj *>(io->items.obj());
        TupleObj *hidden = static_cast<TupleObj *>(io->hidden.obj());
        for (usize i = 0; i < STAT_ALL; i++)
            items[i] = i < STAT_SHOWN ? shown->items()[i] : hidden->items()[i - STAT_SHOWN];
        items[0]   = Value::of_int(tty ? 0020620 : 0010600);
        Value made = info_new(&stat_type, items, STAT_NAMES, STAT_ALL, STAT_SHOWN);
        return made.is_nil() ? R::Err : cont_done(k, made);
    }

    if (!sys_turn(k, q, r, f1, f2)) {
        if ((op == OP_OPEN || (op == OP_TRUNCATE && stage == 0)) &&
            sys_open_retry(k, r, stage, &k->x[3]))
            return r;
        if (r == R::Err && op == OP_TRUNCATE && stage == 1) {
            // Keep the error, close, and raise it then.
            k->caught = exc_pending();
            err_clear();
            k->i = 2;
            return one_step(k, Value());
        }
        if (r == R::Err && op == OP_RENAME && vm_sys_answer().err == Error::Unsupported) {
            err_clear();
            return err_errno(18, f1, f2);
        }
        return r;
    }
    SysAns &a = vm_sys_answer();

    switch (op) {
    case OP_STAT:
    case OP_LSTAT:
        return cont_done(k, stat_new(a.kind, a.size, a.mtime, fake_ino(q.path)));
    case OP_FSTAT: {
        i64 ino = 0x40000000 + s.fd;
        return cont_done(k, stat_new(a.kind, a.size, a.mtime, ino));
    }
    case OP_UNLINK:
    case OP_RMDIR:
        if (stage == 0) {
            bool dir = a.kind == SYS_KIND_DIR;
            if (op == OP_UNLINK && dir)
                return err_errno(21, f1);
            if (op == OP_RMDIR && !dir)
                return err_errno(20, f1);
            k->i = 1;
            return one_step(k, Value());
        }
        return cont_done(k, value_none());
    case OP_READLINK: {
        Value t = is_bytes(k->s[6]) ? bytes_new(a.data.str()) : fs_decode(a.data.str());
        return t.is_nil() ? R::Err : cont_done(k, t);
    }
    case OP_CHDIR:
        sys_set_cwd(a.data.str());
        return cont_done(k, value_none());
    case OP_UTIME_CHECK:
        return cont_done(k, value_none());
    case OP_LISTDIR:
    case OP_SCANDIR: {
        bool bytes = is_bytes(k->s[6]);
        ListObj *l = list_new();
        if (!l)
            return oom();
        Root rl{ obj_value(l) };
        for (SysEnt &e : a.ents) {
            Root name{ bytes ? bytes_new(e.name.str()) : fs_decode(e.name.str()) };
            if (name.v.is_nil())
                return R::Err;
            if (op == OP_LISTDIR) {
                if (!list_push(list_of(rl.v), name.v))
                    return oom();
                continue;
            }
            // (name, kind, size, mtime) for each, which the DirEntry keeps.
            TupleObj *t = tuple_new(4);
            if (!t)
                return oom();
            t->items()[0] = name.v;
            t->items()[1] = Value::of_int(i32(e.kind));
            Root rt{ obj_value(t) };
            Value sz = int_from_i64(i64(e.size));
            if (sz.is_nil())
                return R::Err;
            static_cast<TupleObj *>(rt.v.obj())->items()[2] = sz;
            Value mt                                        = int_from_i64(i64(e.mtime));
            if (mt.is_nil())
                return R::Err;
            static_cast<TupleObj *>(rt.v.obj())->items()[3] = mt;
            if (!list_push(list_of(rl.v), rt.v))
                return oom();
        }
        if (op == OP_LISTDIR)
            return cont_done(k, rl.v);
        Value it = scandir_new(k->s[0], rl.v, bytes);
        return it.is_nil() ? R::Err : cont_done(k, it);
    }
    case OP_OPEN:
    case OP_DUP:
    case OP_WRITE:
    case OP_LSEEK:
        return cont_done(k, int_from_i64(a.n));
    case OP_READ:
    case OP_READINTO: {
        Str got = a.data.str();
        if (stage >= 2) {
            ArrayObj *acc = array_of(k->s[4]);
            for (usize i = 0; i < got.size(); i++)
                if (!acc->data.push(u8(got[i])))
                    return oom();
            if (!got.empty() && acc->data.size() < s.max)
                return one_step(k, Value());
            got = acc->str();
        }
        if (op == OP_READ) {
            Value b = bytes_new(got);
            return b.is_nil() ? R::Err : cont_done(k, b);
        }
        u8 *p   = nullptr;
        usize n = 0;
        if (!io_writable_span(k->s[6], p, n))
            return R::Err;
        usize take = got.size() < n ? got.size() : n;
        for (usize i = 0; i < take; i++)
            p[i] = u8(got[i]);
        return cont_done(k, int_from_i64(i64(take)));
    }
    case OP_TRUNCATE:
        if (stage == 0) {
            k->a[0] = Value::of_int(i32(a.n));
            k->i    = 1;
            return one_step(k, Value());
        }
        k->i = 2;
        return one_step(k, Value());
    case OP_TERMSIZE:
        if (!a.n)
            return err_errno(25);
        return cont_done(k, tsize_new(a.kind, a.size));
    case OP_PIPE: {
        TupleObj *t = tuple_new(2);
        if (!t)
            return oom();
        t->items()[0] = Value::of_int(i32(a.n));
        t->items()[1] = Value::of_int(i32(a.off));
        return cont_done(k, obj_value(t));
    }
    default:
        return cont_done(k, value_none());
    }
}

// A call on one path, or two. `bytes_out` says names come back as bytes.
R start(u32 op, Value p1, Value p2, const OpState &st, Value &out, Str who, Value data = Value())
{
    Root r1{ p1 }, r2{ p2 }, rd{ data };
    Root b1{ p1.is_nil() ? Value() : path_bytes(p1, who) };
    if (!p1.is_nil() && b1.v.is_nil())
        return R::Err;
    Root b2{ p2.is_nil() ? Value() : path_bytes(p2, who) };
    if (!p2.is_nil() && b2.v.is_nil())
        return R::Err;
    Root blob{ state_blob(st) };
    if (blob.v.is_nil())
        return R::Err;
    Root kv{ cont_new(one_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->j       = op;
    k->s[0]    = r1.v;
    k->s[1]    = b1.v;
    k->s[2]    = r2.v;
    k->s[3]    = b2.v;
    k->s[4]    = rd.v;
    k->s[5]    = blob.v;
    // A marker that names come back as bytes.
    if (!r1.v.is_nil() && wants_bytes(r1.v))
        k->s[6] = r1.v;
    out = kv.v;
    return R::Ok;
}

R start_fd(u32 op, const OpState &st, Value &out, Value data = Value())
{
    return start(op, Value(), Value(), st, out, Str(), data);
}

// ---------------------------------------------------------------- functions

// Every path function takes `(path, *, dir_fd=None)` and a few take more; the
// shape below repeats for each.
#define TAKE_PATH(who, names, least, self)                                     \
    Value v[sizeof names / sizeof names[0]];                                   \
    if (!fn_take(a, who, names, least, v))                                     \
        return R::Err;                                                         \
    {                                                                          \
        R conv;                                                                \
        if (fs_convert(v, sizeof names / sizeof names[0], 1, self, out, conv)) \
            return conv;                                                       \
    }

R p_stat(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "path", "dir_fd", "follow_symlinks" };
    TAKE_PATH("stat", NAMES, 1, p_stat);
    if (!dirfd_none(v[1], "stat"))
        return R::Err;
    OpState st{};
    if (is_fd_arg(v[0])) {
        if (!fd_of(v[0], st.fd))
            return R::Err;
        return start_fd(OP_FSTAT, st, out);
    }
    bool follow = v[2].is_nil() || py_truth(v[2]);
    return start(follow ? OP_STAT : OP_LSTAT, v[0], Value(), st, out, "stat");
}

R p_lstat(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "path", "dir_fd" };
    TAKE_PATH("lstat", NAMES, 1, p_lstat);
    if (!dirfd_none(v[1], "lstat"))
        return R::Err;
    return start(OP_LSTAT, v[0], Value(), OpState{}, out, "lstat");
}

R p_fstat(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "fd" };
    Value v[1];
    if (!fn_take(a, "fstat", NAMES, 1, v))
        return R::Err;
    OpState st{};
    if (!fd_of(v[0], st.fd))
        return R::Err;
    return start_fd(OP_FSTAT, st, out);
}

R p_mkdir(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "path", "mode", "dir_fd" };
    TAKE_PATH("mkdir", NAMES, 1, p_mkdir);
    if (!dirfd_none(v[2], "mkdir"))
        return R::Err;
    return start(OP_MKDIR, v[0], Value(), OpState{}, out, "mkdir");
}

R p_unlink(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "path", "dir_fd" };
    TAKE_PATH("unlink", NAMES, 1, p_unlink);
    if (!dirfd_none(v[1], "unlink"))
        return R::Err;
    return start(OP_UNLINK, v[0], Value(), OpState{}, out, "unlink");
}

R p_remove(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "path", "dir_fd" };
    TAKE_PATH("remove", NAMES, 1, p_remove);
    if (!dirfd_none(v[1], "remove"))
        return R::Err;
    return start(OP_UNLINK, v[0], Value(), OpState{}, out, "remove");
}

R p_rmdir(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "path", "dir_fd" };
    TAKE_PATH("rmdir", NAMES, 1, p_rmdir);
    if (!dirfd_none(v[1], "rmdir"))
        return R::Err;
    return start(OP_RMDIR, v[0], Value(), OpState{}, out, "rmdir");
}

R rename_like(const CallArgs &a, Value &out, Str who, R (*self)(const CallArgs &, Value &))
{
    constexpr Str NAMES[] = { "src", "dst", "src_dir_fd", "dst_dir_fd" };
    Value v[4];
    if (!fn_take(a, who, NAMES, 2, v))
        return R::Err;
    R conv;
    if (fs_convert(v, 4, 3, self, out, conv))
        return conv;
    if (!dirfd_none(v[2], who) || !dirfd_none(v[3], who))
        return R::Err;
    return start(OP_RENAME, v[0], v[1], OpState{}, out, who);
}

R p_rename(const CallArgs &a, Value &out)
{
    return rename_like(a, out, "rename", p_rename);
}

R p_replace(const CallArgs &a, Value &out)
{
    return rename_like(a, out, "replace", p_replace);
}

R p_symlink(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "src", "dst", "target_is_directory", "dir_fd" };
    Value v[4];
    if (!fn_take(a, "symlink", NAMES, 2, v))
        return R::Err;
    R conv;
    if (fs_convert(v, 4, 3, p_symlink, out, conv))
        return conv;
    if (!dirfd_none(v[3], "symlink"))
        return R::Err;
    return start(OP_SYMLINK, v[0], v[1], OpState{}, out, "symlink");
}

R p_readlink(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "path", "dir_fd" };
    TAKE_PATH("readlink", NAMES, 1, p_readlink);
    if (!dirfd_none(v[1], "readlink"))
        return R::Err;
    return start(OP_READLINK, v[0], Value(), OpState{}, out, "readlink");
}

// The store keeps one time per file and can only move it to now, so a time
// the program names is not kept; see README.md.
R p_utime(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "path", "times", "ns", "dir_fd", "follow_symlinks" };
    TAKE_PATH("utime", NAMES, 1, p_utime);
    if (!dirfd_none(v[3], "utime"))
        return R::Err;
    bool given_times = !v[1].is_nil() && !is_none(v[1]);
    if (given_times && !v[2].is_nil())
        return err_set("ValueError", "utime: you may specify either 'times' or 'ns' but not both");
    if (given_times && !is_tuple(v[1]))
        return err_set("TypeError", "utime: 'times' must be either a tuple of two ints or None");
    bool now = !given_times && v[2].is_nil();
    return start(now ? OP_TOUCH : OP_UTIME_CHECK, v[0], Value(), OpState{}, out, "utime");
}

R p_chdir(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "path" };
    TAKE_PATH("chdir", NAMES, 1, p_chdir);
    return start(OP_CHDIR, v[0], Value(), OpState{}, out, "chdir");
}

// chmod and chown: there are no modes or owners to set. The path has to be
// there, which is the one failure a program can see.
R p_chmod(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "path", "mode", "dir_fd", "follow_symlinks" };
    TAKE_PATH("chmod", NAMES, 2, p_chmod);
    if (!dirfd_none(v[2], "chmod"))
        return R::Err;
    OpState st{};
    if (is_fd_arg(v[0])) {
        if (!fd_of(v[0], st.fd))
            return R::Err;
        return start_fd(OP_FSTAT, st, out);
    }
    bool follow = v[3].is_nil() || py_truth(v[3]);
    return start(follow ? OP_UTIME_CHECK : OP_LSTAT, v[0], Value(), st, out, "chmod");
}

R p_getcwd(const CallArgs &a, Value &out)
{
    if (!args_only(a, "getcwd", 0, 0))
        return R::Err;
    out = fs_decode(sys_cwd());
    return out.is_nil() ? R::Err : R::Ok;
}

R p_getcwdb(const CallArgs &a, Value &out)
{
    if (!args_only(a, "getcwdb", 0, 0))
        return R::Err;
    out = bytes_new(sys_cwd());
    return out.is_nil() ? R::Err : R::Ok;
}

R p_listdir(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "path" };
    TAKE_PATH("listdir", NAMES, 0, p_listdir);
    if (is_fd_arg(v[0]))
        return err_set("NotImplementedError", "listdir: a descriptor cannot be listed here");
    Root p{ v[0].is_nil() || is_none(v[0]) ? str_new(".") : v[0] };
    if (p.v.is_nil())
        return R::Err;
    return start(OP_LISTDIR, p.v, Value(), OpState{}, out, "listdir");
}

R p_scandir(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "path" };
    TAKE_PATH("scandir", NAMES, 0, p_scandir);
    if (is_fd_arg(v[0]))
        return err_set("NotImplementedError", "scandir: a descriptor cannot be listed here");
    Root p{ v[0].is_nil() || is_none(v[0]) ? str_new(".") : v[0] };
    if (p.v.is_nil())
        return R::Err;
    return start(OP_SCANDIR, p.v, Value(), OpState{}, out, "scandir");
}

// Linux's O_* to the store's. The mode is not kept.
u32 open_flags(i64 f)
{
    u32 acc   = u32(f & O_ACCMODE);
    u32 flags = acc == O_WRONLY ? SYS_O_WRITE
                : acc == O_RDWR ? SYS_O_READ | SYS_O_WRITE
                                : SYS_O_READ;
    if (f & O_CREAT)
        flags |= SYS_O_CREATE;
    if (f & O_EXCL)
        flags |= SYS_O_EXCL;
    if (f & O_TRUNC)
        flags |= SYS_O_TRUNC;
    if (f & O_APPEND)
        flags |= SYS_O_APPEND;
    if ((f & O_TMPFILE) == O_TMPFILE)
        flags |= SYS_O_HIDDEN;
    return flags;
}

R p_open(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "path", "flags", "mode", "dir_fd" };
    TAKE_PATH("open", NAMES, 2, p_open);
    if (!dirfd_none(v[3], "open"))
        return R::Err;
    i64 f = 0;
    if (!as_int_arg(v[1], f))
        return err_set2("TypeError", "an integer is required", type_name(v[1]));
    OpState st{};
    st.flags = open_flags(f);
    return start(OP_OPEN, v[0], Value(), st, out, "open");
}

// What the VM's own buffers stand for: a write to 1 or 2 goes where print's
// text goes, so the two stay in order.
bool std_sink(i32 fd, String *&sink)
{
    sink = fd == SYS_STDOUT ? vm_out() : fd == SYS_STDERR ? vm_errout() : nullptr;
    return sink != nullptr;
}

R p_close(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "fd" };
    Value v[1];
    if (!fn_take(a, "close", NAMES, 1, v))
        return R::Err;
    OpState st{};
    if (!fd_of(v[0], st.fd))
        return R::Err;
    if (st.fd < 0)
        return err_badfd();
    return start_fd(OP_CLOSE, st, out);
}

R p_closerange(const CallArgs &a, Value &out)
{
    // The descriptors are the store's; closing a range blindly would close
    // the driver's own too. Nothing a program here opens is left behind.
    if (!args_only(a, "closerange", 2, 2))
        return R::Err;
    out = value_none();
    return R::Ok;
}

R p_read(const CallArgs &a, Value &out)
{
    if (!args_only(a, "read", 2, 2))
        return R::Err;
    OpState st{};
    i64 n = 0;
    if (!fd_of(a.args[0], st.fd))
        return R::Err;
    if (!as_int_arg(a.args[1], n))
        return err_set2("TypeError", "an integer is required", type_name(a.args[1]));
    if (n < 0)
        return err_errno(22);
    if (st.fd < 0)
        return err_badfd();
    if (n == 0) {
        out = bytes_new(Str());
        return out.is_nil() ? R::Err : R::Ok;
    }
    st.max = u32(n > 0x7fffffff ? 0x7fffffff : n);
    return start_fd(OP_READ, st, out);
}

R p_readinto(const CallArgs &a, Value &out)
{
    if (!args_only(a, "readinto", 2, 2))
        return R::Err;
    OpState st{};
    if (!fd_of(a.args[0], st.fd))
        return R::Err;
    u8 *p   = nullptr;
    usize n = 0;
    if (!io_writable_span(a.args[1], p, n))
        return R::Err;
    if (st.fd < 0)
        return err_badfd();
    if (n == 0) {
        out = Value::of_int(0);
        return R::Ok;
    }
    st.max = u32(n);
    if (start_fd(OP_READINTO, st, out) != R::Ok)
        return R::Err;
    cont_of(out)->s[6] = a.args[1];
    return R::Ok;
}

R p_write(const CallArgs &a, Value &out)
{
    if (!args_only(a, "write", 2, 2))
        return R::Err;
    OpState st{};
    if (!fd_of(a.args[0], st.fd))
        return R::Err;
    Str data;
    if (!bytes_like(a.args[1], data))
        return err_not("a bytes-like object is required", a.args[1], true);
    if (st.fd < 0)
        return err_badfd();
    String *sink;
    if (std_sink(st.fd, sink)) {
        if (!sink->append(data))
            return oom();
        out = int_from_i64(i64(data.size()));
        return R::Ok;
    }
    Root rd{ bytes_new(data) };
    if (rd.v.is_nil())
        return R::Err;
    return start_fd(OP_WRITE, st, out, rd.v);
}

R p_lseek(const CallArgs &a, Value &out)
{
    if (!args_only(a, "lseek", 3, 3))
        return R::Err;
    OpState st{};
    i64 w = 0;
    if (!fd_of(a.args[0], st.fd))
        return R::Err;
    if (!as_int_arg(a.args[1], st.off))
        return err_set2("TypeError", "an integer is required", type_name(a.args[1]));
    if (!as_int_arg(a.args[2], w))
        return err_set2("TypeError", "an integer is required", type_name(a.args[2]));
    if (w < 0 || w > 2)
        return err_errno(22);
    st.whence = u32(w);
    if (st.fd >= 0 && st.fd < i32(SYS_FD_MIN))
        return err_errno(29);
    return start_fd(OP_LSEEK, st, out);
}

R p_ftruncate(const CallArgs &a, Value &out)
{
    if (!args_only(a, "ftruncate", 2, 2))
        return R::Err;
    OpState st{};
    if (!fd_of(a.args[0], st.fd))
        return R::Err;
    if (!as_int_arg(a.args[1], st.off))
        return err_set2("TypeError", "an integer is required", type_name(a.args[1]));
    if (st.off < 0)
        return err_errno(22);
    return start_fd(OP_FTRUNCATE, st, out);
}

R p_truncate(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "path", "length" };
    TAKE_PATH("truncate", NAMES, 2, p_truncate);
    OpState st{};
    if (!as_int_arg(v[1], st.off))
        return err_set2("TypeError", "an integer is required", type_name(v[1]));
    if (st.off < 0)
        return err_errno(22);
    if (is_fd_arg(v[0])) {
        if (!fd_of(v[0], st.fd))
            return R::Err;
        return start_fd(OP_FTRUNCATE, st, out);
    }
    return start(OP_TRUNCATE, v[0], Value(), st, out, "truncate");
}

R p_dup(const CallArgs &a, Value &out)
{
    if (!args_only(a, "dup", 1, 1))
        return R::Err;
    OpState st{};
    if (!fd_of(a.args[0], st.fd))
        return R::Err;
    if (st.fd < 0)
        return err_badfd();
    return start_fd(OP_DUP, st, out);
}

R p_isatty(const CallArgs &a, Value &out)
{
    if (!args_only(a, "isatty", 1, 1))
        return R::Err;
    OpState st{};
    if (!fd_of(a.args[0], st.fd))
        return R::Err;
    bool known = false;
    bool tty   = sys_tty(st.fd, known);
    if (known || st.fd < 0) {
        out = value_bool(tty);
        return R::Ok;
    }
    return start_fd(OP_ISATTY, st, out);
}

R p_get_terminal_size(const CallArgs &a, Value &out)
{
    if (!args_only(a, "get_terminal_size", 0, 1))
        return R::Err;
    OpState st{};
    st.fd = SYS_STDOUT;
    if (a.nargs && !fd_of(a.args[0], st.fd))
        return R::Err;
    if (st.fd < 0)
        return err_badfd();
    return start_fd(OP_TERMSIZE, st, out);
}

R p_device_encoding(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "fd" };
    Value v[1];
    if (!fn_take(a, "device_encoding", NAMES, 1, v))
        return R::Err;
    i32 fd = 0;
    if (!fd_of(v[0], fd))
        return R::Err;
    bool known = false;
    out        = sys_tty(fd, known) ? str_new("UTF-8") : value_none();
    return out.is_nil() ? R::Err : R::Ok;
}

R p_pipe(const CallArgs &a, Value &out)
{
    if (!args_only(a, "pipe", 0, 0))
        return R::Err;
    return start_fd(OP_PIPE, OpState{}, out);
}

R p_kill(const CallArgs &a, Value &out)
{
    if (!args_only(a, "kill", 2, 2))
        return R::Err;
    OpState st{};
    i64 sig = 0;
    if (!fd_of(a.args[0], st.fd) || !as_int_arg(a.args[1], sig))
        return err_pending() ? R::Err : err_set("TypeError", "an integer is required");
    // Only a child can be named, and this process has none.
    if (st.fd <= 0 || u32(st.fd) == proc_pid())
        return err_set("NotImplementedError", "kill: this process cannot signal itself here");
    st.flags = u32(sig);
    return start_fd(OP_KILL, st, out);
}

R p_access(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "path", "mode", "dir_fd", "effective_ids", "follow_symlinks" };
    TAKE_PATH("access", NAMES, 2, p_access);
    if (!dirfd_none(v[2], "access"))
        return R::Err;
    i64 mode = 0;
    if (!as_int_arg(v[1], mode))
        return err_set2("TypeError", "an integer is required", type_name(v[1]));
    return start(OP_ACCESS, v[0], Value(), OpState{}, out, "access");
}

R p_fsync(const CallArgs &a, Value &out)
{
    // Every write is already the store's; there is nothing to force.
    if (!args_only(a, "fsync", 1, 1))
        return R::Err;
    i32 fd = 0;
    if (is_fd_arg(a.args[0])) {
        if (!fd_of(a.args[0], fd))
            return R::Err;
        if (fd < 0)
            return err_badfd();
    }
    out = value_none();
    return R::Ok;
}

R p_urandom(const CallArgs &a, Value &out)
{
    if (!args_only(a, "urandom", 1, 1))
        return R::Err;
    i64 n = 0;
    if (!as_int_arg(a.args[0], n))
        return err_set2("TypeError", "an integer is required", type_name(a.args[0]));
    if (n < 0)
        return err_set("ValueError", "negative argument not allowed");
    String b;
    if (!b.reserve(usize(n)))
        return oom();
    for (i64 i = 0; i < n; i += 4) {
        u32 w = proc_random();
        for (i64 k = 0; k < 4 && i + k < n; k++)
            b.push(char(w >> (8 * k)));
    }
    out = bytes_new(b.str());
    return out.is_nil() ? R::Err : R::Ok;
}

R p_getpid(const CallArgs &a, Value &out)
{
    if (!args_only(a, "getpid", 0, 0))
        return R::Err;
    out = int_from_i64(proc_pid());
    return R::Ok;
}

R p_cpu_count(const CallArgs &a, Value &out)
{
    if (!args_only(a, "cpu_count", 0, 0))
        return R::Err;
    out = Value::of_int(1);
    return R::Ok;
}

R p_strerror(const CallArgs &a, Value &out)
{
    if (!args_only(a, "strerror", 1, 1))
        return R::Err;
    i64 code = 0;
    if (!as_int_arg(a.args[0], code))
        return err_set2("TypeError", "an integer is required", type_name(a.args[0]));
    Str t = errno_text(code);
    Buf<48> b;
    if (t.empty()) {
        char tmp[24];
        b.put("Unknown error ").put(int_text(tmp, sizeof tmp, code));
        t = b.str();
    }
    out = str_new(t);
    return out.is_nil() ? R::Err : R::Ok;
}

R p_fspath(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "path" };
    Value v[1];
    if (!fn_take(a, "fspath", NAMES, 1, v))
        return R::Err;
    R conv;
    if (fs_convert(v, 1, 1, p_fspath, out, conv))
        return conv;
    out = fs_path_of(v[0]);
    return out.is_nil() ? R::Err : R::Ok;
}

R p_umask(const CallArgs &a, Value &out)
{
    if (!args_only(a, "umask", 1, 1))
        return R::Err;
    i64 m = 0;
    if (!as_int_arg(a.args[0], m))
        return err_set2("TypeError", "an integer is required", type_name(a.args[0]));
    Home *h = here();
    if (!h)
        return oom();
    out      = int_from_i64(h->umask);
    h->umask = m & 0777;
    return R::Ok;
}

R p_exit(const CallArgs &a, Value &out)
{
    if (!args_only(a, "_exit", 1, 1))
        return R::Err;
    i64 n = 0;
    if (!as_int_arg(a.args[0], n))
        return err_set2("TypeError", "an integer is required", type_name(a.args[0]));
    vm_hard_exit(i32(n));
    out = value_none();
    return R::Ok;
}

R p_abort(const CallArgs &a, Value &out)
{
    if (!args_only(a, "abort", 0, 0))
        return R::Err;
    vm_hard_exit(134);
    out = value_none();
    return R::Ok;
}

R p_uname(const CallArgs &a, Value &out)
{
    if (!args_only(a, "uname", 0, 0))
        return R::Err;
    constexpr Str NAMES[] = { "sysname", "nodename", "release", "version", "machine" };
    constexpr Str VALS[]  = { "Braam", "braam", "0.9", "0.9", "wasm32" };
    Value items[5];
    Roots pin{ items, 5 };
    for (usize i = 0; i < 5; i++)
        if ((items[i] = str_new(VALS[i])).is_nil())
            return R::Err;
    out = info_new(&uname_type, items, NAMES, 5);
    return out.is_nil() ? R::Err : R::Ok;
}

R p_times(const CallArgs &a, Value &out)
{
    if (!args_only(a, "times", 0, 0))
        return R::Err;
    constexpr Str NAMES[] = { "user", "system", "children_user", "children_system", "elapsed" };
    Value items[5];
    Roots pin{ items, 5 };
    f64 now  = f64(proc_now()) / 1000.0;
    items[0] = float_new(now);
    for (usize i = 1; i < 4; i++)
        items[i] = float_new(0.0);
    items[4] = float_new(now);
    for (Value v : items)
        if (v.is_nil())
            return R::Err;
    out = info_new(&times_type, items, NAMES, 5);
    return out.is_nil() ? R::Err : R::Ok;
}

bool env_name_ok(Str k)
{
    for (usize i = 0; i < k.size(); i++)
        if (k[i] == '=')
            return false;
    return !k.empty();
}

// The environment is fixed at spawn and there is no second process to hand a
// changed one to, so these keep nothing: os.environ is the copy that changes.
R p_putenv(const CallArgs &a, Value &out)
{
    if (!args_only(a, "putenv", 2, 2))
        return R::Err;
    String k;
    if (!fs_bytes(a.args[0], "putenv", k))
        return R::Err;
    if (!env_name_ok(k.str()))
        return err_set("ValueError", "illegal environment variable name");
    out = value_none();
    return R::Ok;
}

R p_unsetenv(const CallArgs &a, Value &out)
{
    if (!args_only(a, "unsetenv", 1, 1))
        return R::Err;
    String k;
    if (!fs_bytes(a.args[0], "unsetenv", k))
        return R::Err;
    if (!env_name_ok(k.str()))
        return err_set("ValueError", "illegal environment variable name");
    out = value_none();
    return R::Ok;
}

// Descriptors here are neither inherited nor non-blocking.
R p_get_inheritable(const CallArgs &a, Value &out)
{
    i32 fd = 0;
    if (!args_only(a, "get_inheritable", 1, 1) || !fd_of(a.args[0], fd))
        return R::Err;
    if (fd < 0)
        return err_badfd();
    out = value_bool(fd < i32(SYS_FD_MIN));
    return R::Ok;
}

R p_set_inheritable(const CallArgs &a, Value &out)
{
    i32 fd = 0;
    if (!args_only(a, "set_inheritable", 2, 2) || !fd_of(a.args[0], fd))
        return R::Err;
    if (fd < 0)
        return err_badfd();
    out = value_none();
    return R::Ok;
}

R p_get_blocking(const CallArgs &a, Value &out)
{
    i32 fd = 0;
    if (!args_only(a, "get_blocking", 1, 1) || !fd_of(a.args[0], fd))
        return R::Err;
    if (fd < 0)
        return err_badfd();
    out = value_bool(true);
    return R::Ok;
}

R p_set_blocking(const CallArgs &a, Value &out)
{
    i32 fd = 0;
    if (!args_only(a, "set_blocking", 2, 2) || !fd_of(a.args[0], fd))
        return R::Err;
    if (fd < 0)
        return err_badfd();
    if (!py_truth(a.args[1]))
        return err_errno(95);
    out = value_none();
    return R::Ok;
}

R p_waitstatus_to_exitcode(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "status" };
    Value v[1];
    if (!fn_take(a, "waitstatus_to_exitcode", NAMES, 1, v))
        return R::Err;
    i64 s = 0;
    if (!as_int_arg(v[0], s))
        return err_set2("TypeError", "an integer is required", type_name(v[0]));
    out = int_from_i64(s);
    return R::Ok;
}

// ----------------------------------------------------------------- DirEntry

struct DirEntObj : Obj {
    Value name;
    Value path;
    Value lstat_cache;
    Value stat_cache;
    u32 kind;
    u64 size, mtime;
};

void dirent_trace(Obj *o)
{
    DirEntObj *d = static_cast<DirEntObj *>(o);
    gc_mark(d->name);
    gc_mark(d->path);
    gc_mark(d->lstat_cache);
    gc_mark(d->stat_cache);
}

R dirent_repr(Value v, String &out)
{
    DirEntObj *d = static_cast<DirEntObj *>(v.obj());
    if (!out.append("<DirEntry "))
        return oom();
    if (py_repr(d->name, out) != R::Ok)
        return R::Err;
    return out.push('>') ? R::Ok : oom();
}

R dirent_getattr(Value v, StrObj *name, Value &out)
{
    DirEntObj *d = static_cast<DirEntObj *>(v.obj());
    if (name->str() == "name")
        out = d->name;
    else if (name->str() == "path")
        out = d->path;
    else
        return R::NotImpl;
    return R::Ok;
}

extern const Type dirent_type;

constexpr Type dirent_type{ .name    = "posix.DirEntry",
                            .trace   = dirent_trace,
                            .repr    = dirent_repr,
                            .getattr = dirent_getattr,
                            .final   = true };

DirEntObj *dirent_self(const CallArgs &a, Str who)
{
    if (!a.nargs || !a.args[0].is_obj() || a.args[0].obj()->type != &dirent_type) {
        err_set2("TypeError", "a DirEntry is required", who);
        return nullptr;
    }
    return static_cast<DirEntObj *>(a.args[0].obj());
}

bool follow_kw(const CallArgs &a, Str who, bool &follow)
{
    constexpr Str NAMES[] = { "follow_symlinks" };
    Value v[1];
    if (!meth_take(a, who, NAMES, 0, v))
        return false;
    follow = v[0].is_nil() || py_truth(v[0]);
    return true;
}

// s[0] the entry; j what to answer: 0 stat, 1 is_dir, 2 is_file.
R dirent_step(ContObj *k, Value in)
{
    DirEntObj *d = static_cast<DirEntObj *>(k->s[0].obj());
    if (k->i == 0 && d->stat_cache.is_nil()) {
        k->i = 1;
        Root call;
        if (start(OP_STAT, d->path, Value(), OpState{}, call.v, "stat") != R::Ok)
            return R::Err;
        // A dangling link is neither a directory nor a file.
        if (k->j)
            k->catching = CATCH_ANY;
        return cont_await(k, call.v);
    }
    if (k->i == 1) {
        k->i        = 2;
        k->catching = CATCH_NONE;
        if (in.is_nil()) {
            Root e{ k->caught };
            k->caught = Value();
            if (!is_oserror(e.v))
                return err_set_value(e.v);
            return cont_done(k, value_bool(false));
        }
        d->stat_cache = in;
    }
    if (k->j == 0)
        return cont_done(k, d->stat_cache);
    Value mode;
    StrObj *n = str_intern("st_mode");
    if (!n || info_getattr(d->stat_cache, n, mode) != R::Ok)
        return err_pending() ? R::Err : err_set("SystemError", "stat_result without st_mode");
    i64 m = 0;
    as_int_arg(mode, m);
    i64 fmt = m & 0170000;
    return cont_done(k, value_bool(k->j == 1 ? fmt == S_IFDIR : fmt == S_IFREG));
}

R dirent_ask(const CallArgs &a, Value &out, u32 what)
{
    Root rv{ a.args[0] };
    Root kv{ cont_new(dirent_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = rv.v;
    cont_of(kv.v)->j    = what;
    out                 = kv.v;
    return R::Ok;
}

R de_is_dir(const CallArgs &a, Value &out)
{
    DirEntObj *d = dirent_self(a, "is_dir");
    bool follow  = true;
    if (!d || !follow_kw(a, "is_dir", follow))
        return R::Err;
    if (d->kind != SYS_KIND_LINK || !follow) {
        out = value_bool(d->kind == SYS_KIND_DIR);
        return R::Ok;
    }
    return dirent_ask(a, out, 1);
}

R de_is_file(const CallArgs &a, Value &out)
{
    DirEntObj *d = dirent_self(a, "is_file");
    bool follow  = true;
    if (!d || !follow_kw(a, "is_file", follow))
        return R::Err;
    if (d->kind != SYS_KIND_LINK || !follow) {
        out = value_bool(d->kind == SYS_KIND_FILE);
        return R::Ok;
    }
    return dirent_ask(a, out, 2);
}

R de_is_symlink(const CallArgs &a, Value &out)
{
    DirEntObj *d = dirent_self(a, "is_symlink");
    if (!d || !meth_args(a, "is_symlink", 0, 0))
        return R::Err;
    out = value_bool(d->kind == SYS_KIND_LINK);
    return R::Ok;
}

R de_is_junction(const CallArgs &a, Value &out)
{
    if (!dirent_self(a, "is_junction") || !meth_args(a, "is_junction", 0, 0))
        return R::Err;
    out = value_bool(false);
    return R::Ok;
}

R de_stat(const CallArgs &a, Value &out)
{
    DirEntObj *d = dirent_self(a, "stat");
    bool follow  = true;
    if (!d || !follow_kw(a, "stat", follow))
        return R::Err;
    if (d->kind == SYS_KIND_LINK && follow) {
        if (!d->stat_cache.is_nil()) {
            out = d->stat_cache;
            return R::Ok;
        }
        return dirent_ask(a, out, 0);
    }
    if (d->lstat_cache.is_nil()) {
        Root rv{ a.args[0] };
        String p;
        if (!fs_bytes(d->path, "stat", p))
            return R::Err;
        Value st = stat_new(d->kind, d->size, d->mtime, fake_ino(p.str()));
        if (st.is_nil())
            return R::Err;
        d              = static_cast<DirEntObj *>(rv.v.obj());
        d->lstat_cache = st;
        if (d->kind != SYS_KIND_LINK)
            d->stat_cache = st;
    }
    out = d->lstat_cache;
    return R::Ok;
}

R de_inode(const CallArgs &a, Value &out)
{
    DirEntObj *d = dirent_self(a, "inode");
    if (!d || !meth_args(a, "inode", 0, 0))
        return R::Err;
    String p;
    if (!fs_bytes(d->path, "inode", p))
        return R::Err;
    out = int_from_i64(fake_ino(p.str()));
    return R::Ok;
}

R de_fspath(const CallArgs &a, Value &out)
{
    DirEntObj *d = dirent_self(a, "__fspath__");
    if (!d || !meth_args(a, "__fspath__", 0, 0))
        return R::Err;
    out = d->path;
    return R::Ok;
}

R de_class_getitem(const CallArgs &a, Value &out)
{
    if (a.nargs < 2)
        return err_set("TypeError", "__class_getitem__ needs an argument");
    TupleObj *t = tuple_new(2);
    if (!t)
        return oom();
    t->items()[0] = a.args[0];
    t->items()[1] = a.args[1];
    Root rt{ obj_value(t) };
    return b_genericalias(CallArgs{ t->items(), 2 }, out);
}

constexpr Method DIRENT_METHODS[] = {
    { "is_dir", de_is_dir },           { "is_file", de_is_file }, { "is_symlink", de_is_symlink },
    { "is_junction", de_is_junction }, { "stat", de_stat },       { "inode", de_inode },
    { "__fspath__", de_fspath },
};

// ------------------------------------------------------------ the iterator

struct ScanObj : Obj {
    Value dir;  // what scandir was given, as a str or bytes
    Value ents; // ListObj of (name, kind, size, mtime)
    u32 at;
    bool closed;
    bool bytes;
};

void scan_trace(Obj *o)
{
    gc_mark(static_cast<ScanObj *>(o)->dir);
    gc_mark(static_cast<ScanObj *>(o)->ents);
}

Value scan_iter(Value v)
{
    return v;
}

R scan_next(Value v, Value &out)
{
    ScanObj *s = static_cast<ScanObj *>(v.obj());
    if (s->closed || s->at >= list_of(s->ents)->items.size()) {
        s->closed = true;
        return R::NotImpl;
    }
    Root rs{ v };
    TupleObj *t = static_cast<TupleObj *>(list_of(s->ents)->items[s->at++].obj());
    Root rt{ obj_value(t) };
    // The path is the directory joined with the name, as os.path.join would.
    String p;
    Str sep = "/";
    Str dir = s->bytes ? static_cast<BytesObj *>(s->dir.obj())->str() : str_of(s->dir)->str();
    Str nm  = s->bytes ? static_cast<BytesObj *>(t->items()[0].obj())->str()
                       : str_of(t->items()[0])->str();
    if (!p.append(dir) || (!dir.empty() && !dir.ends_with("/") && !p.append(sep)) || !p.append(nm))
        return oom();
    Root path{ s->bytes ? bytes_new(p.str()) : str_new(p.str()) };
    if (path.v.is_nil())
        return R::Err;
    DirEntObj *d = static_cast<DirEntObj *>(obj_alloc(&dirent_type, sizeof(DirEntObj)));
    if (!d)
        return oom();
    t              = static_cast<TupleObj *>(rt.v.obj());
    d->name        = t->items()[0];
    d->path        = path.v;
    d->lstat_cache = Value();
    d->stat_cache  = Value();
    d->kind        = u32(t->items()[1].as_int());
    i64 n          = 0;
    as_int_arg(t->items()[2], n);
    d->size = u64(n);
    as_int_arg(t->items()[3], n);
    d->mtime = u64(n);
    out      = obj_value(d);
    return R::Ok;
}

R scan_repr(Value, String &out)
{
    return out.append("<posix.ScandirIterator object>") ? R::Ok : oom();
}

constexpr Type scan_type{ .name  = "posix.ScandirIterator",
                          .trace = scan_trace,
                          .repr  = scan_repr,
                          .iter  = scan_iter,
                          .next  = scan_next,
                          .final = true };

ScanObj *scan_self(const CallArgs &a)
{
    Value v = a.nargs ? a.args[0] : Value();
    if (!v.is_obj() || v.obj()->type != &scan_type)
        return err_set("TypeError", "a ScandirIterator is required"), nullptr;
    return static_cast<ScanObj *>(v.obj());
}

R sc_close(const CallArgs &a, Value &out)
{
    ScanObj *s = scan_self(a);
    if (!s)
        return R::Err;
    s->closed = true;
    out       = value_none();
    return R::Ok;
}

R sc_enter(const CallArgs &a, Value &out)
{
    if (!scan_self(a))
        return R::Err;
    out = a.args[0];
    return R::Ok;
}

R sc_exit(const CallArgs &a, Value &out)
{
    ScanObj *s = scan_self(a);
    if (!s)
        return R::Err;
    s->closed = true;
    out       = value_bool(false);
    return R::Ok;
}

constexpr Method SCAN_METHODS[] = {
    { "close", sc_close },
    { "__enter__", sc_enter },
    { "__exit__", sc_exit },
};

Value scandir_new(Value dir, Value ents, bool bytes)
{
    Root rd{ dir }, re{ ents };
    // The directory as the names will be joined to it.
    Root shown{ bytes ? path_bytes(dir, "scandir") : fs_path_of(dir) };
    if (shown.v.is_nil())
        return Value();
    ScanObj *s = static_cast<ScanObj *>(obj_alloc(&scan_type, sizeof(ScanObj)));
    if (!s)
        return oom(), Value();
    s->dir    = shown.v;
    s->ents   = re.v;
    s->at     = 0;
    s->closed = false;
    s->bytes  = bytes;
    if (!bytes && !is_str(s->dir))
        s->dir = inst_of(s->dir)->native;
    return obj_value(s);
}

// ---------------------------------------------------------------- the table

constexpr ModDef DEFS[] = {
    { "stat", p_stat },
    { "lstat", p_lstat },
    { "fstat", p_fstat },
    { "mkdir", p_mkdir },
    { "unlink", p_unlink },
    { "remove", p_remove },
    { "rmdir", p_rmdir },
    { "rename", p_rename },
    { "replace", p_replace },
    { "symlink", p_symlink },
    { "readlink", p_readlink },
    { "utime", p_utime },
    { "chdir", p_chdir },
    { "chmod", p_chmod },
    { "getcwd", p_getcwd },
    { "getcwdb", p_getcwdb },
    { "listdir", p_listdir },
    { "scandir", p_scandir },
    { "open", p_open },
    { "close", p_close },
    { "closerange", p_closerange },
    { "read", p_read },
    { "readinto", p_readinto },
    { "write", p_write },
    { "lseek", p_lseek },
    { "ftruncate", p_ftruncate },
    { "truncate", p_truncate },
    { "dup", p_dup },
    { "isatty", p_isatty },
    { "get_terminal_size", p_get_terminal_size },
    { "device_encoding", p_device_encoding },
    { "pipe", p_pipe },
    { "kill", p_kill },
    { "access", p_access },
    { "fsync", p_fsync },
    { "fdatasync", p_fsync },
    { "urandom", p_urandom },
    { "getpid", p_getpid },
    { "cpu_count", p_cpu_count },
    { "strerror", p_strerror },
    { "fspath", p_fspath },
    { "umask", p_umask },
    { "_exit", p_exit },
    { "abort", p_abort },
    { "uname", p_uname },
    { "times", p_times },
    { "putenv", p_putenv },
    { "unsetenv", p_unsetenv },
    { "get_inheritable", p_get_inheritable },
    { "set_inheritable", p_set_inheritable },
    { "get_blocking", p_get_blocking },
    { "set_blocking", p_set_blocking },
    { "waitstatus_to_exitcode", p_waitstatus_to_exitcode },
};

struct IntDef {
    Str name;
    i64 v;
};

constexpr IntDef INTS[] = {
    { "F_OK", 0 },
    { "R_OK", 4 },
    { "W_OK", 2 },
    { "X_OK", 1 },
    { "O_RDONLY", O_RDONLY },
    { "O_WRONLY", O_WRONLY },
    { "O_RDWR", O_RDWR },
    { "O_ACCMODE", O_ACCMODE },
    { "O_CREAT", O_CREAT },
    { "O_EXCL", O_EXCL },
    { "O_NOCTTY", O_NOCTTY },
    { "O_TRUNC", O_TRUNC },
    { "O_APPEND", O_APPEND },
    { "O_NONBLOCK", O_NONBLOCK },
    { "O_NDELAY", O_NONBLOCK },
    { "O_DIRECTORY", O_DIRECTORY },
    { "O_NOFOLLOW", O_NOFOLLOW },
    { "O_CLOEXEC", O_CLOEXEC },
    { "O_TMPFILE", O_TMPFILE },
    { "EX_OK", 0 },
    { "EX_USAGE", 64 },
    { "EX_DATAERR", 65 },
    { "EX_NOINPUT", 66 },
    { "EX_SOFTWARE", 70 },
    { "EX_OSERR", 71 },
    { "EX_IOERR", 74 },
    { "SEEK_SET", 0 },
    { "SEEK_CUR", 1 },
    { "SEEK_END", 2 },
};

bool environ_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    DictObj *env = dict_new();
    if (!env)
        return oom() == R::Ok;
    Root re{ obj_value(env) };
    for (usize i = 0; i < proc_env_count(); i++) {
        Str w     = proc_env_at(i);
        usize cut = 0;
        while (cut < w.size() && w[cut] != '=')
            cut++;
        if (cut == 0 || cut == w.size())
            continue;
        Root k{ bytes_new(w.substr(0, cut)) };
        Root v{ bytes_new(w.substr(cut + 1)) };
        if (k.v.is_nil() || v.v.is_nil())
            return false;
        if (dict_set(static_cast<DictObj *>(re.v.obj()), k.v, v.v) != R::Ok)
            return false;
    }
    if (Home *h = here())
        h->environ = re.v;
    return mod_put(static_cast<DictObj *>(rd.v.obj()), "environ", re.v);
}

} // namespace

Value posix_direntry_path(Value v)
{
    if (v.is_obj() && v.obj()->type == &dirent_type)
        return static_cast<DirEntObj *>(v.obj())->path;
    return Value();
}

bool posix_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    if (!mod_defs(d, DEFS))
        return false;
    for (const IntDef &i : INTS)
        if (!mod_int(d, i.name, i.v))
            return false;
    if (!method_install(&dirent_type, DIRENT_METHODS) || !method_install(&scan_type, SCAN_METHODS))
        return false;
    {
        Root w{ type_wrap(&dirent_type) };
        Root fn{ native_new("__class_getitem__", de_class_getitem) };
        Root cm{ fn.v.is_nil() ? Value() : classmethod_new(fn.v) };
        StrObj *n = str_intern("__class_getitem__");
        if (w.v.is_nil() || cm.v.is_nil() || !n ||
            dict_set(static_cast<DictObj *>(type_obj(w.v)->dict.obj()), obj_value(n), cm.v) !=
                R::Ok)
            return false;
    }
    if (!mod_type(d, &dirent_type) || !mod_type(d, &stat_type, b_stat_result) ||
        !mod_type(d, &tsize_type, b_terminal_size) || !mod_type(d, &uname_type) ||
        !mod_type(d, &times_type))
        return false;
    if (!environ_install(d))
        return false;
    // What os.py reads to decide what supports what. Nothing takes dir_fd.
    ListObj *have = list_new();
    if (!have)
        return oom() == R::Ok;
    Root rh{ obj_value(have) };
    constexpr Str HAVE[] = { "HAVE_LSTAT", "HAVE_FTRUNCATE" };
    for (Str s : HAVE) {
        Value v = str_new(s);
        if (v.is_nil() || !list_push(list_of(rh.v), v))
            return v.is_nil() ? false : oom() == R::Ok;
    }
    return mod_put(d, "_have_functions", rh.v);
}
