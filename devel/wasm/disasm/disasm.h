// disasm's core: a module's code as llvm-objdump -d prints it, and its data
// as rows of bytes. Plain C++ over bytes already in memory; only braam.cpp
// awaits.
#pragma once

#include "../nm/symbols.h"
#include "code.h"
#include "diag.h"
#include "kernel/span.h"
#include "kernel/str.h"
#include "kernel/string.h"
#include "kernel/vec.h"
#include "out.h"

struct DisasmConfig {
    bool data     = false; // -D adds it
    bool relocs   = false; // -r
    bool demangle = false; // -C
    bool raw      = true;  // --no-show-raw-insn clears it
};

// A stretch of a section headed by a label: from one symbol's address to the
// next's.
struct Chunk {
    u64 start; // an address
    u64 end;
    Str name;     // the label printed
    bool section; // code: headed by the section's count, not by a function's locals
    u32 symbols;  // code: how many are at `start`, each tried for the locals
    u32 seg;      // data: its segment
};

struct DataSeg {
    u64 base; // its address; 0 for a passive one
    Bytes content;
    u32 content_off; // into the DATA section's contents
    Str name;        // an object's, from its linking section
};

struct RelocLine {
    u64 at; // code: into the section's contents; data: an address
    u8 type;
    u32 index; // a type, where there is no symbol
    i32 addend;
    Str symbol;
    bool named; // false for a type index, printed as its number
};

// One module, from its header to its last row.
struct Module {
    Bytes code;
    u64 code_addr = 0;
    Vec<Chunk> code_chunks;
    Vec<RelocLine> code_relocs;
    Vec<DataSeg> segs;
    Vec<Chunk> data_chunks;
    Vec<RelocLine> data_relocs;
    Vec<NmSymbol> syms;
    Flow flow;
    usize next     = 0; // the chunk to print
    usize reloc    = 0;
    bool in_data   = false;
    bool data_head = false;
    bool wide      = false; // wasm64: addresses of 16 digits
};

struct Disasm {
    DisasmConfig c;
    Out out; // for stdout
    String scratch;

    // The file in hand: its modules, one at a time, and the one open.
    struct Job {
        String what; // "file", or "archive(member)"
        Bytes bytes;
    };
    Vec<Job> jobs;
    usize job = 0;
    Module *m = nullptr;
};

// Starts on a file, a module or an archive of modules.
void disasm_file(Disasm &d, Str name, Bytes file, Diag &diag);

// Prints the next part of the file onto `d.out`, so that output can be
// written as it is made; errors go to `diag`. False once the file is done.
bool disasm_more(Disasm &d, Diag &diag);

// Drops what is left of the file.
void disasm_end(Disasm &d);
