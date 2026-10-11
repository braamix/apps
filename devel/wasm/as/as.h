// as's core: LLVM's WebAssembly assembly in, a relocatable object out.
// Plain C++ over a source already in memory; only braam.cpp awaits.
#pragma once

#include "diag.h"
#include "kernel/str.h"
#include "kernel/vec.h"

// One source into `out`. False with the messages in `diag` when it cannot
// be assembled, and `out` is then not to be written.
bool assemble(Str name, Str source, Vec<u8> &out, Diag &diag);
