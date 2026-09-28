// The braam section: what exec reads to know a module is a Braam program.
// Five little-endian u32s in a custom section named "braam".
#pragma once

#include "emit.h"
#include "module.h"

inline constexpr u32 BRAAM_MAGIC = 0x6d617262;

struct Stamp {
    u32 magic, abi, flags, initial_pages, max_pages;
};

// The braam section's contents, if the module has a well-formed one: the
// first section of that name, as exec takes it, at least 20 bytes long and
// with the magic.
bool find_stamp(const Vec<Section> &sections, Stamp &s);

// The whole section, name and all.
void emit_stamp(Emit &e, const Stamp &s);
