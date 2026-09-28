// The command line, in wasm-ld's spelling, turned into a Config. Plain C++:
// nothing here reads a file.
#pragma once

#include "diag.h"
#include "kernel/span.h"
#include "kernel/vec.h"

// An input as named on the command line: a path, or -l's library name.
struct InputArg {
    Str name;
    bool lib; // -l<name>
};

struct Config {
    Vec<InputArg> inputs;
    Vec<Str> lib_dirs; // -L
    Str output;
    Str entry = "_start";
    Vec<Str> exports;   // --export
    Vec<Str> undefined; // -u
    Str why_extract;    // --why-extract=<file>; "-" is stdout
    bool trace             = false;
    bool allow_undefined   = false;
    bool gc_sections       = true;
    bool print_gc_sections = false;
    bool stack_first       = false;
    bool import_memory     = false;
    u32 stack_size         = 65536;
    u32 initial_memory     = 0;
    u32 max_memory         = 0;
    u32 error_limit        = 20;

    // wlink's own.
    bool dump        = false; // --dump: the inputs are to be dumped, not linked
    bool dump_symtab = false; // --dump-symtab: print the resolved symbols
};

// False with a message in `diag` for an argument that is not understood.
bool parse_args(Span<const Str> words, Config &cfg, Diag &diag);
