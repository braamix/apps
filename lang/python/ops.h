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
R py_delitem(Value v, Value key);
R py_contains(Value v, Value item, bool &out);

R py_getattr(Value v, StrObj *name, Value &out);

// Nil with TypeError pending when the type does not iterate.
Value py_iter(Value v);

// "'int' object is not iterable", pending. Always R::Err.
R not_iterable(Value v);

// NotImpl at the end of the iteration, Err on a failure.
R py_next(Value it, Value &out);

// Everything an iterable yields, as a fresh list. Null with the error pending.
ListObj *py_list_of(Value v);

// R::Err with TypeError pending when neither side answers.
R py_binop(Value a, Value b, Op op, Value &out);

// The TypeError for an operator neither side answers. A sequence on the left
// of `+` says it concatenates only its own kind, as CPython's sq_concat does.
R binop_failed(Value a, Value b, Op op);

// The same, but NotImpl rather than a TypeError when no type answered. The VM
// asks this first where the left operand is a built-in and the right a class:
// the left's own operator goes before the right's reflected one.
R py_binop_try(Value a, Value b, Op op, Value &out);

// `a op= b`. Only a list mutates; everything else is py_binop.
R py_inplace(Value a, Value b, Op op, Value &out);

R py_neg(Value a, Value &out);
R py_pos(Value a, Value &out);
R py_invert(Value a, Value &out);

// A sequence index: negative counts from the end, out of range is IndexError.
R index_of(Value key, usize len, usize &out);

// Shared by tuple and list, which compare and search alike.
R seq_eq(const Value *x, usize nx, const Value *y, usize ny, bool &out);
R seq_order(const Value *x, usize nx, const Value *y, usize ny, Cmp op, bool &out);
R seq_contains(const Value *x, usize n, Value item, bool &out);

// "<head>, not int" -- CPython's spelling of a wrong type -- pending, with the
// type name in quotes where `quoted`. Always R::Err.
R err_not(Str head, Value v, bool quoted = false);
