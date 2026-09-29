// Every instruction of the text format by its name, as
// Wasm_Assembly_Language.md §7.5 and §10 list them: its encoding, the
// shape of its immediates, and its natural alignment. `else`, `end`,
// `catch`, `catch_all` and `delegate` are syntax, not instructions.
#pragma once

#include "kernel/str.h"
#include "kernel/types.h"

// The immediates as §7.4 writes them, and the constructor of wat.asdl's
// Instr each builds.
enum class Imm : u8 {
    NONE,          // Plain
    SELECT,        // Select: result*
    BLOCK,         // Block: block, loop
    IF,            // If
    TRY_TABLE,     // TryTable
    TRY,           // Try, or TryDelegate
    LABEL,         // Br: labelidx
    BR_TABLE,      // BrTable: labelidx+
    BR_ON_CAST,    // BrOnCast: labelidx reftype reftype
    IDX,           // Index: x
    IDX_OPT,       // Index: x?
    IDX2,          // Index2: x y
    IDX2_OPT,      // Index2: (x y)?
    IDX_OPT_IDX,   // Index2: x? y
    NEW_FIXED,     // ArrayNewFixed: typeidx u32
    CALL_INDIRECT, // CallIndirect: tableidx? typeuse
    MEM,           // MemArg: ma
    MEM_LANE,      // MemArgLane: ma lane
    I32,           // I32Const
    I64,           // I64Const
    F32,           // F32Const
    F64,           // F64Const
    V128,          // V128Const: a shape and its lanes
    LANE,          // Lane: lane
    SHUFFLE,       // Shuffle: lane¹⁶
    HEAP_TYPE,     // RefNull: heaptype
    REF_TYPE,      // RefTypeOp: reftype
};

// What an index immediate counts.
enum class Space : u8 {
    NONE,
    FUNC,
    LOCAL,
    GLOBAL,
    TABLE,
    MEMORY,
    TYPE,
    TAG,
    ELEM,
    DATA,
    LABEL,
    FIELD
};

struct OpDef {
    Str name;
    u8 prefix; // 0 for a one-byte opcode
    u16 sub;   // the opcode, or the sub-opcode after the prefix
    Imm imm;
    u8 align; // the natural alignment, log2, of a memory access
    Space x;  // the first index's space
    Space y;  // the second's
};

// The encodings with a choice take the first: select is 0x1c when typed,
// ref.test and ref.cast one more when nullable. atomic.fence is followed
// by a zero byte.

// Instructions are numbered by their place in the table.
u32 op_count();
const OpDef &op_def(u16 op);
Str op_name(u16 op);

// The instruction named `name`, into `op`; false for none.
bool find_op(Str name, u16 &op);
