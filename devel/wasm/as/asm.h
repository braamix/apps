// What a source assembles to before it is an object: sections of bytes with
// the fixups still to be resolved, and symbols in the order llvm-mc
// registers them, which is the order of the object's imports, functions
// and symbol table.
#pragma once

#include "kernel/hash.h"
#include "kernel/str.h"
#include "kernel/string.h"
#include "kernel/types.h"
#include "kernel/vec.h"

namespace as {

// A symbol's type: none reads as data, as llvm's does.
enum Kind : u8 { K_NONE, K_FUNCTION, K_DATA, K_GLOBAL, K_TABLE };

// A section by its name's prefix: .text, data of every kind, or metadata.
enum SecKind : u8 { S_TEXT, S_DATA, S_META };

// The field a fixup fills, and so the relocation it becomes.
enum FixKind : u8 {
    F_SLEB32,    // i32.const
    F_SLEB64,    // i64.const
    F_ULEB32,    // an index or a memory offset
    F_ULEB64,    // a 64-bit memory offset
    F_TYPEINDEX, // call_indirect's signature
    F_DATA1,     // .int8 to .int64
    F_DATA2,
    F_DATA4,
    F_DATA8,
};

struct Sig {
    Vec<u8> params;
    Vec<u8> results;
};

struct Symbol {
    Str name;
    u8 kind    = K_NONE;
    bool temp  = false; // a call_indirect signature's, nameless
    bool known = false; // registered: it is in Asm::order

    bool external   = false;
    bool weak       = false;
    bool hidden     = false;
    bool no_strip   = false;
    bool defined    = false;
    bool in_reloc   = false; // some relocation names it
    bool has_size   = false;
    bool has_module = false;
    bool has_import = false;
    bool has_export = false;

    i32 sig = -1; // into Asm::sigs; a function's or a temp's

    u8 global_type      = 0;
    bool global_mutable = true;

    u8 table_elem   = 0;
    u32 table_flags = 0;
    u32 table_min   = 0;
    u32 table_max   = 0;

    u32 section = 0; // where a label put it
    u32 offset  = 0;
    u64 size    = 0;

    Str import_module;
    Str import_name;
    Str export_name;
};

struct Fixup {
    u32 offset; // in the section
    u32 sym;
    i64 addend;
    u8 kind;
    u8 reloc = 0; // the relocation type, once the writer has chosen it
};

struct Section {
    Str name;
    u8 kind      = S_DATA;
    u32 flags    = 0; // segment flags: strings, TLS, retain
    bool passive = false;
    u32 align    = 0; // log2
    Vec<u8> bytes;
    Vec<Fixup> fixups;
};

struct Asm {
    Vec<Symbol> syms;
    HashMap<Str, u32> by_name;
    Vec<u32> order; // registration, as MCAssembler::symbols()
    Vec<Section> sections;
    HashMap<Str, u32> section_by_name;
    Vec<Sig> sigs;
    Vec<String> strings; // names built or decoded, which the Strs above view
    u32 table = 0;       // __indirect_function_table, made at the start
    bool oom  = false;

    // The symbol of that name, made when first named.
    u32 symbol(Str name);
    // A nameless temporary.
    u32 temp();
    // Into `order`, once.
    void enlist(u32 sym);
    // A string that outlives the source: kept in `strings`.
    Str keep(Str s);
};

} // namespace as
