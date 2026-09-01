// util.cpp — util.c, by Mark Adler. Pattern matching, the environment, and
// the number formatting the messages use.

#include "zip.h"

#define WILDCHR_SINGLE '?'
#define WILDCHR_MULTI  '*'
#define DIRSEP_CHR     '/'

// Local functions
local int recmatch OF((ZCONST char *, ZCONST char *, int));
local int count_args OF((char *s));

// 2004-11-12 SMS.
// Changed to use z*o() functions, and ftell() test from >= 0 to != -1.
// This solves problems with negative 32-bit offsets, even on small-file
// products.
int fseekable(FILE *fp)
{
    // Upstream probed with a seek and a tell. That needs a co_await here, and
    // flush_block() asks from plain code, so zip.cpp records the answer when
    // it opens the archive instead.
    return fp == NULL || output_seekable;
}

char *isshexp(char *p)
{
    for (; *p; INCSTR(p))
        if (*p == '\\' && *(p + 1))
            p++;
        else if (*p == WILDCHR_SINGLE || *p == WILDCHR_MULTI || *p == '[')
            return p;
    return NULL;
}

local int recmatch(ZCONST char *p, ZCONST char *s, int cs)
{
    int c; // pattern char or start of range in [-] loop
    // Get first character, the pattern for new recmatch calls follows

    // This fix provided by akt@m5.dion.ne.jp for Japanese.
    // See 21 July 2006 mail.
    // It only applies when p is pointing to a doublebyte character and
    // things like / and wildcards are not doublebyte.  This probably
    // should not be needed.

    c = *POSTINCSTR(p);

    // If that was the end of the pattern, match if string empty too
    if (c == 0)
        return *s == 0;

    // '?' (or '%' or '#') matches any character (but not an empty string)
    if (c == WILDCHR_SINGLE) {
        if (wild_stop_at_dir)
            return (*s && *s != DIRSEP_CHR) ? recmatch(p, s + CLEN(s), cs) : 0;
        else
            return *s ? recmatch(p, s + CLEN(s), cs) : 0;
    }

    // WILDCHR_MULTI ('*') matches any number of characters, including zero
    if (!no_wild && c == WILDCHR_MULTI) {
        if (wild_stop_at_dir) {
            // Check for an immediately following WILDCHR_MULTI
            if (*p != WILDCHR_MULTI) {
                // Single WILDCHR_MULTI ('*'): this doesn't match slashes
                for (; *s && *s != DIRSEP_CHR; INCSTR(s))
                    if ((c = recmatch(p, s, cs)) != 0)
                        return c;
                // end of pattern: matched if at end of string, else continue
                if (*p == 0)
                    return (*s == 0);
                // continue to match if at DIRSEP_CHR in pattern, else give up
                return (*p == DIRSEP_CHR || (*p == '\\' && p[1] == DIRSEP_CHR)) ? recmatch(p, s, cs)
                                                                                : 2;
            }
            // Two consecutive WILDCHR_MULTI ("**"): this matches DIRSEP_CHR ('/')
            p++; // move p past the second WILDCHR_MULTI
                 // continue with the normal non-WILD_STOP_AT_DIR code
        } // wild_stop_at_dir

        // Not wild_stop_at_dir
        if (*p == 0)
            return 1;
        if (!isshexp((char *)p)) {
            // optimization for rest of pattern being a literal string

            // optimization to handle patterns like *.txt
            // if the first char in the pattern is '*' and there
            // are no other shell expression chars, i.e. a literal string
            // then just compare the literal string at the end

            ZCONST char *srest;

            srest = s + (strlen(s) - strlen(p));
            if (srest - s < 0)
                // remaining literal string from pattern is longer than rest of
                // test string, there can't be a match
                return 0;
            else
                // compare the remaining literal pattern string with the last bytes
                // of the test string to check for a match
                return ((cs ? strcmp(p, srest) : namecmp(p, srest)) == 0);
        } else {
            // pattern contains more wildcards, continue with recursion...
            for (; *s; INCSTR(s))
                if ((c = recmatch(p, s, cs)) != 0)
                    return c;
            return 2; // 2 means give up--shmatch will return false
        }
    }

    // Parse and process the list of characters and ranges in brackets
    if (!no_wild && allow_regex && c == '[') {
        int e;          // flag true if next char to be taken literally
        ZCONST char *q; // pointer to end of [-] group
        int r;          // flag true to match anything but the range

        if (*s == 0) // need a character to match
            return 0;
        p += (r = (*p == '!' || *p == '^')); // see if reverse
        for (q = p, e = 0; *q; q++)          // find closing bracket
            if (e)
                e = 0;
            else if (*q == '\\')
                e = 1;
            else if (*q == ']')
                break;
        if (*q != ']') // nothing matches if bad syntax
            return 0;
        for (c = 0, e = *p == '-'; p < q; p++) // go through the list
        {
            if (e == 0 && *p == '\\') /* set escape flag if \ */
                e = 1;
            else if (e == 0 && *p == '-') // set start of range if -
                c = *(p - 1);
            else {
                uch cc = (cs ? (uch)*s : case_map((uch)*s));
                uch uc = (uch)c;
                if (*(p + 1) != '-')
                    for (uc = uc ? uc : (uch)*p; uc <= (uch)*p; uc++)
                        // compare range
                        if ((cs ? uc : case_map(uc)) == cc)
                            return r ? 0 : recmatch(q + CLEN(q), s + CLEN(s), cs);
                c = e = 0; // clear range, escape flags
            }
        }
        return r ? recmatch(q + CLEN(q), s + CLEN(s), cs) : 0;
        // bracket match failed
    }

    // If escape ('\'), just compare next character
    if (!no_wild && c == '\\')
        if ((c = *p++) == '\0') // if \ at end, then syntax error
            return 0;

    // Just a character--compare it
    return (cs ? c == *s : case_map((uch)c) == case_map((uch)*s)) ? recmatch(p, s + CLEN(s), cs)
                                                                  : 0;
}

