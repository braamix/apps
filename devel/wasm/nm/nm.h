// nm's core: symbols filtered, sorted and printed as llvm-nm prints them.
// Plain C++ over bytes already in memory; only braam.cpp awaits.
#pragma once

#include "diag.h"
#include "kernel/span.h"
#include "kernel/str.h"
#include "kernel/string.h"
#include "out.h"
#include "symbols.h"

enum class NmFormat : u8 {
    Bsd,
    Sysv,
    Posix,
    Darwin, // Mach-O's; for wasm, Bsd without a label per file
    JustSymbols,
};

struct NmConfig {
    NmFormat format     = NmFormat::Bsd;
    u32 radix           = 16;
    bool defined_only   = false; // -U
    bool undefined_only = false; // -u
    bool extern_only    = false; // -g
    bool no_weak        = false; // -W
    bool demangle       = false; // -C
    bool dynamic        = false; // -D
    bool export_symbols = false;
    bool numeric_sort   = false; // -n
    bool size_sort      = false;
    bool no_sort        = false; // -p
    bool reverse_sort   = false; // -r
    bool print_size     = false; // -S
    bool print_address  = true;
    bool print_file     = false; // -A
    bool print_armap    = false; // -M
    bool quiet          = false;
};

// What nm prints, a file at a time. --export-symbols gathers every file's
// symbols first, so their names must outlive the file they came from.
struct Nm {
    NmConfig c;
    bool multiple = false; // more than one file named
    Out out;               // for stdout
    Vec<ModuleSymbol> exports;
    String scratch; // a demangled name
};

// The options' effects on one another, as llvm-nm settles them.
void nm_settle(NmConfig &c);

// One file, a module or an archive of modules, onto `n.out`; errors, and a
// note of a module with no symbols, into `diag`.
void nm_file(Nm &n, Str name, Bytes file, Diag &diag);

// The --export-symbols list, once every file is read.
void nm_exports(Nm &n);
