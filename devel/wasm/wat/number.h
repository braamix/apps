// Literals to bits, §4 of the language: the text of a Nat, Int or Float
// token, underscores and all. Floats are rounded exactly, to nearest and
// ties to even, straight to the width asked for. False for a value out of
// range, or text that is not a number of that kind.
#pragma once

#include "kernel/str.h"
#include "kernel/types.h"

// uN: no sign, below 2^bits.
bool parse_uint(Str s, u32 bits, u64 &v);

// iN: a sign or none, from -2^(bits-1) to 2^bits - 1; a negative value
// as its two's complement in `bits` bits.
bool parse_int(Str s, u32 bits, u64 &v);

// fN, integers included: 1, -0x10, 1.5e3, inf, -nan, nan:0x1.
bool parse_f32(Str s, u32 &bits);
bool parse_f64(Str s, u64 &bits);
