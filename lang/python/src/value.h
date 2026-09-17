// A Python value in one 32-bit word.
//
// Bit 0 set is a 31-bit signed integer; otherwise the word is a pointer to an
// Obj, which the heap aligns to at least 4. Zero is Nil -- no value at all,
// which is not None: None is an object, and has a type.
#pragma once

#include "kernel/types.h"

struct Obj;

struct Value {
    u32 w = 0;

    constexpr Value() = default;

    static constexpr i32 SMALL_MIN = -(1 << 30);
    static constexpr i32 SMALL_MAX = (1 << 30) - 1;

    static constexpr bool fits_small(i64 n) { return n >= SMALL_MIN && n <= SMALL_MAX; }

    static constexpr Value of_int(i32 n)
    {
        Value v;
        v.w = (u32(n) << 1) | 1;
        return v;
    }

    static Value of_obj(Obj *o)
    {
        Value v;
        v.w = u32(reinterpret_cast<usize>(o));
        return v;
    }

    constexpr bool is_nil() const { return w == 0; }

    constexpr bool is_int() const { return (w & 1) != 0; }

    constexpr bool is_obj() const { return w != 0 && (w & 1) == 0; }

    // Arithmetic, so the sign comes back.
    constexpr i32 as_int() const { return i32(w) >> 1; }

    Obj *obj() const { return reinterpret_cast<Obj *>(usize(w)); }

    constexpr bool operator==(Value o) const { return w == o.w; }

    constexpr bool operator!=(Value o) const { return w != o.w; }
};
