// One instruction, decoded and printed. Plain C++ over bytes already in
// memory.
#pragma once

#include "cursor.h"
#include "kernel/span.h"
#include "kernel/string.h"
#include "kernel/vec.h"
#include "object.h"
#include "out.h"

// The blocks open in the function being decoded. Labels are numbered from 0
// in each function, in the order the blocks open.
struct Flow {
    enum Kind : u8 { BLOCK, LOOP, IF, TRY, TRY_TABLE };
    enum Eh : u8 { NONE, CATCH, CATCH_ALL }; // what a try has seen
    struct Frame {
        u32 label;
        Kind kind;
        Eh eh;
    };
    Vec<Frame> stack;
    u32 counter = 0;

    void reset()
    {
        stack.clear();
        counter = 0;
    }
};

// What decoding knows of the module: the names of what an index can refer to,
// empty where there is none, and the types.
struct Ctx {
    Flow flow;
    Vec<Str> funcs;
    Vec<Str> globals;
    Vec<Str> tables;
    Vec<Str> tags;
    Vec<FuncType> types;
    bool demangle = false;
    String scratch;
    Out ops; // the operands of the instruction in hand
};

// Decodes the instruction at the start of `b`, which runs to the end of its
// section. `text` gets what is printed after the raw bytes, from its tab;
// `notes` the comment lines, each ending in '\n'. Returns the bytes taken,
// which for an instruction that cannot be decoded are those read before it
// failed; `ok` is false then, and `text` is "\t<unknown>".
usize decode(Bytes b, Ctx &ctx, Out &text, Out &notes, bool &ok);

// A value type in `b`, as a name; false where it is not one.
bool put_valtype(Out &o, Cursor &c);

// Decimals of an int64_t or a uint64_t.
void put_i64(Out &o, i64 v);
void put_u64(Out &o, u64 v);

// Lower-case hex, zero-padded to `digits`, or right-aligned in `width`.
void put_hex(Out &o, u64 v, usize digits);
void put_rhex(Out &o, u64 v, usize width);

// The column `s` ends at, a tab advancing to a multiple of 8.
usize column(Str s);

// Spaces from column `at` to `to`, at least one.
void pad_to(Out &o, usize at, usize to);