int shmatch(ZCONST char *p, ZCONST char *s, int cs)
{
    return recmatch(p, s, cs) == 1;
}

zvoid far **search(ZCONST zvoid *b, ZCONST zvoid far **a, extent n,
                   int(*cmp) OF((ZCONST zvoid *, ZCONST zvoid far *)))

// Search for b in the pointer list a[0..n-1] using the compare function
// cmp(b, c) where c is an element of a[i] and cmp() returns negative if
// b < *c, zero if *b == *c, or positive if *b > *c.  If *b is found,
// search returns a pointer to the entry in a[], else search() returns
// NULL.  The nature and size of *b and *c (they can be different) are
// left up to the cmp() function.  A binary search is used, and it is
// assumed that the list is sorted in ascending order.
{
    ZCONST zvoid far **i; // pointer to midpoint of current range
    ZCONST zvoid far **l; // pointer to lower end of current range
    int r;                // result of (*cmp)() call
    ZCONST zvoid far **u; // pointer to upper end of current range

    l = (ZCONST zvoid far **)a;
    u = l + (n - 1);
    while (u >= l) {
        i = l + ((unsigned)(u - l) >> 1);
        if ((r = (*cmp)(b, (ZCONST char far *)*(struct zlist far **)i)) < 0)
            u = i - 1;
        else if (r > 0)
            l = i + 1;
        else
            return (zvoid far **)i;
    }
    return NULL; // If b were in list, it would belong at l
}

void init_upper()
{
    unsigned int c;
    for (c = 0; c < sizeof(upper); c++)
        upper[c] = lower[c] = (uch)c;
    for (c = 'a'; c <= 'z'; c++)
        upper[c] = (uch)(c - 'a' + 'A');
    for (c = 'A'; c <= 'Z'; c++)
        lower[c] = (uch)(c - 'A' + 'a');
}

int namecmp(ZCONST char *string1, ZCONST char *string2)
{
    int d;

    for (;;) {
        d = (int)(uch)case_map(*string1) - (int)(uch)case_map(*string2);

        if (d || *string1 == 0 || *string2 == 0)
            return d;

        string1++;
        string2++;
    }
}

// DBCS support for Info-ZIP's zip  (mainly for japanese (-: )
// by Yoshioka Tsuneo (QWF00133@nifty.ne.jp,tsuneo-y@is.aist-nara.ac.jp)
// This code is public domain!   Date: 1998/12/20

// ***************************************************************
// | envargs - add default options from environment to command line
// |----------------------------------------------------------------
// | Author: Bill Davidsen, original 10/13/91, revised 23 Oct 1991.
// | This program is in the public domain.
// |----------------------------------------------------------------
// | Minor program notes:
// |  1. Yes, the indirection is a tad complex
// |  2. Parenthesis were added where not needed in some cases
// |     to make the action of the code less obscure.
// **************************************************************

