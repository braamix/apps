// as's core: WebAssembly text in, a relocatable object or a module out.
// Plain C++ over a source already in memory; only braam.cpp awaits.
#pragma once

#include "diag.h"
#include "kernel/str.h"
#include "kernel/vec.h"
#include "out.h"

struct AsConfig {
    bool module  = false; // a plain module, not a relocatable object
    bool tokens  = false; // print the tokens, write nothing
    bool numbers = false; // print the bits of typed literals, write nothing
    bool tree    = false; // print the syntax tree, write nothing
};

// One source into `out`. False with a message in `diag` when it cannot be
// assembled, and `out` is then not to be written.
bool assemble(Str name, Str source, const AsConfig &c, Vec<u8> &out, Diag &diag);

// Every token of a source onto `out`, a line each: where, what, and its
// text; a string's and an id's decoded. False at the first error.
bool dump_tokens(Str name, Str source, Out &out, Diag &diag);

// The syntax tree of a source onto `out`, as ast.h prints it. False at
// the first error.
bool dump_tree(Str name, Str source, Out &out, Diag &diag);

// A source of pairs, a type and a literal (i32 -1, f32 0x1p-3; i8 to i64,
// u8 to u64, f32 and f64), onto `out` a line each with the literal's bits
// in hex, or why it has none. False at a lexical error.
bool dump_numbers(Str name, Str source, Out &out, Diag &diag);
