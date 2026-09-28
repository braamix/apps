// A module's symbols, as llvm's WasmObjectFile lists them: an object's from
// its symbol table; a program's from its name section, or failing that from
// its exports. Plain C++ over bytes already in memory.
#pragma once

#include "kernel/span.h"
#include "kernel/str.h"
#include "kernel/vec.h"
#include "out.h"

struct NmSymbol {
    Str name; // a view into the file
    u64 addr = 0;
    u64 size = 0;
    char type; // U, w, W, t, T, d or D
    bool undefined;
    bool global;
    bool weak;
};

// The symbols of the module in `file`, in its order. False on a malformed
// module, with the message, naming `what`, in `err`.
bool read_symbols(Str what, Bytes file, Vec<NmSymbol> &syms, Out &err);
