/*
                            __  __            _
                         ___\ \/ /_ __   __ _| |_
                        / _ \\  /| '_ \ / _` | __|
                       |  __//  \| |_) | (_| | |_
                        \___/_/\_\ .__/ \__,_|\__|
                                 |_| XML parser

   Copyright (c) 1997-2000 Thai Open Source Software Center Ltd
   Copyright (c) 2000      Clark Cooper <coopercc@users.sourceforge.net>
   Copyright (c) 2017-2019 Sebastian Pipping <sebastian@pipping.org>
   Licensed under the MIT license:

   Permission is  hereby granted,  free of charge,  to any  person obtaining
   a  copy  of  this  software   and  associated  documentation  files  (the
   "Software"),  to  deal in  the  Software  without restriction,  including
   without  limitation the  rights  to use,  copy,  modify, merge,  publish,
   distribute, sublicense, and/or sell copies of the Software, and to permit
   persons  to whom  the Software  is  furnished to  do so,  subject to  the
   following conditions:

   The above copyright  notice and this permission notice  shall be included
   in all copies or substantial portions of the Software.

   THE  SOFTWARE  IS  PROVIDED  "AS  IS",  WITHOUT  WARRANTY  OF  ANY  KIND,
   EXPRESS  OR IMPLIED,  INCLUDING  BUT  NOT LIMITED  TO  THE WARRANTIES  OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
   NO EVENT SHALL THE AUTHORS OR  COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
   DAMAGES OR  OTHER LIABILITY, WHETHER  IN AN  ACTION OF CONTRACT,  TORT OR
   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
   USE OR OTHER DEALINGS IN THE SOFTWARE.

   SPDX-License-Identifier: MIT
*/

#pragma once

#include "ascii.h"
#include "expat_config.h"
#include "nametab.h"
#include "xmltok.h"

// Upstream includes this body three times, once per byte width, with a set of
// macros redefined around each inclusion; here the encoding is a template
// parameter and the bodies are written once. The macros below are the same
// macros, spelled against `E` -- so what is inside a scanner is upstream's
// text, line for line.

#define AS_NORMAL_ENCODING(enc) ((const struct normal_encoding *)(enc))

#define SB_BYTE_TYPE(enc, p) (((const struct normal_encoding *)(enc))->type[(unsigned char)*(p)])

#define UCS2_GET_NAMING(pages, hi, lo) \
    (namingBitmap[(pages[hi] << 3) + ((lo) >> 5)] & (1u << ((lo) & 0x1F)))

// A scanner reaches these three through the encoding it was handed, because
// the answer for a lead byte depends on which table the encoding carries.
struct normal_encoding {
    ENCODING enc;
    unsigned char type[256];
    int (*isName2)(const ENCODING *, const char *);
    int (*isName3)(const ENCODING *, const char *);
    int (*isName4)(const ENCODING *, const char *);
    int (*isNmstrt2)(const ENCODING *, const char *);
    int (*isNmstrt3)(const ENCODING *, const char *);
    int (*isNmstrt4)(const ENCODING *, const char *);
    int (*isInvalid2)(const ENCODING *, const char *);
    int (*isInvalid3)(const ENCODING *, const char *);
    int (*isInvalid4)(const ENCODING *, const char *);
};

int checkCharRefNumber(int result);
int unicode_byte_type(char hi, char lo);

// One byte per character: UTF-8, Latin-1, US-ASCII and whatever an
// XML_UnknownEncodingHandler described. What a lead byte means is the
// encoding's business, so the three predicates go through its table.
struct NormalEnc {
    static const int MINBPC = 1;

    static int byteType(const ENCODING *enc, const char *p) { return SB_BYTE_TYPE(enc, p); }

    static int byteToAscii(const ENCODING *, const char *p) { return *p; }

    static bool charMatches(const ENCODING *, const char *p, int c) { return *p == c; }

    static int isName2(const ENCODING *enc, const char *p)
    {
        return AS_NORMAL_ENCODING(enc)->isName2(enc, p);
    }
    static int isName3(const ENCODING *enc, const char *p)
    {
        return AS_NORMAL_ENCODING(enc)->isName3(enc, p);
    }
    static int isName4(const ENCODING *enc, const char *p)
    {
        return AS_NORMAL_ENCODING(enc)->isName4(enc, p);
    }
    static int isNmstrt2(const ENCODING *enc, const char *p)
    {
        return AS_NORMAL_ENCODING(enc)->isNmstrt2(enc, p);
    }
    static int isNmstrt3(const ENCODING *enc, const char *p)
    {
        return AS_NORMAL_ENCODING(enc)->isNmstrt3(enc, p);
    }
    static int isNmstrt4(const ENCODING *enc, const char *p)
    {
        return AS_NORMAL_ENCODING(enc)->isNmstrt4(enc, p);
    }
    static int isInvalid2(const ENCODING *enc, const char *p)
    {
        return AS_NORMAL_ENCODING(enc)->isInvalid2(enc, p);
    }
    static int isInvalid3(const ENCODING *enc, const char *p)
    {
        return AS_NORMAL_ENCODING(enc)->isInvalid3(enc, p);
    }
    static int isInvalid4(const ENCODING *enc, const char *p)
    {
        return AS_NORMAL_ENCODING(enc)->isInvalid4(enc, p);
    }

    // A single byte is never a name character in its own right here: the
    // table above has already said so.
    static int isNameMinbpc(const ENCODING *, const char *) { return 0; }
    static int isNmstrtMinbpc(const ENCODING *, const char *) { return 0; }
};

// Two bytes per character, little end first. A lead byte is a surrogate and
// the naming bitmap answers directly, so nothing goes through the encoding.
struct Little2Enc {
    static const int MINBPC = 2;

    static int byteType(const ENCODING *enc, const char *p)
    {
        return p[1] == 0 ? SB_BYTE_TYPE(enc, p) : unicode_byte_type(p[1], p[0]);
    }

    static int byteToAscii(const ENCODING *, const char *p) { return p[1] == 0 ? p[0] : -1; }

    static bool charMatches(const ENCODING *, const char *p, int c)
    {
        return p[1] == 0 && p[0] == c;
    }

    static int isName2(const ENCODING *, const char *) { return 0; }
    static int isName3(const ENCODING *, const char *) { return 0; }
    static int isName4(const ENCODING *, const char *) { return 0; }
    static int isNmstrt2(const ENCODING *, const char *) { return 0; }
    static int isNmstrt3(const ENCODING *, const char *) { return 0; }
    static int isNmstrt4(const ENCODING *, const char *) { return 0; }
    static int isInvalid2(const ENCODING *, const char *) { return 0; }
    static int isInvalid3(const ENCODING *, const char *) { return 0; }
    static int isInvalid4(const ENCODING *, const char *) { return 0; }

    static int isNameMinbpc(const ENCODING *, const char *p)
    {
        return UCS2_GET_NAMING(namePages, (unsigned char)p[1], (unsigned char)p[0]);
    }
    static int isNmstrtMinbpc(const ENCODING *, const char *p)
    {
        return UCS2_GET_NAMING(nmstrtPages, (unsigned char)p[1], (unsigned char)p[0]);
    }
};

// Two bytes per character, big end first.
struct Big2Enc {
    static const int MINBPC = 2;

    static int byteType(const ENCODING *enc, const char *p)
    {
        return p[0] == 0 ? SB_BYTE_TYPE(enc, p + 1) : unicode_byte_type(p[0], p[1]);
    }

    static int byteToAscii(const ENCODING *, const char *p) { return p[0] == 0 ? p[1] : -1; }

    static bool charMatches(const ENCODING *, const char *p, int c)
    {
        return p[0] == 0 && p[1] == c;
    }

    static int isName2(const ENCODING *, const char *) { return 0; }
    static int isName3(const ENCODING *, const char *) { return 0; }
    static int isName4(const ENCODING *, const char *) { return 0; }
    static int isNmstrt2(const ENCODING *, const char *) { return 0; }
    static int isNmstrt3(const ENCODING *, const char *) { return 0; }
    static int isNmstrt4(const ENCODING *, const char *) { return 0; }
    static int isInvalid2(const ENCODING *, const char *) { return 0; }
    static int isInvalid3(const ENCODING *, const char *) { return 0; }
    static int isInvalid4(const ENCODING *, const char *) { return 0; }

    static int isNameMinbpc(const ENCODING *, const char *p)
    {
        return UCS2_GET_NAMING(namePages, (unsigned char)p[0], (unsigned char)p[1]);
    }
    static int isNmstrtMinbpc(const ENCODING *, const char *p)
    {
        return UCS2_GET_NAMING(nmstrtPages, (unsigned char)p[0], (unsigned char)p[1]);
    }
};

#define MINBPC(enc)                   E::MINBPC
#define BYTE_TYPE(enc, p)             E::byteType(enc, p)
#define BYTE_TO_ASCII(enc, p)         E::byteToAscii(enc, p)
#define CHAR_MATCHES(enc, p, c)       E::charMatches(enc, p, c)
#define IS_NAME_CHAR(enc, p, n)       E::isName##n(enc, p)
#define IS_NAME_CHAR_MINBPC(enc, p)   E::isNameMinbpc(enc, p)
#define IS_NMSTRT_CHAR(enc, p, n)     E::isNmstrt##n(enc, p)
#define IS_NMSTRT_CHAR_MINBPC(enc, p) E::isNmstrtMinbpc(enc, p)
#define IS_INVALID_CHAR(enc, p, n)    E::isInvalid##n(enc, p)

