// The generic operations: what the VM dispatches through, and what resolves
// the fallbacks the slots do not -- the number tower, len for truth, and so
// on.
#pragma once

#include "obj.h"

bool py_truth(Value v);

R py_hash(Value v, u32 &out);
R py_eq(Value a, Value b, bool &out);
R py_cmp(Value a, Value b, Cmp op, bool &out);

R py_repr(Value v, String &out);
R py_str(Value v, String &out);

R py_len(Value v, usize &out);
R py_getitem(Value v, Value key, Value &out);
R py_setitem(Value v, Value key, Value item);
R py_contains(Value v, Value item, bool &out);

// R::Err with TypeError pending when neither side answers.
R py_binop(Value a, Value b, Op op, Value &out);
R py_neg(Value a, Value &out);

// A sequence index: negative counts from the end, out of range is IndexError.
R index_of(Value key, usize len, usize &out);

// Shared by tuple and list, which compare and search alike.
R seq_eq(const Value *x, usize nx, const Value *y, usize ny, bool &out);
R seq_order(const Value *x, usize nx, const Value *y, usize ny, Cmp op, bool &out);
R seq_contains(const Value *x, usize n, Value item, bool &out);
