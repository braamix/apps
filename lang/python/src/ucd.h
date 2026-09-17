// The Unicode character database: what a codepoint is, how its case maps,
// what it decomposes into and what it is called. The tables are ucddb.cpp,
// which tools/mkucd.py writes from the host CPython's own answers.
#pragma once

#include "kernel/str.h"
#include "kernel/string.h"
#include "kernel/types.h"
#include "kernel/vec.h"

struct UcdRec {
    u8 cat, bidi, width, comb;
    u16 flags;
    u8 decimal, digit, numeric;
    u16 casing;
};

struct UcdCase {
    i32 lower, upper, title;
    u16 xlower, xupper, xtitle, xfold;
};

// UcdRec::flags. The same bits as tools/mkucd.py's F_*.
enum : u16 {
    UCD_ALPHA          = 1 << 0,
    UCD_DECIMAL        = 1 << 1,
    UCD_DIGIT          = 1 << 2,
    UCD_NUMERIC        = 1 << 3,
    UCD_LOWER          = 1 << 4,
    UCD_UPPER          = 1 << 5,
    UCD_TITLE          = 1 << 6,
    UCD_SPACE          = 1 << 7,
    UCD_LINEBREAK      = 1 << 8,
    UCD_PRINTABLE      = 1 << 9,
    UCD_XID_START      = 1 << 10,
    UCD_XID_CONTINUE   = 1 << 11,
    UCD_CASED          = 1 << 12,
    UCD_CASE_IGNORABLE = 1 << 13,
    UCD_MIRRORED       = 1 << 14,
};

extern const Str UCD_VERSION;

const UcdRec &ucd_rec(u32 cp);

inline bool ucd_is(u32 cp, u16 flag)
{
    return (ucd_rec(cp).flags & flag) != 0;
}

// The two-letter category, the bidirectional class and the East Asian width,
// as unicodedata spells them. An unassigned codepoint is "Cn", "" and "N".
Str ucd_category(u32 cp);
Str ucd_bidirectional(u32 cp);
Str ucd_east_asian_width(u32 cp);
u8 ucd_combining(u32 cp);

// -1 where the codepoint has no such value.
int ucd_decimal(u32 cp);
int ucd_digit(u32 cp);
bool ucd_numeric(u32 cp, f64 &out);

// The one-codepoint mappings, which a regular expression folds with.
u32 ucd_lower(u32 cp);
u32 ucd_upper(u32 cp);

// The full mappings str's methods use: up to three codepoints into `out`,
// and how many.
enum class UcdMap : u8 { Lower, Upper, Title, Fold };
constexpr usize UCD_MAP_MAX = 3;
usize ucd_map(u32 cp, UcdMap how, u32 *out);

// unicodedata.decomposition: "<compat> 0020 0301", or empty.
bool ucd_decomposition(u32 cp, String &out);

enum class UcdForm : u8 { NFC, NFD, NFKC, NFKD };

// The text in `cps`, normalized in place; by Unicode 3.2.0's decompositions
// where `v320`. False only when out of memory.
bool ucd_normalize(UcdForm form, Vec<u32> &cps, bool v320 = false);

// Unicode 3.2.0, which unicodedata.ucd_3_2_0 answers and the idna codec
// reads: a record for each character it answered otherwise than this version
// does. Each field is an index as UcdRec has it, a value, UCD_OLD_SAME for
// what this version says, or UCD_OLD_NONE for no value at all.
struct UcdOld {
    u32 cp;
    u8 cat, bidi, width, mirrored, decimal, numeric;
};

enum : u8 { UCD_OLD_NONE = 0xfe, UCD_OLD_SAME = 0xff };

// 3.2.0 had not assigned it, and answers as for any unassigned codepoint.
bool ucd_old_unassigned(u32 cp);

// Its 3.2.0 record; null where 3.2.0 answers as this version does.
const UcdOld *ucd_old(u32 cp);

f64 ucd_old_number(u8 index);

// The names behind a record's category, bidi and width indices.
Str ucd_category_name(u8 index);
Str ucd_bidi_name(u8 index);
Str ucd_width_name(u8 index);

// The character's name, appended; false where it has none.
bool ucd_name(u32 cp, String &out);

// A name to what it names, case-blind. Aliases count; a named sequence counts
// only where `sequences` says so, which is unicodedata.lookup and not \N{}.
// False when nothing has that name.
bool ucd_lookup(Str name, bool sequences, Vec<u32> &out);

// The longest name ucd_lookup can match; anything longer is refused early.
usize ucd_longest_name();

// What int(), float() and complex() read out of a str: whitespace as a space,
// a decimal digit of any script as its ASCII digit, ASCII as itself and
// anything else as '?', which no number grammar takes.
bool ucd_ascii_number(Str text, String &out);
