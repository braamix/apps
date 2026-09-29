// The command line, in llvm-nm's spelling, turned into an NmArgs. Plain C++:
// nothing here reads a file.
#pragma once

#include "diag.h"
#include "kernel/span.h"
#include "kernel/vec.h"
#include "nm.h"

struct NmArgs {
    NmConfig nm;
    Vec<Str> inputs; // - is stdin
    bool help    = false;
    bool version = false;
};

// False with a message in `diag` for an argument that is not understood. A
// --format, --radix or -X value that is not known is an error too, but the
// files are still read, as llvm-nm does.
bool parse_args(Span<const Str> words, NmArgs &a, Diag &diag);
