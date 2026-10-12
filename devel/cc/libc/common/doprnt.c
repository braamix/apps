/*
 * Core formatting engine shared by printf / sprintf / snprintf, for a
 * byte-addressed target with IEEE-754 double, and long of 32 or 64 bits.
 *
 * Derived from the BESM-6 engine (libc/besm6/doprnt.c), itself from the FreeBSD
 * kernel printf.  Differences: conversion letters keep their case, the length
 * modifiers hh/h/l/ll/j/z/t select the argument type, values are 64-bit, %s and %p
 * read real pointers, and infinities and NaNs print as inf/nan.
 *
 * If `to_buf` is zero the formatted bytes go to putbyte(); otherwise they are
 * stored into buf (at most size-1, NUL-terminated).  The return value is the total
 * length that would be produced.
 */
#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>

enum {
    MAXNBUF  = 32,                  /* integer digits: 64-bit octal (22) + slack  */
    FBUFSIZE = DBL_MAX_10_EXP + 44, /* %f of DBL_MAX: its integer digits + '.' +
                                       precision + exponent (352 for binary64)    */
    MAX_DIG  = DBL_DECIMAL_DIG,     /* max meaningful significant digits          */
    DEF_PREC = 6, /* default precision for %f/%e and significant digits for %g     */
};

static int g_to_buf; /* 1 = store into g_buf, 0 = emit via putbyte */
static char *g_buf;  /* target buffer when g_to_buf                */
static int g_size;   /* capacity of g_buf                          */
static int g_len;    /* characters produced so far                 */

static void emit(int c)
{
    if (g_to_buf) {
        if (g_len < g_size - 1)
            g_buf[g_len] = (char)c;
    } else {
        putbyte(c);
    }
    ++g_len;
}

static void emit_pad(int c, int count)
{
    while (count > 0) {
        emit(c);
        --count;
    }
}

static void emit_str(const char *s)
{
    while (*s)
        emit(*s++);
}

/*
 * Convert `ul` to base `base` digits, written into nbuf in reverse order.
 * nbuf[0] is a 0 sentinel; digits occupy nbuf[1..].  At least `prec` digits are
 * produced (zero padded).  Returns a pointer to the most-significant digit and
 * stores the digit count in *lenp.
 */
static char *ksprintn(char *nbuf, unsigned long long ul, int base, int prec, int upper, int *lenp)
{
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char *p            = nbuf;

    *p = 0;
    do {
        *++p = digits[ul % (unsigned)base];
        ul   = ul / (unsigned)base;
    } while (--prec > 0 || ul);
    *lenp = (int)(p - nbuf);
    return p;
}

static int cvt(double number, int prec, int sharpflag, int *negp, int fmtch, char *b, int bsize,
               int *startidx);

