#include "braam.h"

#include <errno.h>

#include "proc/rt.h"
#include "zip.h"

// -------------------------------------------------------- what the kit leaves

// "%4d-%2d-%2d" or "%2d%2d%4d", which is what -t and -tt accept.
int zparse_date(const char *s, int *yyyy, int *mm, int *dd)
{
    auto num = [](const char *p, int n, int &out) {
        out = 0;
        for (int i = 0; i < n; i++) {
            if (p[i] < '0' || p[i] > '9')
                return false;
            out = out * 10 + (p[i] - '0');
        }
        return true;
    };

    if (strlen(s) >= 10 && s[4] == '-' && s[7] == '-') {
        if (num(s, 4, *yyyy) && num(s + 5, 2, *mm) && num(s + 8, 2, *dd))
            return 3;
    }
    if (strlen(s) >= 8) {
        if (num(s, 2, *mm) && num(s + 2, 2, *dd) && num(s + 4, 4, *yyyy))
            return 3;
    }
    return 0;
}

// -------------------------------------------------------------- the calendar

i32 ztz_min     = 0;
i64 zstart_time = 0;

int zargv(Args argv, char ***out)
{
    // Every main calls this first, and every main names mesg for itself; this
    // is only so a diagnostic before that has somewhere to go.
    if (mesg == nullptr)
        mesg = stdout;

    int n    = (int)argv.size();
    char **a = (char **)malloc((n + 2) * sizeof(char *));
    if (a == nullptr)
        return 0;
    for (int i = 0; i < n; i++) {
        if ((a[i] = (char *)malloc(argv[i].size() + 1)) == nullptr)
            return 0;
        memcpy(a[i], argv[i].data(), argv[i].size());
        a[i][argv[i].size()] = 0;
    }
    a[n] = nullptr;
    *out = a;
    return n;
}

Task<void> zclock_init()
{
    Result<Clock> c = Err(Error::NoMemory);
    if (Task<Result<Clock>> t = clock_now())
        c = co_await t;
    if (c.is_ok()) {
        ztz_min     = c.value().tz_min;
        zstart_time = (i64)(c.value().epoch_ms / 1000);
    }
}

time_t ztime(time_t *t)
{
    time_t now = zstart_time + (time_t)(proc_now() / 1000);
    if (t)
        *t = now;
    return now;
}

u32 zdostime(u64 mtime_ms)
{
    if (mtime_ms == 0)
        return DOSTIME_MINIMUM_;

    time_t secs = (time_t)(mtime_ms / 1000) + ztz_min * 60;
    struct tm t;
    gmtime_r(&secs, &t);
    if (t.tm_year + 1900 < 1980)
        return DOSTIME_MINIMUM_;

    return ((u32)(t.tm_year + 1900 - 1980) << 25) | ((u32)(t.tm_mon + 1) << 21) |
           ((u32)t.tm_mday << 16) | ((u32)t.tm_hour << 11) | ((u32)t.tm_min << 5) |
           ((u32)t.tm_sec >> 1);
}

i64 zdos2unix(u32 dostime)
{
    u32 mon = (dostime >> 21) & 0x0f;
    u32 day = (dostime >> 16) & 0x1f;
    if (mon < 1 || mon > 12 || day < 1)
        return 0;

    // timegm, where this had Howard Hinnant's days_from_civil: the kit inverts
    // the calendar now, and normalises the fields while it is there.
    struct tm t  = {};
    t.tm_year    = (int)((dostime >> 25) & 0x7f) + 1980 - 1900;
    t.tm_mon     = (int)mon - 1;
    t.tm_mday    = (int)day;
    t.tm_hour    = (int)((dostime >> 11) & 0x1f);
    t.tm_min     = (int)((dostime >> 5) & 0x3f);
    t.tm_sec     = (int)(dostime & 0x1f) * 2;
    return (i64)timegm(&t) - (i64)ztz_min * 60;
}

// ------------------------------------------- what unix/zipup.h did: raw reads

int zread_failed = 0;

Task<unsigned> zread(ftype f, char *buf, unsigned len)
{
    ssize_t got = co_await b_read((int)f, buf, len);

    // This is where a long run parks, so this is where a ^C reaches it.
    // Upstream installed a handler that called ziperr(ZE_ABORT). There is no
    // handler here: a delivered signal abandons the parked call with EINTR, or
    // is simply pending if the call had already answered — so it is sig_take()
    // that says one arrived, not the error. The entry loop unwinds on
    // zip_fatal.
    if (sig_take(SIG_INT))
        zip_fail(ZE_ABORT, "aborting");

    if (got < 0) {
        if (errno == EINTR)
            zip_fail(ZE_ABORT, "aborting");
        zread_failed = 1;
        co_return 0;
    }
    co_return (unsigned)got;
}
