// The ERE engine against the host's, natively. The engine is the one part of
// this port that needs no kernel, and upstream's own ere*/replace* tests
// exercise only ^ $ .$ ^$ and four literals -- far too little.
//
// Build and run with test/regex.sh.

#include "../regex.h"

#include <stdio.h>
#include <string.h>

extern "C" int oracle(const char *pat, const char *str, int notbol, long *out, int nout);

static int failures;
static int checked;

static void check(const char *pat, const char *str, int notbol)
{
    long want[20];
    regmatch_t got[10];
    regex_t re;
    int i;

    for (i = 0; i < 20; i++)
        want[i] = -1;

    int owant = oracle(pat, str, notbol, want, 20);
    int rc    = regcomp(&re, pat, REG_EXTENDED | REG_NEWLINE);

    if (owant < 0) {
        if (rc == 0) {
            printf("FAIL /%s/: host refused it, we compiled it\n", pat);
            regfree(&re);
            failures++;
        }
        checked++;
        return;
    }
    if (rc != 0) {
        printf("FAIL /%s/: we refused it, host compiled it\n", pat);
        failures++;
        checked++;
        return;
    }

    int ogot = regexec(&re, str, 10, got, notbol ? REG_NOTBOL : 0);
    regfree(&re);

    if ((owant == 0) != (ogot == 0)) {
        printf("FAIL /%s/ on \"%s\"%s: host %s, we %s\n", pat, str, notbol ? " NOTBOL" : "",
               owant == 0 ? "matched" : "did not", ogot == 0 ? "matched" : "did not");
        failures++;
        checked++;
        return;
    }
    if (owant == 0) {
        for (i = 0; i < 10; i++) {
            if (want[2 * i] != (long)got[i].rm_so || want[2 * i + 1] != (long)got[i].rm_eo) {
                printf("FAIL /%s/ on \"%s\"%s: group %d host [%ld,%ld) we [%ld,%ld)\n", pat, str,
                       notbol ? " NOTBOL" : "", i, want[2 * i], want[2 * i + 1],
                       (long)got[i].rm_so, (long)got[i].rm_eo);
                failures++;
                break;
            }
        }
    }
    checked++;
}

// What the oracle cannot judge: in the C locale the host's `.` takes one byte,
// and this engine takes a whole UTF-8 sequence on purpose -- a match must never
// leave eh's `here` mid-character. Plus the paths that have no answer to
// compare, only a requirement not to crash.
static void want(const char *pat, const char *str, long so, long eo)
{
    regex_t re;
    regmatch_t m[10];

    checked++;
    if (regcomp(&re, pat, REG_EXTENDED | REG_NEWLINE) != 0) {
        printf("FAIL /%s/: did not compile\n", pat);
        failures++;
        return;
    }
    int rc = regexec(&re, str, 10, m, 0);
    regfree(&re);

    if (so < 0) {
        if (rc == 0) {
            printf("FAIL /%s/: matched, wanted no match\n", pat);
            failures++;
        }
        return;
    }
    if (rc != 0) {
        printf("FAIL /%s/: no match, wanted [%ld,%ld)\n", pat, so, eo);
        failures++;
        return;
    }
    if ((long)m[0].rm_so != so || (long)m[0].rm_eo != eo) {
        printf("FAIL /%s/: [%ld,%ld), wanted [%ld,%ld)\n", pat, (long)m[0].rm_so,
               (long)m[0].rm_eo, so, eo);
        failures++;
    }
}

