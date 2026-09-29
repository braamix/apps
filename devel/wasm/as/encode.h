// A resolved tree to a module's bytes: sections in their order, each once,
// empty ones left out, and every choice the binary format leaves open made
// as wabt's wat2wasm makes it. Plain C++; blocks nest on a stack of its
// own, not the native one.
#pragma once

#include "ast.h"
#include "diag.h"

// `m`, resolved, onto `out`: a relocatable object when `object`, by
// decisions 1 and 2 of Plan.md, and a module otherwise; with a name
// section of its ids too when `names`. False with a message in `diag` when
// out of memory.
bool encode(const wat::Module &m, bool object, bool names, Vec<u8> &out, Diag &diag);