int __doprnt(const char *fmt, va_list ap, char *buf, int size, int to_buf)
{
    char nbuf[MAXNBUF];
    char fbuf[FBUFSIZE];
    int i, c, base, ladjust, sharpflag, neg, dot, upper, lmod;
    int n, width, dwidth, sign, blank, extrazeros, padding, dlen;
    unsigned long long ul;
    char *s, *msd;

    g_to_buf = to_buf;
    g_buf    = buf;
    g_size   = size;
    g_len    = 0;

    i = 0;
    for (;;) {
        while ((c = fmt[i]) != '%') {
            if (!c)
                goto done;
            emit(c);
            ++i;
        }
        ++i;
        padding    = ' ';
        width      = 0;
        extrazeros = 0;
        ladjust    = 0;
        sharpflag  = 0;
        neg        = 0;
        sign       = 0;
        blank      = 0;
        dot        = 0;
        dwidth     = -1;
        lmod       = 0; /* -2 hh, -1 h, 0 none, 1 l/z/t, 2 L, 3 ll/j */

    reswitch:
        c = fmt[i];
        ++i;
        if (c == '.') {
            dot     = 1;
            padding = ' ';
            dwidth  = 0;
            goto reswitch;
        }
        if (c == '#') {
            sharpflag = 1;
            goto reswitch;
        }
        if (c == '+') {
            sign = -1;
            goto reswitch;
        }
        if (c == ' ') {
            blank = 1;
            goto reswitch;
        }
        if (c == '-') {
            ladjust = 1;
            goto reswitch;
        }
        if (c == 'h') {
            lmod = lmod == -1 ? -2 : -1;
            goto reswitch;
        }
        if (c == 'l') {
            lmod = lmod == 1 ? 3 : 1;
            goto reswitch;
        }
        if (c == 'j') {
            lmod = 3;
            goto reswitch;
        }
        if (c == 'z' || c == 't') {
            lmod = sizeof(size_t) == sizeof(long) ? 1 : 0;
            goto reswitch;
        }
        if (c == 'L') {
            lmod = 2; /* printed with double precision */
            goto reswitch;
        }
        if (c == '*') {
            if (!dot) {
                width = va_arg(ap, int);
                if (width < 0) {
                    ladjust = 1;
                    width   = -width;
                }
            } else {
                dwidth = va_arg(ap, int);
            }
            goto reswitch;
        }
        if (c == '0' && !dot) {
            padding = '0';
            goto reswitch;
        }
        if (c >= '0' && c <= '9') {
            n = 0;
            for (;;) {
                n = n * 10 + c - '0';
                c = fmt[i];
                if (c < '0' || c > '9')
                    break;
                ++i;
            }
            if (dot)
                dwidth = n;
            else
                width = n;
            goto reswitch;
        }

        if (c == '%') {
            emit('%');
            continue;
        }

        if (c == 'c') {
            if (!ladjust)
                emit_pad(' ', width - 1);
            emit((unsigned char)va_arg(ap, int));
            if (ladjust)
                emit_pad(' ', width - 1);
            continue;
        }

        if (c == 's') {
            s = va_arg(ap, char *);
            if (!s)
                s = "(null)";
            n = 0;
            while ((!dot || n < dwidth) && s[n])
                ++n;
            width -= n;
            if (!ladjust)
                emit_pad(' ', width);
            for (c = 0; c < n; ++c)
                emit(s[c]);
            if (ladjust)
                emit_pad(' ', width);
            continue;
        }

        /* ---- floating point ---- */
        if (c == 'f' || c == 'F' || c == 'e' || c == 'E' || c == 'g' || c == 'G') {
            double d;
            int sidx, slen;
            d     = lmod == 2 ? (double)va_arg(ap, long double) : va_arg(ap, double);
            upper = c == 'F' || c == 'E' || c == 'G';
            if (d < 0 || (d == 0 && 1 / d < 0)) {
                neg = 1;
                d   = -d;
            }
            if (d != d || d - d != 0) {
                /* NaN or infinity: no digits, no zero padding */
                s    = d != d ? (upper ? "NAN" : "nan") : (upper ? "INF" : "inf");
                dlen = 3 + (neg || sign || blank ? 1 : 0);
                if (!ladjust)
                    emit_pad(' ', width - dlen);
                if (neg)
                    emit('-');
                else if (sign)
                    emit('+');
                else if (blank)
                    emit(' ');
                emit_str(s);
                if (ladjust)
                    emit_pad(' ', width - dlen);
                continue;
            }
            if (dwidth > MAX_DIG) {
                if ((c != 'g' && c != 'G') || sharpflag)
                    extrazeros = dwidth - MAX_DIG;
                dwidth = MAX_DIG;
            } else if (dwidth == -1) {
                dwidth = DEF_PREC;
            }
            slen = cvt(d, dwidth, sharpflag, &neg, c, fbuf, FBUFSIZE, &sidx);
            if (!neg && sign)
                neg = '+';
            else if (!neg && blank)
                neg = ' ';
            else if (neg)
                neg = '-';
            dlen = slen + (neg ? 1 : 0);
            if (!ladjust && padding == ' ' && (width - dlen) > 0)
                emit_pad(' ', width - dlen);
            if (neg)
                emit(neg);
            if (!ladjust && padding == '0' && (width - dlen) > 0)
                emit_pad('0', width - dlen);
            for (n = 0; n < slen; ++n) {
                if (extrazeros && (fbuf[sidx + n] == 'e' || fbuf[sidx + n] == 'E')) {
                    emit_pad('0', extrazeros);
                    extrazeros = 0;
                }
                emit(fbuf[sidx + n]);
            }
            if (extrazeros)
                emit_pad('0', extrazeros);
            if (ladjust && (width - dlen) > 0)
                emit_pad(' ', width - dlen);
            continue;
        }

        /* ---- integer conversions ---- */
        upper = c == 'X';
        if (c == 'd' || c == 'i') {
            long long l;
            if (lmod == 3)
                l = va_arg(ap, long long);
            else if (lmod > 0)
                l = va_arg(ap, long);
            else if (lmod == -1)
                l = (short)va_arg(ap, int);
            else if (lmod == -2)
                l = (signed char)va_arg(ap, int);
            else
                l = va_arg(ap, int);
            if (!sign)
                sign = 1;
            if (l < 0) {
                neg = '-';
                ul  = 0 - (unsigned long long)l;
            } else {
                ul = (unsigned long long)l;
            }
            base = 10;
            goto number;
        }
        if (c == 'u' || c == 'o' || c == 'x' || c == 'X') {
            if (lmod == 3)
                ul = va_arg(ap, unsigned long long);
            else if (lmod > 0)
                ul = va_arg(ap, unsigned long);
            else if (lmod == -1)
                ul = (unsigned short)va_arg(ap, unsigned);
            else if (lmod == -2)
                ul = (unsigned char)va_arg(ap, unsigned);
            else
                ul = va_arg(ap, unsigned);
            base = c == 'u' ? 10 : c == 'o' ? 8 : 16;
            goto nosign;
        }
        if (c == 'p') {
            ul        = (unsigned long)va_arg(ap, void *);
            base      = 16;
            sharpflag = 1;
            goto nosign;
        }

        /* unknown conversion: echo it verbatim */
        emit('%');
        emit(c);
        continue;

    nosign:
        sign  = 0;
        blank = 0;
    number:
        if (!neg) {
            if (sign < 0)
                neg = '+';
            else if (sign && blank)
                neg = ' ';
        }
        if (dot && padding == '0')
            padding = ' ';
        if (dwidth >= MAXNBUF) {
            extrazeros = dwidth - MAXNBUF + 1;
            dwidth     = MAXNBUF - 1;
        }
        if (dot && dwidth == 0 && ul == 0) {
            dlen = 0; /* "%.0d" of 0 prints no digits */
            msd  = nbuf;
        } else {
            msd = ksprintn(nbuf, ul, base, dwidth, upper, &dlen);
        }
        if (sharpflag && ul != 0) {
            if (base == 8 && dwidth <= dlen)
                dlen += 1;
            else if (base == 16)
                dlen += 2;
        }
        if (neg)
            ++dlen;

        if (!ladjust && padding == ' ' && (width - dlen) > 0)
            emit_pad(' ', width - dlen);
        if (neg)
            emit(neg);
        if (sharpflag && ul != 0) {
            if (base == 8 && dwidth <= dlen) {
                emit('0');
            } else if (base == 16) {
                emit('0');
                emit(upper ? 'X' : 'x');
            }
        }
        if (extrazeros)
            emit_pad('0', extrazeros);
        if (!ladjust && padding == '0' && (width - dlen) > 0)
            emit_pad('0', width - dlen);
        while (msd > nbuf)
            emit(*msd--);
        if (ladjust && (width - dlen) > 0)
            emit_pad(' ', width - dlen);
        continue;
    }

done:
    if (g_to_buf && g_size > 0) {
        n = g_len;
        if (n > g_size - 1)
            n = g_size - 1;
        g_buf[n] = 0;
    }
    return g_len;
}

