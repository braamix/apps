// Symbol resolution, by lld's rules and in lld's order: inputs are added in
// command-line order, an archive member is loaded the moment something needs
// what it defines, and loading one is depth-first, as lld's recursion is. The
// recursion here is an explicit stack, because the native one is small.
#pragma once

#include "diag.h"
#include "driver.h"
#include "object.h"
#include "kernel/hash.h"
#include "layout.h"

// A file as the front end read it.
struct Source {
    Str name;
    Bytes bytes;
};

// A string of a merged segment: where it was, and where it went in the output
// segment.
struct Piece {
    u32 in;
    u32 out;
};

// An object, given on the command line or a member of an archive.
struct InputFile {
    Object obj;
    bool lazy   = false; // an archive member not (yet) loaded
    bool member = false; // came out of an archive
    bool loaded = false;
    bool live   = false; // something it defines is live
    Vec<u32> symbols;    // object symbol -> global symbol; NONE for a local one
    Vec<u8> called;      // object symbol is the target of a direct call
    Vec<u8> kept;        // comdat was this file's to keep
    Vec<u8> live_functions, live_segments, live_globals, live_tables;
    Vec<u32> type_map;       // object type -> output type; NONE if unused
    Vec<u32> function_index; // defined function -> output index; NONE if dead
    Vec<u32> global_index;
    Vec<u32> slot;               // defined function -> table slot; 0 for none
    Vec<u32> segment_out;        // segment -> output segment
    Vec<u32> segment_off;        // segment -> offset in it; NONE for merged strings
    Vec<u32> piece_start;        // segment -> its first piece, and the end last
    Vec<Piece> pieces;           // merged strings, each segment's in offset order
    Vec<u32> custom_off;         // custom section -> offset in its output; NONE if not there
    Vec<u32> custom_piece_start; // custom section -> first piece, as piece_start
    Vec<Piece> custom_pieces;    // merged debug strings, pooled apart
};

enum class State : u8 {
    Undefined,
    Lazy, // defined by an archive member not loaded
    Defined,
};

// One name in the global symbol table.
struct Sym {
    Str name;
    State state;
    u8 kind;   // wasm::SymKind; a lazy symbol's is its member's
    u32 flags; // of the definition or reference that holds the name
    u32 file;  // definer, lazy member or first referrer; NONE for the linker's
    u32 index; // the file's symbol number
    const FuncType *sig = nullptr;
    bool called         = false; // an undefined function called directly
    bool stub           = false; // weak undefined function: a body that traps
    bool mismatch       = false; // the stub stands for a signature that differs
    bool live           = false;
    u32 out_index       = NONE; // linker's function or global, or an import
    u32 slot            = 0;    // table slot of an imported function
    u32 va              = 0;    // linker's data symbol
    Str import_module;          // undefined: where it would be imported from
    Str import_name;            // undefined: set only by an explicit name

    bool weak() const { return flags & wasm::SYM_WEAK; }

    bool synthetic() const { return file == NONE; }
};

struct Linker {
    const Config &cfg;
    Diag &diag;
    Out out; // stdout: --trace, --dump-symtab
    Out why; // --why-extract
    Vec<InputFile *> files;
    Vec<Sym> syms;
    HashMap<Str, u32> names;
    Vec<u32> objects;          // files in lld's order: each after the members it pulled
    HashMap<Str, u32> comdats; // name -> the file that keeps it
    Vec<Str> comdat_order;
    Vec<u32> stubs; // syms whose bodies trap, in wasm-ld's order
    Layout layout;

    Linker(const Config &c, Diag &d) : cfg(c), diag(d) {}

    ~Linker();

    Str file_name(u32 f) const { return f == NONE ? Str("<internal>") : files[f]->obj.name.str(); }
};

// Reads every input and resolves every name. False on an error, which is in
// the Diag.
bool resolve(Linker &l, Span<const Source> inputs);

// Undefined symbols that nothing makes allowable, reported per relocation
// from live chunks.
bool check_undefined(Linker &l);

void dump_symtab(const Linker &l, Out &out);

// Whether an undefined symbol becomes an import; data never does.
bool imported(const Linker &l, const Sym &g);

bool same_sig(const FuncType *a, const FuncType *b);

Str valtype_name(u8 t);

// A symbol's name as wasm-ld shows it: __main_argc_argv as main, and
// demangled unless --no-demangle. `tmp` holds what is returned.
Str shown(const Config &c, Str name, String &tmp);

// "(i32, i32) -> i32", as lld writes a signature.
void put_sig(Out &m, const FuncType *t);
