// One typecode's worth of machine representation: what `array` stores and what
// `memoryview` reads back.
//
// The codes are `array`'s, which are `struct`'s less the ones that mean
// nothing to a homogeneous buffer, plus `u` for a codepoint. `_struct` has a
// format language of its own -- byte order, repeats, alignment -- so it does
// not share this; what it shares is the arithmetic, and that is small.
#pragma once

#include "obj.h"

enum : u8 { IT_INT, IT_UINT, IT_FLOAT, IT_CHAR, IT_BOOL };

struct ItemKind {
    char code;
    u8 width;
    u8 kind;
};

// Null for a code no typecode names.
const ItemKind *item_kind(char code);

// One item out of `octets` at byte offset `at`. Nil with the error pending.
Value item_get(Str octets, usize at, const ItemKind *k);

// One item into `octets` at byte offset `at`, native order. R::Err with the
// error pending when the value is the wrong kind or out of range.
R item_put(char *octets, usize at, const ItemKind *k, Value v);

// An `array` object's octets and its typecode, for memoryview: the array
// module is the one thing here that gives a buffer a width. False for
// anything that is not an array. Defined in arraymod.cpp.
bool array_bytes(Value v, Str &out, char &code);

// The same octets, writable. Null when `v` is not an array.
u8 *array_data(Value v);
