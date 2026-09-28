// The output module: sections in wasm-ld's order, function bodies and data
// copied with their relocations applied, and the braam stamp last.
#pragma once

#include "kernel/vec.h"

struct Linker;

// False on an error, which is in the Diag.
bool write_module(Linker &l, Vec<u8> &out);
