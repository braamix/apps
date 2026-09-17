// `time`, over the one clock reading the driver took.
//
// Sys::Now is milliseconds since this worker started and cannot name a day;
// clock_now() can, and is asynchronous, so the driver reads it once before the
// program starts and time.time() counts on from that reading. A program that
// runs for an hour is an hour past it, which is the best a monotonic counter
// and one wall reading can do.
//
// The rest is Modules/timemodule.c's surface over no C library: the broken-
// down time is computed here, and strftime is the BSD one CPython's goldens
// on the host come from. The local zone is the browser's offset and has no
// daylight-saving table; see README.md.
#include "bigint.h"
#include "builtin.h"
#include "call.h"
#include "exc.h"
#include "gc.h"
#include "info.h"
#include "intern.h"
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

u64 epoch_at_boot; // the wall clock when the driver read it
u32 now_at_boot;   // and what proc_now() said at that instant
i32 tz_offset_min; // the browser's offset from UTC, east positive

// Milliseconds since the epoch, now.
u64 wall_ms()
{
    u32 since = proc_now() - now_at_boot;
    return epoch_at_boot + since;
}

// Division that floors, as Python's does.
inline i64 floordiv(i64 a, i64 b)
{
    i64 q = a / b;
    return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
}

// ----------------------------------------------------------- struct_time

INFO_TYPE(struct_time_type, "time.struct_time");

constexpr usize TM_ITEMS            = 11;
constexpr usize TM_SHOWN            = 9;
constexpr Str TIME_FIELDS[TM_ITEMS] = { "tm_year",  "tm_mon",  "tm_mday",  "tm_hour",
                                        "tm_min",   "tm_sec",  "tm_wday",  "tm_yday",
                                        "tm_isdst", "tm_zone", "tm_gmtoff" };

// C's struct tm, with the year as it is and not less 1900, wide enough that a
// year past int is seen rather than wrapped.
struct Tm {
    i64 year;
    i32 mon;  // 0..11
    i32 mday; // 1..31
    i32 hour, min, sec;
    i32 wday; // 0..6, Sunday first
    i32 yday; // 0..365
    i32 isdst;
    i64 gmtoff;
    String zone;
    bool has_zone;
};

// Days since the epoch to a civil date. Howard Hinnant's algorithm, which is
// exact over the whole range and has no table in it.
void civil_from_days(i64 z, i64 &y, i32 &m, i32 &d)
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
    y       = yr + (m <= 2);
}