void envargs(int *Pargc, char ***Pargv, char *envstr, char *envstr2)
{
    char *envptr;   // value returned by getenv
    char *bufptr;   // copy of env info
    int argc;       // internal arg count
    int ch;         // spare temp value
    char **argv;    // internal arg vector
    char **argvect; // copy of vector address

    // see if anything in the environment
    envptr = getenv(envstr);
    if (envptr != NULL)               // usual var
        while (isspace((uch)*envptr)) // we must discard leading spaces
            envptr++;
    if (envptr == NULL || *envptr == '\0')
        if ((envptr = getenv(envstr2)) != NULL) // alternate
            while (isspace((uch)*envptr))
                envptr++;
    if (envptr == NULL || *envptr == '\0')
        return;

    // count the args so we can allocate room for them
    argc   = count_args(envptr);
    bufptr = (char *)malloc(1 + strlen(envptr));
    if (bufptr == NULL) {
        zip_fail(ZE_MEM, "Can't get memory for arguments");
        return;
    }
    strcpy(bufptr, envptr);

    // allocate a vector large enough for all args
    argv = (char **)malloc((argc + *Pargc + 1) * sizeof(char *));
    if (argv == NULL) {
        free(bufptr);
        {
            zip_fail(ZE_MEM, "Can't get memory for arguments");
            return;
        }
    }
    argvect = argv;

    // copy the program name first, that's always true
    *(argv++) = *((*Pargv)++);

    // copy the environment args first, may be changed
    do {
        if (*bufptr == '"') {
            char *argstart = ++bufptr;
            *(argv++)      = argstart;
            for (ch = *bufptr; ch != '\0' && ch != '\"'; ch = *PREINCSTR(bufptr))
                if (ch == '\\' && bufptr[1] != '\0')
                    ++bufptr; // skip to char after backslash
            if (ch != '\0')   // overwrite trailing '"'
                *(bufptr++) = '\0';

            // remove escape characters
            while ((argstart = MBSCHR(argstart, '\\')) != NULL) {
                strcpy(argstart, argstart + 1);
                if (*argstart)
                    ++argstart;
            }
        } else {
            *(argv++) = bufptr;
            while ((ch = *bufptr) != '\0' && !isspace((uch)ch))
                INCSTR(bufptr);
            if (ch != '\0')
                *(bufptr++) = '\0';
        }
        while ((ch = *bufptr) != '\0' && isspace((uch)ch))
            INCSTR(bufptr);
    } while (ch);

    // now save old argc and copy in the old args
    argc += *Pargc;
    while (--(*Pargc))
        *(argv++) = *((*Pargv)++);

    // finally, add a NULL after the last arg, like UNIX
    *argv = NULL;

    // save the values and return
    *Pargv = argvect;
    *Pargc = argc;
}

local int count_args(char *s)
{
    int count = 0;
    char ch;

    do {
        // count and skip args
        ++count;
        if (*s == '\"') {
            for (ch = *PREINCSTR(s); ch != '\0' && ch != '\"'; ch = *PREINCSTR(s))
                if (ch == '\\' && s[1] != '\0')
                    INCSTR(s);
            if (*s)
                INCSTR(s); // trailing quote
        } else
            while ((ch = *s) != '\0' && !isspace((uch)ch))
                INCSTR(s);
        while ((ch = *s) != '\0' && isspace((uch)ch))
            INCSTR(s);
    } while (ch);

    return (count);
}

// Extended argument processing -- by Rich Wales
// This function currently deals only with the MKS shell, but could be
// extended later to understand other conventions.
//
// void expand_args(int *argcp, char ***argvp)
//
// Substitutes the extended command line argument list produced by
// the MKS Korn Shell in place of the command line info from DOS.
//
// The MKS shell gets around DOS's 128-byte limit on the length of
// a command line by passing the "real" command line in the envi-
// ronment.  The "real" arguments are flagged by prepending a tilde
// (~) to each one.
//
// This "expand_args" routine creates a new argument list by scanning
// the environment from the beginning, looking for strings begin-
// ning with a tilde character.  The new list replaces the original
// "argv" (pointed to by "argvp"), and the number of arguments
// in the new list replaces the original "argc" (pointed to by
// "argcp").
void expand_args(int *argcp, char ***argvp)
{
    if (argcp || argvp)
        return;
}

