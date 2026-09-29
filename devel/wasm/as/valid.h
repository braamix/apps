// Validation (§3 of the core spec) of a resolved tree: the reference
// interpreter's algorithm, in its order and with its messages, so that `as`
// refuses what an engine would. Plain C++; blocks nest on a stack of its
// own, not the native one.
#pragma once

#include "ast.h"
#include "diag.h"

// `m`, resolved from `name`, valid; keys for its types go into `arena`.
// False with a message in `diag`, name:line:col first, at the first error.
bool validate(Str name, wat::Arena &arena, const wat::Module &m, Diag &diag);
