// One instruction, decoded and printed as llvm-objdump prints it. Plain C++
// over bytes already in memory.
#pragma once

#include "kernel/span.h"
#include "kernel/vec.h"
#include "out.h"

// What llvm's WebAssembly printer remembers between instructions, for as
// long as one module is disassembled: its label counter and its control
// stacks. `end` pops nothing, because 0x0b decodes to an end the printer
// does not follow, so the stacks only grow, but for `catch` and `delegate`.
struct Flow {
    struct Frame {
        u32 label;
        bool loop;
    };
    Vec<Frame> stack;
    Vec<u32> tries; // labels of the open trys
    Vec<u8> eh;     // what each try has seen: a try, a catch or a catch_all
    u32 counter = 0;
};

// Decodes the instruction at the start of `b`, which runs to the end of its
// section. `text` gets what is printed after the raw bytes, from its tab;
// `notes` the comment lines, each ending in '\n'. Returns the bytes taken,
// which for an instruction that cannot be decoded are those read before it
// failed; `ok` is false then, and `text` is "\t<unknown>".
usize decode(Bytes b, Flow &flow, Out &text, Out &notes, bool &ok);

// Decimals as llvm prints an int64_t or a uint64_t.
void put_i64(Out &o, i64 v);
void put_u64(Out &o, u64 v);

// The column `s` ends at, a tab advancing to a multiple of 8.
usize column(Str s);

// Spaces from column `at` to `to`, at least one.
void pad_to(Out &o, usize at, usize to);
