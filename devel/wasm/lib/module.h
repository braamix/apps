// Framing: a module's header and its sections, for any module, object or
// program. Every Str and Bytes is a view into the file's own bytes.
#pragma once

#include "kernel/span.h"
#include "kernel/str.h"
#include "kernel/vec.h"
#include "out.h"

inline constexpr u32 NONE = ~u32(0);

// One section of the file. `body` is its contents; a custom section's starts
// after its name.
struct Section {
    u8 id;
    Str name; // a custom section's name, else the standard one
    Bytes body;
    u32 file_off;      // where `body` starts in the file
    u32 comdat = NONE; // a custom section's
};

// The file's header and its sections, in order. Checks only framing:
// magic, version, each size within the file, a custom section's name
// within its section, standard sections in order and not repeated.
// False on failure, with the message, naming `name`, in `err`.
bool read_module(Str name, Bytes file, Vec<Section> &sections, Out &err);

// What kind of module: an object has a `linking` section.
bool is_object(const Vec<Section> &sections);
