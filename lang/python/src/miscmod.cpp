// Two small modules: `errno`, which is a table of numbers, and `gc` over the
// collector in gc.cpp. `time` is timemod.cpp.
#include "bigint.h"
#include "call.h"
#include "gc.h"
#include "info.h"
#include "intern.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "posix.h"
#include "proc/rt.h"
#include "type.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// ------------------------------------------------------------------ errno

struct Errno {
    Str name;
    i64 code;
    Str text; // strerror's, in glibc's words
};

// musl's numbers, which is the dialect the port kit's <errno.h> already uses.
constexpr Errno ERRNOS[] = {
    { "EPERM", 1, "Operation not permitted" },
    { "ENOENT", 2, "No such file or directory" },
    { "ESRCH", 3, "No such process" },
    { "EINTR", 4, "Interrupted system call" },
    { "EIO", 5, "Input/output error" },
    { "ENXIO", 6, "No such device or address" },
    { "E2BIG", 7, "Argument list too long" },
    { "ENOEXEC", 8, "Exec format error" },
    { "EBADF", 9, "Bad file descriptor" },
    { "ECHILD", 10, "No child processes" },
    { "EAGAIN", 11, "Resource temporarily unavailable" },
    { "ENOMEM", 12, "Cannot allocate memory" },
    { "EACCES", 13, "Permission denied" },
    { "EFAULT", 14, "Bad address" },
    { "EBUSY", 16, "Device or resource busy" },
    { "EEXIST", 17, "File exists" },
    { "EXDEV", 18, "Invalid cross-device link" },
    { "ENODEV", 19, "No such device" },
    { "ENOTDIR", 20, "Not a directory" },
    { "EISDIR", 21, "Is a directory" },
    { "EINVAL", 22, "Invalid argument" },
    { "ENFILE", 23, "Too many open files in system" },
    { "EMFILE", 24, "Too many open files" },
    { "ENOTTY", 25, "Inappropriate ioctl for device" },
    { "EFBIG", 27, "File too large" },
    { "ENOSPC", 28, "No space left on device" },
    { "ESPIPE", 29, "Illegal seek" },
    { "EROFS", 30, "Read-only file system" },
    { "EMLINK", 31, "Too many links" },
    { "EPIPE", 32, "Broken pipe" },
    { "EDOM", 33, "Numerical argument out of domain" },
    { "ERANGE", 34, "Numerical result out of range" },
    { "ENAMETOOLONG", 36, "File name too long" },
    { "ENOSYS", 38, "Function not implemented" },
    { "ENOTEMPTY", 39, "Directory not empty" },
    { "ELOOP", 40, "Too many levels of symbolic links" },
    { "EOVERFLOW", 75, "Value too large for defined data type" },
    { "EILSEQ", 84, "Invalid or incomplete multibyte or wide character" },
    { "EOPNOTSUPP", 95, "Operation not supported" },
    { "ECONNABORTED", 103, "Software caused connection abort" },
    { "ECONNRESET", 104, "Connection reset by peer" },
    { "ENOTCONN", 107, "Transport endpoint is not connected" },
    { "ESHUTDOWN", 108, "Cannot send after transport endpoint shutdown" },
    { "ETIMEDOUT", 110, "Connection timed out" },
    { "ECONNREFUSED", 111, "Connection refused" },
    { "EALREADY", 114, "Operation already in progress" },
    { "EINPROGRESS", 115, "Operation now in progress" },
    { "ECANCELED", 125, "Operation canceled" },
};

} // namespace

Str errno_text(i64 code)
{
    for (const Errno &e : ERRNOS)
        if (e.code == code)
            return e.text;
    return Str();
}