static void self_tests(void)
{
    /* "é" is two bytes, "🔥" is four. */
    want(".", "\xc3\xa9", 0, 2);
    want("..", "\xc3\xa9\xc3\xa9", 0, 4);
    want(".", "\xf0\x9f\x94\xa5", 0, 4);
    want("a.b", "a\xc3\xa9"
                "b",
         0, 4);
    want(".$", "x\xc3\xa9", 1, 3);
    want("[^x]", "\xc3\xa9", 0, 2);
    want("[\xc3\xa9]", "\xc3\xa9", 0, 2);          /* a literal multibyte in a bracket */
    want("\xc3\xa9*", "\xc3\xa9\xc3\xa9", 0, 4);   /* repeated whole rune */
    want("[\xce\xb1-\xcf\x89]+", "\xce\xb2\xce\xb3", 0, 4); /* a Greek range */

    /* A malformed byte is one unit, never a swallowed sequence. */
    want(".", "\x82", 0, 1);
    want("..", "\x82\x82", 0, 2);

    /* The mark stack, not the native stack: one frame per character here would
       trap in a wasm process. */
    {
        static char big[200000];
        memset(big, 'a', sizeof(big) - 2);
        big[sizeof(big) - 2] = 'b';
        big[sizeof(big) - 1] = '\0';
        want(".*b", big, 0, (long)sizeof(big) - 1);
        want("a*b", big, 0, (long)sizeof(big) - 1);
        want("[a]*b", big, 0, (long)sizeof(big) - 1);
    }

    /* Budget and depth, not a hang: the answer may be "no match", but it must
       come back. */
    want("(a*)*(a*)*(a*)*c", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaab", -1, -1);
}

int main(void)
{
    static const char *PATS[] = {
        // What upstream's own tests reach, and the kludges around them.
        "^", "$", ".$", "^$", "line", "butt", "WOOT", "A:", "\\)", "\\}", "\t",
        // Leftmost-longest, which a leftmost-first backtracker gets wrong.
        "foo|foobar", "foobar|foo", "a|ab|abc", "(a|ab)(c|bcd)",
        // Repetition.
        "a*", "a+", "a?", "ab*c", "a.*b", "x*y*z*", "a{2}", "a{2,}", "a{1,3}",
        "(ab)*", "(a*)*b", "(a|b)+",
        // Anchors inside and around.
        "^a", "a$", "^a$", "^.*$", "^$", "^\n", "\n$", "^ab*$",
        // Brackets.
        "[abc]", "[^abc]", "[a-z]+", "[^a-z]+", "[]a]", "[^]a]", "[a-]", "[-a]",
        "[[:alpha:]]+", "[[:digit:]]+", "[[:space:]]", "[[:alnum:]_]+", "[^[:space:]]+",
        // Groups and captures.
        "(a)(b)(c)", "(a(b(c)))", "(a)|(b)", "((a)*)b", "(foo)?bar", "(a|b)*c",
        // Dot and newline under REG_NEWLINE.
        ".", ".*", "a.c", "[^x]*",
        // Nesting and alternation depth.
        "(a|b)(c|d)(e|f)", "((x|y)z)+", "a(b|c)*d",
    };
    static const char *STRS[] = {
        "",
        "a",
        "ab",
        "abc",
        "abcabc",
        "aaa",
        "foobar",
        "hello world",
        "hello\nworld",
        "\n",
        "\n\n",
        "a\nb\nc",
        "a\nb\nc\n",
        "-last line-",
        "this file might be a",
        "A: alpha",
        "x)y}z",
        "\tindented",
        "123 456",
        "the_name_42",
        "  spaced  ",
        "aXbXc",
        "abcdef",
        "xyzxyz",
    };

    for (unsigned p = 0; p < sizeof(PATS) / sizeof(PATS[0]); p++)
        for (unsigned s = 0; s < sizeof(STRS) / sizeof(STRS[0]); s++)
            for (int nb = 0; nb < 2; nb++)
                check(PATS[p], STRS[s], nb);

    // Patterns that must not compile.
    static const char *BAD[] = { ")(", "(", "[", "a{2,1}", "*", "+a", "[z-a]", "(a" };
    for (unsigned i = 0; i < sizeof(BAD) / sizeof(BAD[0]); i++) {
        regex_t re;
        if (regcomp(&re, BAD[i], REG_EXTENDED | REG_NEWLINE) == 0) {
            printf("FAIL /%s/: compiled, should not have\n", BAD[i]);
            regfree(&re);
            failures++;
        }
        checked++;
    }

    self_tests();

    printf("%d checked, %d failed\n", checked, failures);
    return failures != 0;
}
