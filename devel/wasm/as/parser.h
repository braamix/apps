// Tokens to the tree of ast.h: §5 to §10 of the language, with the
// parse-time abbreviations wat.asdl's header lists. Plain C++ over a source
// in memory. Blocks and folded instructions nest on a stack of its own, not
// the native one. Messages are the reference interpreter's.
#pragma once

#include "ast.h"
#include "diag.h"

// `source` into `m`, its nodes in `arena`. False with a message in `diag`,
// name:line:col first, at the first error.
bool parse(Str name, Str source, wat::Arena &arena, wat::Module &m, Diag &diag);
