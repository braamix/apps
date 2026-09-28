// A relocatable object, as read_object leaves it. Every Str and Bytes is a
// view into the file's own bytes, which the caller keeps alive.
//
// Every read is bounds-checked; a failure leaves one message naming the file,
// the section and the offset. By default the reader also refuses what ld
// does not link, in ld's words: thread-local storage, shared memory,
// exception tags, -fPIC, wasm64 and GC types. With `link` false it reads
// them, for a tool that only shows an object, and the fields that model
// what ld links stay as they are for the rest; a GC type's `params` and
// `results` are then its encoded bytes, and a wasm64 limit is cut to 32
// bits.
#pragma once

#include "kernel/span.h"
#include "kernel/str.h"
#include "kernel/string.h"
#include "kernel/vec.h"
#include "module.h"
#include "out.h"
#include "wasm.h"

struct Limits {
    u8 flags;
    u32 min;
    u32 max;
};

// A constant expression: its bytes up to and including `end`, and the value
// when it is exactly `i32.const v`.
struct Expr {
    Bytes code;
    bool is_i32;
    i32 value;
    bool is_i64; // exactly `i64.const v`, in `wide`
    i64 wide;
};

struct FuncType {
    Bytes params;
    Bytes results;
};

struct Import {
    Str module;
    Str field;
    u8 kind;
    u32 type;      // function: type index
    u8 valtype;    // global: value type; table: reference type
    bool mut;      // global
    Limits limits; // table, memory
};

// A defined function. Offsets are into the CODE section's contents.
struct Function {
    u32 type;
    u32 code_off; // its size field
    u32 body_off; // its locals, just past the size field
    u32 body_size;
    u32 comdat = NONE;
    Str name; // its first symbol's
};

struct Global {
    u8 valtype;
    bool mut;
    Expr init;
    u32 off; // into the GLOBAL section's contents
    u32 size;
    Str name;
};

struct Table {
    u8 reftype;
    Limits limits;
    Str name;
};

struct Export {
    Str name;
    u8 kind;
    u32 index;
};

// A data segment, with what WASM_SEGMENT_INFO says of it.
struct Segment {
    u32 flags; // 0 active, 1 passive, 2 active with a memory index
    Expr offset;
    Bytes content;
    u32 content_off; // into the DATA section's contents
    Str name;
    u32 align     = 0; // log2
    u32 seg_flags = 0;
    u32 comdat    = NONE;
};

struct Symbol {
    u8 kind;
    u32 flags;
    Str name;
    u32 index  = NONE; // function, global, table: element; section: section
    u32 import = NONE; // undefined function, global, table: its import
    Str import_module;
    Str import_field;
    u32 segment = NONE; // defined data
    u32 offset  = 0;
    u32 size    = 0;

    bool undefined() const { return flags & wasm::SYM_UNDEFINED; }

    bool local() const { return flags & wasm::SYM_LOCAL; }

    bool weak() const { return flags & wasm::SYM_WEAK; }
};

struct InitFunc {
    u32 priority;
    u32 symbol;
};

struct ComdatEntry {
    u8 kind;
    u32 index;
};

struct Comdat {
    Str name;
    Vec<ComdatEntry> entries;
};

// `offset` counts from the start of the target section's contents, `at` from
// the start of the chunk it lands in: a function body from its locals, just
// past the size field; a data segment or a custom section from its first byte.
struct Reloc {
    u8 type;
    u32 offset;
    u32 index;
    i32 addend;
    u32 chunk; // function or segment index; 0 in a custom section
    u32 at;
};

struct RelocSection {
    u32 target; // section index
    Vec<Reloc> relocs;
};

struct Feature {
    u8 prefix; // '+', '-' or '='
    Str name;
};

struct Producer {
    Str field;
    Str name;
    Str version;
};

struct Object {
    String name; // "a.o", or "lib.a(a.o)"
    Bytes file;
    bool link = true; // refuse what ld does not link

    Vec<Section> sections;
    Vec<FuncType> types;
    Vec<Import> imports;
    u32 imported_functions = 0;
    u32 imported_globals   = 0;
    u32 imported_tables    = 0;
    u32 imported_memories  = 0;
    u32 imported_tags      = 0;
    u32 tags               = 0; // defined
    Vec<Function> functions;
    Vec<Table> tables;
    Vec<Limits> memories;
    Vec<Global> globals;
    Vec<Export> exports;
    u32 start         = NONE;
    u32 elem_segments = 0;
    u32 data_count    = NONE;
    Vec<Segment> segments;

    Vec<Symbol> symbols;
    Vec<InitFunc> init_funcs;
    Vec<Comdat> comdats;
    Vec<RelocSection> relocs;
    Vec<Feature> features;
    Vec<Producer> producers;

    u32 code_section   = NONE;
    u32 data_section   = NONE;
    u32 global_section = NONE;
    u32 table_section  = NONE;

    u32 total_functions() const { return imported_functions + functions.size(); }

    u32 total_globals() const { return imported_globals + globals.size(); }

    u32 total_tables() const { return imported_tables + tables.size(); }

    u32 total_tags() const { return imported_tags + tags; }
};

// Parses `o.file` into `o`, whose `name` is already set. False on failure,
// with the message in `err`.
bool read_object(Object &o, Out &err);