enum {
    BT_NONXML,   /* e.g. noncharacter-FFFF */
    BT_MALFORM,  /* illegal, with regard to encoding */
    BT_LT,       /* less than = "<" */
    BT_AMP,      /* ampersand = "&" */
    BT_RSQB,     /* right square bracket = "[" */
    BT_LEAD2,    /* lead byte of a 2-byte UTF-8 character */
    BT_LEAD3,    /* lead byte of a 3-byte UTF-8 character */
    BT_LEAD4,    /* lead byte of a 4-byte UTF-8 character */
    BT_TRAIL,    /* trailing unit, e.g. second 16-bit unit of a 4-byte char. */
    BT_CR,       /* carriage return = "\r" */
    BT_LF,       /* line feed = "\n" */
    BT_GT,       /* greater than = ">" */
    BT_QUOT,     /* quotation character = "\"" */
    BT_APOS,     /* apostrophe = "'" */
    BT_EQUALS,   /* equal sign = "=" */
    BT_QUEST,    /* question mark = "?" */
    BT_EXCL,     /* exclamation mark = "!" */
    BT_SOL,      /* solidus, slash = "/" */
    BT_SEMI,     /* semicolon = ";" */
    BT_NUM,      /* number sign = "#" */
    BT_LSQB,     /* left square bracket = "[" */
    BT_S,        /* white space, e.g. "\t", " "[, "\r"] */
    BT_NMSTRT,   /* non-hex name start letter = "G".."Z" + "g".."z" + "_" */
    BT_COLON,    /* colon = ":" */
    BT_HEX,      /* hex letter = "A".."F" + "a".."f" */
    BT_DIGIT,    /* digit = "0".."9" */
    BT_NAME,     /* dot and middle dot = "." + chr(0xb7) */
    BT_MINUS,    /* minus = "-" */
    BT_OTHER,    /* known not to be a name or name start character */
    BT_NONASCII, /* might be a name or name start character */
    BT_PERCNT,   /* percent sign = "%" */
    BT_LPAR,     /* left parenthesis = "(" */
    BT_RPAR,     /* right parenthesis = "(" */
    BT_AST,      /* asterisk = "*" */
    BT_PLUS,     /* plus sign = "+" */
    BT_COMMA,    /* comma = "," */
    BT_VERBAR    /* vertical bar = "|" */
};

#define INVALID_LEAD_CASE(n, ptr, nextTokPtr) \
    case BT_LEAD##n:                          \
        if (end - ptr < n)                    \
            return XML_TOK_PARTIAL_CHAR;      \
        if (IS_INVALID_CHAR(enc, ptr, n)) {   \
            *(nextTokPtr) = (ptr);            \
            return XML_TOK_INVALID;           \
        }                                     \
        ptr += n;                             \
        break;

#define INVALID_CASES(ptr, nextTokPtr)    \
    INVALID_LEAD_CASE(2, ptr, nextTokPtr) \
    INVALID_LEAD_CASE(3, ptr, nextTokPtr) \
    INVALID_LEAD_CASE(4, ptr, nextTokPtr) \
    case BT_NONXML:                       \
    case BT_MALFORM:                      \
    case BT_TRAIL:                        \
        *(nextTokPtr) = (ptr);            \
        return XML_TOK_INVALID;

#define CHECK_NAME_CASE(n, enc, ptr, end, nextTokPtr)                     \
    case BT_LEAD##n:                                                      \
        if (end - ptr < n)                                                \
            return XML_TOK_PARTIAL_CHAR;                                  \
        if (IS_INVALID_CHAR(enc, ptr, n) || !IS_NAME_CHAR(enc, ptr, n)) { \
            *nextTokPtr = ptr;                                            \
            return XML_TOK_INVALID;                                       \
        }                                                                 \
        ptr += n;                                                         \
        break;

#define CHECK_NAME_CASES(enc, ptr, end, nextTokPtr)   \
    case BT_NONASCII:                                 \
        if (!IS_NAME_CHAR_MINBPC(enc, ptr)) {         \
            *nextTokPtr = ptr;                        \
            return XML_TOK_INVALID;                   \
        }                                             \
        [[fallthrough]];                              \
    case BT_NMSTRT:                                   \
    case BT_HEX:                                      \
    case BT_DIGIT:                                    \
    case BT_NAME:                                     \
    case BT_MINUS:                                    \
        ptr += MINBPC(enc);                           \
        break;                                        \
        CHECK_NAME_CASE(2, enc, ptr, end, nextTokPtr) \
        CHECK_NAME_CASE(3, enc, ptr, end, nextTokPtr) \
        CHECK_NAME_CASE(4, enc, ptr, end, nextTokPtr)

#define CHECK_NMSTRT_CASE(n, enc, ptr, end, nextTokPtr)                     \
    case BT_LEAD##n:                                                        \
        if ((end) - (ptr) < (n))                                            \
            return XML_TOK_PARTIAL_CHAR;                                    \
        if (IS_INVALID_CHAR(enc, ptr, n) || !IS_NMSTRT_CHAR(enc, ptr, n)) { \
            *nextTokPtr = ptr;                                              \
            return XML_TOK_INVALID;                                         \
        }                                                                   \
        ptr += n;                                                           \
        break;

#define CHECK_NMSTRT_CASES(enc, ptr, end, nextTokPtr)   \
    case BT_NONASCII:                                   \
        if (!IS_NMSTRT_CHAR_MINBPC(enc, ptr)) {         \
            *nextTokPtr = ptr;                          \
            return XML_TOK_INVALID;                     \
        }                                               \
        [[fallthrough]];                                \
    case BT_NMSTRT:                                     \
    case BT_HEX:                                        \
        ptr += MINBPC(enc);                             \
        break;                                          \
        CHECK_NMSTRT_CASE(2, enc, ptr, end, nextTokPtr) \
        CHECK_NMSTRT_CASE(3, enc, ptr, end, nextTokPtr) \
        CHECK_NMSTRT_CASE(4, enc, ptr, end, nextTokPtr)

#define HAS_CHARS(enc, ptr, end, count) ((end) - (ptr) >= ((count) * MINBPC(enc)))

#define HAS_CHAR(enc, ptr, end) HAS_CHARS(enc, ptr, end, 1)

#define REQUIRE_CHARS(enc, ptr, end, count)     \
    {                                           \
        if (!HAS_CHARS(enc, ptr, end, count)) { \
            return XML_TOK_PARTIAL;             \
        }                                       \
    }

#define REQUIRE_CHAR(enc, ptr, end) REQUIRE_CHARS(enc, ptr, end, 1)

/* ptr points to character following "<!-" */

template <class E>
static int scanComment(const ENCODING *enc, const char *ptr, const char *end,
                       const char **nextTokPtr)
{
    if (HAS_CHAR(enc, ptr, end)) {
        if (!CHAR_MATCHES(enc, ptr, ASCII_MINUS)) {
            *nextTokPtr = ptr;
            return XML_TOK_INVALID;
        }
        ptr += MINBPC(enc);
        while (HAS_CHAR(enc, ptr, end)) {
            switch (BYTE_TYPE(enc, ptr)) {
                INVALID_CASES(ptr, nextTokPtr)
            case BT_MINUS:
                ptr += MINBPC(enc);
                REQUIRE_CHAR(enc, ptr, end);
                if (CHAR_MATCHES(enc, ptr, ASCII_MINUS)) {
                    ptr += MINBPC(enc);
                    REQUIRE_CHAR(enc, ptr, end);
                    if (!CHAR_MATCHES(enc, ptr, ASCII_GT)) {
                        *nextTokPtr = ptr;
                        return XML_TOK_INVALID;
                    }
                    *nextTokPtr = ptr + MINBPC(enc);
                    return XML_TOK_COMMENT;
                }
                break;
            default:
                ptr += MINBPC(enc);
                break;
            }
        }
    }
    return XML_TOK_PARTIAL;
}

/* ptr points to character following "<!" */

