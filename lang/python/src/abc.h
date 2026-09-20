// `_abc`: what CPython's abc.py stands on when its accelerator is there.
#pragma once

#include "obj.h"

// Fill a module namespace with the eight names abc.py imports. `issubclass`
// is the builtin, which the subclass check asks for as a call rather than
// answering a virtual subclass itself.
bool abc_install(DictObj *into, Value issubclass);

// Whether the class still has an abstract method, and which: every name
// quoted and comma-separated in order, with `count` saying how many. This is
// what makes instantiating an abstract class an error.
bool abc_abstract(Value cls, String &names, usize &count);
