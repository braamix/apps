// Slices, ranges and the iterators. Everything the `for` loop and the
// subscript with a colon in it reach.
#pragma once

#include "obj.h"

extern const Type slice_type;
extern const Type range_type;

struct SliceObj : Obj {
    Value start, stop, step;
};

Value slice_new(Value start, Value stop, Value step);

inline bool is_slice(Value v)
{
    return v.is_obj() && v.obj()->type == &slice_type;
}

// Resolve a slice against a length: the first index, one past the last, the
// step, and how many items it selects. `stop` is the adjusted index rather
// than the last item's, which is what a slice of a range reports as its own.
// False leaves the error pending.
bool slice_resolve(Value v, usize len, i64 &start, i64 &stop, i64 &step, usize &count);

struct RangeObj : Obj {
    i64 start, stop, step;
};

// Nil with ValueError pending on a zero step.
Value range_new(i64 start, i64 stop, i64 step);

// An iterator over anything with a len and an integer getitem: str, bytes,
// tuple, list.
Value seq_iter(Value seq);

// An iterator over a dict's keys or a set's members, in insertion order.
Value table_iter(Value owner);

// enumerate(): an iterator over another, pairing each item with its index.
Value enum_iter(Value seq, i64 start);