template <class E>
static int scanDecl(const ENCODING *enc, const char *ptr, const char *end, const char **nextTokPtr)
{
    REQUIRE_CHAR(enc, ptr, end);
    switch (BYTE_TYPE(enc, ptr)) {
    case BT_MINUS:
        return scanComment<E>(enc, ptr + MINBPC(enc), end, nextTokPtr);
    case BT_LSQB:
        *nextTokPtr = ptr + MINBPC(enc);
        return XML_TOK_COND_SECT_OPEN;
    case BT_NMSTRT:
    case BT_HEX:
        ptr += MINBPC(enc);
        break;
    default:
        *nextTokPtr = ptr;
        return XML_TOK_INVALID;
    }
    while (HAS_CHAR(enc, ptr, end)) {
        switch (BYTE_TYPE(enc, ptr)) {
        case BT_PERCNT:
            REQUIRE_CHARS(enc, ptr, end, 2);
            /* don't allow <!ENTITY% foo "whatever"> */
            switch (BYTE_TYPE(enc, ptr + MINBPC(enc))) {
            case BT_S:
            case BT_CR:
            case BT_LF:
            case BT_PERCNT:
                *nextTokPtr = ptr;
                return XML_TOK_INVALID;
            }
            [[fallthrough]];
        case BT_S:
        case BT_CR:
        case BT_LF:
            *nextTokPtr = ptr;
            return XML_TOK_DECL_OPEN;
        case BT_NMSTRT:
        case BT_HEX:
            ptr += MINBPC(enc);
            break;
        default:
            *nextTokPtr = ptr;
            return XML_TOK_INVALID;
        }
    }
    return XML_TOK_PARTIAL;
}

template <class E>
static int checkPiTarget(const ENCODING *enc, const char *ptr, const char *end, int *tokPtr)
{
    int upper = 0;
    (void)enc;
    *tokPtr = XML_TOK_PI;
    if (end - ptr != MINBPC(enc) * 3)
        return 1;
    switch (BYTE_TO_ASCII(enc, ptr)) {
    case ASCII_x:
        break;
    case ASCII_X:
        upper = 1;
        break;
    default:
        return 1;
    }
    ptr += MINBPC(enc);
    switch (BYTE_TO_ASCII(enc, ptr)) {
    case ASCII_m:
        break;
    case ASCII_M:
        upper = 1;
        break;
    default:
        return 1;
    }
    ptr += MINBPC(enc);
    switch (BYTE_TO_ASCII(enc, ptr)) {
    case ASCII_l:
        break;
    case ASCII_L:
        upper = 1;
        break;
    default:
        return 1;
    }
    if (upper)
        return 0;
    *tokPtr = XML_TOK_XML_DECL;
    return 1;
}

/* ptr points to character following "<?" */

template <class E>
static int scanPi(const ENCODING *enc, const char *ptr, const char *end, const char **nextTokPtr)
{
    int tok;
    const char *target = ptr;
    REQUIRE_CHAR(enc, ptr, end);
    switch (BYTE_TYPE(enc, ptr)) {
        CHECK_NMSTRT_CASES(enc, ptr, end, nextTokPtr)
    default:
        *nextTokPtr = ptr;
        return XML_TOK_INVALID;
    }
    while (HAS_CHAR(enc, ptr, end)) {
        switch (BYTE_TYPE(enc, ptr)) {
            CHECK_NAME_CASES(enc, ptr, end, nextTokPtr)
        case BT_S:
        case BT_CR:
        case BT_LF:
            if (!checkPiTarget<E>(enc, target, ptr, &tok)) {
                *nextTokPtr = ptr;
                return XML_TOK_INVALID;
            }
            ptr += MINBPC(enc);
            while (HAS_CHAR(enc, ptr, end)) {
                switch (BYTE_TYPE(enc, ptr)) {
                    INVALID_CASES(ptr, nextTokPtr)
                case BT_QUEST:
                    ptr += MINBPC(enc);
                    REQUIRE_CHAR(enc, ptr, end);
                    if (CHAR_MATCHES(enc, ptr, ASCII_GT)) {
                        *nextTokPtr = ptr + MINBPC(enc);
                        return tok;
                    }
                    break;
                default:
                    ptr += MINBPC(enc);
                    break;
                }
            }
            return XML_TOK_PARTIAL;
        case BT_QUEST:
            if (!checkPiTarget<E>(enc, target, ptr, &tok)) {
                *nextTokPtr = ptr;
                return XML_TOK_INVALID;
            }
            ptr += MINBPC(enc);
            REQUIRE_CHAR(enc, ptr, end);
            if (CHAR_MATCHES(enc, ptr, ASCII_GT)) {
                *nextTokPtr = ptr + MINBPC(enc);
                return tok;
            }
            [[fallthrough]];
        default:
            *nextTokPtr = ptr;
            return XML_TOK_INVALID;
        }
    }
    return XML_TOK_PARTIAL;
}

template <class E>
static int scanCdataSection(const ENCODING *enc, const char *ptr, const char *end,
                            const char **nextTokPtr)
{
    static const char CDATA_LSQB[] = { ASCII_C, ASCII_D, ASCII_A, ASCII_T, ASCII_A, ASCII_LSQB };
    int i;
    (void)enc;
    /* CDATA[ */
    REQUIRE_CHARS(enc, ptr, end, 6);
    for (i = 0; i < 6; i++, ptr += MINBPC(enc)) {
        if (!CHAR_MATCHES(enc, ptr, CDATA_LSQB[i])) {
            *nextTokPtr = ptr;
            return XML_TOK_INVALID;
        }
    }
    *nextTokPtr = ptr;
    return XML_TOK_CDATA_SECT_OPEN;
}

template <class E>
static int cdataSectionTok(const ENCODING *enc, const char *ptr, const char *end,
                           const char **nextTokPtr)
{
    if (ptr >= end)
        return XML_TOK_NONE;
    if (MINBPC(enc) > 1) {
        usize n = end - ptr;
        if (n & (MINBPC(enc) - 1)) {
            n &= ~(MINBPC(enc) - 1);
            if (n == 0)
                return XML_TOK_PARTIAL;
            end = ptr + n;
        }
    }
    switch (BYTE_TYPE(enc, ptr)) {
    case BT_RSQB:
        ptr += MINBPC(enc);
        REQUIRE_CHAR(enc, ptr, end);
        if (!CHAR_MATCHES(enc, ptr, ASCII_RSQB))
            break;
        ptr += MINBPC(enc);
        REQUIRE_CHAR(enc, ptr, end);
        if (!CHAR_MATCHES(enc, ptr, ASCII_GT)) {
            ptr -= MINBPC(enc);
            break;
        }
        *nextTokPtr = ptr + MINBPC(enc);
        return XML_TOK_CDATA_SECT_CLOSE;
    case BT_CR:
        ptr += MINBPC(enc);
        REQUIRE_CHAR(enc, ptr, end);
        if (BYTE_TYPE(enc, ptr) == BT_LF)
            ptr += MINBPC(enc);
        *nextTokPtr = ptr;
        return XML_TOK_DATA_NEWLINE;
    case BT_LF:
        *nextTokPtr = ptr + MINBPC(enc);
        return XML_TOK_DATA_NEWLINE;
        INVALID_CASES(ptr, nextTokPtr)
    default:
        ptr += MINBPC(enc);
        break;
    }
    while (HAS_CHAR(enc, ptr, end)) {
        switch (BYTE_TYPE(enc, ptr)) {
#define LEAD_CASE(n)                                         \
    case BT_LEAD##n:                                         \
        if (end - ptr < n || IS_INVALID_CHAR(enc, ptr, n)) { \
            *nextTokPtr = ptr;                               \
            return XML_TOK_DATA_CHARS;                       \
        }                                                    \
        ptr += n;                                            \
        break;
            LEAD_CASE(2)
            LEAD_CASE(3)
            LEAD_CASE(4)
#undef LEAD_CASE
        case BT_NONXML:
        case BT_MALFORM:
        case BT_TRAIL:
        case BT_CR:
        case BT_LF:
        case BT_RSQB:
            *nextTokPtr = ptr;
            return XML_TOK_DATA_CHARS;
        default:
            ptr += MINBPC(enc);
            break;
        }
    }
    *nextTokPtr = ptr;
    return XML_TOK_DATA_CHARS;
}

/* ptr points to character following "</" */

template <class E>
static int scanEndTag(const ENCODING *enc, const char *ptr, const char *end,
                      const char **nextTokPtr)
{
    REQUIRE_CHAR(enc, ptr, end);
    switch (BYTE_TYPE(enc, ptr)) {
        CHECK_NMSTRT_CASES(enc, ptr, end, nextTokPtr)
    default:
        *nextTokPtr = ptr;
        return XML_TOK_INVALID;
    }
    while (HAS_CHAR(enc, ptr, end)) {
        switch (BYTE_TYPE(enc, ptr)) {
            CHECK_NAME_CASES(enc, ptr, end, nextTokPtr)
        case BT_S:
        case BT_CR:
        case BT_LF:
            for (ptr += MINBPC(enc); HAS_CHAR(enc, ptr, end); ptr += MINBPC(enc)) {
                switch (BYTE_TYPE(enc, ptr)) {
                case BT_S:
                case BT_CR:
                case BT_LF:
                    break;
                case BT_GT:
                    *nextTokPtr = ptr + MINBPC(enc);
                    return XML_TOK_END_TAG;
                default:
                    *nextTokPtr = ptr;
                    return XML_TOK_INVALID;
                }
            }
            return XML_TOK_PARTIAL;
#ifdef XML_NS
        case BT_COLON:
            /* no need to check qname syntax here,
               since end-tag must match exactly */
            ptr += MINBPC(enc);
            break;
#endif
        case BT_GT:
            *nextTokPtr = ptr + MINBPC(enc);
            return XML_TOK_END_TAG;
        default:
            *nextTokPtr = ptr;
            return XML_TOK_INVALID;
        }
    }
    return XML_TOK_PARTIAL;
}