// Fast routine for detection of plain text
// (ASCII or an ASCII-compatible extension such as ISO-8859, UTF-8, etc.)
// Author: Cosmin Truta.
// See "proginfo/txtvsbin.txt" for more information.
//
// This function returns the same result as set_file_type() in "trees.c".
// Unlike in set_file_type(), however, the speed depends on the buffer size,
// so the optimal implementation is different.
int is_text_buf(ZCONST char *buf_ptr, unsigned buf_size)
{
    int result = 0;
    unsigned i;
    unsigned char c;

    for (i = 0; i < buf_size; ++i) {
        c = (unsigned char)buf_ptr[i];
        if (c >= 32)    // speed up the loop by checking this first
            result = 1; // white-listed character found; keep looping
        else            // speed up the loop by inlining the following check
            if ((c <= 6) || (c >= 14 && c <= 25) || (c >= 28 && c <= 31))
                return 0; // black-listed character found; stop
    }

    return result;
}

// Below is used to format zoff_t values, which can be either long or long long
// depending on if LARGE FILES are supported.  Function provided by SMS.
// 10/17/04 EG

// 2004-12-01 SMS.
// Brought in fancy fzofft() from UnZip.

// This implementation assumes that no more than FZOFF_NUM values will be
// needed in any printf using it.

// zip_fzofft(): Format a zoff_t value in a cylindrical buffer set.
// This version renamed from fzofft because of name conflict in unzip
// when combined in WiZ.

// 2004-12-19 SMS.
// I still claim than the smart move would have been to disable one or
// the other instance with #if for Wiz.  But fine.  We'll change the
// name.

// This is likely not thread safe.  Needs to be done without static storage.
// 12/29/04 EG

// zip_fzofft(): Format a zoff_t value in a cylindrical buffer set.

#define FZOFFT_NUM 4  // Number of chambers.
#define FZOFFT_LEN 24 // Number of characters/chamber.

// Format a zoff_t value in a cylindrical buffer set.

char *zip_fzofft(zoff_t val, char *pre, char *post)
{
    // Storage cylinder.
    static char fzofft_buf[FZOFFT_NUM][FZOFFT_LEN];
    static int fzofft_index = 0;

    // Temporary format string storage.
    static char fmt[16] = "%";

    // Assemble the format string.
    fmt[1] = '\0';             // Start after initial "%".
    if (pre == FZOFFT_HEX_WID) // Special hex width.
    {
        strcat(fmt, FZOFFT_HEX_WID_VALUE);
    } else if (pre == FZOFFT_HEX_DOT_WID) // Special hex ".width".
    {
        strcat(fmt, ".");
        strcat(fmt, FZOFFT_HEX_WID_VALUE);
    } else if (pre != NULL) // Caller's prefix (width).
    {
        strcat(fmt, pre);
    }

    strcat(fmt, FZOFFT_FMT); // Long or long-long or whatever.

    if (post == NULL)
        strcat(fmt, "d"); // Default radix = decimal.
    else
        strcat(fmt, post); // Caller's radix.

    // Advance the cylinder.
    fzofft_index = (fzofft_index + 1) % FZOFFT_NUM;

    // Write into the current chamber.
    sprintf(fzofft_buf[fzofft_index], fmt, val);

    // Return a pointer to this chamber.
    return fzofft_buf[fzofft_index];
}

// Format a uzoff_t value in a cylindrical buffer set.
// Added to support uzoff_t type.  12/29/04

char *zip_fuzofft(uzoff_t val, char *pre, char *post)
{
    // Storage cylinder.
    static char fuzofft_buf[FZOFFT_NUM][FZOFFT_LEN];
    static int fuzofft_index = 0;

    // Temporary format string storage.
    static char fmt[16] = "%";

    // Assemble the format string.
    fmt[1] = '\0';             // Start after initial "%".
    if (pre == FZOFFT_HEX_WID) // Special hex width.
    {
        strcat(fmt, FZOFFT_HEX_WID_VALUE);
    } else if (pre == FZOFFT_HEX_DOT_WID) // Special hex ".width".
    {
        strcat(fmt, ".");
        strcat(fmt, FZOFFT_HEX_WID_VALUE);
    } else if (pre != NULL) // Caller's prefix (width).
    {
        strcat(fmt, pre);
    }

    strcat(fmt, FZOFFT_FMT); // Long or long-long or whatever.

    if (post == NULL)
        strcat(fmt, "u"); // Default radix = decimal.
    else
        strcat(fmt, post); // Caller's radix.

    // Advance the cylinder.
    fuzofft_index = (fuzofft_index + 1) % FZOFFT_NUM;

    // Write into the current chamber.
    sprintf(fuzofft_buf[fuzofft_index], fmt, val);

    // Return a pointer to this chamber.
    return fuzofft_buf[fuzofft_index];
}

// Display number to mesg stream
// 5/15/05 EG

