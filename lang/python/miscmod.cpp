// Three small modules: `time` over the one clock reading the driver took,
// `errno` which is a table of numbers, and `gc` over the collector in gc.cpp.
//
// The clock is the interesting one. Sys::Now is milliseconds since this
// worker started and cannot name a day; clock_now() can, and is asynchronous,
// so the driver reads it once before the program starts and time.time() counts
// on from that reading. A program that runs for an hour is an hour past it,
// which is the best a monotonic counter and one wall reading can do.
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
#include "proc/rt.h"
#include "type.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// ------------------------------------------------------------------- time

u64 epoch_at_boot; // the wall clock when the driver read it
u32 now_at_boot;   // and what proc_now() said at that instant
i32 tz_offset_min; // the browser's offset from UTC, east positive

// Milliseconds since the epoch, now.
u64 wall_ms()
{
    u32 since = proc_now() - now_at_boot;
    return epoch_at_boot + since;
}

INFO_TYPE(struct_time_type, "time.struct_time");

constexpr Str TIME_FIELDS[9] = { "tm_year", "tm_mon",  "tm_mday", "tm_hour", "tm_min",
                                 "tm_sec",  "tm_wday", "tm_yday", "tm_isdst" };

// The broken-down time, in the order struct_time holds it.
struct Broken {
    i32 year, mon, mday, hour, min, sec, wday, yday;
};

// Days since the epoch to a civil date. Howard Hinnant's algorithm, which is
// exact over the whole range and has no table in it.
void civil_from_days(i64 z, i32 &y, i32 &m, i32 &d)
{
    z += 719468;
    i64 era = (z >= 0 ? z : z - 146096) / 146097;
    i64 doe = z - era * 146097;
    i64 yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    i64 yr  = yoe + era * 400;
    i64 doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    i64 mp  = (5 * doy + 2) / 153;
    d       = i32(doy - (153 * mp + 2) / 5 + 1);
    m       = i32(mp < 10 ? mp + 3 : mp - 9);
    y       = i32(yr + (m <= 2));
}

