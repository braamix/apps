// The command line, in llvm-size's spelling, turned into a SizeArgs. Plain
// C++: nothing here reads a file.
#pragma once

#include "diag.h"
#include "kernel/span.h"
#include "kernel/vec.h"
#include "size.h"

struct SizeArgs {
    SizeConfig size;
    Vec<Str> inputs; // a.out if none; - is stdin
    bool help = false;
};

// False with a message in `diag` for an argument that is not understood. A
// --format or --radix value that is not known is an error too, but the
// default stands and the files are still sized, as llvm-size does.
bool parse_args(Span<const Str> words, SizeArgs &a, Diag &diag);