/* ptr points to character following "&#X" */

template <class E>
static int scanHexCharRef(const ENCODING *enc, const char *ptr, const char *end,
                          const char **nextTokPtr)
{
    if (HAS_CHAR(enc, ptr, end)) {
        switch (BYTE_TYPE(enc, ptr)) {
        case BT_DIGIT:
        case BT_HEX:
            break;
        default:
            *nextTokPtr = ptr;
            return XML_TOK_INVALID;
        }
        for (ptr += MINBPC(enc); HAS_CHAR(enc, ptr, end); ptr += MINBPC(enc)) {
            switch (BYTE_TYPE(enc, ptr)) {
            case BT_DIGIT:
            case BT_HEX:
                break;
            case BT_SEMI:
                *nextTokPtr = ptr + MINBPC(enc);
                return XML_TOK_CHAR_REF;
            default:
                *nextTokPtr = ptr;
                return XML_TOK_INVALID;
            }
        }
    }
    return XML_TOK_PARTIAL;
}

/* ptr points to character following "&#" */

template <class E>
static int scanCharRef(const ENCODING *enc, const char *ptr, const char *end,
                       const char **nextTokPtr)
{
    if (HAS_CHAR(enc, ptr, end)) {
        if (CHAR_MATCHES(enc, ptr, ASCII_x))
            return scanHexCharRef<E>(enc, ptr + MINBPC(enc), end, nextTokPtr);
        switch (BYTE_TYPE(enc, ptr)) {
        case BT_DIGIT:
            break;
        default:
            *nextTokPtr = ptr;
            return XML_TOK_INVALID;
        }
        for (ptr += MINBPC(enc); HAS_CHAR(enc, ptr, end); ptr += MINBPC(enc)) {
            switch (BYTE_TYPE(enc, ptr)) {
            case BT_DIGIT:
                break;
            case BT_SEMI:
                *nextTokPtr = ptr + MINBPC(enc);
                return XML_TOK_CHAR_REF;
            default:
                *nextTokPtr = ptr;
                return XML_TOK_INVALID;
            }
        }
    }
    return XML_TOK_PARTIAL;
}

/* ptr points to character following "&" */

template <class E>
static int scanRef(const ENCODING *enc, const char *ptr, const char *end, const char **nextTokPtr)
{
    REQUIRE_CHAR(enc, ptr, end);
    switch (BYTE_TYPE(enc, ptr)) {
        CHECK_NMSTRT_CASES(enc, ptr, end, nextTokPtr)
    case BT_NUM:
        return scanCharRef<E>(enc, ptr + MINBPC(enc), end, nextTokPtr);
    default:
        *nextTokPtr = ptr;
        return XML_TOK_INVALID;
    }
    while (HAS_CHAR(enc, ptr, end)) {
        switch (BYTE_TYPE(enc, ptr)) {
            CHECK_NAME_CASES(enc, ptr, end, nextTokPtr)
        case BT_SEMI:
            *nextTokPtr = ptr + MINBPC(enc);
            return XML_TOK_ENTITY_REF;
        default:
            *nextTokPtr = ptr;
            return XML_TOK_INVALID;
        }
    }
    return XML_TOK_PARTIAL;
}

/* ptr points to character following first character of attribute name */

template <class E>
static int scanAtts(const ENCODING *enc, const char *ptr, const char *end, const char **nextTokPtr)
{
#ifdef XML_NS
    int hadColon = 0;
#endif
    while (HAS_CHAR(enc, ptr, end)) {
        switch (BYTE_TYPE(enc, ptr)) {
            CHECK_NAME_CASES(enc, ptr, end, nextTokPtr)
#ifdef XML_NS
        case BT_COLON:
            if (hadColon) {
                *nextTokPtr = ptr;
                return XML_TOK_INVALID;
            }
            hadColon = 1;
            ptr += MINBPC(enc);
            REQUIRE_CHAR(enc, ptr, end);
            switch (BYTE_TYPE(enc, ptr)) {
                CHECK_NMSTRT_CASES(enc, ptr, end, nextTokPtr)
            default:
                *nextTokPtr = ptr;
                return XML_TOK_INVALID;
            }
            break;
#endif
        case BT_S:
        case BT_CR:
        case BT_LF:
            for (;;) {
                int t;

                ptr += MINBPC(enc);
                REQUIRE_CHAR(enc, ptr, end);
                t = BYTE_TYPE(enc, ptr);
                if (t == BT_EQUALS)
                    break;
                switch (t) {
                case BT_S:
                case BT_LF:
                case BT_CR:
                    break;
                default:
                    *nextTokPtr = ptr;
                    return XML_TOK_INVALID;
                }
            }
            [[fallthrough]];
        case BT_EQUALS: {
            int open;
#ifdef XML_NS
            hadColon = 0;
#endif
            for (;;) {
                ptr += MINBPC(enc);
                REQUIRE_CHAR(enc, ptr, end);
                open = BYTE_TYPE(enc, ptr);
                if (open == BT_QUOT || open == BT_APOS)
                    break;
                switch (open) {
                case BT_S:
                case BT_LF:
                case BT_CR:
                    break;
                default:
                    *nextTokPtr = ptr;
                    return XML_TOK_INVALID;
                }
            }
            ptr += MINBPC(enc);
            /* in attribute value */
            for (;;) {
                int t;
                REQUIRE_CHAR(enc, ptr, end);
                t = BYTE_TYPE(enc, ptr);
                if (t == open)
                    break;
                switch (t) {
                    INVALID_CASES(ptr, nextTokPtr)
                case BT_AMP: {
                    int tok = scanRef<E>(enc, ptr + MINBPC(enc), end, &ptr);
                    if (tok <= 0) {
                        if (tok == XML_TOK_INVALID)
                            *nextTokPtr = ptr;
                        return tok;
                    }
                    break;
                }
                case BT_LT:
                    *nextTokPtr = ptr;
                    return XML_TOK_INVALID;
                default:
                    ptr += MINBPC(enc);
                    break;
                }
            }
            ptr += MINBPC(enc);
            REQUIRE_CHAR(enc, ptr, end);
            switch (BYTE_TYPE(enc, ptr)) {
            case BT_S:
            case BT_CR:
            case BT_LF:
                break;
            case BT_SOL:
                goto sol;
            case BT_GT:
                goto gt;
            default:
                *nextTokPtr = ptr;
                return XML_TOK_INVALID;
            }
            /* ptr points to closing quote */
            for (;;) {
                ptr += MINBPC(enc);
                REQUIRE_CHAR(enc, ptr, end);
                switch (BYTE_TYPE(enc, ptr)) {
                    CHECK_NMSTRT_CASES(enc, ptr, end, nextTokPtr)
                case BT_S:
                case BT_CR:
                case BT_LF:
                    continue;
                case BT_GT:
                gt:
                    *nextTokPtr = ptr + MINBPC(enc);
                    return XML_TOK_START_TAG_WITH_ATTS;
                case BT_SOL:
                sol:
                    ptr += MINBPC(enc);
                    REQUIRE_CHAR(enc, ptr, end);
                    if (!CHAR_MATCHES(enc, ptr, ASCII_GT)) {
                        *nextTokPtr = ptr;
                        return XML_TOK_INVALID;
                    }
                    *nextTokPtr = ptr + MINBPC(enc);
                    return XML_TOK_EMPTY_ELEMENT_WITH_ATTS;
                default:
                    *nextTokPtr = ptr;
                    return XML_TOK_INVALID;
                }
                break;
            }
            break;
        }
        default:
            *nextTokPtr = ptr;
            return XML_TOK_INVALID;
        }
    }
    return XML_TOK_PARTIAL;
}

/* ptr points to character following "<" */