Task<int> DisplayNumString(FILE *file, uzoff_t i)
{
    char tempstrg[100];
    int j;
    char *s = tempstrg;

    WriteNumString(i, tempstrg);
    // skip spaces
    for (j = 0; j < 3; j++) {
        if (*s != ' ')
            break;
        s++;
    }
    co_await b_fprintf(file, "%s", s);

    co_return 0;
}

// Read numbers with trailing size multiplier (like 10M) and return number.
// 10/30/04 EG

Task<uzoff_t> ReadNumString(char *numstring)
{
    zoff_t num    = 0;
    char multchar = ' ';
    int i;
    uzoff_t mult = 1;

    // check if valid number (currently no negatives)
    if (numstring == NULL) {
        co_await zipwarn("Unable to read empty number in ReadNumString", "");
        co_return (uzoff_t) - 1;
    }
    if (numstring[0] < '0' || numstring[0] > '9') {
        co_await zipwarn("Unable to read number (must start with digit): ", numstring);
        co_return (uzoff_t) - 1;
    }
    if (strlen(numstring) > 8) {
        co_await zipwarn("Number too long to read (8 characters max): ", numstring);
        co_return (uzoff_t) - 1;
    }

    // get the number part
    num = atoi(numstring);

    // find trailing multiplier
    for (i = 0; numstring[i] && isdigit(numstring[i]); i++)
        ;

    // co_return if no multiplier
    if (numstring[i] == '\0') {
        co_return (uzoff_t) num;
    }

    // nothing follows multiplier
    if (numstring[i + 1]) {
        co_return (uzoff_t) - 1;
    }

    // get multiplier
    multchar = toupper(numstring[i]);

    if (multchar == 'K') {
        mult <<= 10;
    } else if (multchar == 'M') {
        mult <<= 20;
    } else if (multchar == 'G') {
        mult <<= 30;
    } else if (multchar == 'T') {
        mult <<= 40;
    } else {
        co_return (uzoff_t) - 1;
    }

    co_return (uzoff_t) num *mult;
}

// Write the number as a string with a multiplier (like 10M) to outstring.
// Always writes no more than 3 digits followed maybe by a multiplier and
// returns the characters written or -1 if error.
// 10/30/04 EG

int WriteNumString(uzoff_t num, char *outstring)
{
    int mult;
    int written = 0;
    int i;
    int j;
    char digits[4];
    int dig;

    *outstring = '\0';

    // shift number 1 K until less than 10000
    for (mult = 0; num >= 10240; mult++) {
        num >>= 10;
    }

    // write digits as "    0"
    for (i = 1; i < 4; i++) {
        digits[i] = ' ';
    }
    digits[0] = '0';

    if (num >= 1000) {
        i = 3;
        num *= 10;
        num >>= 10;
        mult++;
        digits[0] = (char)(num % 10) + '0';
        digits[1] = '.';
        digits[2] = (char)(num / 10) + '0';
    } else {
        for (i = 0; num; i++) {
            dig = (int)(num % 10);
            num /= 10;
            digits[i] = dig + '0';
        }
    }
    if (i == 0)
        i = 1;
    for (j = i; j > 0; j--) {
        *outstring = digits[j - 1];
        outstring++;
        written++;
    }

    // output multiplier
    if (mult == 0) {
    } else if (mult == 1) {
        *outstring = 'K';
        outstring++;
        written++;
    } else if (mult == 2) {
        *outstring = 'M';
        outstring++;
        written++;
    } else if (mult == 3) {
        *outstring = 'G';
        outstring++;
        written++;
    } else if (mult == 4) {
        *outstring = 'T';
        outstring++;
        written++;
    } else {
        *outstring = '?';
        outstring++;
        written++;
    }

    *outstring = '\0';

    return written;
}

// returns true if abbrev is abbreviation for matchstring
int abbrevmatch(char *matchstring, char *abbrev, int case_sensitive, int minmatch)
{
    int cnt = 0;
    char *m;
    char *a;

    m = matchstring;
    a = abbrev;

    for (; *m && *a; m++, a++) {
        cnt++;
        if (case_sensitive) {
            if (*m != *a) {
                // mismatch
                return 0;
            }
        } else {
            if (toupper(*m) != toupper(*a)) {
                // mismatch
                return 0;
            }
        }
    }
    if (cnt < minmatch) {
        // not big enough string
        return 0;
    }
    if (*a != '\0') {
        // abbreviation longer than match string
        return 0;
    }
    // either abbreviation or match
    return 1;
}
