// Symbol resolution, by lld's rules and in lld's order: inputs are added in
// command-line order, an archive member is loaded the moment something needs
// what it defines, and loading one is depth-first, as lld's recursion is. The
// recursion here is an explicit stack, because the native one is small.
#pragma once

#include "diag.h"
#include "driver.h"
#include "input.h"
#include "kernel/hash.h"

// A file as the front end read it.
struct Source {
    Str name;
    Bytes bytes;
};

// An object, given on the command line or a member of an archive.
struct InputFile {
    Object obj;
    bool lazy   = false; // an archive member not (yet) loaded
    bool loaded = false;
    Vec<u32> symbols; // object symbol -> global symbol; NONE for a local one
    Vec<u8> called;   // object symbol is the target of a direct call
    Vec<u8> kept;     // comdat was this file's to keep
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
    Str import_module;           // undefined: where it would be imported from
    Str import_name;             // undefined: set only by an explicit name

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
    Vec<u32> order;            // files in the order they were loaded
    HashMap<Str, u32> comdats; // name -> the file that keeps it
    Vec<Str> comdat_order;

    Linker(const Config &c, Diag &d) : cfg(c), diag(d) {}

    ~Linker();

    Str file_name(u32 f) const { return f == NONE ? Str("<internal>") : files[f]->obj.name.str(); }
};

// Reads every input and resolves every name. False on an error, which is in
// the Diag.
bool resolve(Linker &l, Span<const Source> inputs);

// Undefined symbols that nothing makes allowable, reported per relocation.
bool check_undefined(Linker &l);

void dump_symtab(const Linker &l, Out &out);
