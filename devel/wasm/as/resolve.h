// Ids to numbers (§6 of the language), and the rest of what needs the
// whole module; wat.asdl's header says what the tree is after. Plain C++
// over a parsed tree, which it changes in place. Blocks nest on a stack of
// its own, not the native one. Messages are the reference interpreter's.
#pragma once

#include "ast.h"
#include "diag.h"

// `m`, parsed from `name`, resolved; new nodes in `arena`. False with a
// message in `diag`, name:line:col first, at the first error.
bool resolve(Str name, wat::Arena &arena, wat::Module &m, Diag &diag);
