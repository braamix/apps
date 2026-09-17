// The engine under `_sre`: Secret Labs' matcher, from CPython's
// Modules/_sre/sre_lib.h, over a code array re/_compiler.py emits.
//
// Its contexts were already on a data stack rather than the C stack, which is
// what ground rule 4 asks. What is added here is that a match can stop in the
// middle and be entered again: SRE_SUSPEND says the slice is spent, the
// contexts stay where they are, and the next call carries on from them.
#pragma once

#include "kernel/types.h"

using SreCode = u32;

constexpr u32 SRE_MAGIC    = 20260622;
constexpr u32 SRE_CODESIZE = 4;
// Py_ssize_t is 32 bits here, so these are CPython's 32-bit values.
constexpr SreCode SRE_MAXREPEAT = 0x7fffffff;
constexpr SreCode SRE_MAXGROUPS = 0x7fffffff / 4 / 2;

enum : SreCode {
    SRE_OP_FAILURE,
    SRE_OP_SUCCESS,
    SRE_OP_ANY,
    SRE_OP_ANY_ALL,
    SRE_OP_ASSERT,
    SRE_OP_ASSERT_NOT,
    SRE_OP_AT,
    SRE_OP_BRANCH,
    SRE_OP_CATEGORY,
    SRE_OP_CHARSET,
    SRE_OP_BIGCHARSET,
    SRE_OP_GROUPREF,
    SRE_OP_GROUPREF_EXISTS,
    SRE_OP_IN,
    SRE_OP_INFO,
    SRE_OP_JUMP,
    SRE_OP_LITERAL,
    SRE_OP_MARK,
    SRE_OP_MAX_UNTIL,
    SRE_OP_MIN_UNTIL,
    SRE_OP_NOT_LITERAL,
    SRE_OP_NEGATE,
    SRE_OP_RANGE,
    SRE_OP_REPEAT,
    SRE_OP_REPEAT_ONE,
    SRE_OP_SUBPATTERN,
    SRE_OP_MIN_REPEAT_ONE,
    SRE_OP_ATOMIC_GROUP,
    SRE_OP_POSSESSIVE_REPEAT,
    SRE_OP_POSSESSIVE_REPEAT_ONE,
    SRE_OP_GROUPREF_IGNORE,
    SRE_OP_IN_IGNORE,
    SRE_OP_LITERAL_IGNORE,
    SRE_OP_NOT_LITERAL_IGNORE,
    SRE_OP_GROUPREF_LOC_IGNORE,
    SRE_OP_IN_LOC_IGNORE,
    SRE_OP_LITERAL_LOC_IGNORE,
    SRE_OP_NOT_LITERAL_LOC_IGNORE,
    SRE_OP_GROUPREF_UNI_IGNORE,
    SRE_OP_IN_UNI_IGNORE,
    SRE_OP_LITERAL_UNI_IGNORE,
    SRE_OP_NOT_LITERAL_UNI_IGNORE,
    SRE_OP_RANGE_UNI_IGNORE,
};

enum : SreCode {
    SRE_AT_BEGINNING,
    SRE_AT_BEGINNING_LINE,
    SRE_AT_BEGINNING_STRING,
    SRE_AT_BOUNDARY,
    SRE_AT_NON_BOUNDARY,
    SRE_AT_END,
    SRE_AT_END_LINE,
    SRE_AT_END_STRING,
    SRE_AT_LOC_BOUNDARY,
    SRE_AT_LOC_NON_BOUNDARY,
    SRE_AT_UNI_BOUNDARY,
    SRE_AT_UNI_NON_BOUNDARY,
};

// The last of the 68 category codes; the codes themselves are in sre.cpp.
constexpr SreCode SRE_CATEGORY_LAST = 67;

enum : i32 {
    SRE_FLAG_IGNORECASE = 2,
    SRE_FLAG_LOCALE     = 4,
    SRE_FLAG_MULTILINE  = 8,
    SRE_FLAG_DOTALL     = 16,
    SRE_FLAG_UNICODE    = 32,
    SRE_FLAG_VERBOSE    = 64,
    SRE_FLAG_DEBUG      = 128,
    SRE_FLAG_ASCII      = 256,
};

enum : SreCode { SRE_INFO_PREFIX = 1, SRE_INFO_LITERAL = 2, SRE_INFO_CHARSET = 4 };

// What the engine answers besides 0 (no match) and 1 (a match).
enum : isize {
    SRE_ERROR_ILLEGAL = -1,
    SRE_ERROR_STATE   = -2,
    SRE_ERROR_MEMORY  = -9,
    SRE_SUSPEND       = -20, // the slice is spent; call again to go on
};

struct SreRepeat {
    isize count;
    const SreCode *pattern;
    const void *last_ptr;
    SreRepeat *prev;
    SreRepeat *pool_prev;
    SreRepeat *pool_next;
};

// SRE_STATE, less the Python half, which sremod.cpp keeps beside it.
struct SreState {
    const void *ptr;
    const void *beginning;
    const void *start;
    const void *end;
    isize pos, endpos;
    int charsize; // 1 or 4
    bool match_all;
    bool must_advance;
    int lastmark;
    int lastindex;
    const void **mark;
    int save_marks;
    char *data_stack;
    usize data_stack_size;
    usize data_stack_base;
    SreRepeat *repeat;
    SreRepeat *repeat_pool_used;
    SreRepeat *repeat_pool_unused;
    u32 sigcount;

    // The suspension. `depth` counts the match calls live, and only the
    // outermost may stop; `resume` is its context, -1 when nothing is parked.
    int depth;
    isize resume;
    // Where search() was when its match stopped, and on what.
    int search_arm;
    const void *search_ptr;
    // How many dispatches a slice is, and whether stopping is allowed at all.
    u32 slice;
    bool suspended;
};

// Zero everything; `groups` marks are the caller's to allocate.
void sre_state_clear(SreState *s);

// Between two searches of one state: the marks and the data stack go.
void sre_state_reset(SreState *s);

// Everything the state holds on the heap, marks included.
void sre_state_fini(SreState *s);

// The two entry points. A suspended state is resumed by calling the same one
// again with the same code.
isize sre_match(SreState *s, const SreCode *code);
isize sre_search(SreState *s, const SreCode *code);

// A parked match given up on: the next call starts afresh.
void sre_abandon(SreState *s);

// The code, checked before it is trusted. False when it is not well formed.
bool sre_validate(const SreCode *code, usize n, isize groups);

// The case helpers the compiler asks the module for.
u32 sre_lower_ascii(u32 ch);
u32 sre_lower_unicode(u32 ch);
u32 sre_upper_unicode(u32 ch);
bool sre_ascii_iscased(u32 ch);
bool sre_unicode_iscased(u32 ch);