template <class E>
static int scanLt(const ENCODING *enc, const char *ptr, const char *end, const char **nextTokPtr)
{
#ifdef XML_NS
    int hadColon;
#endif
    REQUIRE_CHAR(enc, ptr, end);
    switch (BYTE_TYPE(enc, ptr)) {
        CHECK_NMSTRT_CASES(enc, ptr, end, nextTokPtr)
    case BT_EXCL:
        ptr += MINBPC(enc);
        REQUIRE_CHAR(enc, ptr, end);
        switch (BYTE_TYPE(enc, ptr)) {
        case BT_MINUS:
            return scanComment<E>(enc, ptr + MINBPC(enc), end, nextTokPtr);
        case BT_LSQB:
            return scanCdataSection<E>(enc, ptr + MINBPC(enc), end, nextTokPtr);
        }
        *nextTokPtr = ptr;
        return XML_TOK_INVALID;
    case BT_QUEST:
        return scanPi<E>(enc, ptr + MINBPC(enc), end, nextTokPtr);
    case BT_SOL:
        return scanEndTag<E>(enc, ptr + MINBPC(enc), end, nextTokPtr);
    default:
        *nextTokPtr = ptr;
        return XML_TOK_INVALID;
    }
#ifdef XML_NS
    hadColon = 0;
#endif
    /* we have a start-tag */
    while (HAS_CHAR(enc, ptr, end)) {
        switch (BYTE_TYPE(enc, ptr)) {
            CHECK_NAME_CASES(enc, ptr, end, nextTokPtr)
#ifdef XML_NS
        case BT_COLON:
            if (hadColon) {
                *nextTokPtr = ptr;
                return XML_TOK_INVALID;
            }
            hadColon = 1;
            ptr += MINBPC(enc);
            REQUIRE_CHAR(enc, ptr, end);
            switch (BYTE_TYPE(enc, ptr)) {
                CHECK_NMSTRT_CASES(enc, ptr, end, nextTokPtr)
            default:
                *nextTokPtr = ptr;
                return XML_TOK_INVALID;
            }
            break;
#endif
        case BT_S:
        case BT_CR:
        case BT_LF: {
            ptr += MINBPC(enc);
            while (HAS_CHAR(enc, ptr, end)) {
                switch (BYTE_TYPE(enc, ptr)) {
                    CHECK_NMSTRT_CASES(enc, ptr, end, nextTokPtr)
                case BT_GT:
                    goto gt;
                case BT_SOL:
                    goto sol;
                case BT_S:
                case BT_CR:
                case BT_LF:
                    ptr += MINBPC(enc);
                    continue;
                default:
                    *nextTokPtr = ptr;
                    return XML_TOK_INVALID;
                }
                return scanAtts<E>(enc, ptr, end, nextTokPtr);
            }
            return XML_TOK_PARTIAL;
        }
        case BT_GT:
        gt:
            *nextTokPtr = ptr + MINBPC(enc);
            return XML_TOK_START_TAG_NO_ATTS;
        case BT_SOL:
        sol:
            ptr += MINBPC(enc);
            REQUIRE_CHAR(enc, ptr, end);
            if (!CHAR_MATCHES(enc, ptr, ASCII_GT)) {
                *nextTokPtr = ptr;
                return XML_TOK_INVALID;
            }
            *nextTokPtr = ptr + MINBPC(enc);
            return XML_TOK_EMPTY_ELEMENT_NO_ATTS;
        default:
            *nextTokPtr = ptr;
            return XML_TOK_INVALID;
        }
    }
    return XML_TOK_PARTIAL;
}

template <class E>
static int contentTok(const ENCODING *enc, const char *ptr, const char *end,
                      const char **nextTokPtr)
{
    if (ptr >= end)
        return XML_TOK_NONE;
    if (MINBPC(enc) > 1) {
        usize n = end - ptr;
        if (n & (MINBPC(enc) - 1)) {
            n &= ~(MINBPC(enc) - 1);
            if (n == 0)
                return XML_TOK_PARTIAL;
            end = ptr + n;
        }
    }
    switch (BYTE_TYPE(enc, ptr)) {
    case BT_LT:
        return scanLt<E>(enc, ptr + MINBPC(enc), end, nextTokPtr);
    case BT_AMP:
        return scanRef<E>(enc, ptr + MINBPC(enc), end, nextTokPtr);
    case BT_CR:
        ptr += MINBPC(enc);
        if (!HAS_CHAR(enc, ptr, end))
            return XML_TOK_TRAILING_CR;
        if (BYTE_TYPE(enc, ptr) == BT_LF)
            ptr += MINBPC(enc);
        *nextTokPtr = ptr;
        return XML_TOK_DATA_NEWLINE;
    case BT_LF:
        *nextTokPtr = ptr + MINBPC(enc);
        return XML_TOK_DATA_NEWLINE;
    case BT_RSQB:
        ptr += MINBPC(enc);
        if (!HAS_CHAR(enc, ptr, end))
            return XML_TOK_TRAILING_RSQB;
        if (!CHAR_MATCHES(enc, ptr, ASCII_RSQB))
            break;
        ptr += MINBPC(enc);
        if (!HAS_CHAR(enc, ptr, end))
            return XML_TOK_TRAILING_RSQB;
        if (!CHAR_MATCHES(enc, ptr, ASCII_GT)) {
            ptr -= MINBPC(enc);
            break;
        }
        *nextTokPtr = ptr;
        return XML_TOK_INVALID;
        INVALID_CASES(ptr, nextTokPtr)
    default:
        ptr += MINBPC(enc);
        break;
    }
    while (HAS_CHAR(enc, ptr, end)) {
        switch (BYTE_TYPE(enc, ptr)) {
#define LEAD_CASE(n)                                         \
    case BT_LEAD##n:                                         \
        if (end - ptr < n || IS_INVALID_CHAR(enc, ptr, n)) { \
            *nextTokPtr = ptr;                               \
            return XML_TOK_DATA_CHARS;                       \
        }                                                    \
        ptr += n;                                            \
        break;
            LEAD_CASE(2)
            LEAD_CASE(3)
            LEAD_CASE(4)
#undef LEAD_CASE
        case BT_RSQB:
            if (HAS_CHARS(enc, ptr, end, 2)) {
                if (!CHAR_MATCHES(enc, ptr + MINBPC(enc), ASCII_RSQB)) {
                    ptr += MINBPC(enc);
                    break;
                }
                if (HAS_CHARS(enc, ptr, end, 3)) {
                    if (!CHAR_MATCHES(enc, ptr + 2 * MINBPC(enc), ASCII_GT)) {
                        ptr += MINBPC(enc);
                        break;
                    }
                    *nextTokPtr = ptr + 2 * MINBPC(enc);
                    return XML_TOK_INVALID;
                }
            }
            [[fallthrough]];
        case BT_AMP:
        case BT_LT:
        case BT_NONXML:
        case BT_MALFORM:
        case BT_TRAIL:
        case BT_CR:
        case BT_LF:
            *nextTokPtr = ptr;
            return XML_TOK_DATA_CHARS;
        default:
            ptr += MINBPC(enc);
            break;
        }
    }
    *nextTokPtr = ptr;
    return XML_TOK_DATA_CHARS;
}

/* ptr points to character following "%" */

template <class E>
static int scanPercent(const ENCODING *enc, const char *ptr, const char *end,
                       const char **nextTokPtr)
{
    REQUIRE_CHAR(enc, ptr, end);
    switch (BYTE_TYPE(enc, ptr)) {
        CHECK_NMSTRT_CASES(enc, ptr, end, nextTokPtr)
    case BT_S:
    case BT_LF:
    case BT_CR:
    case BT_PERCNT:
        *nextTokPtr = ptr;
        return XML_TOK_PERCENT;
    default:
        *nextTokPtr = ptr;
        return XML_TOK_INVALID;
    }
    while (HAS_CHAR(enc, ptr, end)) {
        switch (BYTE_TYPE(enc, ptr)) {
            CHECK_NAME_CASES(enc, ptr, end, nextTokPtr)
        case BT_SEMI:
            *nextTokPtr = ptr + MINBPC(enc);
            return XML_TOK_PARAM_ENTITY_REF;
        default:
            *nextTokPtr = ptr;
            return XML_TOK_INVALID;
        }
    }
    return XML_TOK_PARTIAL;
}

template <class E>
static int scanPoundName(const ENCODING *enc, const char *ptr, const char *end,
                         const char **nextTokPtr)
{
    REQUIRE_CHAR(enc, ptr, end);
    switch (BYTE_TYPE(enc, ptr)) {
        CHECK_NMSTRT_CASES(enc, ptr, end, nextTokPtr)
    default:
        *nextTokPtr = ptr;
        return XML_TOK_INVALID;
    }
    while (HAS_CHAR(enc, ptr, end)) {
        switch (BYTE_TYPE(enc, ptr)) {
            CHECK_NAME_CASES(enc, ptr, end, nextTokPtr)
        case BT_CR:
        case BT_LF:
        case BT_S:
        case BT_RPAR:
        case BT_GT:
        case BT_PERCNT:
        case BT_VERBAR:
            *nextTokPtr = ptr;
            return XML_TOK_POUND_NAME;
        default:
            *nextTokPtr = ptr;
            return XML_TOK_INVALID;
        }
    }
    return -XML_TOK_POUND_NAME;
}

template <class E>
static int scanLit(int open, const ENCODING *enc, const char *ptr, const char *end,
                   const char **nextTokPtr)
{
    while (HAS_CHAR(enc, ptr, end)) {
        int t = BYTE_TYPE(enc, ptr);
        switch (t) {
            INVALID_CASES(ptr, nextTokPtr)
        case BT_QUOT:
        case BT_APOS:
            ptr += MINBPC(enc);
            if (t != open)
                break;
            if (!HAS_CHAR(enc, ptr, end))
                return -XML_TOK_LITERAL;
            *nextTokPtr = ptr;
            switch (BYTE_TYPE(enc, ptr)) {
            case BT_S:
            case BT_CR:
            case BT_LF:
            case BT_GT:
            case BT_PERCNT:
            case BT_LSQB:
                return XML_TOK_LITERAL;
            default:
                return XML_TOK_INVALID;
            }
        default:
            ptr += MINBPC(enc);
            break;
        }
    }
    return XML_TOK_PARTIAL;
}

