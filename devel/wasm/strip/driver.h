// The command line, in llvm-strip's spelling, turned into a StripArgs. Plain
// C++: nothing here reads a file.
#pragma once

#include "diag.h"
#include "kernel/span.h"
#include "kernel/vec.h"
#include "strip.h"

struct StripArgs {
    StripConfig strip;
    Vec<Str> inputs;
    Str output; // -o; empty: in place
    bool help = false;
};

// False with a message in `diag` for an argument that is not understood, or
// for a command line that cannot be carried out.
bool parse_args(Span<const Str> words, StripArgs &a, Diag &diag);
