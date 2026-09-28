// Where everything goes: the index spaces, the table and the memory map, in
// the order wasm-ld assigns them.
#pragma once

#include "object.h"
#include "out.h"

// A function or global: an input's (file, its index among the file's own
// definitions), or the linker's (file NONE, index a Sym).
struct Ref {
    u32 file;
    u32 index;
};

// The strings of STRINGS segments, pooled and each kept once: wasm-ld's
// -O1. One per output segment and segment flags.
struct Merged {
    u32 flags;
    u32 off = 0;      // in the output segment
    Vec<Ref> members; // file, segment
    Vec<u8> bytes;
};

// Input segments merged by name: .rodata, .data, .bss and the rest.
struct OutSegment {
    Str name;
    u32 align = 0; // log2
    u32 size  = 0;
    u32 addr  = 0;
    bool bss  = false;
    Vec<Ref> inputs; // file, segment; or NONE, a Merged
    Vec<Merged> merged;
};

// What -Map needs of the written module: each section's place, and where
// each function and data segment went in its section's body.
struct MapSection {
    u8 id;
    Str name; // a custom section's
    u32 off;
    u32 size;
};

struct Map {
    Vec<MapSection> sections;
    Vec<u32> code_off;      // Layout::functions[i], from the CODE body
    Vec<u32> seg_off;       // Layout::segments[i], from the DATA body; 0 unwritten
    Vec<u32> seg_data;      // where its bytes start; 0 unwritten
    u32 code = 0, data = 0; // those sections' offsets
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
    Map map;
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

struct InputFile;
struct Piece;

// A string being pooled: the piece it was, and its bytes.
struct PoolString {
    Piece *piece;
    const u8 *p;
    u32 n;
};

// Strings merged as wasm-ld merges them: split at each NUL, and pooled as
// StringTableBuilder's RAW table pools them, each once, a string that ends
// the one before it in its order sharing its bytes. Each piece's `out` is
// set to its offset in `bytes`. All false only for want of memory.
bool split_strings(Bytes c, Vec<Piece> &out);
bool gather(Bytes c, Vec<Piece> &pieces, u32 from, u32 to, Vec<PoolString> &all);
bool pool_strings(const Vec<PoolString> &all, Vec<u8> &bytes);

// Where offset `off` of segment `seg` went in its output segment.
u32 segment_offset(const InputFile &in, u32 seg, u32 off);

// Where an undefined symbol is imported from.
Str import_module(const Sym &g);
Str import_field(const Sym &g);

// Assigns every index and address. False on an error, which is in the Diag.
bool layout(Linker &l);

// --dump-layout: the index spaces, then the memory map in -Map's layout less
// its file offsets.
void dump_layout(const Linker &l, Out &out);

// -Map=<file>, as wasm-ld writes it, once the module is written.
void write_map(const Linker &l, Out &out);