/*
 * Round the decimal digits start..end by the rest of the value: `fract` when
 * nonzero, else the next digit `ch` (`tie` telling that nothing nonzero follows
 * it).  An exact half rounds to even.  Carries propagate; `expo` (when non-null)
 * carries the e-format exponent so a carry out of the leading digit bumps it;
 * otherwise an f-format carry extends left into the reserved slot and moves
 * *startp back one.
 */
static void cvtround(double fract, int *expo, char **startp, char *end, int ch, int tie, int *negp)
{
    double tmp;
    char *start, *p, *last;
    int up;

    start = *startp;
    p     = end;

    if (fract) {
        tie = modf(fract * 10, &tmp) == 0;
        up  = (int)tmp;
    } else {
        up = ch - '0';
    }
    last = *end == '.' ? end - 1 : end;
    if (up > 5 || (up == 5 && (!tie || (*last - '0') % 2))) {
        for (;; --p) {
            if (*p == '.')
                --p;
            ++*p;
            if (*p <= '9')
                break;
            *p = '0';
            if (p == start) {
                if (expo) { /* e: increment exponent */
                    *p = '1';
                    ++*expo;
                } else { /* f: prepend a digit into the reserved slot */
                    --p;
                    *p = '1';
                    --start;
                }
                break;
            }
        }
    } else if (*negp) {
        /* "%.3f" of -0.0004 must not print a negative zero */
        for (;; --p) {
            if (*p == '.')
                --p;
            if (*p != '0')
                break;
            if (p == start)
                *negp = 0;
        }
    }
    *startp = start;
}

/* Append the exponent suffix ("e+NN") at write cursor p; return the new cursor. */
static char *exponent(char *p, int expin, int fmtch)
{
    char eb[8];
    int expo, k;

    expo = expin;

    *p++ = (char)fmtch;
    if (expo < 0) {
        expo = -expo;
        *p++ = '-';
    } else {
        *p++ = '+';
    }
    k = 8;
    if (expo > 9) {
        do {
            --k;
            eb[k] = (char)(expo % 10 + '0');
            expo  = expo / 10;
        } while (expo > 9);
        --k;
        eb[k] = (char)(expo + '0');
        for (; k < 8; ++k)
            *p++ = eb[k];
    } else {
        *p++ = '0';
        *p++ = (char)(expo + '0');
    }
    return p;
}

