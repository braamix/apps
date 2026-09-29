// as's core: WebAssembly text in, a relocatable object or a module out.
// Plain C++ over a source already in memory; only braam.cpp awaits.
#pragma once

#include "diag.h"
#include "kernel/str.h"
#include "kernel/vec.h"

struct AsConfig {
    bool module = false; // a plain module, not a relocatable object
};

// One source into `out`. False with a message in `diag` when it cannot be
// assembled, and `out` is then not to be written.
bool assemble(Str name, Str source, const AsConfig &c, Vec<u8> &out, Diag &diag);
