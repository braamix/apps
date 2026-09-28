// Where everything goes: the index spaces, the table and the memory map, in
// the order wasm-ld assigns them.
#pragma once

#include "input.h"
#include "out.h"

// A function or global: an input's (file, its index among the file's own
// definitions), or the linker's (file NONE, index a Sym).
struct Ref {
    u32 file;
    u32 index;
};

// Input segments merged by name: .rodata, .data, .bss and the rest.
struct OutSegment {
    Str name;
    u32 align = 0; // log2
    u32 size  = 0;
    u32 addr  = 0;
    bool bss  = false;
    Vec<Ref> inputs; // file, segment
};

struct Layout {
    Vec<const FuncType *> types;
    Vec<u32> imports; // Sym ids: functions and globals, in symbol order
    u32 imported_functions = 0;
    u32 imported_globals   = 0;
    Vec<Ref> functions; // defined, numbered after the imports
    Vec<Ref> globals;   // defined, numbered after the imports
    Vec<u32> elems;     // the function index in table slot i + 1
    bool table = false;
    Vec<OutSegment> segments;
    Vec<u8> ctors; // __wasm_call_ctors, from its size field
    u32 stack_pointer = 0;
    u32 pages         = 0;
    u32 max_pages     = 0; // 0: none
};

struct Linker;
struct Sym;

// What symbol i of a file names: a definition (file, symbol), or else the
// linker's own or an undefined one (NONE, Sym).
struct Target {
    u32 file;
    u32 index;
};

Target target_of(const Linker &l, u32 f, u32 i);

// The output index of the function symbol i of file f names; NONE for none.
u32 function_index_of(const Linker &l, u32 f, u32 i);

u32 type_index_of(const Layout &lay, const FuncType *t);

// Where an undefined symbol is imported from.
Str import_module(const Sym &g);
Str import_field(const Sym &g);

// Assigns every index and address. False on an error, which is in the Diag.
bool layout(Linker &l);

// --dump-layout: the index spaces, then the memory map in -Map's layout less
// its file offsets.
void dump_layout(const Linker &l, Out &out);
