// The command line turned into an AsArgs, and where each output goes. Plain
// C++: nothing here reads a file.
#pragma once

#include "as.h"
#include "diag.h"
#include "kernel/span.h"
#include "kernel/string.h"
#include "kernel/vec.h"

struct AsArgs {
    AsConfig as;
    Vec<Str> inputs;
    Str output; // -o; one input only
    bool help = false;
};

// False with a message in `diag` for an argument that is not understood, or
// for a command line that cannot be carried out.
bool parse_args(Span<const Str> words, AsArgs &a, Diag &diag);

// Where `input` is written: -o, or its base name in the current directory
// with its extension replaced by .o, or .wasm with --module. False with a
// message when that would overwrite the input.
bool output_name(const AsArgs &a, Str input, String &out, Diag &diag);
