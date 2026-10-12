// <time.h> for Braam: libc/common's calendar, with time() a coroutine over
// Sys::Clock.  localtime is in the zone the last time() reported; before one, UTC.
// There is no process clock, so clock() is not declared.
#ifndef _TIME_H
#define _TIME_H

#include <braam.h>
#include <stddef.h>

typedef long time_t;

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

coro(braam_call *) time_t time(time_t *t);

struct tm *gmtime(const time_t *t);
struct tm *localtime(const time_t *t);
size_t strftime(char *s, size_t max, const char *format, const struct tm *tm);

#endif // _TIME_H
