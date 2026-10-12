/*
 * gmtime — seconds since the epoch as UTC's calendar (C11 §7.27.3.3), over the
 * proleptic Gregorian calendar.  __civil is localtime's too.
 */
#include <time.h>

/* t, shifted by `offset` seconds, into *tm. */
struct tm *__civil(long long t, long offset, struct tm *tm)
{
    t += offset;
    long long days = t / 86400, secs = t % 86400;
    if (secs < 0) {
        secs += 86400;
        days--;
    }
    tm->tm_hour = (int)(secs / 3600);
    tm->tm_min  = (int)(secs / 60 % 60);
    tm->tm_sec  = (int)(secs % 60);
    tm->tm_wday = (int)((days % 7 + 11) % 7); /* 1970-01-01 was a Thursday */

    /* Days to a civil date, in eras of 400 years from March 1st, year 0. */
    long long z   = days + 719468;
    long long era = (z >= 0 ? z : z - 146096) / 146097;
    long long doe = z - era * 146097;
    long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long long mp  = (5 * doy + 2) / 153;
    int mday      = (int)(doy - (153 * mp + 2) / 5 + 1);
    int mon       = (int)(mp < 10 ? mp + 2 : mp - 10);
    long long y   = yoe + era * 400 + (mon < 2);

    int leap                     = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    static const short first[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    tm->tm_year                  = (int)(y - 1900);
    tm->tm_mon                   = mon;
    tm->tm_mday                  = mday;
    tm->tm_yday                  = first[mon] + mday - 1 + (leap && mon > 1);
    tm->tm_isdst                 = 0;
    return tm;
}

struct tm *gmtime(const time_t *t)
{
    static struct tm tm;
    return __civil(*t, 0, &tm);
}
