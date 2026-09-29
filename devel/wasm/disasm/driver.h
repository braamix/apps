// The command line, in llvm-objdump's spelling, turned into DisasmArgs.
// Plain C++: nothing here reads a file.
#pragma once

#include "diag.h"
#include "disasm.h"
#include "kernel/span.h"
#include "kernel/vec.h"

struct DisasmArgs {
    DisasmConfig c;
    Vec<Str> inputs; // - is stdin
    bool help = false;
};

// False with a message in `diag` for an argument that is not understood.
bool parse_args(Span<const Str> words, DisasmArgs &a, Diag &diag);
