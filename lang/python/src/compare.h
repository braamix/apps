// Comparisons that have to call Python, driven from C++.
//
// py_cmp and py_eq cannot push a frame -- ground rule 2 -- so a class with a
// __lt__ cannot be sorted and one with an __eq__ cannot be found by a search
// written in C++. Each of these hands back a ContObj that owns the loop and
// asks for one comparison at a time; the VM drives it the way it drives any
// other suspended builtin.
#pragma once

#include "obj.h"

// Whether comparing this value needs a Python call: `order` asks about the
// four orderings, otherwise about equality.
bool cmp_is_python(Value v, bool order);
bool cmp_any_python(const Vec<Value> &xs, bool order);

// A stable sort. `vals` is the list to answer with and `keys` what to compare
// by, or Nil when they are the same list; `inplace` writes the order back into
// `vals` and answers None. Nil with the error pending.
Value cmp_sort(Value vals, Value keys, bool rev, bool inplace);

// min (`least`) or max over a list, by the same pairing of values and keys.
Value cmp_fold(Value vals, Value keys, bool least);

// What a search over a list is to answer.
enum : u32 { CMP_INDEX, CMP_COUNT, CMP_IN, CMP_NOTIN };

// index, count, `in` and remove: `from` and `end` bound the span searched.
Value cmp_find(Value items, Value target, u32 what, usize from, usize end);

// `a == b` (or `!=`) between two sequences, item by item.
Value cmp_seq(Value a, Value b, bool ne);

// A list or a tuple, as a list. Nil for anything else.
Value cmp_items(Value v);

// Both are lists, or both are tuples. A list and a tuple are never equal
// however their items compare, so they are not compared item by item.
bool cmp_same_kind(Value a, Value b);