namespace {

// --------------------------------------------------------------------- gc

R g_collect(const CallArgs &a, Value &out)
{
    if (!args_only(a, "collect", 0, 1))
        return R::Err;
    usize before = gc_stats().objects;
    gc_collect();
    usize after = gc_stats().objects;
    out         = Value::of_int(i32(before > after ? before - after : 0));
    return R::Ok;
}

R g_enable(const CallArgs &a, Value &out)
{
    if (!args_only(a, "enable", 0, 0))
        return R::Err;
    gc_enable(true);
    out = value_none();
    return R::Ok;
}

R g_disable(const CallArgs &a, Value &out)
{
    if (!args_only(a, "disable", 0, 0))
        return R::Err;
    gc_enable(false);
    out = value_none();
    return R::Ok;
}

R g_isenabled(const CallArgs &a, Value &out)
{
    if (!args_only(a, "isenabled", 0, 0))
        return R::Err;
    out = value_bool(gc_enabled());
    return R::Ok;
}

// CPython counts allocations per generation; there is one generation here, so
// the first number is the pressure in bytes and the other two are zero.
R g_get_count(const CallArgs &a, Value &out)
{
    if (!args_only(a, "get_count", 0, 0))
        return R::Err;
    TupleObj *t = tuple_new(3);
    if (!t)
        return oom();
    t->items()[0] = Value::of_int(i32(gc_pressure() / 1024));
    t->items()[1] = Value::of_int(0);
    t->items()[2] = Value::of_int(0);
    out           = obj_value(t);
    return R::Ok;
}

R g_get_threshold(const CallArgs &a, Value &out)
{
    if (!args_only(a, "get_threshold", 0, 0))
        return R::Err;
    TupleObj *t = tuple_new(3);
    if (!t)
        return oom();
    t->items()[0] = Value::of_int(i32(gc_threshold() / 1024));
    t->items()[1] = Value::of_int(0);
    t->items()[2] = Value::of_int(0);
    out           = obj_value(t);
    return R::Ok;
}

R g_set_threshold(const CallArgs &a, Value &out)
{
    if (!args_only(a, "set_threshold", 1, 3))
        return R::Err;
    i64 n = 0;
    if (!as_index(a.args[0], n))
        return err_set("TypeError", "an integer is required");
    gc_set_threshold(n > 0 ? usize(n) * 1024 : 0);
    out = value_none();
    return R::Ok;
}

R g_get_objects(const CallArgs &a, Value &out)
{
    if (a.nkw || a.nargs > 1)
        return err_set("TypeError", "get_objects() takes at most one argument");
    ListObj *l = gc_objects();
    if (!l)
        return R::Err;
    out = obj_value(l);
    return R::Ok;
}

R g_get_stats(const CallArgs &a, Value &out)
{
    if (!args_only(a, "get_stats", 0, 0))
        return R::Err;
    GcStats s  = gc_stats();
    DictObj *d = dict_new();
    if (!d)
        return oom();
    Root rd{ obj_value(d) };
    struct Row {
        Str name;
        usize n;
    };
    Row rows[4] = { { "collections", s.collections },
                    { "objects", s.objects },
                    { "bytes", s.bytes },
                    { "collected", s.freed } };
    for (const Row &r : rows) {
        Root v{ int_from_i64(i64(r.n)) };
        if (v.v.is_nil() || !mod_put(static_cast<DictObj *>(rd.v.obj()), r.name, v.v))
            return R::Err;
    }
    ListObj *l = list_new();
    if (!l || !list_push(l, rd.v))
        return oom();
    out = obj_value(l);
    return R::Ok;
}

// Everything here is on one heap list the collector walks, so everything is
// tracked. CPython's answer depends on the type; this one does not.
R g_is_tracked(const CallArgs &a, Value &out)
{
    if (!args_only(a, "is_tracked", 1, 1))
        return R::Err;
    out = value_bool(a.args[0].is_obj() && !(a.args[0].obj()->flags & OBJ_IMMORTAL));
    return R::Ok;
}

R g_noop(const CallArgs &a, Value &out)
{
    (void)a;
    out = value_none();
    return R::Ok;
}

R g_get_debug(const CallArgs &a, Value &out)
{
    if (!args_only(a, "get_debug", 0, 0))
        return R::Err;
    out = Value::of_int(0);
    return R::Ok;
}

constexpr ModDef GC_DEFS[] = {
    { "collect", g_collect },
    { "enable", g_enable },
    { "disable", g_disable },
    { "isenabled", g_isenabled },
    { "get_count", g_get_count },
    { "get_threshold", g_get_threshold },
    { "set_threshold", g_set_threshold },
    { "get_objects", g_get_objects },
    { "get_stats", g_get_stats },
    { "is_tracked", g_is_tracked },
    { "set_debug", g_noop },
    { "get_debug", g_get_debug },
    { "freeze", g_noop },
    { "unfreeze", g_noop },
};

} // namespace

bool errno_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    DictObj *d     = static_cast<DictObj *>(rd.v.obj());
    DictObj *codes = dict_new();
    if (!codes)
        return oom() == R::Ok;
    Root rc{ obj_value(codes) };
    for (const Errno &e : ERRNOS) {
        if (!mod_int(d, e.name, e.code))
            return false;
        Root name{ str_new(e.name) };
        if (name.v.is_nil())
            return false;
        if (dict_set(static_cast<DictObj *>(rc.v.obj()), Value::of_int(i32(e.code)), name.v) !=
            R::Ok)
            return false;
    }
    return mod_int(d, "EWOULDBLOCK", 11) && mod_int(d, "ENOTSUP", 95) &&
           mod_put(d, "errorcode", rc.v);
}

bool gcmod_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    if (!mod_defs(d, GC_DEFS))
        return false;
    // Nothing is ever put here: an object with a __del__ is finalized and then
    // freed, and a cycle among them is not held back.
    ListObj *garbage = list_new();
    ListObj *cbs     = list_new();
    if (!garbage || !cbs)
        return oom() == R::Ok;
    Root rg{ obj_value(garbage) }, rk{ obj_value(cbs) };
    return mod_put(d, "garbage", rg.v) && mod_put(d, "callbacks", rk.v) &&
           mod_int(d, "DEBUG_STATS", 1) && mod_int(d, "DEBUG_COLLECTABLE", 2) &&
           mod_int(d, "DEBUG_UNCOLLECTABLE", 4) && mod_int(d, "DEBUG_SAVEALL", 32) &&
           mod_int(d, "DEBUG_LEAK", 38);
}