i64 days_from_civil(i64 y, i64 m, i64 d)
{
    y -= m <= 2;
    i64 era = (y >= 0 ? y : y - 399) / 400;
    i64 yoe = y - era * 400;
    i64 doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    i64 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

bool is_leap(i64 y)
{
    return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

// The local zone's name: UTC, or the offset as the IANA Etc zones spell it.
void zone_name(i32 off_min, String &out)
{
    out.clear();
    if (off_min == 0) {
        out.append("UTC");
        return;
    }
    i32 a = off_min < 0 ? -off_min : off_min;
    out.push(off_min < 0 ? '-' : '+');
    out.push(char('0' + a / 600));
    out.push(char('0' + a / 60 % 10));
    if (a % 60) {
        out.push(char('0' + a % 60 / 10));
        out.push(char('0' + a % 60 % 10));
    }
}

// Seconds since the epoch, broken down; `local` shifts by the browser's
// offset. False with OSError(EOVERFLOW) pending when the year is past int.
bool break_down(i64 secs, bool local, Tm &tm)
{
    i64 off = local ? i64(tz_offset_min) * 60 : 0;
    secs += off;
    i64 days = floordiv(secs, 86400);
    i64 rem  = secs - days * 86400;
    civil_from_days(days, tm.year, tm.mon, tm.mday);
    if (tm.year - 1900 > 2147483647ll || tm.year - 1900 < -2147483647ll - 1)
        return err_errno(75) == R::Ok;
    tm.mon -= 1;
    tm.hour   = i32(rem / 3600);
    tm.min    = i32(rem % 3600 / 60);
    tm.sec    = i32(rem % 60);
    tm.wday   = i32(((days % 7) + 11) % 7);
    tm.yday   = i32(days - days_from_civil(tm.year, 1, 1));
    tm.isdst  = 0;
    tm.gmtoff = off;
    if (local)
        zone_name(tz_offset_min, tm.zone);
    else
        tm.zone.assign("UTC");
    tm.has_zone = true;
    return true;
}

// tmtotuple.
Value struct_time_of(const Tm &tm)
{
    Value items[TM_ITEMS];
    Roots pin{ items, TM_ITEMS };
    items[0]  = int_from_i64(tm.year);
    items[1]  = Value::of_int(tm.mon + 1);
    items[2]  = Value::of_int(tm.mday);
    items[3]  = Value::of_int(tm.hour);
    items[4]  = Value::of_int(tm.min);
    items[5]  = Value::of_int(tm.sec);
    items[6]  = Value::of_int((tm.wday + 6) % 7);
    items[7]  = Value::of_int(tm.yday + 1);
    items[8]  = Value::of_int(tm.isdst);
    items[9]  = str_new(tm.zone.str());
    items[10] = int_from_i64(tm.gmtoff);
    for (Value v : items)
        if (v.is_nil())
            return Value();
    return info_new(&struct_time_type, items, TIME_FIELDS, TM_ITEMS, TM_SHOWN);
}

bool is_struct_time(Value v)
{
    return v.is_obj() && v.obj()->type == &struct_time_type;
}

// A C int out of an item of the tuple, as PyArg_ParseTuple's "i" takes one.
bool c_int(Value v, i32 &out)
{
    if (is_float(v))
        return err_set("TypeError", "'float' object cannot be interpreted as an integer") == R::Ok;
    i64 n = 0;
    if (!is_intval(v)) {
        if (!as_int_arg(v, n))
            return err_not_index(v) == R::Ok;
    } else if (!int_to_i64(v, n)) {
        n = int_is_neg(v) ? -(i64(1) << 40) : i64(1) << 40;
    }
    if (n > 2147483647ll)
        return err_set("OverflowError", "signed integer is greater than maximum") == R::Ok;
    if (n < -2147483647ll - 1)
        return err_set("OverflowError", "signed integer is less than minimum") == R::Ok;
    out = i32(n);
    return true;
}

// gettmarg: a tuple or a struct_time of nine, the C way round.
bool gettmarg(Value v, Str who, Tm &tm)
{
    tm          = Tm{};
    tm.has_zone = false;
    const Value *items;
    u32 n = 0;
    if (is_tuple(v)) {
        items = static_cast<TupleObj *>(v.obj())->items();
        n     = static_cast<TupleObj *>(v.obj())->len;
    } else if (is_struct_time(v)) {
        TupleObj *t = static_cast<TupleObj *>(static_cast<InfoObj *>(v.obj())->items.obj());
        items       = t->items();
        n           = t->len;
    } else {
        return err_set("TypeError", "Tuple or struct_time argument required") == R::Ok;
    }
    if (n != 9) {
        Buf<96> b;
        b.put(who).put("(): illegal time tuple argument");
        return err_set("TypeError", b.str()) == R::Ok;
    }
    i32 f[9];
    for (u32 i = 0; i < 9; i++)
        if (!c_int(items[i], f[i]))
            return false;
    if (f[0] < -2147483647 - 1 + 1900)
        return err_set("OverflowError", "year out of range") == R::Ok;
    tm.year  = f[0];
    tm.mon   = f[1] - 1;
    tm.mday  = f[2];
    tm.hour  = f[3];
    tm.min   = f[4];
    tm.sec   = f[5];
    tm.wday  = (f[6] + 1) % 7;
    tm.yday  = f[7] - 1;
    tm.isdst = f[8];
    if (is_struct_time(v)) {
        TupleObj *h = static_cast<TupleObj *>(static_cast<InfoObj *>(v.obj())->hidden.obj());
        if (h->len >= 2) {
            Value z = h->items()[0], g = h->items()[1];
            if (!is_none(z)) {
                if (!is_str(z))
                    return err_set2("TypeError", "bad argument type for built-in operation",
                                    type_name(z)) == R::Ok;
                tm.zone.assign(str_of(z)->str());
                tm.has_zone = true;
            }
            if (!is_none(g)) {
                i64 off = 0;
                if (!as_int_arg(g, off))
                    return err_not_index(g) == R::Ok;
                tm.gmtoff = off;
            }
        }
    }
    return true;
}

bool checktm(Tm &tm)
{
    if (tm.mon == -1)
        tm.mon = 0;
    else if (tm.mon < 0 || tm.mon > 11)
        return err_set("ValueError", "month out of range") == R::Ok;
    if (tm.mday == 0)
        tm.mday = 1;
    else if (tm.mday < 0 || tm.mday > 31)
        return err_set("ValueError", "day of month out of range") == R::Ok;
    if (tm.hour < 0 || tm.hour > 23)
        return err_set("ValueError", "hour out of range") == R::Ok;
    if (tm.min < 0 || tm.min > 59)
        return err_set("ValueError", "minute out of range") == R::Ok;
    if (tm.sec < 0 || tm.sec > 61)
        return err_set("ValueError", "seconds out of range") == R::Ok;
    if (tm.wday < 0)
        return err_set("ValueError", "day of week out of range") == R::Ok;
    if (tm.yday == -1)
        tm.yday = 0;
    else if (tm.yday < 0 || tm.yday > 365)
        return err_set("ValueError", "day of year out of range") == R::Ok;
    return true;
}

// The local seconds a broken-down time names, fields out of range carried as
// mktime carries them.
bool tm_to_secs(const Tm &tm, i64 off_secs, i64 &out)
{
    i64 y = tm.year + floordiv(tm.mon, 12);
    i64 m = tm.mon - floordiv(tm.mon, 12) * 12 + 1;
    if (y > 2147483647ll || y < -2147483647ll)
        return false;
    i64 days = days_from_civil(y, m, 1) + tm.mday - 1;
    out      = days * 86400 + i64(tm.hour) * 3600 + i64(tm.min) * 60 + tm.sec - off_secs;
    return true;
}

// ------------------------------------------------------------ the clocks

// _PyTime_ObjectToTime_t with ROUND_FLOOR, or now when `v` is Nil or None.
bool time_t_arg(Value v, i64 &out)
{
    if (v.is_nil() || is_none(v)) {
        out = floordiv(i64(wall_ms()), 1000);
        return true;
    }
    if (is_float(v)) {
        f64 d = float_of(v);
        if (d != d)
            return err_set("ValueError", "Invalid value NaN (not a number)") == R::Ok;
        d = __builtin_floor(d);
        if (!(d >= -9223372036854775808.0 && d < 9223372036854775808.0))
            return err_set("OverflowError", "timestamp out of range for platform time_t") == R::Ok;
        out = i64(d);
        return true;
    }
    if (is_intval(v)) {
        if (!int_to_i64(v, out))
            return err_set("OverflowError", "timestamp out of range for platform time_t") == R::Ok;
        return true;
    }
    Buf<96> b;
    b.put("must be real number, not ").put(type_name(v));
    return err_set("TypeError", b.str()) == R::Ok;
}

R t_time(const CallArgs &a, Value &out)
{
    if (!args_only(a, "time", 0, 0))
        return R::Err;
    out = float_new(f64(wall_ms()) / 1000.0);
    return out.is_nil() ? R::Err : R::Ok;
}

R ns_of(i64 ms, Value &out)
{
    Root v{ int_from_i64(ms) };
    if (v.v.is_nil())
        return R::Err;
    Value got;
    if (int_arith(v.v, Value::of_int(1000000), Op::Mul, got) != R::Ok)
        return R::Err;
    out = got;
    return R::Ok;
}

R t_time_ns(const CallArgs &a, Value &out)
{
    if (!args_only(a, "time_ns", 0, 0))
        return R::Err;
    return ns_of(i64(wall_ms()), out);
}

// monotonic, perf_counter, process_time and thread_time: one counter.
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
    return ns_of(i64(proc_now()), out);
}

// Linux's clock numbers.
enum : i32 {
    CLOCK_REALTIME           = 0,
    CLOCK_MONOTONIC          = 1,
    CLOCK_PROCESS_CPUTIME_ID = 2,
    CLOCK_THREAD_CPUTIME_ID  = 3,
    CLOCK_MONOTONIC_RAW      = 4,
    CLOCK_BOOTTIME           = 7,
    CLOCK_TAI                = 11,
};

// The clock's reading in milliseconds; false with OSError(EINVAL) for an id
// that is none of them.
bool clock_ms(Value id, i64 &ms)
{
    i32 c = 0;
    if (!c_int(id, c))
        return false;
    switch (c) {
    case CLOCK_REALTIME:
    case CLOCK_TAI:
        ms = i64(wall_ms());
        return true;
    case CLOCK_MONOTONIC:
    case CLOCK_PROCESS_CPUTIME_ID:
    case CLOCK_THREAD_CPUTIME_ID:
    case CLOCK_MONOTONIC_RAW:
    case CLOCK_BOOTTIME:
        ms = i64(proc_now());
        return true;
    default:
        return err_errno(22) == R::Ok;
    }
}

R t_clock_gettime(const CallArgs &a, Value &out)
{
    i64 ms = 0;
    if (!args_only(a, "clock_gettime", 1, 1) || !clock_ms(a.args[0], ms))
        return R::Err;
    out = float_new(f64(ms) / 1000.0);
    return out.is_nil() ? R::Err : R::Ok;
}

R t_clock_gettime_ns(const CallArgs &a, Value &out)
{
    i64 ms = 0;
    if (!args_only(a, "clock_gettime_ns", 1, 1) || !clock_ms(a.args[0], ms))
        return R::Err;
    return ns_of(ms, out);
}

R t_clock_getres(const CallArgs &a, Value &out)
{
    i64 ms = 0;
    if (!args_only(a, "clock_getres", 1, 1) || !clock_ms(a.args[0], ms))
        return R::Err;
    out = float_new(0.001);
    return out.is_nil() ? R::Err : R::Ok;
}

// Nothing here may set a clock.
R t_clock_settime(const CallArgs &a, Value &out)
{
    i64 ms = 0;
    (void)out;
    if (!args_only(a, "clock_settime", 2, 2) || !clock_ms(a.args[0], ms))
        return R::Err;
    f64 x = 0;
    if (!as_number(a.args[1], x))
        return err_set2("TypeError", "must be real number, not", type_name(a.args[1]));
    return err_errno(1);
}

R t_clock_settime_ns(const CallArgs &a, Value &out)
{
    i64 ms = 0;
    (void)out;
    if (!args_only(a, "clock_settime_ns", 2, 2) || !clock_ms(a.args[0], ms))
        return R::Err;
    if (!is_intval(a.args[1]))
        return err_not_index(a.args[1]);
    return err_errno(1);
}

R t_get_clock_info(const CallArgs &a, Value &out)
{
    if (!args_only(a, "get_clock_info", 1, 1))
        return R::Err;
    if (!is_str(a.args[0])) {
        Buf<96> b;
        b.put("get_clock_info() argument must be str, not ").put(type_name(a.args[0]));
        return err_set("TypeError", b.str());
    }
    struct Info {
        Str name, impl;
        bool monotonic, adjustable;
    };
    constexpr Info INFOS[] = {
        { "time", "clock_gettime(CLOCK_REALTIME)", false, true },
        { "monotonic", "clock_gettime(CLOCK_MONOTONIC)", true, false },
        { "perf_counter", "clock_gettime(CLOCK_MONOTONIC)", true, false },
        { "process_time", "clock_gettime(CLOCK_PROCESS_CPUTIME_ID)", true, false },
        { "thread_time", "clock_gettime(CLOCK_THREAD_CPUTIME_ID)", true, false },
    };
    Str name = str_of(a.args[0])->str();
    for (const Info &i : INFOS) {
        if (i.name != name)
            continue;
        DictObj *d = dict_new();
        if (!d)
            return oom();
        Root rd{ obj_value(d) };
        Root res{ float_new(0.001) };
        if (res.v.is_nil() || !mod_str(d, "implementation", i.impl) ||
            !mod_put(d, "monotonic", value_bool(i.monotonic)) ||
            !mod_put(d, "adjustable", value_bool(i.adjustable)) || !mod_put(d, "resolution", res.v))
            return R::Err;
        out = namespace_new(rd.v);
        return out.is_nil() ? R::Err : R::Ok;
    }
    return err_set("ValueError", "unknown clock");
}

// j the length, x[0] when it ends. A signal cuts a sleep short; its handler
// runs, and the sleep goes on for what is left, as PEP 475 says.
R sleep_step(ContObj *k, Value)
{
    switch (k->i) {
    case 0:
        k->x[0] = i64(proc_now()) + k->j;
        k->i    = 1;
        return cont_sleep(k, k->j);
    case 1: {
        u32 sig = vm_take_signal();
        if (!sig)
            return cont_done(k, value_none());
        Value h;
        if (sig_handler(sig, h) != R::Ok)
            return R::Err;
        if (!h.is_nil()) {
            k->i    = 2;
            Value f = vm_frame();
            return cont_call(k, h, Value::of_int(i32(sig)), 2, f.is_nil() ? value_none() : f);
        }
        [[fallthrough]];
    }
    default: {
        i64 left = k->x[0] - i64(proc_now());
        if (left <= 0)
            return cont_done(k, value_none());
        k->i = 1;
        return cont_sleep(k, u32(left));
    }
    }
}

R t_sleep(const CallArgs &a, Value &out)
{
    if (!args_only(a, "sleep", 1, 1))
        return R::Err;
    f64 secs = 0;
    if (!as_number(a.args[0], secs)) {
        Buf<96> b;
        b.put("must be real number, not ").put(type_name(a.args[0]));
        return err_set("TypeError", b.str());
    }
    if (secs != secs)
        return err_set("ValueError", "Invalid value NaN (not a number)");
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

// ------------------------------------------------------ broken-down time

R break_call(const CallArgs &a, Value &out, Str who, bool local)
{
    if (!args_only(a, who, 0, 1))
        return R::Err;
    i64 secs = 0;
    if (!time_t_arg(a.nargs ? a.args[0] : Value(), secs))
        return R::Err;
    Tm tm;
    if (!break_down(secs, local, tm))
        return R::Err;
    out = struct_time_of(tm);
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
    Tm tm;
    if (!gettmarg(a.args[0], "mktime", tm))
        return R::Err;
    i64 secs = 0;
    if (!tm_to_secs(tm, i64(tz_offset_min) * 60, secs))
        return err_set("OverflowError", "mktime argument out of range");
    out = float_new(f64(secs));
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr Str WDAY[7]  = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
constexpr Str WDAYF[7] = { "Sunday",   "Monday", "Tuesday", "Wednesday",
                           "Thursday", "Friday", "Saturday" };
constexpr Str MON[12]  = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
constexpr Str MONF[12] = { "January", "February", "March",     "April",   "May",      "June",
                           "July",    "August",   "September", "October", "November", "December" };

// "%s %s%3d %.2d:%.2d:%.2d %d".
bool asctime_text(const Tm &tm, String &out)
{
    char tmp[32];
    Buf<96> b;
    b.put(WDAY[tm.wday % 7]).put(' ').put(MON[tm.mon % 12]);
    Str d = int_text(tmp, sizeof tmp, tm.mday);
    for (usize i = d.size(); i < 3; i++)
        b.put(' ');
    b.put(d).put(' ');
    auto two = [&](i32 v) {
        Str s = int_text(tmp, sizeof tmp, v < 0 ? -v : v);
        if (v < 0)
            b.put('-');
        if (s.size() < 2)
            b.put('0');
        b.put(s);
    };
    two(tm.hour);
    b.put(':');
    two(tm.min);
    b.put(':');
    two(tm.sec);
    b.put(' ').put(int_text(tmp, sizeof tmp, tm.year));
    return out.append(b.str());
}

R t_asctime(const CallArgs &a, Value &out)
{
    if (!args_only(a, "asctime", 0, 1))
        return R::Err;
    Tm tm;
    if (a.nargs == 0) {
        if (!break_down(floordiv(i64(wall_ms()), 1000), true, tm))
            return R::Err;
    } else if (!gettmarg(a.args[0], "asctime", tm) || !checktm(tm)) {
        return R::Err;
    }
    String text;
    if (!asctime_text(tm, text))
        return oom();
    out = str_new(text.str());
    return out.is_nil() ? R::Err : R::Ok;
}

R t_ctime(const CallArgs &a, Value &out)
{
    if (!args_only(a, "ctime", 0, 1))
        return R::Err;
    i64 secs = 0;
    Tm tm;
    if (!time_t_arg(a.nargs ? a.args[0] : Value(), secs) || !break_down(secs, true, tm))
        return R::Err;
    String text;
    if (!asctime_text(tm, text))
        return oom();
    out = str_new(text.str());
    return out.is_nil() ? R::Err : R::Ok;
}

// ---------------------------------------------------------------- strftime

enum class Pad : u8 { Default, None, Space, Zero };

// One number, `width` wide, padded as the conversion wants unless a flag
// said otherwise.
void conv(String &out, i64 v, u32 width, char dflt, Pad pad)
{
    char tmp[32];
    Str s     = int_text(tmp, sizeof tmp, v < 0 ? -v : v);
    char fill = pad == Pad::Space ? ' ' : pad == Pad::Zero ? '0' : dflt;
    if (v < 0)
        out.push('-');
    if (pad != Pad::None)
        for (usize i = s.size() + (v < 0 ? 1 : 0); i < width; i++)
            out.push(fill);
    out.append(s);
}

// FreeBSD's _yconv: the century, the two last digits, or both.
void yconv(String &out, i64 a, i64 b, bool top, bool yy, Pad pad)
{
    constexpr i64 D = 100;
    i64 trail       = a % D + b % D;
    i64 lead        = a / D + b / D + trail / D;
    trail %= D;
    if (trail < 0 && lead > 0) {
        trail += D;
        --lead;
    } else if (lead < 0 && trail > 0) {
        trail -= D;
        ++lead;
    }
    if (top) {
        if (lead == 0 && trail < 0)
            out.append("-0");
        else
            conv(out, lead, 2, '0', pad);
    }
    if (yy)
        conv(out, trail < 0 ? -trail : trail, 2, '0', pad);
}

// The ISO 8601 year and week of a date.
void iso_week(const Tm &tm, i64 &year, i32 &week)
{
    constexpr i32 DAYSPERWEEK = 7;
    year                      = tm.year;
    i32 yday                  = tm.yday;
    i32 wday                  = tm.wday;
    for (;;) {
        i32 len = is_leap(year) ? 366 : 365;
        // What yday (-3 ... 3) does the ISO year begin on?
        i32 bot = ((yday + 11 - wday) % DAYSPERWEEK) - 3;
        // What yday does the NEXT ISO year begin on?
        i32 top = bot - (len % DAYSPERWEEK);
        if (top < -3)
            top += DAYSPERWEEK;
        top += len;
        if (yday >= top) {
            ++year;
            week = 1;
            return;
        }
        if (yday >= bot) {
            week = 1 + ((yday - bot) / DAYSPERWEEK);
            return;
        }
        --year;
        yday += is_leap(year) ? 366 : 365;
    }
}

void fmt(String &out, Str f, const Tm &tm);

// One conversion, after its flags. False when it is not one, and the
// character itself is what goes out.
void fmt_one(String &out, char c, const Tm &tm, Pad pad)
{
    switch (c) {
    case 'A':
        out.append(tm.wday < 0 || tm.wday > 6 ? Str("?") : WDAYF[tm.wday]);
        return;
    case 'a':
        out.append(tm.wday < 0 || tm.wday > 6 ? Str("?") : WDAY[tm.wday]);
        return;
    case 'B':
        out.append(tm.mon < 0 || tm.mon > 11 ? Str("?") : MONF[tm.mon]);
        return;
    case 'b':
    case 'h':
        out.append(tm.mon < 0 || tm.mon > 11 ? Str("?") : MON[tm.mon]);
        return;
    case 'C':
        yconv(out, tm.year - 1900, 1900, true, false, pad);
        return;
    case 'c':
        fmt(out, "%a %b %e %H:%M:%S %Y", tm);
        return;
    case 'D':
    case 'x':
        fmt(out, "%m/%d/%y", tm);
        return;
    case 'd':
        conv(out, tm.mday, 2, '0', pad);
        return;
    case 'e':
        conv(out, tm.mday, 2, ' ', pad);
        return;
    case 'F':
        fmt(out, "%Y-%m-%d", tm);
        return;
    case 'H':
        conv(out, tm.hour, 2, '0', pad);
        return;
    case 'I':
        conv(out, (tm.hour % 12) ? (tm.hour % 12) : 12, 2, '0', pad);
        return;
    case 'j':
        conv(out, tm.yday + 1, 3, '0', pad);
        return;
    case 'k':
        conv(out, tm.hour, 2, ' ', pad);
        return;
    case 'l':
        conv(out, (tm.hour % 12) ? (tm.hour % 12) : 12, 2, ' ', pad);
        return;
    case 'M':
        conv(out, tm.min, 2, '0', pad);
        return;
    case 'm':
        conv(out, tm.mon + 1, 2, '0', pad);
        return;
    case 'n':
        out.push('\n');
        return;
    case 'p':
        out.append(tm.hour >= 12 ? Str("PM") : Str("AM"));
        return;
    case 'R':
        fmt(out, "%H:%M", tm);
        return;
    case 'r':
        fmt(out, "%I:%M:%S %p", tm);
        return;
    case 'S':
        conv(out, tm.sec, 2, '0', pad);
        return;
    case 's': {
        i64 secs = 0;
        if (tm_to_secs(tm, i64(tz_offset_min) * 60, secs))
            conv(out, secs, 1, '0', Pad::None);
        return;
    }
    case 'T':
    case 'X':
        fmt(out, "%H:%M:%S", tm);
        return;
    case 't':
        out.push('\t');
        return;
    case 'U':
        conv(out, (tm.yday + 7 - tm.wday) / 7, 2, '0', pad);
        return;
    case 'u':
        conv(out, tm.wday == 0 ? 7 : tm.wday, 1, '0', pad);
        return;
    case 'V':
    case 'G':
    case 'g': {
        i64 year = 0;
        i32 week = 0;
        iso_week(tm, year, week);
        if (c == 'V')
            conv(out, week, 2, '0', pad);
        else if (c == 'g')
            yconv(out, year - 1900, 1900, false, true, pad);
        else
            yconv(out, year - 1900, 1900, true, true, pad);
        return;
    }
    case 'v':
        fmt(out, "%e-%b-%Y", tm);
        return;
    case 'W':
        conv(out, (tm.yday + 7 - (tm.wday ? tm.wday - 1 : 6)) / 7, 2, '0', pad);
        return;
    case 'w':
        conv(out, tm.wday, 1, '0', pad);
        return;
    case 'y':
        yconv(out, tm.year - 1900, 1900, false, true, pad);
        return;
    case 'Y':
        yconv(out, tm.year - 1900, 1900, true, true, pad);
        return;
    case 'Z':
        if (tm.has_zone) {
            out.append(tm.zone.str());
        } else if (tm.isdst >= 0) {
            String z;
            zone_name(tz_offset_min, z);
            out.append(z.str());
        }
        return;
    case 'z': {
        i64 diff = tm.gmtoff;
        bool neg = diff < 0;
        if (neg)
            diff = -diff;
        out.push(neg ? '-' : '+');
        diff /= 60;
        conv(out, (diff / 60) * 100 + diff % 60, 4, '0', pad);
        return;
    }
    case '+':
        fmt(out, "%a %b %e %H:%M:%S %Z %Y", tm);
        return;
    case '%':
    default:
        out.push(c);
        return;
    }
}

void fmt(String &out, Str f, const Tm &tm)
{
    for (usize i = 0; i < f.size(); i++) {
        if (f[i] != '%') {
            out.push(f[i]);
            continue;
        }
        Pad pad = Pad::Default;
        for (;;) {
            if (++i >= f.size()) {
                // A lone % at the end, or a modifier, goes out as itself.
                out.push(f[i - 1]);
                return;
            }
            char c = f[i];
            if (c == 'E' || c == 'O')
                continue;
            if (c == '-' || c == '_' || c == '0') {
                pad = c == '-' ? Pad::None : c == '_' ? Pad::Space : Pad::Zero;
                continue;
            }
            fmt_one(out, c, tm, pad);
            break;
        }
    }
}

R t_strftime(const CallArgs &a, Value &out)
{
    if (!args_only(a, "strftime", 1, 2))
        return R::Err;
    if (!is_str(a.args[0])) {
        Buf<96> b;
        b.put("strftime() argument 1 must be str, not ").put(type_name(a.args[0]));
        return err_set("TypeError", b.str());
    }
    Tm tm;
    if (a.nargs < 2) {
        if (!break_down(floordiv(i64(wall_ms()), 1000), true, tm))
            return R::Err;
    } else if (!gettmarg(a.args[1], "strftime", tm) || !checktm(tm)) {
        return R::Err;
    }
    if (tm.isdst < -1)
        tm.isdst = -1;
    else if (tm.isdst > 1)
        tm.isdst = 1;
    String text;
    // Only ASCII is formatted, as CPython splits the format; the rest is
    // copied as it is.
    Str f   = str_of(a.args[0])->str();
    usize i = 0;
    while (i < f.size()) {
        usize j = i;
        while (j < f.size() && u8(f[j]) < 0x80 && f[j] != 0)
            j++;
        fmt(text, f.substr(i, j - i), tm);
        i = j;
        while (j < f.size() && f[j] != '%')
            j++;
        text.append(f.substr(i, j - i));
        i = j;
    }
    out = str_new(text.str());
    return out.is_nil() ? R::Err : R::Ok;
}

// strptime(string, format): _strptime._strptime_time, which is Python.
// s[0] the arguments.
R strptime_step(ContObj *k, Value in)
{
    switch (k->i++) {
    case 0: {
        StrObj *imp = str_intern("__import__");
        Value fn;
        if (!imp || dict_get(builtins_dict(), obj_value(imp), fn) != R::Ok)
            return err_pending() ? R::Err : oom();
        Value mod = str_new("_strptime");
        if (mod.is_nil())
            return R::Err;
        return cont_call(k, fn, mod);
    }
    case 1:
        return cont_attr(k, in, "_strptime_time");
    case 2:
        return cont_call_v(k, in, k->s[0]);
    default:
        return cont_done(k, in);
    }
}

R t_strptime(const CallArgs &a, Value &out)
{
    if (a.nkw)
        return err_set("TypeError", "strptime() takes no keyword arguments");
    TupleObj *t = tuple_new(a.nargs);
    if (!t)
        return oom();
    for (u32 i = 0; i < a.nargs; i++)
        t->items()[i] = a.args[i];
    Root rt{ obj_value(t) };
    Value kv = cont_new(strptime_step);
    if (kv.is_nil())
        return R::Err;
    cont_of(kv)->s[0] = rt.v;
    out               = kv;
    return R::Ok;
}

R t_tzset(const CallArgs &a, Value &out)
{
    if (!args_only(a, "tzset", 0, 0))
        return R::Err;
    out = value_none();
    return R::Ok;
}

R t_struct_time(const CallArgs &a, Value &out)
{
    return info_construct(&struct_time_type, TIME_FIELDS, TM_ITEMS, TM_SHOWN, a, out);
}

constexpr ModDef TIME_DEFS[] = {
    { "time", t_time },
    { "time_ns", t_time_ns },
    { "clock_gettime", t_clock_gettime },
    { "clock_gettime_ns", t_clock_gettime_ns },
    { "clock_settime", t_clock_settime },
    { "clock_settime_ns", t_clock_settime_ns },
    { "clock_getres", t_clock_getres },
    { "sleep", t_sleep },
    { "gmtime", t_gmtime },
    { "localtime", t_localtime },
    { "asctime", t_asctime },
    { "ctime", t_ctime },
    { "mktime", t_mktime },
    { "strftime", t_strftime },
    { "strptime", t_strptime },
    { "tzset", t_tzset },
    { "monotonic", t_monotonic },
    { "monotonic_ns", t_monotonic_ns },
    { "process_time", t_monotonic },
    { "process_time_ns", t_monotonic_ns },
    { "thread_time", t_monotonic },
    { "thread_time_ns", t_monotonic_ns },
    { "perf_counter", t_monotonic },
    { "perf_counter_ns", t_monotonic_ns },
    { "get_clock_info", t_get_clock_info },
};

struct Clock {
    Str name;
    i64 id;
};

constexpr Clock CLOCKS[] = {
    { "CLOCK_REALTIME", CLOCK_REALTIME },
    { "CLOCK_MONOTONIC", CLOCK_MONOTONIC },
    { "CLOCK_PROCESS_CPUTIME_ID", CLOCK_PROCESS_CPUTIME_ID },
    { "CLOCK_THREAD_CPUTIME_ID", CLOCK_THREAD_CPUTIME_ID },
    { "CLOCK_MONOTONIC_RAW", CLOCK_MONOTONIC_RAW },
    { "CLOCK_BOOTTIME", CLOCK_BOOTTIME },
    { "CLOCK_TAI", CLOCK_TAI },
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
    if (!mod_defs(d, TIME_DEFS) || !mod_type(d, &struct_time_type, t_struct_time))
        return false;
    for (const Clock &c : CLOCKS)
        if (!mod_int(d, c.name, c.id))
            return false;
    // West of UTC is positive here, as C's timezone is and the browser's is
    // not. There is no daylight-saving table: the browser has already applied
    // one, and its offset is what tz_min carries.
    String zone;
    zone_name(tz_offset_min, zone);
    TupleObj *names = tuple_new(2);
    if (!names)
        return oom() == R::Ok;
    Root rn{ obj_value(names) };
    Root z0{ str_new(zone.str()) };
    Root z1{ str_new(zone.str()) };
    if (z0.v.is_nil() || z1.v.is_nil())
        return false;
    static_cast<TupleObj *>(rn.v.obj())->items()[0] = z0.v;
    static_cast<TupleObj *>(rn.v.obj())->items()[1] = z1.v;
    return mod_int(d, "timezone", -i64(tz_offset_min) * 60) &&
           mod_int(d, "altzone", -i64(tz_offset_min) * 60) && mod_int(d, "daylight", 0) &&
           mod_put(d, "tzname", rn.v) && mod_int(d, "_STRUCT_TM_ITEMS", TM_ITEMS);
}