/*
 * Format the magnitude `number` into b[] for %f/%e/%g.  b[0] is reserved for a
 * rounding carry; formatting runs from b+1.  *startidx receives the index of the
 * first character (0 if a carry extended left); the character count is returned.
 */
static int cvt(double number, int precin, int sharpflag, int *negp, int fmtch, char *b, int bsize,
               int *startidx)
{
    double fract, integer, tmp;
    char *p, *t, *start, *endp;
    int expcnt, gformat, dotrim, prec, ftc, lower;

    prec    = precin;
    lower   = fmtch >= 'a';
    ftc     = lower ? fmtch - ('a' - 'A') : fmtch;
    expcnt  = 0;
    gformat = 0;
    dotrim  = 0;
    fract   = modf(number, &integer);

    endp  = b + bsize - 1;
    start = b + 1; /* reserved rounding slot at b[0] */
    t     = b + 1;

    /* integer part, least-significant first, into the top of the buffer */
    p = endp - 1;
    while (integer) {
        tmp = modf(integer / 10, &integer);
        *p  = (char)((int)((tmp + 0.01) * 10) + '0');
        --p;
        ++expcnt;
    }

    if (ftc == 'F') {
        if (expcnt) {
            ++p;
            while (p < endp)
                *t++ = *p++;
        } else {
            *t++ = '0';
        }
        if (prec || sharpflag)
            *t++ = '.';
        if (fract) {
            if (prec) {
                do {
                    fract = modf(fract * 10, &tmp);
                    *t++  = (char)((int)tmp + '0');
                } while (--prec && fract);
            }
            if (fract)
                cvtround(fract, 0, &start, t - 1, '0', 0, negp);
        }
        for (; prec > 0; --prec)
            *t++ = '0';
        *startidx = (int)(start - b);
        return (int)(t - start);
    }

    if (ftc == 'E') {
    eformat:
        if (expcnt) {
            ++p;
            *t++ = *p;
            if (prec || sharpflag)
                *t++ = '.';
            for (;;) {
                if (!prec)
                    break;
                ++p;
                if (p >= endp)
                    break;
                *t++ = *p;
                --prec;
            }
            if (!prec) {
                ++p;
                if (p < endp) {
                    char *q = p + 1;
                    while (q < endp && *q == '0')
                        ++q;
                    cvtround(0, &expcnt, &start, t - 1, *p, q == endp && !fract, negp);
                    fract = 0;
                }
            }
            --expcnt;
        } else if (fract) {
            for (expcnt = -1;; --expcnt) {
                fract = modf(fract * 10, &tmp);
                if (tmp)
                    break;
            }
            *t++ = (char)((int)tmp + '0');
            if (prec || sharpflag)
                *t++ = '.';
        } else {
            *t++ = '0';
            if (prec || sharpflag)
                *t++ = '.';
        }
        if (fract) {
            if (prec) {
                do {
                    fract = modf(fract * 10, &tmp);
                    *t++  = (char)((int)tmp + '0');
                } while (--prec && fract);
            }
            if (fract)
                cvtround(fract, &expcnt, &start, t - 1, '0', 0, negp);
        }
        for (; prec > 0; --prec)
            *t++ = '0';
        if (gformat && !sharpflag) {
            while (t > start) {
                --t;
                if (*t != '0')
                    break;
            }
            if (*t == '.')
                --t;
            ++t;
        }
        t         = exponent(t, expcnt, lower ? ftc + ('a' - 'A') : ftc);
        *startidx = (int)(start - b);
        return (int)(t - start);
    }

    /* ftc == 'G' */
    if (!prec)
        ++prec;
    if (expcnt > prec || (!expcnt && fract && fract < 0.0001)) {
        --prec;
        ftc     = 'E';
        gformat = 1;
        goto eformat;
    }
    if (expcnt) {
        for (;;) {
            ++p;
            if (p >= endp)
                break;
            *t++ = *p;
            --prec;
        }
    } else {
        *t++ = '0';
    }
    if (prec || sharpflag) {
        dotrim = 1;
        *t++   = '.';
    }
    while (prec && fract) {
        fract = modf(fract * 10, &tmp);
        *t++  = (char)((int)tmp + '0');
        --prec;
    }
    if (fract)
        cvtround(fract, 0, &start, t - 1, '0', 0, negp);
    if (sharpflag) {
        for (; prec > 0; --prec)
            *t++ = '0';
    } else if (dotrim) {
        while (t > start) {
            --t;
            if (*t != '0')
                break;
        }
        if (*t != '.')
            ++t;
    }
    *startidx = (int)(start - b);
    return (int)(t - start);
}