i64 days_from_civil(i64 y, i32 m, i32 d)
{
    y -= m <= 2;
    i64 era = (y >= 0 ? y : y - 399) / 400;
    i64 yoe = y - era * 400;
    i64 doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    i64 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

// Seconds since the epoch, broken down. `local` shifts by the browser's offset.
Broken break_down(i64 secs, bool local)
{
    if (local)
        secs += i64(tz_offset_min) * 60;
    i64 days = secs / 86400;
    i64 rem  = secs % 86400;
    if (rem < 0) {
        rem += 86400;
        days -= 1;
    }
    Broken b{};
    civil_from_days(days, b.year, b.mon, b.mday);
    b.hour = i32(rem / 3600);
    b.min  = i32(rem % 3600 / 60);
    b.sec  = i32(rem % 60);
    // 1970-01-01 was a Thursday, and Python counts Monday as 0.
    b.wday = i32(((days % 7) + 10) % 7);
    b.yday = i32(days - days_from_civil(b.year, 1, 1)) + 1;
    return b;
}

Value struct_time_new(const Broken &b, i32 isdst)
{
    Value items[9] = { Value::of_int(b.year), Value::of_int(b.mon),  Value::of_int(b.mday),
                       Value::of_int(b.hour), Value::of_int(b.min),  Value::of_int(b.sec),
                       Value::of_int(b.wday), Value::of_int(b.yday), Value::of_int(isdst) };
    return info_new(&struct_time_type, items, TIME_FIELDS, 9);
}

// A struct_time, a 9-tuple or a list, back to the fields.
bool take_time(Value v, Broken &b, i32 &isdst)
{
    usize n = 0;
    if (py_len(v, n) != R::Ok)
        return false;
    if (n != 9)
        return err_set("TypeError", "the argument must have 9 items"), false;
    i64 got[9];
    for (usize i = 0; i < 9; i++) {
        Value one;
        if (py_getitem(v, Value::of_int(i32(i)), one) != R::Ok)
            return false;
        if (!as_index(one, got[i]))
            return err_set("TypeError", "an integer is required"), false;
    }
    b.year = i32(got[0]);
    b.mon  = i32(got[1]);
    b.mday = i32(got[2]);
    b.hour = i32(got[3]);
    b.min  = i32(got[4]);
    b.sec  = i32(got[5]);
    b.wday = i32(got[6]);
    b.yday = i32(got[7]);
    isdst  = i32(got[8]);
    return true;
}

constexpr Str WDAY[7]  = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
constexpr Str WDAYF[7] = { "Monday", "Tuesday",  "Wednesday", "Thursday",
                           "Friday", "Saturday", "Sunday" };
constexpr Str MON[12]  = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
constexpr Str MONF[12] = { "January", "February", "March",     "April",   "May",      "June",
                           "July",    "August",   "September", "October", "November", "December" };

void put2(String &out, i32 n)
{
    char d[2] = { char('0' + (n / 10) % 10), char('0' + n % 10) };
    out.append(Str(d, 2));
}

void put3(String &out, i32 n)
{
    char d[3] = { char('0' + (n / 100) % 10), char('0' + (n / 10) % 10), char('0' + n % 10) };
    out.append(Str(d, 3));
}

// The conversions strftime answers. Anything else is copied through, which is
// what CPython's does with an unknown one on most platforms.
bool strftime_one(String &out, char c, const Broken &b)
{
    char tmp[24];
    switch (c) {
    case 'a':
        return out.append(WDAY[b.wday % 7]);
    case 'A':
        return out.append(WDAYF[b.wday % 7]);
    case 'b':
    case 'h':
        return out.append(MON[(b.mon - 1) % 12]);
    case 'B':
        return out.append(MONF[(b.mon - 1) % 12]);
    case 'd':
        return put2(out, b.mday), true;
    case 'e':
        return b.mday < 10 ? (out.push(' ') && out.push(char('0' + b.mday)))
                           : (put2(out, b.mday), true);
    case 'H':
        return put2(out, b.hour), true;
    case 'I':
        return put2(out, b.hour % 12 == 0 ? 12 : b.hour % 12), true;
    case 'j':
        return put3(out, b.yday), true;
    case 'm':
        return put2(out, b.mon), true;
    case 'M':
        return put2(out, b.min), true;
    case 'p':
        return out.append(b.hour < 12 ? Str("AM") : Str("PM"));
    case 'S':
        return put2(out, b.sec), true;
    case 'w':
        return out.push(char('0' + (b.wday + 1) % 7));
    case 'y':
        return put2(out, b.year % 100), true;
    case 'Y':
        return out.append(int_text(tmp, sizeof tmp, b.year));
    case 'Z':
        return out.append("UTC");
    case 'n':
        return out.push('\n');
    case 't':
        return out.push('\t');
    case '%':
        return out.push('%');
    default:
        return out.push('%') && out.push(c);
    }
}

// "Sun Jun 20 23:21:05 1993", which asctime and ctime both print.
bool asctime_text(const Broken &b, String &out)
{
    char tmp[24];
    if (!out.append(WDAY[b.wday % 7]) || !out.push(' ') || !out.append(MON[(b.mon - 1) % 12]) ||
        !out.push(' '))
        return false;
    if (b.mday < 10 && !out.push(' '))
        return false;
    if (!out.append(int_text(tmp, sizeof tmp, b.mday)) || !out.push(' '))
        return false;
    put2(out, b.hour);
    if (!out.push(':'))
        return false;
    put2(out, b.min);
    if (!out.push(':'))
        return false;
    put2(out, b.sec);
    return out.push(' ') && out.append(int_text(tmp, sizeof tmp, b.year));
}

R t_time(const CallArgs &a, Value &out)
{
    if (!args_only(a, "time", 0, 0))
        return R::Err;
    out = float_new(f64(wall_ms()) / 1000.0);
    return out.is_nil() ? R::Err : R::Ok;
}

R t_time_ns(const CallArgs &a, Value &out)
{
    if (!args_only(a, "time_ns", 0, 0))
        return R::Err;
    Root ms{ int_from_i64(i64(wall_ms())) };
    if (ms.v.is_nil())
        return R::Err;
    Value got;
    if (int_arith(ms.v, Value::of_int(1000000), Op::Mul, got) != R::Ok)
        return R::Err;
    out = got;
    return R::Ok;
}

R t_monotonic(const CallArgs &a, Value &out)
{
    if (!args_only(a, "monotonic", 0, 0))
        return R::Err;
    out = float_new(f64(proc_now()) / 1000.0);
    return out.is_nil() ? R::Err : R::Ok;
}

R t_monotonic_ns(const CallArgs &a, Value &out)
{
    if (!args_only(a, "monotonic_ns", 0, 0))
        return R::Err;
    Root ms{ int_from_i64(i64(proc_now())) };
    if (ms.v.is_nil())
        return R::Err;
    Value got;
    if (int_arith(ms.v, Value::of_int(1000000), Op::Mul, got) != R::Ok)
        return R::Err;
    out = got;
    return R::Ok;
}

// The step a sleep has come back from; there is nothing to do but answer.
R sleep_step(ContObj *k, Value)
{
    if (k->i) {
        k->i = 0;
        return cont_done(k, value_none());
    }
    k->i = 1;
    return cont_sleep(k, k->j);
}

R t_sleep(const CallArgs &a, Value &out)
{
    if (!args_only(a, "sleep", 1, 1))
        return R::Err;
    f64 secs = 0;
    if (!as_number(a.args[0], secs))
        return err_set2("TypeError", "sleep() wants a number", type_name(a.args[0]));
    if (secs < 0)
        return err_set("ValueError", "sleep length must be non-negative");
    f64 ms = secs * 1000.0;
    Root kv{ cont_new(sleep_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->j = u32(ms > 4000000000.0 ? 4000000000.0 : ms);
    out              = kv.v;
    return R::Ok;
}

R break_call(const CallArgs &a, Value &out, Str who, bool local)
{
    if (!args_only(a, who, 0, 1))
        return R::Err;
    i64 secs = 0;
    if (a.nargs && !is_none(a.args[0])) {
        f64 x = 0;
        if (!as_number(a.args[0], x))
            return err_set2("TypeError", "a number is required", type_name(a.args[0]));
        secs = i64(x);
    } else {
        secs = i64(wall_ms() / 1000);
    }
    out = struct_time_new(break_down(secs, local), local ? 0 : 0);
    return out.is_nil() ? R::Err : R::Ok;
}

R t_gmtime(const CallArgs &a, Value &out)
{
    return break_call(a, out, "gmtime", false);
}

R t_localtime(const CallArgs &a, Value &out)
{
    return break_call(a, out, "localtime", true);
}

R t_mktime(const CallArgs &a, Value &out)
{
    if (!args_only(a, "mktime", 1, 1))
        return R::Err;
    Broken b{};
    i32 isdst = 0;
    if (!take_time(a.args[0], b, isdst))
        return R::Err;
    i64 days = days_from_civil(b.year, b.mon, b.mday);
    i64 secs = days * 86400 + b.hour * 3600 + b.min * 60 + b.sec - i64(tz_offset_min) * 60;
    out      = float_new(f64(secs));
    return out.is_nil() ? R::Err : R::Ok;
}

R t_asctime(const CallArgs &a, Value &out)
{
    if (!args_only(a, "asctime", 0, 1))
        return R::Err;
    Broken b{};
    i32 isdst = 0;
    if (a.nargs && !is_none(a.args[0])) {
        if (!take_time(a.args[0], b, isdst))
            return R::Err;
    } else {
        b = break_down(i64(wall_ms() / 1000), true);
    }
    String text;
    if (!asctime_text(b, text))
        return oom();
    out = str_new(text.str());
    return out.is_nil() ? R::Err : R::Ok;
}

R t_ctime(const CallArgs &a, Value &out)
{
    if (!args_only(a, "ctime", 0, 1))
        return R::Err;
    i64 secs = i64(wall_ms() / 1000);
    if (a.nargs && !is_none(a.args[0])) {
        f64 x = 0;
        if (!as_number(a.args[0], x))
            return err_set2("TypeError", "a number is required", type_name(a.args[0]));
        secs = i64(x);
    }
    String text;
    if (!asctime_text(break_down(secs, true), text))
        return oom();
    out = str_new(text.str());
    return out.is_nil() ? R::Err : R::Ok;
}

R t_strftime(const CallArgs &a, Value &out)
{
    if (!args_only(a, "strftime", 1, 2))
        return R::Err;
    if (!is_str(a.args[0]))
        return err_set2("TypeError", "strftime() wants a format", type_name(a.args[0]));
    Broken b{};
    i32 isdst = 0;
    if (a.nargs > 1) {
        if (!take_time(a.args[1], b, isdst))
            return R::Err;
    } else {
        b = break_down(i64(wall_ms() / 1000), true);
    }
    Str fmt = str_of(a.args[0])->str();
    String text;
    for (usize i = 0; i < fmt.size(); i++) {
        if (fmt[i] != '%') {
            if (!text.push(fmt[i]))
                return oom();
            continue;
        }
        if (++i >= fmt.size())
            return err_set("ValueError", "stray %% in format");
        if (!strftime_one(text, fmt[i], b))
            return oom();
    }
    out = str_new(text.str());
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr ModDef TIME_DEFS[] = {
    { "time", t_time },
    { "time_ns", t_time_ns },
    { "monotonic", t_monotonic },
    { "monotonic_ns", t_monotonic_ns },
    { "perf_counter", t_monotonic },
    { "perf_counter_ns", t_monotonic_ns },
    { "process_time", t_monotonic },
    { "process_time_ns", t_monotonic_ns },
    { "sleep", t_sleep },
    { "gmtime", t_gmtime },
    { "localtime", t_localtime },
    { "mktime", t_mktime },
    { "asctime", t_asctime },
    { "ctime", t_ctime },
    { "strftime", t_strftime },
};

// ------------------------------------------------------------------ errno

struct Errno {
    Str name;
    i64 code;
};

// musl's numbers, which is the dialect the port kit's <errno.h> already uses.
constexpr Errno ERRNOS[] = {
    { "EPERM", 1 },         { "ENOENT", 2 },  { "ESRCH", 3 },       { "EINTR", 4 },
    { "EIO", 5 },           { "ENXIO", 6 },   { "E2BIG", 7 },       { "ENOEXEC", 8 },
    { "EBADF", 9 },         { "ECHILD", 10 }, { "EAGAIN", 11 },     { "ENOMEM", 12 },
    { "EACCES", 13 },       { "EFAULT", 14 }, { "EBUSY", 16 },      { "EEXIST", 17 },
    { "EXDEV", 18 },        { "ENODEV", 19 }, { "ENOTDIR", 20 },    { "EISDIR", 21 },
    { "EINVAL", 22 },       { "ENFILE", 23 }, { "EMFILE", 24 },     { "ENOTTY", 25 },
    { "EFBIG", 27 },        { "ENOSPC", 28 }, { "ESPIPE", 29 },     { "EROFS", 30 },
    { "EMLINK", 31 },       { "EPIPE", 32 },  { "EDOM", 33 },       { "ERANGE", 34 },
    { "ENAMETOOLONG", 36 }, { "ENOSYS", 38 }, { "ENOTEMPTY", 39 },  { "ELOOP", 40 },
    { "EOVERFLOW", 75 },    { "EILSEQ", 84 }, { "EOPNOTSUPP", 95 },
};

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

void time_set_clock(u64 epoch_ms, i32 tz_min, u32 at_now)
{
    epoch_at_boot = epoch_ms;
    now_at_boot   = at_now;
    tz_offset_min = tz_min;
}

bool time_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    if (!mod_defs(d, TIME_DEFS))
        return false;
    Root t{ type_wrap(&struct_time_type) };
    if (t.v.is_nil() || !mod_put(d, "struct_time", t.v))
        return false;
    // West of UTC is positive here, as C's timezone is and the browser's is
    // not. There is no daylight-saving table: the browser has already applied
    // one, and its offset is what tz_min carries.
    TupleObj *names = tuple_new(2);
    if (!names)
        return oom() == R::Ok;
    Root rn{ obj_value(names) };
    Value utc = str_new("UTC"), loc = str_new("UTC");
    if (utc.is_nil() || loc.is_nil())
        return false;
    static_cast<TupleObj *>(rn.v.obj())->items()[0] = utc;
    static_cast<TupleObj *>(rn.v.obj())->items()[1] = loc;
    return mod_int(d, "timezone", -i64(tz_offset_min) * 60) &&
           mod_int(d, "altzone", -i64(tz_offset_min) * 60) && mod_int(d, "daylight", 0) &&
           mod_put(d, "tzname", rn.v);
}

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
    return mod_int(d, "EWOULDBLOCK", 11) && mod_put(d, "errorcode", rc.v);
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
