// The intern table: the same bytes always answer the same StrObj. Identifiers
// go here, so a name comparison is a pointer comparison.
//
// It is a root, so an interned string lives as long as the process. That is
// what CPython does with the names in a module, and it is why the table is
// traced rather than swept.
#pragma once

#include "kernel/str.h"

struct StrObj;

StrObj *str_intern(Str s);

// Whether `s` is the table's own object for its bytes.
bool str_is_interned(const StrObj *s);

// Called by the collector, from gc_collect.
void intern_mark();

usize intern_count();

// `__name` inside class `priv` is `_priv__name`, which is how a class keeps a
// name private; anything else is itself. Interned; null on OOM.
StrObj *py_mangle(StrObj *priv, Str name);
