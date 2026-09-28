// The instructions llvm-objdump decodes: for each, the text it prints and
// the shape of its operands.
#pragma once

#include "kernel/str.h"

enum class OpKind : u8 {
    NONE,          // no operands
    ULEB,          // an index, printed as a signed 64-bit number
    ULEB2,         // two, "a, b"
    SLEB,          // an index read as a signed LEB, as bulk memory's are
    SLEB2,         // two
    I32,           // a signed LEB
    I64,           // a signed LEB, 64 bits
    F32,           // four bytes, printed as a double
    F64,           // eight bytes
    MEM,           // alignment and offset
    MEM_LANE,      // alignment, offset and a lane byte
    ATOMIC,        // alignment, an ordering byte if flagged, offset
    LANE,          // a lane byte
    V128,          // sixteen bytes, as four 32-bit words
    SHUFFLE,       // sixteen lane bytes
    FENCE,         // an ordering byte
    SIG,           // a block type: if
    BLOCK,         // a block type, and a label
    LOOP,          // a block type, and a label named where it stands
    TRY,           // a block type, a label and a catch
    BR,            // a branch depth
    BR_TABLE,      // a count of depths, then the default
    CALL_INDIRECT, // a type index and a table
    CATCH,         // a tag, closing a try
    CATCH_ALL,     // closing a try
    RETHROW,       // a depth
    DELEGATE,      // a depth, closing a try
    SELECT_T,      // a count of value types, a byte each
    REF_NULL,      // a heap type, which names the instruction
    TRY_TABLE,     // a block type and a list of catches
};

struct OpInfo {
    u8 prefix; // 0 for a one-byte opcode
    u16 sub;
    OpKind kind;
    u8 align; // the natural alignment, log2, which goes unprinted
    Str text; // up to the operands
};

// The instruction, or null where llvm-objdump prints <unknown>.
const OpInfo *find_op(u8 prefix, u32 sub);
