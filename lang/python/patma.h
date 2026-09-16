// What a match statement asks of its subject at run time: whether it is a
// sequence or a mapping, its values for a mapping pattern's keys, and its
// attributes for a class pattern.
//
// The last two may call Python -- a mapping's own `get`, an __instancecheck__,
// a property -- so each hands back either the answer or a ContObj whose
// answer it will be, which the VM lands like a call's.
#pragma once

#include "obj.h"

// PATMA_SEQ, PATMA_MAP or neither, for the subject's class.
u8 patma_kind(Value subject);

// The class a class pattern names matches its subject itself.
bool patma_self(Value cls);

// A class's own flags, as `__abc_tpflags__` or a registration sets them.
void patma_set(Value cls, u8 flags);

// MatchKeys: a tuple of the values under `keys`, or None when one is missing.
R patma_keys(Value subject, Value keys, Value &out);

// MatchClass: a tuple of `nargs` positional attributes and then the named
// ones, or None when the subject is not an instance or lacks one.
R patma_class(Value subject, Value cls, u32 nargs, Value names, Value &out);

// CopyDict: `dict(subject)`, for the `**rest` of a mapping pattern.
R patma_copy(Value subject, Value &out);
