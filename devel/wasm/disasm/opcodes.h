// The instructions disasm decodes: for each, its name and the shape of its
// operands.
#pragma once

#include "kernel/str.h"

enum class OpKind : u8 {
    NONE,          // no operands
    ULEB,          // an index, printed as a signed 64-bit number
    ULEB2,         // two, "a, b"
    FUNC,          // a function index, named in a comment
    GLOBAL,        // a global index, likewise
    TABLE,         // a table index, likewise
    TABLE2,        // two, destination and source
    TAG,           // a tag index, likewise
    TYPE,          // a type index, its signature in a comment
    SLEB,          // an index read as a signed LEB, as bulk memory's are
    SLEB2,         // two
    I32,           // a signed LEB
    I64,           // a signed LEB, 64 bits
    F32,           // four bytes
    F64,           // eight bytes
    MEM,           // alignment and offset
    MEM_LANE,      // alignment, offset and a lane byte
    ATOMIC,        // alignment, an ordering byte if flagged, offset
    LANE,          // a lane byte
    V128,          // sixteen bytes, as four 32-bit words
    SHUFFLE,       // sixteen lane bytes
    FENCE,         // an ordering byte
    IF,            // a block type, and a label
    BLOCK,         // a block type, and a label
    LOOP,          // a block type, and a label
    TRY,           // a block type, a label and a catch
    BR,            // a branch depth
    BR_TABLE,      // a count of depths, then the default
    CALL_INDIRECT, // a type index and a table index
    CATCH,         // a tag, closing a try
    CATCH_ALL,     // closing a try
    RETHROW,       // a depth
    DELEGATE,      // a depth, closing a try
    SELECT_T,      // a count of value types, a byte each
    REF_NULL,      // a heap type
    TRY_TABLE,     // a block type and a list of catches
    END,           // closing a block
};

struct OpInfo {
    u8 prefix; // 0 for a one-byte opcode
    u16 sub;
    OpKind kind;
    u8 align; // the natural alignment, log2, which goes unprinted
    Str name;
};

// The instruction, or null for one disasm does not know.
const OpInfo *find_op(u8 prefix, u32 sub);
