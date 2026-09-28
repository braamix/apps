// The library's one awaiting part: reading and writing whole files for a
// tool's front end. Kept apart, so the pure core never includes it.
#pragma once

#include "kernel/str.h"
#include "kernel/string.h"
#include "proc/io.h"

Task<Result<void>> say(u32 fd, Str s);

Task<Result<String>> slurp(Str path);

// Writes `s` to a file, created if need be; `flags` add to the write, such as
// SYS_O_TRUNC to replace it or SYS_O_EXCL to refuse an existing one.
Task<Result<void>> spill(Str path, Str s, u32 flags = SYS_O_TRUNC);
