/* The host's POSIX regex, behind a C interface, so regex_test.cpp can compare
   against it without the two <regex.h> declarations meeting in one file.
   Build and run with test/regex.sh. */

#include <regex.h>
#include <stdlib.h>
#include <string.h>

/* 0 on a match, filling so/eo and 2*ngroup offsets; 1 for no match; -1 when the
   pattern did not compile. */
int oracle(const char *pat, const char *str, int notbol, long *out, int nout)
{
    regex_t re;
    regmatch_t m[10];
    int i;

    if (regcomp(&re, pat, REG_EXTENDED | REG_NEWLINE) != 0)
        return -1;
    if (regexec(&re, str, 10, m, notbol ? REG_NOTBOL : 0) != 0) {
        regfree(&re);
        return 1;
    }
    for (i = 0; i < nout / 2 && i < 10; i++) {
        out[2 * i]     = (long)m[i].rm_so;
        out[2 * i + 1] = (long)m[i].rm_eo;
    }
    regfree(&re);
    return 0;
}
