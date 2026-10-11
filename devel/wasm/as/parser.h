// A source into an Asm, statement by statement, as llvm's AsmParser and
// WebAssemblyAsmParser read it. Errors are worded and placed as theirs:
// file:line:col, the line, and a caret under the column.
#pragma once

#include "asm.h"
#include "diag.h"
#include "kernel/str.h"

namespace as {

// False when any statement was refused; every error is in `diag`.
bool parse(Str name, Str source, Asm &a, Diag &diag);

// An error or a warning at byte `at` of the source, in llvm's form.
void report(Diag &diag, Str name, Str source, u32 at, Str kind, Str msg);

} // namespace as
