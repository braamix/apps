// The package's share directory, where the examples live.
#pragma once

#include "kernel/str.h"
#include "kernel/string.h"
#include "kernel/task.h"

// Finds the directory once, at startup. Empty when there is none.
Task<void> epath_init();

// <datadir>/<name> in `out`, or false when there is no datadir, `name` is not
// a bare name, or the join fails.
bool epath_file(Str name, String &out);