template <class E>
static int prologTok(const ENCODING *enc, const char *ptr, const char *end, const char **nextTokPtr)
{
    int tok;
    if (ptr >= end)
        return XML_TOK_NONE;
    if (MINBPC(enc) > 1) {
        usize n = end - ptr;
        if (n & (MINBPC(enc) - 1)) {
            n &= ~(MINBPC(enc) - 1);
            if (n == 0)
                return XML_TOK_PARTIAL;
            end = ptr + n;
        }
    }
    switch (BYTE_TYPE(enc, ptr)) {
    case BT_QUOT:
        return scanLit<E>(BT_QUOT, enc, ptr + MINBPC(enc), end, nextTokPtr);
    case BT_APOS:
        return scanLit<E>(BT_APOS, enc, ptr + MINBPC(enc), end, nextTokPtr);
    case BT_LT: {
        ptr += MINBPC(enc);
        REQUIRE_CHAR(enc, ptr, end);
        switch (BYTE_TYPE(enc, ptr)) {
        case BT_EXCL:
            return scanDecl<E>(enc, ptr + MINBPC(enc), end, nextTokPtr);
        case BT_QUEST:
            return scanPi<E>(enc, ptr + MINBPC(enc), end, nextTokPtr);
        case BT_NMSTRT:
        case BT_HEX:
        case BT_NONASCII:
        case BT_LEAD2:
        case BT_LEAD3:
        case BT_LEAD4:
            *nextTokPtr = ptr - MINBPC(enc);
            return XML_TOK_INSTANCE_START;
        }
        *nextTokPtr = ptr;
        return XML_TOK_INVALID;
    }
    case BT_CR:
        if (ptr + MINBPC(enc) == end) {
            *nextTokPtr = end;
            /* indicate that this might be part of a CR/LF pair */
            return -XML_TOK_PROLOG_S;
        }
        [[fallthrough]];
    case BT_S:
    case BT_LF:
        for (;;) {
            ptr += MINBPC(enc);
            if (!HAS_CHAR(enc, ptr, end))
                break;
            switch (BYTE_TYPE(enc, ptr)) {
            case BT_S:
            case BT_LF:
                break;
            case BT_CR:
                /* don't split CR/LF pair */
                if (ptr + MINBPC(enc) != end)
                    break;
                [[fallthrough]];
            default:
                *nextTokPtr = ptr;
                return XML_TOK_PROLOG_S;
            }
        }
        *nextTokPtr = ptr;
        return XML_TOK_PROLOG_S;
    case BT_PERCNT:
        return scanPercent<E>(enc, ptr + MINBPC(enc), end, nextTokPtr);
    case BT_COMMA:
        *nextTokPtr = ptr + MINBPC(enc);
        return XML_TOK_COMMA;
    case BT_LSQB:
        *nextTokPtr = ptr + MINBPC(enc);
        return XML_TOK_OPEN_BRACKET;
    case BT_RSQB:
        ptr += MINBPC(enc);
        if (!HAS_CHAR(enc, ptr, end))
            return -XML_TOK_CLOSE_BRACKET;
        if (CHAR_MATCHES(enc, ptr, ASCII_RSQB)) {
            REQUIRE_CHARS(enc, ptr, end, 2);
            if (CHAR_MATCHES(enc, ptr + MINBPC(enc), ASCII_GT)) {
                *nextTokPtr = ptr + 2 * MINBPC(enc);
                return XML_TOK_COND_SECT_CLOSE;
            }
        }
        *nextTokPtr = ptr;
        return XML_TOK_CLOSE_BRACKET;
    case BT_LPAR:
        *nextTokPtr = ptr + MINBPC(enc);
        return XML_TOK_OPEN_PAREN;
    case BT_RPAR:
        ptr += MINBPC(enc);
        if (!HAS_CHAR(enc, ptr, end))
            return -XML_TOK_CLOSE_PAREN;
        switch (BYTE_TYPE(enc, ptr)) {
        case BT_AST:
            *nextTokPtr = ptr + MINBPC(enc);
            return XML_TOK_CLOSE_PAREN_ASTERISK;
        case BT_QUEST:
            *nextTokPtr = ptr + MINBPC(enc);
            return XML_TOK_CLOSE_PAREN_QUESTION;
        case BT_PLUS:
            *nextTokPtr = ptr + MINBPC(enc);
            return XML_TOK_CLOSE_PAREN_PLUS;
        case BT_CR:
        case BT_LF:
        case BT_S:
        case BT_GT:
        case BT_COMMA:
        case BT_VERBAR:
        case BT_RPAR:
            *nextTokPtr = ptr;
            return XML_TOK_CLOSE_PAREN;
        }
        *nextTokPtr = ptr;
        return XML_TOK_INVALID;
    case BT_VERBAR:
        *nextTokPtr = ptr + MINBPC(enc);
        return XML_TOK_OR;
    case BT_GT:
        *nextTokPtr = ptr + MINBPC(enc);
        return XML_TOK_DECL_CLOSE;
    case BT_NUM:
        return scanPoundName<E>(enc, ptr + MINBPC(enc), end, nextTokPtr);
#define LEAD_CASE(n)                        \
    case BT_LEAD##n:                        \
        if (end - ptr < n)                  \
            return XML_TOK_PARTIAL_CHAR;    \
        if (IS_INVALID_CHAR(enc, ptr, n)) { \
            *nextTokPtr = ptr;              \
            return XML_TOK_INVALID;         \
        }                                   \
        if (IS_NMSTRT_CHAR(enc, ptr, n)) {  \
            ptr += n;                       \
            tok = XML_TOK_NAME;             \
            break;                          \
        }                                   \
        if (IS_NAME_CHAR(enc, ptr, n)) {    \
            ptr += n;                       \
            tok = XML_TOK_NMTOKEN;          \
            break;                          \
        }                                   \
        *nextTokPtr = ptr;                  \
        return XML_TOK_INVALID;
        LEAD_CASE(2)
        LEAD_CASE(3)
        LEAD_CASE(4)
#undef LEAD_CASE
    case BT_NMSTRT:
    case BT_HEX:
        tok = XML_TOK_NAME;
        ptr += MINBPC(enc);
        break;
    case BT_DIGIT:
    case BT_NAME:
    case BT_MINUS:
#ifdef XML_NS
    case BT_COLON:
#endif
        tok = XML_TOK_NMTOKEN;
        ptr += MINBPC(enc);
        break;
    case BT_NONASCII:
        if (IS_NMSTRT_CHAR_MINBPC(enc, ptr)) {
            ptr += MINBPC(enc);
            tok = XML_TOK_NAME;
            break;
        }
        if (IS_NAME_CHAR_MINBPC(enc, ptr)) {
            ptr += MINBPC(enc);
            tok = XML_TOK_NMTOKEN;
            break;
        }
        [[fallthrough]];
    default:
        *nextTokPtr = ptr;
        return XML_TOK_INVALID;
    }
    while (HAS_CHAR(enc, ptr, end)) {
        switch (BYTE_TYPE(enc, ptr)) {
            CHECK_NAME_CASES(enc, ptr, end, nextTokPtr)
        case BT_GT:
        case BT_RPAR:
        case BT_COMMA:
        case BT_VERBAR:
        case BT_LSQB:
        case BT_PERCNT:
        case BT_S:
        case BT_CR:
        case BT_LF:
            *nextTokPtr = ptr;
            return tok;
#ifdef XML_NS
        case BT_COLON:
            ptr += MINBPC(enc);
            switch (tok) {
            case XML_TOK_NAME:
                REQUIRE_CHAR(enc, ptr, end);
                tok = XML_TOK_PREFIXED_NAME;
                switch (BYTE_TYPE(enc, ptr)) {
                    CHECK_NAME_CASES(enc, ptr, end, nextTokPtr)
                default:
                    tok = XML_TOK_NMTOKEN;
                    break;
                }
                break;
            case XML_TOK_PREFIXED_NAME:
                tok = XML_TOK_NMTOKEN;
                break;
            }
            break;
#endif
        case BT_PLUS:
            if (tok == XML_TOK_NMTOKEN) {
                *nextTokPtr = ptr;
                return XML_TOK_INVALID;
            }
            *nextTokPtr = ptr + MINBPC(enc);
            return XML_TOK_NAME_PLUS;
        case BT_AST:
            if (tok == XML_TOK_NMTOKEN) {
                *nextTokPtr = ptr;
                return XML_TOK_INVALID;
            }
            *nextTokPtr = ptr + MINBPC(enc);
            return XML_TOK_NAME_ASTERISK;
        case BT_QUEST:
            if (tok == XML_TOK_NMTOKEN) {
                *nextTokPtr = ptr;
                return XML_TOK_INVALID;
            }
            *nextTokPtr = ptr + MINBPC(enc);
            return XML_TOK_NAME_QUESTION;
        default:
            *nextTokPtr = ptr;
            return XML_TOK_INVALID;
        }
    }
    return -tok;
}

