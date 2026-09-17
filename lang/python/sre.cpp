// The matching engine: Modules/_sre/sre_lib.h and the engine half of sre.c,
// kept in their own shape. It is compiled twice, for a one-octet text (bytes,
// and a str that is all ASCII) and for a text of codepoints.
//
// Copyright (c) 1997-2001 by Secret Labs AB.  All rights reserved.
// This version of the SRE library can be redistributed under CNRI's Python
// 1.6 license; see LICENSE.
#include "sre.h"

#include "kernel/alloc.h"
#include "ucd.h"

namespace {

constexpr u32 SRE_CODE_BITS = 32;

enum : SreCode {
    SRE_CATEGORY_DIGIT,
    SRE_CATEGORY_NOT_DIGIT,
    SRE_CATEGORY_SPACE,
    SRE_CATEGORY_NOT_SPACE,
    SRE_CATEGORY_WORD,
    SRE_CATEGORY_NOT_WORD,
    SRE_CATEGORY_LINEBREAK,
    SRE_CATEGORY_NOT_LINEBREAK,
    SRE_CATEGORY_LOC_WORD,
    SRE_CATEGORY_LOC_NOT_WORD,
    SRE_CATEGORY_UNI_DIGIT,
    SRE_CATEGORY_UNI_NOT_DIGIT,
    SRE_CATEGORY_UNI_SPACE,
    SRE_CATEGORY_UNI_NOT_SPACE,
    SRE_CATEGORY_UNI_WORD,
    SRE_CATEGORY_UNI_NOT_WORD,
    SRE_CATEGORY_UNI_LINEBREAK,
    SRE_CATEGORY_UNI_NOT_LINEBREAK,
    SRE_CATEGORY_ALPHA,
    SRE_CATEGORY_NOT_ALPHA,
    SRE_CATEGORY_LOWER,
    SRE_CATEGORY_NOT_LOWER,
    SRE_CATEGORY_UPPER,
    SRE_CATEGORY_NOT_UPPER,
    SRE_CATEGORY_NUMERIC,
    SRE_CATEGORY_NOT_NUMERIC,
    SRE_CATEGORY_PRINTABLE,
    SRE_CATEGORY_NOT_PRINTABLE,
    SRE_CATEGORY_ALNUM,
    SRE_CATEGORY_NOT_ALNUM,
    SRE_CATEGORY_XID_START,
    SRE_CATEGORY_NOT_XID_START,
    SRE_CATEGORY_XID_CONTINUE,
    SRE_CATEGORY_NOT_XID_CONTINUE,
    SRE_CATEGORY_TITLE,
    SRE_CATEGORY_NOT_TITLE,
    SRE_CATEGORY_CASED,
    SRE_CATEGORY_NOT_CASED,
    SRE_CATEGORY_CASE_IGNORABLE,
    SRE_CATEGORY_NOT_CASE_IGNORABLE,
    SRE_CATEGORY_LU,
    SRE_CATEGORY_NOT_LU,
    SRE_CATEGORY_N,
    SRE_CATEGORY_NOT_N,
    SRE_CATEGORY_LM,
    SRE_CATEGORY_NOT_LM,
    SRE_CATEGORY_NL,
    SRE_CATEGORY_NOT_NL,
    SRE_CATEGORY_NO,
    SRE_CATEGORY_NOT_NO,
    SRE_CATEGORY_CF,
    SRE_CATEGORY_NOT_CF,
    SRE_CATEGORY_Z,
    SRE_CATEGORY_NOT_Z,
    SRE_CATEGORY_ZS,
    SRE_CATEGORY_NOT_ZS,
    SRE_CATEGORY_C,
    SRE_CATEGORY_NOT_C,
    SRE_CATEGORY_CN,
    SRE_CATEGORY_NOT_CN,
    SRE_CATEGORY_ASSIGNED,
    SRE_CATEGORY_NOT_ASSIGNED,
    SRE_CATEGORY_BLANK,
    SRE_CATEGORY_NOT_BLANK,
    SRE_CATEGORY_GRAPH,
    SRE_CATEGORY_NOT_GRAPH,
    SRE_CATEGORY_PRINT,
    SRE_CATEGORY_NOT_PRINT,
};

// ------------------------------------------------------ the predicates

bool ascii_alpha(u32 ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

bool ascii_alnum(u32 ch)
{
    return ascii_alpha(ch) || (ch >= '0' && ch <= '9');
}

bool SRE_IS_DIGIT(u32 ch)
{
    return ch >= '0' && ch <= '9';
}

bool SRE_IS_SPACE(u32 ch)
{
    return ch == ' ' || (ch >= 9 && ch <= 13);
}

bool SRE_IS_LINEBREAK(u32 ch)
{
    return ch == '\n';
}

bool SRE_IS_WORD(u32 ch)
{
    return ch <= 'z' && (ascii_alnum(ch) || ch == '_');
}

// The locale is always "C" here, so its classes are ASCII's.
bool SRE_LOC_IS_WORD(u32 ch)
{
    return ch < 256 && (ascii_alnum(ch) || ch == '_');
}

u32 sre_lower_locale(u32 ch)
{
    return ch < 128 && ch >= 'A' && ch <= 'Z' ? ch + 32 : ch;
}

u32 sre_upper_locale(u32 ch)
{
    return ch < 128 && ch >= 'a' && ch <= 'z' ? ch - 32 : ch;
}

u16 uflags(u32 ch)
{
    return ch <= 0x10ffff ? ucd_rec(ch).flags : 0;
}

bool uni(u32 ch, u16 flag)
{
    return (uflags(ch) & flag) != 0;
}

bool SRE_UNI_IS_DIGIT(u32 ch)
{
    return uni(ch, UCD_DECIMAL);
}

// Py_UNICODE_ISSPACE: below 128, CPython's own table, which counts the four
// information separators.
bool SRE_UNI_IS_SPACE(u32 ch)
{
    if (ch < 128)
        return ch == ' ' || (ch >= 9 && ch <= 13) || (ch >= 0x1c && ch <= 0x1f);
    return uni(ch, UCD_SPACE);
}

bool SRE_UNI_IS_LINEBREAK(u32 ch)
{
    return uni(ch, UCD_LINEBREAK);
}

bool SRE_UNI_IS_ALNUM(u32 ch)
{
    return uni(ch, UCD_ALPHA | UCD_DECIMAL | UCD_DIGIT | UCD_NUMERIC);
}

bool SRE_UNI_IS_WORD(u32 ch)
{
    return SRE_UNI_IS_ALNUM(ch) || ch == '_';
}

bool SRE_UNI_IS_ALPHA(u32 ch)
{
    return uni(ch, UCD_ALPHA);
}

bool SRE_UNI_IS_LOWER(u32 ch)
{
    return uni(ch, UCD_LOWER);
}

bool SRE_UNI_IS_UPPER(u32 ch)
{
    return uni(ch, UCD_UPPER);
}

bool SRE_UNI_IS_NUMERIC(u32 ch)
{
    return uni(ch, UCD_NUMERIC);
}

bool SRE_UNI_IS_PRINTABLE(u32 ch)
{
    return uni(ch, UCD_PRINTABLE);
}

bool SRE_UNI_IS_XID_START(u32 ch)
{
    return uni(ch, UCD_XID_START);
}

bool SRE_UNI_IS_XID_CONTINUE(u32 ch)
{
    return uni(ch, UCD_XID_CONTINUE);
}

bool SRE_UNI_IS_TITLE(u32 ch)
{
    return uni(ch, UCD_TITLE);
}

bool SRE_UNI_IS_CASED(u32 ch)
{
    return uni(ch, UCD_CASED);
}

bool SRE_UNI_IS_CASE_IGNORABLE(u32 ch)
{
    return uni(ch, UCD_CASE_IGNORABLE);
}

// General_Category values as combinations of the simple predicates, as
// sre.c has them.
bool SRE_IS_CC(u32 ch)
{
    return ch <= 0x1f || (0x7f <= ch && ch <= 0x9f);
}

bool SRE_IS_CS(u32 ch)
{
    return 0xd800 <= ch && ch <= 0xdfff;
}

bool SRE_IS_CO(u32 ch)
{
    return (0xe000 <= ch && ch <= 0xf8ff) || (0xf0000 <= ch && ch <= 0xffffd) ||
           (0x100000 <= ch && ch <= 0x10fffd);
}

bool SRE_UNI_IS_LU(u32 ch)
{
    return SRE_UNI_IS_UPPER(ch) && SRE_UNI_IS_ALPHA(ch);
}

bool SRE_UNI_IS_N(u32 ch)
{
    return SRE_UNI_IS_ALNUM(ch) && !SRE_UNI_IS_ALPHA(ch);
}

bool SRE_UNI_IS_LM(u32 ch)
{
    return SRE_UNI_IS_ALPHA(ch) && SRE_UNI_IS_CASE_IGNORABLE(ch);
}

bool SRE_UNI_IS_NL(u32 ch)
{
    return SRE_UNI_IS_N(ch) && SRE_UNI_IS_XID_START(ch);
}

bool SRE_UNI_IS_NO(u32 ch)
{
    return SRE_UNI_IS_N(ch) && !SRE_UNI_IS_DIGIT(ch) && !SRE_UNI_IS_XID_START(ch);
}

bool SRE_UNI_IS_CF(u32 ch)
{
    return SRE_UNI_IS_CASE_IGNORABLE(ch) && !SRE_UNI_IS_PRINTABLE(ch);
}

bool SRE_UNI_IS_Z(u32 ch)
{
    return SRE_UNI_IS_SPACE(ch) && !SRE_IS_CC(ch);
}

bool SRE_UNI_IS_ZS(u32 ch)
{
    return SRE_UNI_IS_Z(ch) && ch != 0x2028 && ch != 0x2029;
}

bool SRE_UNI_IS_C(u32 ch)
{
    return !SRE_UNI_IS_PRINTABLE(ch) && !SRE_UNI_IS_Z(ch);
}

bool SRE_UNI_IS_CN(u32 ch)
{
    return SRE_UNI_IS_C(ch) && !SRE_IS_CC(ch) && !SRE_IS_CS(ch) && !SRE_IS_CO(ch) &&
           !SRE_UNI_IS_CASE_IGNORABLE(ch);
}

bool SRE_UNI_IS_ASSIGNED(u32 ch)
{
    return !SRE_UNI_IS_CN(ch);
}

bool SRE_UNI_IS_BLANK(u32 ch)
{
    return SRE_UNI_IS_ZS(ch) || ch == 0x09;
}

bool SRE_UNI_IS_GRAPH(u32 ch)
{
    return !SRE_UNI_IS_SPACE(ch) && !SRE_IS_CC(ch) && !SRE_IS_CS(ch) && !SRE_UNI_IS_CN(ch);
}

bool SRE_UNI_IS_PRINT(u32 ch)
{
    return (SRE_UNI_IS_GRAPH(ch) || SRE_UNI_IS_BLANK(ch)) && !SRE_IS_CC(ch);
}

// The predicate of each positive category, the negative one being its odd
// neighbour.
using Pred = bool (*)(u32);

constexpr Pred CATEGORIES[] = {
    SRE_IS_DIGIT,
    SRE_IS_SPACE,
    SRE_IS_WORD,
    SRE_IS_LINEBREAK,
    SRE_LOC_IS_WORD,
    SRE_UNI_IS_DIGIT,
    SRE_UNI_IS_SPACE,
    SRE_UNI_IS_WORD,
    SRE_UNI_IS_LINEBREAK,
    SRE_UNI_IS_ALPHA,
    SRE_UNI_IS_LOWER,
    SRE_UNI_IS_UPPER,
    SRE_UNI_IS_NUMERIC,
    SRE_UNI_IS_PRINTABLE,
    SRE_UNI_IS_ALNUM,
    SRE_UNI_IS_XID_START,
    SRE_UNI_IS_XID_CONTINUE,
    SRE_UNI_IS_TITLE,
    SRE_UNI_IS_CASED,
    SRE_UNI_IS_CASE_IGNORABLE,
    SRE_UNI_IS_LU,
    SRE_UNI_IS_N,
    SRE_UNI_IS_LM,
    SRE_UNI_IS_NL,
    SRE_UNI_IS_NO,
    SRE_UNI_IS_CF,
    SRE_UNI_IS_Z,
    SRE_UNI_IS_ZS,
    SRE_UNI_IS_C,
    SRE_UNI_IS_CN,
    SRE_UNI_IS_ASSIGNED,
    SRE_UNI_IS_BLANK,
    SRE_UNI_IS_GRAPH,
    SRE_UNI_IS_PRINT,
};

bool sre_category(SreCode category, u32 ch)
{
    if (category > SRE_CATEGORY_LAST)
        return false;
    bool yes = CATEGORIES[category / 2](ch);
    return (category & 1) ? !yes : yes;
}

bool char_loc_ignore(SreCode pattern, SreCode ch)
{
    return ch == pattern || sre_lower_locale(ch) == pattern || sre_upper_locale(ch) == pattern;
}

// ------------------------------------------------------------- helpers

void data_stack_dealloc(SreState *state)
{
    if (state->data_stack) {
        heap_free(state->data_stack);
        state->data_stack = nullptr;
    }
    state->data_stack_size = state->data_stack_base = 0;
}

int data_stack_grow(SreState *state, usize size)
{
    usize minsize = state->data_stack_base + size;
    usize cursize = state->data_stack_size;
    if (cursize < minsize) {
        cursize     = minsize + minsize / 4 + 1024;
        char *stack = static_cast<char *>(heap_alloc(cursize));
        if (!stack) {
            data_stack_dealloc(state);
            return int(SRE_ERROR_MEMORY);
        }
        if (state->data_stack) {
            __builtin_memcpy(stack, state->data_stack, state->data_stack_base);
            heap_free(state->data_stack);
        }
        state->data_stack      = stack;
        state->data_stack_size = cursize;
    }
    return 0;
}

SreRepeat *repeat_pool_malloc(SreState *state)
{
    SreRepeat *repeat;
    if (state->repeat_pool_unused) {
        repeat                    = state->repeat_pool_unused;
        state->repeat_pool_unused = repeat->pool_next;
    } else {
        repeat = static_cast<SreRepeat *>(heap_alloc(sizeof(SreRepeat)));
        if (!repeat)
            return nullptr;
    }
    SreRepeat *temp = state->repeat_pool_used;
    if (temp)
        temp->pool_prev = repeat;
    repeat->pool_prev       = nullptr;
    repeat->pool_next       = temp;
    state->repeat_pool_used = repeat;
    return repeat;
}

void repeat_pool_free(SreState *state, SreRepeat *repeat)
{
    SreRepeat *prev = repeat->pool_prev;
    SreRepeat *next = repeat->pool_next;
    if (prev)
        prev->pool_next = next;
    else
        state->repeat_pool_used = next;
    if (next)
        next->pool_prev = prev;
    repeat->pool_next         = state->repeat_pool_unused;
    state->repeat_pool_unused = repeat;
}

void repeat_pool_clear(SreState *state)
{
    SreRepeat *next         = state->repeat_pool_used;
    state->repeat_pool_used = nullptr;
    while (next) {
        SreRepeat *temp = next;
        next            = temp->pool_next;
        heap_free(temp);
    }
    next                      = state->repeat_pool_unused;
    state->repeat_pool_unused = nullptr;
    while (next) {
        SreRepeat *temp = next;
        next            = temp->pool_next;
        heap_free(temp);
    }
}

// ------------------------------------------------------------ the engine

enum : int {
    JUMP_NONE,
    JUMP_MAX_UNTIL_1,
    JUMP_MAX_UNTIL_2,
    JUMP_MAX_UNTIL_3,
    JUMP_MIN_UNTIL_1,
    JUMP_MIN_UNTIL_2,
    JUMP_MIN_UNTIL_3,
    JUMP_REPEAT,
    JUMP_REPEAT_ONE_1,
    JUMP_REPEAT_ONE_2,
    JUMP_MIN_REPEAT_ONE,
    JUMP_BRANCH,
    JUMP_ASSERT,
    JUMP_ASSERT_NOT,
    JUMP_POSS_REPEAT_1,
    JUMP_POSS_REPEAT_2,
    JUMP_ATOMIC_GROUP,
};

template <class C>
struct MatchContext {
    isize count;
    union {
        SreCode chr;
        SreRepeat *rep;
    } u;
    int lastmark;
    int lastindex;
    const SreCode *pattern;
    const C *ptr;
    int toplevel;
    int jump;
    isize last_ctx_pos;
};

template <class C>
int sre_at(SreState *state, const C *ptr, SreCode at)
{
    isize thisp, thatp;
    switch (at) {
    case SRE_AT_BEGINNING:
    case SRE_AT_BEGINNING_STRING:
        return static_cast<const void *>(ptr) == state->beginning;
    case SRE_AT_BEGINNING_LINE:
        return static_cast<const void *>(ptr) == state->beginning || SRE_IS_LINEBREAK(ptr[-1]);
    case SRE_AT_END:
        return (static_cast<const C *>(state->end) - ptr == 1 && SRE_IS_LINEBREAK(ptr[0])) ||
               static_cast<const void *>(ptr) == state->end;
    case SRE_AT_END_LINE:
        return static_cast<const void *>(ptr) == state->end || SRE_IS_LINEBREAK(ptr[0]);
    case SRE_AT_END_STRING:
        return static_cast<const void *>(ptr) == state->end;
    case SRE_AT_BOUNDARY:
        thatp = static_cast<const void *>(ptr) > state->beginning ? SRE_IS_WORD(ptr[-1]) : 0;
        thisp = static_cast<const void *>(ptr) < state->end ? SRE_IS_WORD(ptr[0]) : 0;
        return thisp != thatp;
    case SRE_AT_NON_BOUNDARY:
        thatp = static_cast<const void *>(ptr) > state->beginning ? SRE_IS_WORD(ptr[-1]) : 0;
        thisp = static_cast<const void *>(ptr) < state->end ? SRE_IS_WORD(ptr[0]) : 0;
        return thisp == thatp;
    case SRE_AT_LOC_BOUNDARY:
        thatp = static_cast<const void *>(ptr) > state->beginning ? SRE_LOC_IS_WORD(ptr[-1]) : 0;
        thisp = static_cast<const void *>(ptr) < state->end ? SRE_LOC_IS_WORD(ptr[0]) : 0;
        return thisp != thatp;
    case SRE_AT_LOC_NON_BOUNDARY:
        thatp = static_cast<const void *>(ptr) > state->beginning ? SRE_LOC_IS_WORD(ptr[-1]) : 0;
        thisp = static_cast<const void *>(ptr) < state->end ? SRE_LOC_IS_WORD(ptr[0]) : 0;
        return thisp == thatp;
    case SRE_AT_UNI_BOUNDARY:
        thatp = static_cast<const void *>(ptr) > state->beginning ? SRE_UNI_IS_WORD(ptr[-1]) : 0;
        thisp = static_cast<const void *>(ptr) < state->end ? SRE_UNI_IS_WORD(ptr[0]) : 0;
        return thisp != thatp;
    case SRE_AT_UNI_NON_BOUNDARY:
        thatp = static_cast<const void *>(ptr) > state->beginning ? SRE_UNI_IS_WORD(ptr[-1]) : 0;
        thisp = static_cast<const void *>(ptr) < state->end ? SRE_UNI_IS_WORD(ptr[0]) : 0;
        return thisp == thatp;
    }
    return 0;
}

int sre_charset(const SreCode *set, SreCode ch)
{
    int ok = 1;
    for (;;) {
        switch (*set++) {
        case SRE_OP_FAILURE:
            return !ok;
        case SRE_OP_LITERAL:
            if (ch == set[0])
                return ok;
            set++;
            break;
        case SRE_OP_CATEGORY:
            if (sre_category(set[0], ch))
                return ok;
            set++;
            break;
        case SRE_OP_CHARSET:
            if (ch < 256 && (set[ch / SRE_CODE_BITS] & (1u << (ch & (SRE_CODE_BITS - 1)))))
                return ok;
            set += 256 / SRE_CODE_BITS;
            break;
        case SRE_OP_RANGE:
            if (set[0] <= ch && ch <= set[1])
                return ok;
            set += 2;
            break;
        case SRE_OP_RANGE_UNI_IGNORE: {
            // ch is already lower cased
            if (set[0] <= ch && ch <= set[1])
                return ok;
            SreCode uch = sre_upper_unicode(ch);
            if (set[0] <= uch && uch <= set[1])
                return ok;
            set += 2;
            break;
        }
        case SRE_OP_NEGATE:
            ok = !ok;
            break;
        case SRE_OP_BIGCHARSET: {
            // <BIGCHARSET> <blockcount> <256 blockindices> <blocks>
            isize count = *(set++);
            isize block;
            if (ch < 0x10000u)
                block = reinterpret_cast<const unsigned char *>(set)[ch >> 8];
            else
                block = -1;
            set += 256 / sizeof(SreCode);
            if (block >= 0 && (set[(usize(block) * 256 + (ch & 255)) / SRE_CODE_BITS] &
                               (1u << (ch & (SRE_CODE_BITS - 1)))))
                return ok;
            set += count * (256 / SRE_CODE_BITS);
            break;
        }
        default:
            // internal error -- pretend it didn't match
            return 0;
        }
    }
}

// Both locale cases of ch against every member.
int sre_charset_loc_ignore(const SreCode *set, SreCode ch)
{
    SreCode lo = sre_lower_locale(ch);
    SreCode up = sre_upper_locale(ch);
    if (up == lo)
        return sre_charset(set, lo);
    int ok = 1;
    for (;;) {
        switch (*set++) {
        case SRE_OP_FAILURE:
            return !ok;
        case SRE_OP_LITERAL:
            if (lo == set[0] || up == set[0])
                return ok;
            set++;
            break;
        case SRE_OP_CATEGORY:
            if (sre_category(set[0], lo) || sre_category(set[0], up))
                return ok;
            set++;
            break;
        case SRE_OP_CHARSET:
            if ((lo < 256 && (set[lo / SRE_CODE_BITS] & (1u << (lo & (SRE_CODE_BITS - 1))))) ||
                (up < 256 && (set[up / SRE_CODE_BITS] & (1u << (up & (SRE_CODE_BITS - 1))))))
                return ok;
            set += 256 / SRE_CODE_BITS;
            break;
        case SRE_OP_RANGE:
            if ((set[0] <= lo && lo <= set[1]) || (set[0] <= up && up <= set[1]))
                return ok;
            set += 2;
            break;
        case SRE_OP_NEGATE:
            ok = !ok;
            break;
        default:
            return 0;
        }
    }
}

template <class C>
isize sre_match_t(SreState *state, const SreCode *pattern, int toplevel);

template <class C>
isize sre_count(SreState *state, const SreCode *pattern, isize maxcount)
{
    SreCode chr;
    SreCode arg;
    C c;
    const C *ptr = static_cast<const C *>(state->ptr);
    const C *end = static_cast<const C *>(state->end);

    // adjust end
    if (maxcount < end - ptr && SreCode(maxcount) != SRE_MAXREPEAT)
        end = ptr + maxcount;

    switch (pattern[0]) {
    case SRE_OP_IN:
        while (ptr < end && sre_charset(pattern + 2, *ptr))
            ptr++;
        break;
    case SRE_OP_ANY:
        while (ptr < end && !SRE_IS_LINEBREAK(*ptr))
            ptr++;
        break;
    case SRE_OP_ANY_ALL:
        ptr = end;
        break;
    case SRE_OP_LITERAL:
        chr = pattern[1];
        c   = C(chr);
        if (SreCode(c) != chr)
            ; // literal can't match: doesn't fit in char width
        else
            while (ptr < end && *ptr == c)
                ptr++;
        break;
    case SRE_OP_LITERAL_IGNORE:
        chr = pattern[1];
        while (ptr < end && sre_lower_ascii(*ptr) == chr)
            ptr++;
        break;
    case SRE_OP_LITERAL_UNI_IGNORE:
        chr = pattern[1];
        while (ptr < end && sre_lower_unicode(*ptr) == chr)
            ptr++;
        break;
    case SRE_OP_LITERAL_LOC_IGNORE:
        chr = pattern[1];
        while (ptr < end && char_loc_ignore(chr, *ptr))
            ptr++;
        break;
    case SRE_OP_NOT_LITERAL:
        chr = pattern[1];
        c   = C(chr);
        if (SreCode(c) != chr)
            ptr = end; // literal can't match: doesn't fit in char width
        else
            while (ptr < end && *ptr != c)
                ptr++;
        break;
    case SRE_OP_NOT_LITERAL_IGNORE:
        chr = pattern[1];
        while (ptr < end && sre_lower_ascii(*ptr) != chr)
            ptr++;
        break;
    case SRE_OP_NOT_LITERAL_UNI_IGNORE:
        chr = pattern[1];
        while (ptr < end && sre_lower_unicode(*ptr) != chr)
            ptr++;
        break;
    case SRE_OP_NOT_LITERAL_LOC_IGNORE:
        chr = pattern[1];
        while (ptr < end && !char_loc_ignore(chr, *ptr))
            ptr++;
        break;
    case SRE_OP_CATEGORY:
        arg = pattern[1];
        while (ptr < end && sre_category(arg, *ptr))
            ptr++;
        break;
    default: {
        // repeated single character pattern
        while (static_cast<const C *>(state->ptr) < end) {
            isize i = sre_match_t<C>(state, pattern, 0);
            if (i < 0)
                return i;
            if (!i)
                break;
        }
        return static_cast<const C *>(state->ptr) - ptr;
    }
    }
    return ptr - static_cast<const C *>(state->ptr);
}

// The data stack, as sre_lib.h's macros had it. A function each rather than
// a macro: `ctx` has to be looked up again after a grow, so the caller does
// that where the macro did.
template <class T>
int data_push(SreState *state, const T *data, usize size)
{
    if (size > state->data_stack_size - state->data_stack_base) {
        int j = data_stack_grow(state, size);
        if (j < 0)
            return j;
    }
    __builtin_memcpy(state->data_stack + state->data_stack_base, data, size);
    state->data_stack_base += size;
    return 0;
}

template <class T>
void data_pop(SreState *state, T *data, usize size, bool discard)
{
    __builtin_memcpy(static_cast<void *>(data), state->data_stack + state->data_stack_base - size,
                     size);
    if (discard)
        state->data_stack_base -= size;
}

#define DATA_LOOKUP_AT(ptr_, pos_) ptr_ = reinterpret_cast<Ctx *>(state->data_stack + (pos_))

#define DATA_ALLOC(ptr_)                                               \
    do {                                                               \
        alloc_pos = isize(state->data_stack_base);                     \
        if (sizeof(Ctx) > state->data_stack_size - usize(alloc_pos)) { \
            int j_ = data_stack_grow(state, sizeof(Ctx));              \
            if (j_ < 0)                                                \
                return leave(j_);                                      \
            if (ctx_pos != -1)                                         \
                DATA_LOOKUP_AT(ctx, ctx_pos);                          \
        }                                                              \
        ptr_ = reinterpret_cast<Ctx *>(state->data_stack + alloc_pos); \
        state->data_stack_base += sizeof(Ctx);                         \
    } while (0)

#define DATA_STACK_PUSH(data_, size_)            \
    do {                                         \
        int j_ = data_push(state, data_, size_); \
        if (j_ < 0)                              \
            return leave(j_);                    \
        if (ctx_pos != -1)                       \
            DATA_LOOKUP_AT(ctx, ctx_pos);        \
    } while (0)

#define DATA_STACK_POP(data_, size_, discard_) data_pop(state, data_, size_, discard_)
#define DATA_STACK_POP_DISCARD(size_)          (state->data_stack_base -= (size_))

#define LASTMARK_SAVE()                    \
    do {                                   \
        ctx->lastmark  = state->lastmark;  \
        ctx->lastindex = state->lastindex; \
    } while (0)
#define LASTMARK_RESTORE()                 \
    do {                                   \
        state->lastmark  = ctx->lastmark;  \
        state->lastindex = ctx->lastindex; \
    } while (0)

#define LAST_PTR_PUSH() DATA_STACK_PUSH(&ctx->u.rep->last_ptr, sizeof(ctx->u.rep->last_ptr))
#define LAST_PTR_POP()  DATA_STACK_POP(&ctx->u.rep->last_ptr, sizeof(ctx->u.rep->last_ptr), true)

#define RETURN_ERROR(i) return leave(i)
#define RETURN_FAILURE \
    do {               \
        ret = 0;       \
        goto exit;     \
    } while (0)
#define RETURN_SUCCESS \
    do {               \
        ret = 1;       \
        goto exit;     \
    } while (0)
#define RETURN_ON_ERROR(i)   \
    do {                     \
        if ((i) < 0)         \
            RETURN_ERROR(i); \
    } while (0)
#define RETURN_ON_SUCCESS(i) \
    do {                     \
        RETURN_ON_ERROR(i);  \
        if ((i) > 0)         \
            RETURN_SUCCESS;  \
    } while (0)
#define RETURN_ON_FAILURE(i) \
    do {                     \
        RETURN_ON_ERROR(i);  \
        if ((i) == 0)        \
            RETURN_FAILURE;  \
    } while (0)

#define MARK_PUSH(lastmark)                                             \
    do                                                                  \
        if ((lastmark) >= 0) {                                          \
            usize marks_size_ = usize((lastmark) + 1) * sizeof(void *); \
            DATA_STACK_PUSH(state->mark, marks_size_);                  \
        }                                                               \
    while (0)
#define MARK_POP(lastmark)                                              \
    do                                                                  \
        if ((lastmark) >= 0) {                                          \
            usize marks_size_ = usize((lastmark) + 1) * sizeof(void *); \
            DATA_STACK_POP(state->mark, marks_size_, true);             \
        }                                                               \
    while (0)
#define MARK_POP_KEEP(lastmark)                                         \
    do                                                                  \
        if ((lastmark) >= 0) {                                          \
            usize marks_size_ = usize((lastmark) + 1) * sizeof(void *); \
            DATA_STACK_POP(state->mark, marks_size_, false);            \
        }                                                               \
    while (0)
#define MARK_POP_DISCARD(lastmark)                                      \
    do                                                                  \
        if ((lastmark) >= 0) {                                          \
            usize marks_size_ = usize((lastmark) + 1) * sizeof(void *); \
            DATA_STACK_POP_DISCARD(marks_size_);                        \
        }                                                               \
    while (0)

#define DO_JUMPX(jumpvalue, jumplabel, nextpattern, toplevel_) \
    ctx->pattern = pattern;                                    \
    ctx->ptr     = ptr;                                        \
    DATA_ALLOC(nextctx);                                       \
    nextctx->pattern      = nextpattern;                       \
    nextctx->toplevel     = toplevel_;                         \
    nextctx->jump         = jumpvalue;                         \
    nextctx->last_ctx_pos = ctx_pos;                           \
    pattern               = nextpattern;                       \
    ctx_pos               = alloc_pos;                         \
    ctx                   = nextctx;                           \
    goto entrance;                                             \
    jumplabel:                                                 \
    pattern = ctx->pattern;                                    \
    ptr     = ctx->ptr;

#define DO_JUMP(jumpvalue, jumplabel, nextpattern) \
    DO_JUMPX(jumpvalue, jumplabel, nextpattern, ctx->toplevel)

#define DO_JUMP0(jumpvalue, jumplabel, nextpattern) DO_JUMPX(jumpvalue, jumplabel, nextpattern, 0)

// check if string matches the given pattern.  returns <0 for error, 0 for
// failure, 1 for success, and SRE_SUSPEND when the slice is spent.
template <class C>
isize sre_match_t(SreState *state, const SreCode *pattern, int toplevel)
{
    using Ctx    = MatchContext<C>;
    const C *end = static_cast<const C *>(state->end);
    isize alloc_pos, ctx_pos = -1;
    isize ret = 0;
    int jump;
    u32 sigcount = state->sigcount;
    int pushed   = 0;
    const C *ptr = nullptr;
    Ctx *ctx;
    Ctx *nextctx;

    // Every return goes through here, so the depth stays true.
    auto leave = [state](isize r) {
        state->depth--;
        return r;
    };

    state->depth++;
    if (state->depth == 1 && state->resume >= 0) {
        // Back from a suspension: the context chain is still on the stack.
        ctx_pos       = state->resume;
        state->resume = -1;
        DATA_LOOKUP_AT(ctx, ctx_pos);
        pattern = ctx->pattern;
        ptr     = ctx->ptr;
        goto dispatch;
    }

    DATA_ALLOC(ctx);
    ctx->last_ctx_pos = -1;
    ctx->jump         = JUMP_NONE;
    ctx->toplevel     = toplevel;
    ctx_pos           = alloc_pos;

entrance:
    ptr = static_cast<const C *>(state->ptr);

    if (pattern[0] == SRE_OP_INFO) {
        // <INFO> <1=skip> <2=flags> <3=min> ...
        if (pattern[3] && usize(end - ptr) < pattern[3])
            RETURN_FAILURE;
        pattern += pattern[1] + 1;
    }

dispatch:
    // The one addition: a long match stops here, where the whole of its
    // state is `pattern`, `ptr` and the context chain.
    if (++sigcount >= state->slice && state->slice && state->depth == 1) {
        sigcount         = 0;
        ctx->pattern     = pattern;
        ctx->ptr         = ptr;
        state->resume    = ctx_pos;
        state->sigcount  = 0;
        state->suspended = true;
        return leave(SRE_SUSPEND);
    }
    switch (*pattern++) {
    case SRE_OP_MARK: {
        // <MARK> <gid>
        int i = int(pattern[0]);
        if (i & 1)
            state->lastindex = i / 2 + 1;
        if (i > state->lastmark) {
            // lastmark is the highest valid index in mark; marks skipped
            // over are set to null to say they have not been seen.
            int j = state->lastmark + 1;
            while (j < i)
                state->mark[j++] = nullptr;
            state->lastmark = i;
        }
        state->mark[i] = ptr;
        pattern++;
        goto dispatch;
    }

    case SRE_OP_LITERAL:
        // <LITERAL> <code>
        if (ptr >= end || SreCode(ptr[0]) != pattern[0])
            RETURN_FAILURE;
        pattern++;
        ptr++;
        goto dispatch;

    case SRE_OP_NOT_LITERAL:
        if (ptr >= end || SreCode(ptr[0]) == pattern[0])
            RETURN_FAILURE;
        pattern++;
        ptr++;
        goto dispatch;

    case SRE_OP_SUCCESS:
        if (ctx->toplevel && ((state->match_all && ptr != state->end) ||
                              (state->must_advance && ptr == state->start)))
            RETURN_FAILURE;
        state->ptr = ptr;
        RETURN_SUCCESS;

    case SRE_OP_AT:
        // <AT> <code>
        if (!sre_at<C>(state, ptr, *pattern))
            RETURN_FAILURE;
        pattern++;
        goto dispatch;

    case SRE_OP_CATEGORY:
        // <CATEGORY> <code>
        if (ptr >= end || !sre_category(pattern[0], ptr[0]))
            RETURN_FAILURE;
        pattern++;
        ptr++;
        goto dispatch;

    case SRE_OP_ANY:
        if (ptr >= end || SRE_IS_LINEBREAK(ptr[0]))
            RETURN_FAILURE;
        ptr++;
        goto dispatch;

    case SRE_OP_ANY_ALL:
        if (ptr >= end)
            RETURN_FAILURE;
        ptr++;
        goto dispatch;

    case SRE_OP_IN:
        // <IN> <skip> <set>
        if (ptr >= end || !sre_charset(pattern + 1, *ptr))
            RETURN_FAILURE;
        pattern += pattern[0];
        ptr++;
        goto dispatch;

    case SRE_OP_LITERAL_IGNORE:
        if (ptr >= end || sre_lower_ascii(*ptr) != *pattern)
            RETURN_FAILURE;
        pattern++;
        ptr++;
        goto dispatch;

    case SRE_OP_LITERAL_UNI_IGNORE:
        if (ptr >= end || sre_lower_unicode(*ptr) != *pattern)
            RETURN_FAILURE;
        pattern++;
        ptr++;
        goto dispatch;

    case SRE_OP_LITERAL_LOC_IGNORE:
        if (ptr >= end || !char_loc_ignore(*pattern, *ptr))
            RETURN_FAILURE;
        pattern++;
        ptr++;
        goto dispatch;

    case SRE_OP_NOT_LITERAL_IGNORE:
        if (ptr >= end || sre_lower_ascii(*ptr) == *pattern)
            RETURN_FAILURE;
        pattern++;
        ptr++;
        goto dispatch;

    case SRE_OP_NOT_LITERAL_UNI_IGNORE:
        if (ptr >= end || sre_lower_unicode(*ptr) == *pattern)
            RETURN_FAILURE;
        pattern++;
        ptr++;
        goto dispatch;

    case SRE_OP_NOT_LITERAL_LOC_IGNORE:
        if (ptr >= end || char_loc_ignore(*pattern, *ptr))
            RETURN_FAILURE;
        pattern++;
        ptr++;
        goto dispatch;

    case SRE_OP_IN_IGNORE:
        if (ptr >= end || !sre_charset(pattern + 1, sre_lower_ascii(*ptr)))
            RETURN_FAILURE;
        pattern += pattern[0];
        ptr++;
        goto dispatch;

    case SRE_OP_IN_UNI_IGNORE:
        if (ptr >= end || !sre_charset(pattern + 1, sre_lower_unicode(*ptr)))
            RETURN_FAILURE;
        pattern += pattern[0];
        ptr++;
        goto dispatch;

    case SRE_OP_IN_LOC_IGNORE:
        if (ptr >= end || !sre_charset_loc_ignore(pattern + 1, *ptr))
            RETURN_FAILURE;
        pattern += pattern[0];
        ptr++;
        goto dispatch;

    case SRE_OP_JUMP:
    case SRE_OP_INFO:
        // <JUMP> <offset>
        pattern += pattern[0];
        goto dispatch;

    case SRE_OP_BRANCH:
        // <BRANCH> <0=skip> code <JUMP> ... <NULL>
        LASTMARK_SAVE();
        if (state->save_marks)
            MARK_PUSH(ctx->lastmark);
        for (; pattern[0]; pattern += pattern[0]) {
            if (pattern[1] == SRE_OP_LITERAL && (ptr >= end || SreCode(*ptr) != pattern[2]))
                continue;
            if (pattern[1] == SRE_OP_IN && (ptr >= end || !sre_charset(pattern + 3, *ptr)))
                continue;
            state->ptr = ptr;
            DO_JUMP(JUMP_BRANCH, jump_branch, pattern + 1);
            if (ret) {
                if (state->save_marks)
                    MARK_POP_DISCARD(ctx->lastmark);
                RETURN_ON_ERROR(ret);
                RETURN_SUCCESS;
            }
            if (state->save_marks)
                MARK_POP_KEEP(ctx->lastmark);
            LASTMARK_RESTORE();
        }
        if (state->save_marks)
            MARK_POP_DISCARD(ctx->lastmark);
        RETURN_FAILURE;

    case SRE_OP_REPEAT_ONE:
        // <REPEAT_ONE> <skip> <1=min> <2=max> item <SUCCESS> tail
        if (isize(pattern[1]) > end - ptr)
            RETURN_FAILURE; // cannot match

        state->ptr = ptr;

        ret = sre_count<C>(state, pattern + 3, isize(pattern[2]));
        RETURN_ON_ERROR(ret);
        DATA_LOOKUP_AT(ctx, ctx_pos);
        ctx->count = ret;
        ptr += ctx->count;

        // count is the number of matches and ptr the tail: check the rest
        // matches, and backtrack if not.
        if (ctx->count < isize(pattern[1]))
            RETURN_FAILURE;

        if (pattern[pattern[0]] == SRE_OP_SUCCESS && ptr == state->end &&
            !(ctx->toplevel && state->must_advance && ptr == state->start)) {
            // tail is empty.  we're finished
            state->ptr = ptr;
            RETURN_SUCCESS;
        }

        LASTMARK_SAVE();
        if (state->save_marks)
            MARK_PUSH(ctx->lastmark);

        if (pattern[pattern[0]] == SRE_OP_LITERAL) {
            // tail starts with a literal. skip positions where the rest of
            // the pattern cannot possibly match
            ctx->u.chr = pattern[pattern[0] + 1];
            for (;;) {
                while (ctx->count >= isize(pattern[1]) &&
                       (ptr >= end || SreCode(*ptr) != ctx->u.chr)) {
                    ptr--;
                    ctx->count--;
                }
                if (ctx->count < isize(pattern[1]))
                    break;
                state->ptr = ptr;
                DO_JUMP(JUMP_REPEAT_ONE_1, jump_repeat_one_1, pattern + pattern[0]);
                if (ret) {
                    if (state->save_marks)
                        MARK_POP_DISCARD(ctx->lastmark);
                    RETURN_ON_ERROR(ret);
                    RETURN_SUCCESS;
                }
                if (state->save_marks)
                    MARK_POP_KEEP(ctx->lastmark);
                LASTMARK_RESTORE();

                ptr--;
                ctx->count--;
            }
            if (state->save_marks)
                MARK_POP_DISCARD(ctx->lastmark);
        } else {
            // general case
            while (ctx->count >= isize(pattern[1])) {
                state->ptr = ptr;
                DO_JUMP(JUMP_REPEAT_ONE_2, jump_repeat_one_2, pattern + pattern[0]);
                if (ret) {
                    if (state->save_marks)
                        MARK_POP_DISCARD(ctx->lastmark);
                    RETURN_ON_ERROR(ret);
                    RETURN_SUCCESS;
                }
                if (state->save_marks)
                    MARK_POP_KEEP(ctx->lastmark);
                LASTMARK_RESTORE();

                ptr--;
                ctx->count--;
            }
            if (state->save_marks)
                MARK_POP_DISCARD(ctx->lastmark);
        }
        RETURN_FAILURE;

    case SRE_OP_MIN_REPEAT_ONE:
        // <MIN_REPEAT_ONE> <skip> <1=min> <2=max> item <SUCCESS> tail
        if (isize(pattern[1]) > end - ptr)
            RETURN_FAILURE; // cannot match

        state->ptr = ptr;

        if (pattern[1] == 0) {
            ctx->count = 0;
        } else {
            // count using pattern min as the maximum
            ret = sre_count<C>(state, pattern + 3, isize(pattern[1]));
            RETURN_ON_ERROR(ret);
            DATA_LOOKUP_AT(ctx, ctx_pos);
            if (ret < isize(pattern[1]))
                // didn't match minimum number of times
                RETURN_FAILURE;
            // advance past minimum matches of repeat
            ctx->count = ret;
            ptr += ctx->count;
        }

        if (pattern[pattern[0]] == SRE_OP_SUCCESS &&
            !(ctx->toplevel && ((state->match_all && ptr != state->end) ||
                                (state->must_advance && ptr == state->start)))) {
            // tail is empty.  we're finished
            state->ptr = ptr;
            RETURN_SUCCESS;
        } else {
            // general case
            LASTMARK_SAVE();
            if (state->save_marks)
                MARK_PUSH(ctx->lastmark);

            while (pattern[2] == SRE_MAXREPEAT || ctx->count <= isize(pattern[2])) {
                state->ptr = ptr;
                DO_JUMP(JUMP_MIN_REPEAT_ONE, jump_min_repeat_one, pattern + pattern[0]);
                if (ret) {
                    if (state->save_marks)
                        MARK_POP_DISCARD(ctx->lastmark);
                    RETURN_ON_ERROR(ret);
                    RETURN_SUCCESS;
                }
                if (state->save_marks)
                    MARK_POP_KEEP(ctx->lastmark);
                LASTMARK_RESTORE();

                state->ptr = ptr;
                ret        = sre_count<C>(state, pattern + 3, 1);
                RETURN_ON_ERROR(ret);
                DATA_LOOKUP_AT(ctx, ctx_pos);
                if (ret == 0)
                    break;
                ptr++;
                ctx->count++;
            }
            if (state->save_marks)
                MARK_POP_DISCARD(ctx->lastmark);
        }
        RETURN_FAILURE;

    case SRE_OP_POSSESSIVE_REPEAT_ONE:
        // <POSSESSIVE_REPEAT_ONE> <skip> <1=min> <2=max> item <SUCCESS> tail
        if (isize(pattern[1]) > end - ptr)
            RETURN_FAILURE; // cannot match

        state->ptr = ptr;

        ret = sre_count<C>(state, pattern + 3, isize(pattern[2]));
        RETURN_ON_ERROR(ret);
        DATA_LOOKUP_AT(ctx, ctx_pos);
        ctx->count = ret;
        ptr += ctx->count;

        // Test for not enough repetitions in match
        if (ctx->count < isize(pattern[1]))
            RETURN_FAILURE;

        // Update the pattern to point to the next op code
        pattern += pattern[0];

        // Let the tail be evaluated separately and consider this match
        // successful.
        if (*pattern == SRE_OP_SUCCESS && ptr == state->end &&
            !(ctx->toplevel && state->must_advance && ptr == state->start)) {
            // tail is empty.  we're finished
            state->ptr = ptr;
            RETURN_SUCCESS;
        }

        // Attempt to match the rest of the string
        goto dispatch;

    case SRE_OP_REPEAT:
        // create repeat context.  all the hard work is done by the UNTIL
        // operator (MAX_UNTIL, MIN_UNTIL)
        // <REPEAT> <skip> <1=min> <2=max> <3=repeat_index> item <UNTIL> tail

        // install new repeat context
        ctx->u.rep = repeat_pool_malloc(state);
        if (!ctx->u.rep)
            RETURN_ERROR(SRE_ERROR_MEMORY);
        ctx->u.rep->count    = -1;
        ctx->u.rep->pattern  = pattern;
        ctx->u.rep->prev     = state->repeat;
        ctx->u.rep->last_ptr = nullptr;
        state->repeat        = ctx->u.rep;
        state->save_marks++;

        state->ptr = ptr;
        DO_JUMP(JUMP_REPEAT, jump_repeat, pattern + pattern[0]);
        state->repeat = ctx->u.rep->prev;
        state->save_marks--;
        repeat_pool_free(state, ctx->u.rep);

        if (ret) {
            RETURN_ON_ERROR(ret);
            RETURN_SUCCESS;
        }
        RETURN_FAILURE;

    case SRE_OP_MAX_UNTIL:
        // maximizing repeat
        // <REPEAT> <skip> <1=min> <2=max> item <MAX_UNTIL> tail

        ctx->u.rep = state->repeat;
        if (!ctx->u.rep)
            RETURN_ERROR(SRE_ERROR_STATE);

        state->ptr = ptr;

        ctx->count = ctx->u.rep->count + 1;

        if (ctx->count < isize(ctx->u.rep->pattern[1])) {
            // not enough matches
            ctx->u.rep->count = ctx->count;
            DO_JUMP(JUMP_MAX_UNTIL_1, jump_max_until_1, ctx->u.rep->pattern + 3);
            if (ret) {
                RETURN_ON_ERROR(ret);
                RETURN_SUCCESS;
            }
            ctx->u.rep->count = ctx->count - 1;
            state->ptr        = ptr;
            RETURN_FAILURE;
        }

        if ((ctx->count < isize(ctx->u.rep->pattern[2]) ||
             ctx->u.rep->pattern[2] == SRE_MAXREPEAT) &&
            state->ptr != ctx->u.rep->last_ptr) {
            // we may have enough matches, but if we can match another item,
            // do so
            ctx->u.rep->count = ctx->count;
            LASTMARK_SAVE();
            MARK_PUSH(ctx->lastmark);
            // zero-width match protection
            LAST_PTR_PUSH();
            ctx->u.rep->last_ptr = state->ptr;
            DO_JUMP(JUMP_MAX_UNTIL_2, jump_max_until_2, ctx->u.rep->pattern + 3);
            LAST_PTR_POP();
            if (ret) {
                MARK_POP_DISCARD(ctx->lastmark);
                RETURN_ON_ERROR(ret);
                RETURN_SUCCESS;
            }
            MARK_POP(ctx->lastmark);
            LASTMARK_RESTORE();
            ctx->u.rep->count = ctx->count - 1;
            state->ptr        = ptr;
        }

        // cannot match more repeated items here.  make sure the tail matches
        state->repeat = ctx->u.rep->prev;
        state->save_marks--;
        DO_JUMP(JUMP_MAX_UNTIL_3, jump_max_until_3, pattern);
        state->repeat = ctx->u.rep; // restore repeat before return
        state->save_marks++;

        RETURN_ON_SUCCESS(ret);
        state->ptr = ptr;
        RETURN_FAILURE;

    case SRE_OP_MIN_UNTIL:
        // minimizing repeat
        // <REPEAT> <skip> <1=min> <2=max> item <MIN_UNTIL> tail

        ctx->u.rep = state->repeat;
        if (!ctx->u.rep)
            RETURN_ERROR(SRE_ERROR_STATE);

        state->ptr = ptr;

        ctx->count = ctx->u.rep->count + 1;

        if (ctx->count < isize(ctx->u.rep->pattern[1])) {
            // not enough matches
            ctx->u.rep->count = ctx->count;
            DO_JUMP(JUMP_MIN_UNTIL_1, jump_min_until_1, ctx->u.rep->pattern + 3);
            if (ret) {
                RETURN_ON_ERROR(ret);
                RETURN_SUCCESS;
            }
            ctx->u.rep->count = ctx->count - 1;
            state->ptr        = ptr;
            RETURN_FAILURE;
        }

        // see if the tail matches
        state->repeat = ctx->u.rep->prev;
        state->save_marks--;

        LASTMARK_SAVE();
        if (state->save_marks)
            MARK_PUSH(ctx->lastmark);

        DO_JUMP(JUMP_MIN_UNTIL_2, jump_min_until_2, pattern);
        // save_marks is balanced across the jump, so this equals the value
        // tested for MARK_PUSH above
        pushed        = state->save_marks != 0;
        state->repeat = ctx->u.rep; // restore repeat before return
        state->save_marks++;

        if (ret) {
            if (pushed)
                MARK_POP_DISCARD(ctx->lastmark);
            RETURN_ON_ERROR(ret);
            RETURN_SUCCESS;
        }
        if (pushed)
            MARK_POP(ctx->lastmark);
        LASTMARK_RESTORE();

        state->ptr = ptr;

        if ((ctx->count >= isize(ctx->u.rep->pattern[2]) &&
             ctx->u.rep->pattern[2] != SRE_MAXREPEAT) ||
            state->ptr == ctx->u.rep->last_ptr)
            RETURN_FAILURE;

        ctx->u.rep->count = ctx->count;
        // zero-width match protection
        LAST_PTR_PUSH();
        ctx->u.rep->last_ptr = state->ptr;
        DO_JUMP(JUMP_MIN_UNTIL_3, jump_min_until_3, ctx->u.rep->pattern + 3);
        LAST_PTR_POP();
        if (ret) {
            RETURN_ON_ERROR(ret);
            RETURN_SUCCESS;
        }
        ctx->u.rep->count = ctx->count - 1;
        state->ptr        = ptr;
        RETURN_FAILURE;

    case SRE_OP_POSSESSIVE_REPEAT:
        // <POSSESSIVE_REPEAT> <skip> <1=min> <2=max> pattern <SUCCESS> tail
        state->ptr = ptr;

        // Capture groups in the body can be revisited on backtracking
        // between iterations, so their marks are saved and restored.
        state->save_marks++;

        ctx->count = 0;

        // Check for minimum required matches.
        while (ctx->count < isize(pattern[1])) {
            // not enough matches
            DO_JUMP0(JUMP_POSS_REPEAT_1, jump_poss_repeat_1, &pattern[3]);
            if (ret) {
                RETURN_ON_ERROR(ret);
                ctx->count++;
            } else {
                state->ptr = ptr;
                state->save_marks--;
                RETURN_FAILURE;
            }
        }

        // Clear the context's input pointer so that it does not match the
        // global state, and the loop below is entered.
        ptr = nullptr;

        // Keep trying the sub-pattern until the end is reached.
        while ((ctx->count < isize(pattern[2]) || pattern[2] == SRE_MAXREPEAT) &&
               state->ptr != ptr) {
            LASTMARK_SAVE();
            MARK_PUSH(ctx->lastmark);

            // zero-width match protection: the last known good position
            ptr = static_cast<const C *>(state->ptr);

            DO_JUMP0(JUMP_POSS_REPEAT_2, jump_poss_repeat_2, &pattern[3]);

            if (ret) {
                MARK_POP_DISCARD(ctx->lastmark);
                RETURN_ON_ERROR(ret);
                ctx->count++;
            } else {
                MARK_POP(ctx->lastmark);
                LASTMARK_RESTORE();
                state->ptr = ptr;
                break;
            }
        }

        state->save_marks--;

        // Evaluate the tail: past the skip, and past the SUCCESS after it.
        pattern += pattern[0] + 1;
        ptr = static_cast<const C *>(state->ptr);
        goto dispatch;

    case SRE_OP_ATOMIC_GROUP:
        // <ATOMIC_GROUP> <skip> pattern <SUCCESS> tail
        state->ptr = ptr;

        DO_JUMP0(JUMP_ATOMIC_GROUP, jump_atomic_group, &pattern[1]);

        RETURN_ON_ERROR(ret);

        if (ret == 0) {
            state->ptr = ptr;
            RETURN_FAILURE;
        }

        // Evaluate the tail: past the skip and the SUCCESS after it.
        pattern += pattern[0];
        ptr = static_cast<const C *>(state->ptr);
        goto dispatch;

    case SRE_OP_GROUPREF:
    case SRE_OP_GROUPREF_IGNORE:
    case SRE_OP_GROUPREF_UNI_IGNORE:
    case SRE_OP_GROUPREF_LOC_IGNORE: {
        // match backreference
        SreCode op   = pattern[-1];
        int groupref = int(pattern[0]) * 2;
        if (groupref >= state->lastmark)
            RETURN_FAILURE;
        const C *p = static_cast<const C *>(state->mark[groupref]);
        const C *e = static_cast<const C *>(state->mark[groupref + 1]);
        if (!p || !e || e < p)
            RETURN_FAILURE;
        while (p < e) {
            if (ptr >= end)
                RETURN_FAILURE;
            bool same = op == SRE_OP_GROUPREF ? *ptr == *p
                        : op == SRE_OP_GROUPREF_IGNORE
                            ? sre_lower_ascii(*ptr) == sre_lower_ascii(*p)
                        : op == SRE_OP_GROUPREF_UNI_IGNORE
                            ? sre_lower_unicode(*ptr) == sre_lower_unicode(*p)
                            : sre_lower_locale(*ptr) == sre_lower_locale(*p);
            if (!same)
                RETURN_FAILURE;
            p++;
            ptr++;
        }
        pattern++;
        goto dispatch;
    }

    case SRE_OP_GROUPREF_EXISTS: {
        // <GROUPREF_EXISTS> <group> <skip> codeyes <JUMP> codeno ...
        int groupref = int(pattern[0]) * 2;
        if (groupref >= state->lastmark) {
            pattern += pattern[1];
            goto dispatch;
        }
        const C *p = static_cast<const C *>(state->mark[groupref]);
        const C *e = static_cast<const C *>(state->mark[groupref + 1]);
        if (!p || !e || e < p) {
            pattern += pattern[1];
            goto dispatch;
        }
        pattern += 2;
        goto dispatch;
    }

    case SRE_OP_ASSERT:
        // <ASSERT> <skip> <back> <pattern>
        if (usize(ptr - static_cast<const C *>(state->beginning)) < pattern[1])
            RETURN_FAILURE;
        state->ptr = ptr - pattern[1];
        DO_JUMP0(JUMP_ASSERT, jump_assert, pattern + 2);
        RETURN_ON_FAILURE(ret);
        pattern += pattern[0];
        goto dispatch;

    case SRE_OP_ASSERT_NOT:
        // <ASSERT_NOT> <skip> <back> <pattern>
        if (usize(ptr - static_cast<const C *>(state->beginning)) >= pattern[1]) {
            state->ptr = ptr - pattern[1];
            LASTMARK_SAVE();
            if (state->save_marks)
                MARK_PUSH(ctx->lastmark);

            DO_JUMP0(JUMP_ASSERT_NOT, jump_assert_not, pattern + 2);
            if (ret) {
                if (state->save_marks)
                    MARK_POP_DISCARD(ctx->lastmark);
                RETURN_ON_ERROR(ret);
                RETURN_FAILURE;
            }
            if (state->save_marks)
                MARK_POP(ctx->lastmark);
            LASTMARK_RESTORE();
        }
        pattern += pattern[0];
        goto dispatch;

    case SRE_OP_FAILURE:
        RETURN_FAILURE;

    default:
        RETURN_ERROR(SRE_ERROR_ILLEGAL);
    }

exit:
    ctx_pos = ctx->last_ctx_pos;
    jump    = ctx->jump;
    DATA_STACK_POP_DISCARD(sizeof(Ctx));
    if (ctx_pos == -1) {
        state->sigcount = sigcount;
        return leave(ret);
    }
    DATA_LOOKUP_AT(ctx, ctx_pos);

    switch (jump) {
    case JUMP_MAX_UNTIL_2:
        goto jump_max_until_2;
    case JUMP_MAX_UNTIL_3:
        goto jump_max_until_3;
    case JUMP_MIN_UNTIL_2:
        goto jump_min_until_2;
    case JUMP_MIN_UNTIL_3:
        goto jump_min_until_3;
    case JUMP_BRANCH:
        goto jump_branch;
    case JUMP_MAX_UNTIL_1:
        goto jump_max_until_1;
    case JUMP_MIN_UNTIL_1:
        goto jump_min_until_1;
    case JUMP_POSS_REPEAT_1:
        goto jump_poss_repeat_1;
    case JUMP_POSS_REPEAT_2:
        goto jump_poss_repeat_2;
    case JUMP_REPEAT:
        goto jump_repeat;
    case JUMP_REPEAT_ONE_1:
        goto jump_repeat_one_1;
    case JUMP_REPEAT_ONE_2:
        goto jump_repeat_one_2;
    case JUMP_MIN_REPEAT_ONE:
        goto jump_min_repeat_one;
    case JUMP_ATOMIC_GROUP:
        goto jump_atomic_group;
    case JUMP_ASSERT:
        goto jump_assert;
    case JUMP_ASSERT_NOT:
        goto jump_assert_not;
    case JUMP_NONE:
        break;
    }

    return leave(ret); // should never get here
}

// need to reset capturing groups between two match calls in loops
void reset_capture_group(SreState *state)
{
    state->lastmark = state->lastindex = -1;
}

// The search arms, for a suspension to come back to.
enum : int { ARM_NONE, ARM_LITERAL, ARM_PREFIX, ARM_CHARSET, ARM_FIRST, ARM_GENERAL };

template <class C>
isize sre_search_t(SreState *state, const SreCode *pattern)
{
    const C *ptr           = static_cast<const C *>(state->start);
    const C *end           = static_cast<const C *>(state->end);
    isize status           = 0;
    isize prefix_len       = 0;
    isize prefix_skip      = 0;
    const SreCode *prefix  = nullptr;
    const SreCode *charset = nullptr;
    const SreCode *overlap = nullptr;
    SreCode flags          = 0;
    isize i                = 0;
    C c                    = 0;
    int arm                = ARM_NONE;

    if (state->suspended) {
        // Where the match stopped: everything above the INFO block is worked
        // out again, and only the position is the suspension's.
        arm = state->search_arm;
        ptr = static_cast<const C *>(state->search_ptr);
    } else if (ptr > end) {
        return 0;
    }

    if (pattern[0] == SRE_OP_INFO) {
        // <INFO> <1=skip> <2=flags> <3=min> <4=max> <5=prefix info>
        flags = pattern[2];

        if (arm == ARM_NONE) {
            if (pattern[3] && usize(end - ptr) < pattern[3])
                return 0;
        }
        if (pattern[3] > 1) {
            // adjust end point (but make sure we leave at least one character
            // in there, so literal search will work)
            end -= pattern[3] - 1;
            if (end <= ptr)
                end = ptr;
        }

        if (flags & SRE_INFO_PREFIX) {
            // <length> <skip> <prefix data> <overlap data>
            prefix_len  = isize(pattern[5]);
            prefix_skip = isize(pattern[6]);
            prefix      = pattern + 7;
            overlap     = prefix + prefix_len - 1;
        } else if (flags & SRE_INFO_CHARSET) {
            // <charset>
            charset = pattern + 5;
        }

        pattern += 1 + pattern[1];
    }

    switch (arm) {
    case ARM_LITERAL:
        c   = C(prefix[0]);
        end = static_cast<const C *>(state->end);
        goto resume_literal;
    case ARM_PREFIX:
        end = static_cast<const C *>(state->end);
        i   = prefix_len;
        goto resume_prefix;
    case ARM_CHARSET:
        end = static_cast<const C *>(state->end);
        goto resume_charset;
    case ARM_FIRST:
        goto resume_first;
    case ARM_GENERAL:
        // The general case runs to the adjusted end.
        goto resume_general;
    default:
        break;
    }

    if (prefix_len == 1) {
        // pattern starts with a literal character
        c = C(prefix[0]);
        if (SreCode(c) != prefix[0])
            return 0; // literal can't match: doesn't fit in char width
        end                 = static_cast<const C *>(state->end);
        state->must_advance = false;
        while (ptr < end) {
            while (*ptr != c) {
                if (++ptr >= end)
                    return 0;
            }
            state->start = ptr;
            state->ptr   = ptr + prefix_skip;
            if (flags & SRE_INFO_LITERAL)
                return 1; // we got all of it
            status = sre_match_t<C>(state, pattern + 2 * prefix_skip, 0);
        resume_literal:
            if (arm != ARM_NONE) {
                arm    = ARM_NONE;
                status = sre_match_t<C>(state, pattern + 2 * prefix_skip, 0);
            }
            if (status == SRE_SUSPEND) {
                state->search_arm = ARM_LITERAL;
                state->search_ptr = ptr;
                return status;
            }
            if (status != 0)
                return status;
            ++ptr;
            reset_capture_group(state);
        }
        return 0;
    }

    if (prefix_len > 1) {
        // pattern starts with a known prefix.  use the overlap table to skip
        // forward as fast as we possibly can
        end = static_cast<const C *>(state->end);
        if (prefix_len > end - ptr)
            return 0;
        for (i = 0; i < prefix_len; i++)
            if (SreCode(C(prefix[i])) != prefix[i])
                return 0; // literal can't match: doesn't fit in char width
        while (ptr < end) {
            c = C(prefix[0]);
            while (*ptr++ != c) {
                if (ptr >= end)
                    return 0;
            }
            if (ptr >= end)
                return 0;

            i                   = 1;
            state->must_advance = false;
            do {
                if (*ptr == C(prefix[i])) {
                    if (++i != prefix_len) {
                        if (++ptr >= end)
                            return 0;
                        continue;
                    }
                    // found a potential match
                    state->start = ptr - (prefix_len - 1);
                    state->ptr   = ptr - (prefix_len - prefix_skip - 1);
                    if (flags & SRE_INFO_LITERAL)
                        return 1; // we got all of it
                    status = sre_match_t<C>(state, pattern + 2 * prefix_skip, 0);
                resume_prefix:
                    if (arm != ARM_NONE) {
                        arm    = ARM_NONE;
                        status = sre_match_t<C>(state, pattern + 2 * prefix_skip, 0);
                    }
                    if (status == SRE_SUSPEND) {
                        state->search_arm = ARM_PREFIX;
                        state->search_ptr = ptr;
                        return status;
                    }
                    if (status != 0)
                        return status;
                    // close but no cigar -- try again
                    if (++ptr >= end)
                        return 0;
                    reset_capture_group(state);
                }
                i = isize(overlap[i]);
            } while (i != 0);
        }
        return 0;
    }

    if (charset) {
        // pattern starts with a character from a known set
        end                 = static_cast<const C *>(state->end);
        state->must_advance = false;
        for (;;) {
            while (ptr < end && !sre_charset(charset, *ptr))
                ptr++;
            if (ptr >= end)
                return 0;
            state->start = ptr;
            state->ptr   = ptr;
            status       = sre_match_t<C>(state, pattern, 0);
        resume_charset:
            if (arm != ARM_NONE) {
                arm    = ARM_NONE;
                status = sre_match_t<C>(state, pattern, 0);
            }
            if (status == SRE_SUSPEND) {
                state->search_arm = ARM_CHARSET;
                state->search_ptr = ptr;
                return status;
            }
            if (status != 0)
                break;
            ptr++;
            reset_capture_group(state);
        }
    } else {
        // general case
        state->start = state->ptr = ptr;
        status                    = sre_match_t<C>(state, pattern, 1);
    resume_first:
        if (arm == ARM_FIRST) {
            arm    = ARM_NONE;
            status = sre_match_t<C>(state, pattern, 1);
        }
        if (status == SRE_SUSPEND) {
            state->search_arm = ARM_FIRST;
            state->search_ptr = ptr;
            return status;
        }
        state->must_advance = false;
        if (status == 0 && pattern[0] == SRE_OP_AT &&
            (pattern[1] == SRE_AT_BEGINNING || pattern[1] == SRE_AT_BEGINNING_STRING)) {
            state->start = state->ptr = ptr = end;
            return 0;
        }
        while (status == 0 && ptr < end) {
            ptr++;
            reset_capture_group(state);
            state->start = state->ptr = ptr;
            status                    = sre_match_t<C>(state, pattern, 0);
        resume_general:
            if (arm == ARM_GENERAL) {
                arm    = ARM_NONE;
                status = sre_match_t<C>(state, pattern, 0);
            }
            if (status == SRE_SUSPEND) {
                state->search_arm = ARM_GENERAL;
                state->search_ptr = ptr;
                return status;
            }
        }
    }

    return status;
}

// ----------------------------------------------------------- validation

// Code validation, as sre.c has it: the code is position-independent, every
// jump is forward, and jumps do not cross.
#define FAIL return -1
#define GET_OP           \
    do {                 \
        if (code >= end) \
            FAIL;        \
        op = *code++;    \
    } while (0)
#define GET_ARG          \
    do {                 \
        if (code >= end) \
            FAIL;        \
        arg = *code++;   \
    } while (0)
#define GET_SKIP_ADJ(adj)                     \
    do {                                      \
        if (code >= end)                      \
            FAIL;                             \
        skip = *code;                         \
        if (skip - (adj) > usize(end - code)) \
            FAIL;                             \
        code++;                               \
    } while (0)
#define GET_SKIP GET_SKIP_ADJ(0)

int validate_category(SreCode arg)
{
    return arg <= SRE_CATEGORY_LAST;
}

int validate_charset(const SreCode *code, const SreCode *end)
{
    SreCode op;
    SreCode arg;
    SreCode offset;

    while (code < end) {
        GET_OP;
        switch (op) {
        case SRE_OP_NEGATE:
            break;
        case SRE_OP_LITERAL:
            GET_ARG;
            break;
        case SRE_OP_RANGE:
        case SRE_OP_RANGE_UNI_IGNORE:
            GET_ARG;
            GET_ARG;
            break;
        case SRE_OP_CHARSET:
            offset = 256 / SRE_CODE_BITS; // 256-bit bitmap
            if (offset > usize(end - code))
                FAIL;
            code += offset;
            break;
        case SRE_OP_BIGCHARSET:
            GET_ARG;                        // number of blocks
            offset = 256 / sizeof(SreCode); // 256-byte table
            if (offset > usize(end - code))
                FAIL;
            // each byte must point to a valid block
            for (int i = 0; i < 256; i++)
                if (reinterpret_cast<const unsigned char *>(code)[i] >= arg)
                    FAIL;
            code += offset;
            offset = arg * (256 / SRE_CODE_BITS); // 256-bit bitmap times arg
            if (offset > usize(end - code))
                FAIL;
            code += offset;
            break;
        case SRE_OP_CATEGORY:
            GET_ARG;
            if (!validate_category(arg))
                FAIL;
            break;
        default:
            FAIL;
        }
    }
    return 0;
}

// 0 on success, -1 on failure, and 1 if the last op is JUMP.
int validate_inner(const SreCode *code, const SreCode *end, isize groups)
{
    SreCode op;
    SreCode arg;
    SreCode skip;

    if (code > end)
        FAIL;

    while (code < end) {
        GET_OP;
        switch (op) {
        case SRE_OP_MARK:
            // marks are not checked for nesting; the matcher copes
            GET_ARG;
            if (arg >= 2 * usize(groups))
                FAIL;
            break;

        case SRE_OP_LITERAL:
        case SRE_OP_NOT_LITERAL:
        case SRE_OP_LITERAL_IGNORE:
        case SRE_OP_NOT_LITERAL_IGNORE:
        case SRE_OP_LITERAL_UNI_IGNORE:
        case SRE_OP_NOT_LITERAL_UNI_IGNORE:
        case SRE_OP_LITERAL_LOC_IGNORE:
        case SRE_OP_NOT_LITERAL_LOC_IGNORE:
            GET_ARG;
            break;

        case SRE_OP_SUCCESS:
        case SRE_OP_FAILURE:
            break;

        case SRE_OP_AT:
            GET_ARG;
            if (arg > SRE_AT_UNI_NON_BOUNDARY)
                FAIL;
            break;

        case SRE_OP_CATEGORY:
            GET_ARG;
            if (!validate_category(arg))
                FAIL;
            break;

        case SRE_OP_ANY:
        case SRE_OP_ANY_ALL:
            break;

        case SRE_OP_IN:
        case SRE_OP_IN_IGNORE:
        case SRE_OP_IN_UNI_IGNORE:
        case SRE_OP_IN_LOC_IGNORE:
            GET_SKIP;
            // Stop 1 before the end; we check the FAILURE below
            if (skip < 2 || validate_charset(code, code + skip - 2))
                FAIL;
            if (code[skip - 2] != SRE_OP_FAILURE)
                FAIL;
            code += skip - 1;
            break;

        case SRE_OP_INFO: {
            // <INFO> <1=skip> <2=flags> <3=min> <4=max>, and a prefix or a
            // charset when the flags say so
            SreCode flags;
            const SreCode *newcode;
            GET_SKIP;
            if (skip < 1)
                FAIL;
            newcode = code + skip - 1;
            GET_ARG;
            flags = arg;
            GET_ARG;
            GET_ARG;
            if ((flags & ~(SRE_INFO_PREFIX | SRE_INFO_LITERAL | SRE_INFO_CHARSET)) != 0)
                FAIL;
            if ((flags & SRE_INFO_PREFIX) && (flags & SRE_INFO_CHARSET))
                FAIL;
            if ((flags & SRE_INFO_LITERAL) && !(flags & SRE_INFO_PREFIX))
                FAIL;
            if (flags & SRE_INFO_PREFIX) {
                SreCode prefix_len;
                GET_ARG;
                prefix_len = arg;
                GET_ARG;
                // the prefix string
                if (code > newcode || prefix_len > usize(newcode - code))
                    FAIL;
                code += prefix_len;
                // the overlap table
                if (prefix_len > usize(newcode - code))
                    FAIL;
                for (SreCode i = 0; i < prefix_len; i++)
                    if (code[i] >= prefix_len)
                        FAIL;
                code += prefix_len;
            }
            if (flags & SRE_INFO_CHARSET) {
                if (code > newcode || validate_charset(code, newcode - 1))
                    FAIL;
                if (newcode[-1] != SRE_OP_FAILURE)
                    FAIL;
                code = newcode;
            } else if (code != newcode) {
                FAIL;
            }
            break;
        }

        case SRE_OP_BRANCH: {
            const SreCode *target = nullptr;
            for (;;) {
                GET_SKIP;
                if (skip == 0)
                    break;
                // Stop 2 before the end; we check the JUMP below
                if (skip < 3 || validate_inner(code, code + skip - 3, groups))
                    FAIL;
                code += skip - 3;
                // it ends with a JUMP, and each JUMP has the same target
                GET_OP;
                if (op != SRE_OP_JUMP)
                    FAIL;
                GET_SKIP;
                if (!target)
                    target = code + skip - 1;
                else if (code + skip - 1 != target)
                    FAIL;
            }
            if (code != target)
                FAIL;
            break;
        }

        case SRE_OP_REPEAT_ONE:
        case SRE_OP_MIN_REPEAT_ONE:
        case SRE_OP_POSSESSIVE_REPEAT_ONE: {
            SreCode min, max;
            GET_SKIP;
            GET_ARG;
            min = arg;
            GET_ARG;
            max = arg;
            if (min > max)
                FAIL;
            if (max > SRE_MAXREPEAT)
                FAIL;
            if (skip < 4 || validate_inner(code, code + skip - 4, groups))
                FAIL;
            code += skip - 4;
            GET_OP;
            if (op != SRE_OP_SUCCESS)
                FAIL;
            break;
        }

        case SRE_OP_REPEAT:
        case SRE_OP_POSSESSIVE_REPEAT: {
            SreCode op1 = op, min, max;
            GET_SKIP;
            GET_ARG;
            min = arg;
            GET_ARG;
            max = arg;
            if (min > max)
                FAIL;
            if (max > SRE_MAXREPEAT)
                FAIL;
            if (skip < 3 || validate_inner(code, code + skip - 3, groups))
                FAIL;
            code += skip - 3;
            GET_OP;
            if (op1 == SRE_OP_POSSESSIVE_REPEAT) {
                if (op != SRE_OP_SUCCESS)
                    FAIL;
            } else if (op != SRE_OP_MAX_UNTIL && op != SRE_OP_MIN_UNTIL) {
                FAIL;
            }
            break;
        }

        case SRE_OP_ATOMIC_GROUP:
            GET_SKIP;
            if (skip < 2 || validate_inner(code, code + skip - 2, groups))
                FAIL;
            code += skip - 2;
            GET_OP;
            if (op != SRE_OP_SUCCESS)
                FAIL;
            break;

        case SRE_OP_GROUPREF:
        case SRE_OP_GROUPREF_IGNORE:
        case SRE_OP_GROUPREF_UNI_IGNORE:
        case SRE_OP_GROUPREF_LOC_IGNORE:
            GET_ARG;
            if (arg >= usize(groups))
                FAIL;
            break;

        case SRE_OP_GROUPREF_EXISTS: {
            // '(?(group)then|else)'
            GET_ARG;
            if (arg >= usize(groups))
                FAIL;
            GET_SKIP_ADJ(1);
            code--; // the skip is relative to the first arg
            if (skip < 2)
                FAIL;
            // With an else part the then part ends in a JUMP to past it;
            // without one, it does not.
            int rc = validate_inner(code + 1, code + skip - 1, groups);
            if (rc == 1) {
                code += skip - 2; // after the JUMP, at <skipno>
                GET_SKIP;
                if (skip < 1)
                    FAIL;
                rc = validate_inner(code, code + skip - 1, groups);
            }
            if (rc)
                FAIL;
            code += skip - 1;
            break;
        }

        case SRE_OP_ASSERT:
        case SRE_OP_ASSERT_NOT:
            GET_SKIP;
            GET_ARG; // 0 for lookahead, width for lookbehind
            code--;  // back up over arg to simplify the math below
            // Stop 1 before the end; we check the SUCCESS below
            if (skip < 2 || validate_inner(code + 1, code + skip - 2, groups))
                FAIL;
            code += skip - 2;
            GET_OP;
            if (op != SRE_OP_SUCCESS)
                FAIL;
            break;

        case SRE_OP_JUMP:
            if (code + 1 != end)
                FAIL;
            return 1;

        default:
            FAIL;
        }
    }
    return 0;
}

#undef FAIL

} // namespace

u32 sre_lower_ascii(u32 ch)
{
    return ch < 128 && ch >= 'A' && ch <= 'Z' ? ch + 32 : ch;
}

// Py_UNICODE_TOLOWER and TOUPPER: where the full mapping is not the simple
// one, its first codepoint -- so U+FB05 is cased, its upper being "ST".
u32 sre_lower_unicode(u32 ch)
{
    if (ch > 0x10ffff)
        return ch;
    if (ch < 128)
        return ch >= 'A' && ch <= 'Z' ? ch + 32 : ch;
    u32 full[UCD_MAP_MAX];
    ucd_map(ch, UcdMap::Lower, full);
    return full[0];
}

u32 sre_upper_unicode(u32 ch)
{
    if (ch > 0x10ffff)
        return ch;
    if (ch < 128)
        return ch >= 'a' && ch <= 'z' ? ch - 32 : ch;
    u32 full[UCD_MAP_MAX];
    ucd_map(ch, UcdMap::Upper, full);
    return full[0];
}

bool sre_ascii_iscased(u32 ch)
{
    return ch < 128 && ascii_alpha(ch);
}

bool sre_unicode_iscased(u32 ch)
{
    return ch != sre_lower_unicode(ch) || ch != sre_upper_unicode(ch);
}

void sre_state_clear(SreState *s)
{
    *s            = SreState{};
    s->lastmark   = -1;
    s->lastindex  = -1;
    s->resume     = -1;
    s->search_arm = ARM_NONE;
}

void sre_state_reset(SreState *s)
{
    s->lastmark   = -1;
    s->lastindex  = -1;
    s->repeat     = nullptr;
    s->resume     = -1;
    s->suspended  = false;
    s->search_arm = ARM_NONE;
    s->depth      = 0;
    data_stack_dealloc(s);
}

void sre_state_fini(SreState *s)
{
    data_stack_dealloc(s);
    if (s->mark)
        heap_free(static_cast<void *>(s->mark));
    s->mark = nullptr;
    repeat_pool_clear(s);
}

void sre_abandon(SreState *s)
{
    sre_state_reset(s);
    s->sigcount = 0;
}

isize sre_match(SreState *s, const SreCode *code)
{
    s->depth = 0;
    isize r  = s->charsize == 1 ? sre_match_t<u8>(s, code, 1) : sre_match_t<u32>(s, code, 1);
    if (r != SRE_SUSPEND)
        s->suspended = false;
    return r;
}

isize sre_search(SreState *s, const SreCode *code)
{
    s->depth = 0;
    isize r  = s->charsize == 1 ? sre_search_t<u8>(s, code) : sre_search_t<u32>(s, code);
    if (r != SRE_SUSPEND) {
        s->suspended  = false;
        s->search_arm = ARM_NONE;
    }
    return r;
}

bool sre_validate(const SreCode *code, usize n, isize groups)
{
    const SreCode *end = code + n;
    if (groups < 0 || usize(groups) > SRE_MAXGROUPS || code >= end || end[-1] != SRE_OP_SUCCESS)
        return false;
    return validate_inner(code, end - 1, groups) == 0;
}
