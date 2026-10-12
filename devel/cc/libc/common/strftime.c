// strftime — the "C" locale's conversions (C11 §7.27.3.5), with %z and %Z
// as UTC's: the calendar keeps no zone.
#include <string.h>
#include <time.h>

static const char *const days[7]    = { "Sunday",   "Monday", "Tuesday", "Wednesday",
                                        "Thursday", "Friday", "Saturday" };
static const char *const months[12] = { "January",   "February", "March",    "April",
                                        "May",       "June",     "July",     "August",
                                        "September", "October",  "November", "December" };

typedef struct {
    char *buf;
    size_t cap, n; // n counts past cap, so an overflow shows
} Out;

static void put(Out *o, char c)
{
    if (o->n < o->cap)
        o->buf[o->n] = c;
    o->n++;
}

static void puts_n(Out *o, const char *s, int n)
{
    for (int k = 0; s[k] && (n < 0 || k < n); k++)
        put(o, s[k]);
}

// v in at least `width` digits, padded with `pad`.
static void num(Out *o, long long v, int width, char pad)
{
    char d[24];
    int k = 0, neg = v < 0;
    unsigned long long u = neg ? 0 - (unsigned long long)v : (unsigned long long)v;
    do {
        d[k++] = (char)('0' + u % 10);
        u /= 10;
    } while (u);
    if (neg)
        put(o, '-');
    for (int w = k + neg; w < width; w++)
        put(o, pad);
    while (k)
        put(o, d[--k]);
}

static int iso_weeks(long long y)
{
    long long p = (y + y / 4 - y / 100 + y / 400) % 7,
              q = (y - 1 + (y - 1) / 4 - (y - 1) / 100 + (y - 1) / 400) % 7;
    return 52 + (p == 4 || q == 3);
}

// The ISO 8601 week, and the year it belongs to.
static int iso_week(const struct tm *tm, long long *year)
{
    int mon0 = (tm->tm_wday + 6) % 7; // Monday first
    int week = (tm->tm_yday - mon0 + 10) / 7;
    *year    = tm->tm_year + 1900LL;
    if (week < 1) {
        (*year)--;
        week = iso_weeks(*year);
    } else if (week > iso_weeks(*year)) {
        (*year)++;
        week = 1;
    }
    return week;
}

static void format(Out *o, const char *fmt, const struct tm *tm)
{
    int wday = tm->tm_wday % 7, mon = tm->tm_mon % 12;
    long long year = tm->tm_year + 1900LL, iy;
    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            put(o, *fmt);
            continue;
        }
        char c = *++fmt;
        if (c == 'E' || c == 'O') // the alternative forms are the plain ones here
            c = *++fmt;
        switch (c) {
        case 'a':
            puts_n(o, days[wday], 3);
            break;
        case 'A':
            puts_n(o, days[wday], -1);
            break;
        case 'b':
        case 'h':
            puts_n(o, months[mon], 3);
            break;
        case 'B':
            puts_n(o, months[mon], -1);
            break;
        case 'c':
            format(o, "%a %b %e %H:%M:%S %Y", tm);
            break;
        case 'C':
            num(o, year / 100, 2, '0');
            break;
        case 'd':
            num(o, tm->tm_mday, 2, '0');
            break;
        case 'D':
            format(o, "%m/%d/%y", tm);
            break;
        case 'e':
            num(o, tm->tm_mday, 2, ' ');
            break;
        case 'F':
            format(o, "%Y-%m-%d", tm);
            break;
        case 'g':
            iso_week(tm, &iy);
            num(o, (iy % 100 + 100) % 100, 2, '0');
            break;
        case 'G':
            iso_week(tm, &iy);
            num(o, iy, 1, '0');
            break;
        case 'H':
            num(o, tm->tm_hour, 2, '0');
            break;
        case 'I':
            num(o, tm->tm_hour % 12 ? tm->tm_hour % 12 : 12, 2, '0');
            break;
        case 'j':
            num(o, tm->tm_yday + 1, 3, '0');
            break;
        case 'm':
            num(o, tm->tm_mon + 1, 2, '0');
            break;
        case 'M':
            num(o, tm->tm_min, 2, '0');
            break;
        case 'n':
            put(o, '\n');
            break;
        case 'p':
            puts_n(o, tm->tm_hour < 12 ? "AM" : "PM", -1);
            break;
        case 'r':
            format(o, "%I:%M:%S %p", tm);
            break;
        case 'R':
            format(o, "%H:%M", tm);
            break;
        case 'S':
            num(o, tm->tm_sec, 2, '0');
            break;
        case 't':
            put(o, '\t');
            break;
        case 'T':
            format(o, "%H:%M:%S", tm);
            break;
        case 'u':
            num(o, wday ? wday : 7, 1, '0');
            break;
        case 'U':
            num(o, (tm->tm_yday + 7 - wday) / 7, 2, '0');
            break;
        case 'V':
            num(o, iso_week(tm, &iy), 2, '0');
            break;
        case 'w':
            num(o, wday, 1, '0');
            break;
        case 'W':
            num(o, (tm->tm_yday + 7 - (wday + 6) % 7) / 7, 2, '0');
            break;
        case 'x':
            format(o, "%m/%d/%y", tm);
            break;
        case 'X':
            format(o, "%H:%M:%S", tm);
            break;
        case 'y':
            num(o, (year % 100 + 100) % 100, 2, '0');
            break;
        case 'Y':
            num(o, year, 1, '0');
            break;
        case 'z':
            puts_n(o, "+0000", -1);
            break;
        case 'Z':
            puts_n(o, "UTC", -1);
            break;
        case '%':
            put(o, '%');
            break;
        case 0:
            fmt--;
            break;
        default:
            put(o, '%');
            put(o, c);
            break;
        }
    }
}

size_t strftime(char *s, size_t max, const char *fmt, const struct tm *tm)
{
    if (max == 0)
        return 0;
    Out o = { s, max - 1, 0 };
    format(&o, fmt, tm);
    if (o.n >= max)
        return 0;
    s[o.n] = 0;
    return o.n;
}