template <class E>
static int attributeValueTok(const ENCODING *enc, const char *ptr, const char *end,
                             const char **nextTokPtr)
{
    const char *start;
    if (ptr >= end)
        return XML_TOK_NONE;
    else if (!HAS_CHAR(enc, ptr, end)) {
        /* This line cannot be executed.  The incoming data has already
         * been tokenized once, so incomplete characters like this have
         * already been eliminated from the input.  Retaining the paranoia
         * check is still valuable, however.
         */
        return XML_TOK_PARTIAL; /* LCOV_EXCL_LINE */
    }
    start = ptr;
    while (HAS_CHAR(enc, ptr, end)) {
        switch (BYTE_TYPE(enc, ptr)) {
#define LEAD_CASE(n)                                                   \
    case BT_LEAD##n:                                                   \
        ptr += n; /* NOTE: The encoding has already been validated. */ \
        break;
            LEAD_CASE(2)
            LEAD_CASE(3)
            LEAD_CASE(4)
#undef LEAD_CASE
        case BT_AMP:
            if (ptr == start)
                return scanRef<E>(enc, ptr + MINBPC(enc), end, nextTokPtr);
            *nextTokPtr = ptr;
            return XML_TOK_DATA_CHARS;
        case BT_LT:
            /* this is for inside entity references */
            *nextTokPtr = ptr;
            return XML_TOK_INVALID;
        case BT_LF:
            if (ptr == start) {
                *nextTokPtr = ptr + MINBPC(enc);
                return XML_TOK_DATA_NEWLINE;
            }
            *nextTokPtr = ptr;
            return XML_TOK_DATA_CHARS;
        case BT_CR:
            if (ptr == start) {
                ptr += MINBPC(enc);
                if (!HAS_CHAR(enc, ptr, end))
                    return XML_TOK_TRAILING_CR;
                if (BYTE_TYPE(enc, ptr) == BT_LF)
                    ptr += MINBPC(enc);
                *nextTokPtr = ptr;
                return XML_TOK_DATA_NEWLINE;
            }
            *nextTokPtr = ptr;
            return XML_TOK_DATA_CHARS;
        case BT_S:
            if (ptr == start) {
                *nextTokPtr = ptr + MINBPC(enc);
                return XML_TOK_ATTRIBUTE_VALUE_S;
            }
            *nextTokPtr = ptr;
            return XML_TOK_DATA_CHARS;
        default:
            ptr += MINBPC(enc);
            break;
        }
    }
    *nextTokPtr = ptr;
    return XML_TOK_DATA_CHARS;
}

template <class E>
static int entityValueTok(const ENCODING *enc, const char *ptr, const char *end,
                          const char **nextTokPtr)
{
    const char *start;
    if (ptr >= end)
        return XML_TOK_NONE;
    else if (!HAS_CHAR(enc, ptr, end)) {
        /* This line cannot be executed.  The incoming data has already
         * been tokenized once, so incomplete characters like this have
         * already been eliminated from the input.  Retaining the paranoia
         * check is still valuable, however.
         */
        return XML_TOK_PARTIAL; /* LCOV_EXCL_LINE */
    }
    start = ptr;
    while (HAS_CHAR(enc, ptr, end)) {
        switch (BYTE_TYPE(enc, ptr)) {
#define LEAD_CASE(n)                                                   \
    case BT_LEAD##n:                                                   \
        ptr += n; /* NOTE: The encoding has already been validated. */ \
        break;
            LEAD_CASE(2)
            LEAD_CASE(3)
            LEAD_CASE(4)
#undef LEAD_CASE
        case BT_AMP:
            if (ptr == start)
                return scanRef<E>(enc, ptr + MINBPC(enc), end, nextTokPtr);
            *nextTokPtr = ptr;
            return XML_TOK_DATA_CHARS;
        case BT_PERCNT:
            if (ptr == start) {
                int tok = scanPercent<E>(enc, ptr + MINBPC(enc), end, nextTokPtr);
                return (tok == XML_TOK_PERCENT) ? XML_TOK_INVALID : tok;
            }
            *nextTokPtr = ptr;
            return XML_TOK_DATA_CHARS;
        case BT_LF:
            if (ptr == start) {
                *nextTokPtr = ptr + MINBPC(enc);
                return XML_TOK_DATA_NEWLINE;
            }
            *nextTokPtr = ptr;
            return XML_TOK_DATA_CHARS;
        case BT_CR:
            if (ptr == start) {
                ptr += MINBPC(enc);
                if (!HAS_CHAR(enc, ptr, end))
                    return XML_TOK_TRAILING_CR;
                if (BYTE_TYPE(enc, ptr) == BT_LF)
                    ptr += MINBPC(enc);
                *nextTokPtr = ptr;
                return XML_TOK_DATA_NEWLINE;
            }
            *nextTokPtr = ptr;
            return XML_TOK_DATA_CHARS;
        default:
            ptr += MINBPC(enc);
            break;
        }
    }
    *nextTokPtr = ptr;
    return XML_TOK_DATA_CHARS;
}

#ifdef XML_DTD

template <class E>
static int ignoreSectionTok(const ENCODING *enc, const char *ptr, const char *end,
                            const char **nextTokPtr)
{
    int level = 0;
    if (MINBPC(enc) > 1) {
        usize n = end - ptr;
        if (n & (MINBPC(enc) - 1)) {
            n &= ~(MINBPC(enc) - 1);
            end = ptr + n;
        }
    }
    while (HAS_CHAR(enc, ptr, end)) {
        switch (BYTE_TYPE(enc, ptr)) {
            INVALID_CASES(ptr, nextTokPtr)
        case BT_LT:
            ptr += MINBPC(enc);
            REQUIRE_CHAR(enc, ptr, end);
            if (CHAR_MATCHES(enc, ptr, ASCII_EXCL)) {
                ptr += MINBPC(enc);
                REQUIRE_CHAR(enc, ptr, end);
                if (CHAR_MATCHES(enc, ptr, ASCII_LSQB)) {
                    ++level;
                    ptr += MINBPC(enc);
                }
            }
            break;
        case BT_RSQB:
            ptr += MINBPC(enc);
            REQUIRE_CHAR(enc, ptr, end);
            if (CHAR_MATCHES(enc, ptr, ASCII_RSQB)) {
                ptr += MINBPC(enc);
                REQUIRE_CHAR(enc, ptr, end);
                if (CHAR_MATCHES(enc, ptr, ASCII_GT)) {
                    ptr += MINBPC(enc);
                    if (level == 0) {
                        *nextTokPtr = ptr;
                        return XML_TOK_IGNORE_SECT;
                    }
                    --level;
                }
            }
            break;
        default:
            ptr += MINBPC(enc);
            break;
        }
    }
    return XML_TOK_PARTIAL;
}

#endif /* XML_DTD */

template <class E>
static int isPublicId(const ENCODING *enc, const char *ptr, const char *end, const char **badPtr)
{
    ptr += MINBPC(enc);
    end -= MINBPC(enc);
    for (; HAS_CHAR(enc, ptr, end); ptr += MINBPC(enc)) {
        switch (BYTE_TYPE(enc, ptr)) {
        case BT_DIGIT:
        case BT_HEX:
        case BT_MINUS:
        case BT_APOS:
        case BT_LPAR:
        case BT_RPAR:
        case BT_PLUS:
        case BT_COMMA:
        case BT_SOL:
        case BT_EQUALS:
        case BT_QUEST:
        case BT_CR:
        case BT_LF:
        case BT_SEMI:
        case BT_EXCL:
        case BT_AST:
        case BT_PERCNT:
        case BT_NUM:
#ifdef XML_NS
        case BT_COLON:
#endif
            break;
        case BT_S:
            if (CHAR_MATCHES(enc, ptr, ASCII_TAB)) {
                *badPtr = ptr;
                return 0;
            }
            break;
        case BT_NAME:
        case BT_NMSTRT:
            if (!(BYTE_TO_ASCII(enc, ptr) & ~0x7f))
                break;
            [[fallthrough]];
        default:
            switch (BYTE_TO_ASCII(enc, ptr)) {
            case 0x24: /* $ */
            case 0x40: /* @ */
                break;
            default:
                *badPtr = ptr;
                return 0;
            }
            break;
        }
    }
    return 1;
}

/* This must only be called for a well-formed start-tag or empty
   element tag.  Returns the number of attributes.  Pointers to the
   first attsMax attributes are stored in atts.
*/

