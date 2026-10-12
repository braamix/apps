/*
 * <locale.h> — localization (C11 §7.11).
 *
 * TODO: implement setlocale/localeconv.  Only the "C" locale is meaningful here;
 * the surface is provided for source portability.
 */
#ifndef _LOCALE_H
#define _LOCALE_H

#include <stddef.h>

#define LC_ALL      0
#define LC_COLLATE  1
#define LC_CTYPE    2
#define LC_MONETARY 3
#define LC_NUMERIC  4
#define LC_TIME     5

struct lconv {
    char *decimal_point;
    char *thousands_sep;
    char *grouping;
    char *mon_decimal_point;
    char *mon_thousands_sep;
    char *mon_grouping;
    char *positive_sign;
    char *negative_sign;
    char *currency_symbol;
    char frac_digits;
    char p_cs_precedes;
    char n_cs_precedes;
};

char *setlocale(int category, const char *locale);
struct lconv *localeconv(void);

#endif /* _LOCALE_H */
