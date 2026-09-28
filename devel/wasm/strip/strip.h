// strip's core: a module in, the module without its custom sections out.
// Plain C++ over bytes already in memory; only braam.cpp awaits.
#pragma once

#include "kernel/span.h"
#include "kernel/str.h"
#include "kernel/vec.h"
#include "out.h"

struct StripConfig {
    bool debug_only = false; // -g
    Vec<Str> remove;         // -R
    Vec<Str> keep;           // --keep-section, plus "braam"
};

// What strip refuses to remove, whatever the file: the braam section and the
// standard sections. False with the message in `err`.
bool check_strip_config(const StripConfig &c, Out &err);

// `file` stripped into `out`. False on a malformed module, with the message,
// naming `name`, in `err`. If nothing was removed, `out` equals `file`.
bool strip_module(Str name, Bytes file, const StripConfig &c, Vec<u8> &out, Out &err);
