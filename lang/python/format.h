// The format-spec mini-language, and the three grammars that reach it.
//
// One engine, because format(), str.format, __format__ and an f-string's
// suffix all mean the same thing by ":>10.3f". `%` is a different and older
// grammar and is parsed separately, but it lands here too.
//
// Width and precision count characters, not octets: the grid is one codepoint
// per cell and everything else in this interpreter counts the same way.
#pragma once

#include "obj.h"

// [[fill]align][sign][#][0][width][,|_][.precision][type]
struct Spec {
    u32 fill      = ' ';
    char align    = 0; // < > ^ = , or 0 for the type's own default
    char sign     = 0; // + - space, or 0
    bool alt      = false;
    bool zero     = false;
    i32 width     = -1;
    char grouping = 0; // , _ or 0
    i32 precision = -1;
    char type     = 0;

    // `%`'s precision on an integer is a minimum digit count and not a
    // precision at all, so it arrives here rather than in `precision`.
    i32 min_digits = -1;
};

// False with a ValueError pending. `who` names the type in the complaint.
bool spec_parse(Str s, Spec &out, Str who);

// v formatted by `spec`, for a built-in type. A class instance is the caller's
// business: a slot cannot call Python, so __format__ is dispatched by the VM.
// `min_digits` is `%`'s integer precision, which is a digit count and has no
// spelling in the spec grammar; -1 everywhere else.
R format_builtin(Value v, Str spec, String &out, i32 min_digits = -1);

// The whole of format(v, spec) where v cannot answer __format__ itself.
R py_format(Value v, Str spec, String &out);

// Pad `body` into `out` by the spec, `body` being already sign-and-all. Used
// by str and by anything whose own conversion is done.
R format_pad(Str body, const Spec &s, char def_align, String &out);

// ------------------------------------------------------------- the % grammar

// `fmt % right` for a str. Nil with the error pending; the caller checks for a
// ContObj, which is this saying a value needs Python run before it is done.
Value str_mod(Value fmt, Value right);

// The same over octets, for bytes and bytearray.
Value bytes_mod(Value fmt, Value right);

// ------------------------------------------------------ str.format's grammar

// str.format(*args, **kwargs) and str.format_map(mapping), as a ContObj when a
// field's value answers __format__ in Python and a str when it does not.
Value str_format_call(Value self, Value args, Value kwargs, Value mapping);

// The conversion an f-string field or a replacement field asks for.
enum : u32 { CONV_NONE = 0, CONV_STR = 's', CONV_REPR = 'r', CONV_ASCII = 'a' };

// repr(v) with everything above ASCII escaped, which is `!a` and `%a`.
R py_ascii(Value v, String &out);
