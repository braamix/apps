// size's core: a module's sections added up, as llvm-size adds them. Plain
// C++ over bytes already in memory; only braam.cpp awaits.
#pragma once

#include "diag.h"
#include "kernel/span.h"
#include "kernel/str.h"
#include "out.h"

enum class SizeFormat : u8 {
    Berkeley, // text, data and bss on a line per module
    Sysv,     // every section, its size and address
    Darwin,   // Mach-O's; for wasm, Berkeley without the totals
};

struct SizeConfig {
    SizeFormat format = SizeFormat::Berkeley;
    u32 radix         = 10; // 8, 10 or 16
    bool totals       = false;
};

// What size prints, a file at a time. The Berkeley header is printed once,
// and the totals run across every file.
struct Sizer {
    SizeConfig c;
    Out out; // for stdout
    bool header = false;
    u64 text = 0, data = 0, bss = 0;
};

// One file, a module or an archive of modules, onto `s.out`. A malformed
// module is an error in `diag`, and so is a file that is not wasm; an
// archive's member that is not wasm is skipped, as llvm-size skips it.
void size_file(Sizer &s, Str name, Bytes file, Diag &diag);

// The (TOTALS) line, when -t asked for it in the Berkeley format.
void size_totals(Sizer &s);