template <class E>
static int getAtts(const ENCODING *enc, const char *ptr, int attsMax, ATTRIBUTE *atts)
{
    enum { other, inName, inValue } state = inName;
    int nAtts                             = 0;
    int open                              = 0; /* defined when state == inValue;
                                                  initialization just to shut up compilers */

    for (ptr += MINBPC(enc);; ptr += MINBPC(enc)) {
        switch (BYTE_TYPE(enc, ptr)) {
#define START_NAME                        \
    if (state == other) {                 \
        if (nAtts < attsMax) {            \
            atts[nAtts].name       = ptr; \
            atts[nAtts].normalized = 1;   \
        }                                 \
        state = inName;                   \
    }
#define LEAD_CASE(n)                                                      \
    case BT_LEAD##n: /* NOTE: The encoding has already been validated. */ \
        START_NAME ptr += (n - MINBPC(enc));                              \
        break;
            LEAD_CASE(2)
            LEAD_CASE(3)
            LEAD_CASE(4)
#undef LEAD_CASE
        case BT_NONASCII:
        case BT_NMSTRT:
        case BT_HEX:
            START_NAME
            break;
#undef START_NAME
        case BT_QUOT:
            if (state != inValue) {
                if (nAtts < attsMax)
                    atts[nAtts].valuePtr = ptr + MINBPC(enc);
                state = inValue;
                open  = BT_QUOT;
            } else if (open == BT_QUOT) {
                state = other;
                if (nAtts < attsMax)
                    atts[nAtts].valueEnd = ptr;
                nAtts++;
            }
            break;
        case BT_APOS:
            if (state != inValue) {
                if (nAtts < attsMax)
                    atts[nAtts].valuePtr = ptr + MINBPC(enc);
                state = inValue;
                open  = BT_APOS;
            } else if (open == BT_APOS) {
                state = other;
                if (nAtts < attsMax)
                    atts[nAtts].valueEnd = ptr;
                nAtts++;
            }
            break;
        case BT_AMP:
            if (nAtts < attsMax)
                atts[nAtts].normalized = 0;
            break;
        case BT_S:
            if (state == inName)
                state = other;
            else if (state == inValue && nAtts < attsMax && atts[nAtts].normalized &&
                     (ptr == atts[nAtts].valuePtr || BYTE_TO_ASCII(enc, ptr) != ASCII_SPACE ||
                      BYTE_TO_ASCII(enc, ptr + MINBPC(enc)) == ASCII_SPACE ||
                      BYTE_TYPE(enc, ptr + MINBPC(enc)) == open))
                atts[nAtts].normalized = 0;
            break;
        case BT_CR:
        case BT_LF:
            /* This case ensures that the first attribute name is counted
               Apart from that we could just change state on the quote. */
            if (state == inName)
                state = other;
            else if (state == inValue && nAtts < attsMax)
                atts[nAtts].normalized = 0;
            break;
        case BT_GT:
        case BT_SOL:
            if (state != inValue)
                return nAtts;
            break;
        default:
            break;
        }
    }
    /* not reached */
}

template <class E>
static int charRefNumber(const ENCODING *enc, const char *ptr)
{
    int result = 0;
    /* skip &# */
    (void)enc;
    ptr += 2 * MINBPC(enc);
    if (CHAR_MATCHES(enc, ptr, ASCII_x)) {
        for (ptr += MINBPC(enc); !CHAR_MATCHES(enc, ptr, ASCII_SEMI); ptr += MINBPC(enc)) {
            int c = BYTE_TO_ASCII(enc, ptr);
            switch (c) {
            case ASCII_0:
            case ASCII_1:
            case ASCII_2:
            case ASCII_3:
            case ASCII_4:
            case ASCII_5:
            case ASCII_6:
            case ASCII_7:
            case ASCII_8:
            case ASCII_9:
                result <<= 4;
                result |= (c - ASCII_0);
                break;
            case ASCII_A:
            case ASCII_B:
            case ASCII_C:
            case ASCII_D:
            case ASCII_E:
            case ASCII_F:
                result <<= 4;
                result += 10 + (c - ASCII_A);
                break;
            case ASCII_a:
            case ASCII_b:
            case ASCII_c:
            case ASCII_d:
            case ASCII_e:
            case ASCII_f:
                result <<= 4;
                result += 10 + (c - ASCII_a);
                break;
            }
            if (result >= 0x110000)
                return -1;
        }
    } else {
        for (; !CHAR_MATCHES(enc, ptr, ASCII_SEMI); ptr += MINBPC(enc)) {
            int c = BYTE_TO_ASCII(enc, ptr);
            result *= 10;
            result += (c - ASCII_0);
            if (result >= 0x110000)
                return -1;
        }
    }
    return checkCharRefNumber(result);
}

template <class E>
static int predefinedEntityName(const ENCODING *enc, const char *ptr, const char *end)
{
    (void)enc;
    switch ((end - ptr) / MINBPC(enc)) {
    case 2:
        if (CHAR_MATCHES(enc, ptr + MINBPC(enc), ASCII_t)) {
            switch (BYTE_TO_ASCII(enc, ptr)) {
            case ASCII_l:
                return ASCII_LT;
            case ASCII_g:
                return ASCII_GT;
            }
        }
        break;
    case 3:
        if (CHAR_MATCHES(enc, ptr, ASCII_a)) {
            ptr += MINBPC(enc);
            if (CHAR_MATCHES(enc, ptr, ASCII_m)) {
                ptr += MINBPC(enc);
                if (CHAR_MATCHES(enc, ptr, ASCII_p))
                    return ASCII_AMP;
            }
        }
        break;
    case 4:
        switch (BYTE_TO_ASCII(enc, ptr)) {
        case ASCII_q:
            ptr += MINBPC(enc);
            if (CHAR_MATCHES(enc, ptr, ASCII_u)) {
                ptr += MINBPC(enc);
                if (CHAR_MATCHES(enc, ptr, ASCII_o)) {
                    ptr += MINBPC(enc);
                    if (CHAR_MATCHES(enc, ptr, ASCII_t))
                        return ASCII_QUOT;
                }
            }
            break;
        case ASCII_a:
            ptr += MINBPC(enc);
            if (CHAR_MATCHES(enc, ptr, ASCII_p)) {
                ptr += MINBPC(enc);
                if (CHAR_MATCHES(enc, ptr, ASCII_o)) {
                    ptr += MINBPC(enc);
                    if (CHAR_MATCHES(enc, ptr, ASCII_s))
                        return ASCII_APOS;
                }
            }
            break;
        }
    }
    return 0;
}

template <class E>
static int nameMatchesAscii(const ENCODING *enc, const char *ptr1, const char *end1,
                            const char *ptr2)
{
    (void)enc;
    for (; *ptr2; ptr1 += MINBPC(enc), ptr2++) {
        if (end1 - ptr1 < MINBPC(enc)) {
            /* This line cannot be executed.  The incoming data has already
             * been tokenized once, so incomplete characters like this have
             * already been eliminated from the input.  Retaining the
             * paranoia check is still valuable, however.
             */
            return 0; /* LCOV_EXCL_LINE */
        }
        if (!CHAR_MATCHES(enc, ptr1, *ptr2))
            return 0;
    }
    return ptr1 == end1;
}

template <class E>
static int nameLength(const ENCODING *enc, const char *ptr)
{
    const char *start = ptr;
    for (;;) {
        switch (BYTE_TYPE(enc, ptr)) {
#define LEAD_CASE(n)                                                   \
    case BT_LEAD##n:                                                   \
        ptr += n; /* NOTE: The encoding has already been validated. */ \
        break;
            LEAD_CASE(2)
            LEAD_CASE(3)
            LEAD_CASE(4)
#undef LEAD_CASE
        case BT_NONASCII:
        case BT_NMSTRT:
#ifdef XML_NS
        case BT_COLON:
#endif
        case BT_HEX:
        case BT_DIGIT:
        case BT_NAME:
        case BT_MINUS:
            ptr += MINBPC(enc);
            break;
        default:
            return (int)(ptr - start);
        }
    }
}

template <class E>
static const char *skipS(const ENCODING *enc, const char *ptr)
{
    for (;;) {
        switch (BYTE_TYPE(enc, ptr)) {
        case BT_LF:
        case BT_CR:
        case BT_S:
            ptr += MINBPC(enc);
            break;
        default:
            return ptr;
        }
    }
}

template <class E>
static void updatePosition(const ENCODING *enc, const char *ptr, const char *end, POSITION *pos)
{
    while (HAS_CHAR(enc, ptr, end)) {
        switch (BYTE_TYPE(enc, ptr)) {
#define LEAD_CASE(n)                                                   \
    case BT_LEAD##n:                                                   \
        ptr += n; /* NOTE: The encoding has already been validated. */ \
        pos->columnNumber++;                                           \
        break;
            LEAD_CASE(2)
            LEAD_CASE(3)
            LEAD_CASE(4)
#undef LEAD_CASE
        case BT_LF:
            pos->columnNumber = 0;
            pos->lineNumber++;
            ptr += MINBPC(enc);
            break;
        case BT_CR:
            pos->lineNumber++;
            ptr += MINBPC(enc);
            if (HAS_CHAR(enc, ptr, end) && BYTE_TYPE(enc, ptr) == BT_LF)
                ptr += MINBPC(enc);
            pos->columnNumber = 0;
            break;
        default:
            ptr += MINBPC(enc);
            pos->columnNumber++;
            break;
        }
    }
}

#undef DO_LEAD_CASE
#undef MULTIBYTE_CASES
#undef INVALID_CASES
#undef CHECK_NAME_CASE
#undef CHECK_NAME_CASES
#undef CHECK_NMSTRT_CASE
#undef CHECK_NMSTRT_CASES

#undef MINBPC
#undef BYTE_TYPE
#undef BYTE_TO_ASCII
#undef CHAR_MATCHES
#undef IS_NAME_CHAR
#undef IS_NAME_CHAR_MINBPC
#undef IS_NMSTRT_CHAR
#undef IS_NMSTRT_CHAR_MINBPC
#undef IS_INVALID_CHAR
