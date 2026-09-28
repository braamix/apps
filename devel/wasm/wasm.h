// The numbers of the wasm binary format and of the tool-conventions object
// format (Linking.md) that ld reads and writes.
#pragma once

#include "kernel/str.h"
#include "kernel/types.h"

namespace wasm {

inline constexpr u8 MAGIC[4]         = { 0x00, 0x61, 0x73, 0x6d };
inline constexpr u32 VERSION         = 1;
inline constexpr u32 LINKING_VERSION = 2;

enum Sec : u8 {
    SEC_CUSTOM    = 0,
    SEC_TYPE      = 1,
    SEC_IMPORT    = 2,
    SEC_FUNCTION  = 3,
    SEC_TABLE     = 4,
    SEC_MEMORY    = 5,
    SEC_GLOBAL    = 6,
    SEC_EXPORT    = 7,
    SEC_START     = 8,
    SEC_ELEM      = 9,
    SEC_CODE      = 10,
    SEC_DATA      = 11,
    SEC_DATACOUNT = 12,
    SEC_TAG       = 13,
};

// External kinds, in imports and exports.
enum Ext : u8 {
    EXT_FUNCTION = 0,
    EXT_TABLE    = 1,
    EXT_MEMORY   = 2,
    EXT_GLOBAL   = 3,
    EXT_TAG      = 4,
};

enum ValType : u8 {
    I32       = 0x7f,
    I64       = 0x7e,
    F32       = 0x7d,
    F64       = 0x7c,
    V128      = 0x7b,
    FUNCREF   = 0x70,
    EXTERNREF = 0x6f,
};

inline constexpr u8 FUNC_TYPE = 0x60;

// Limits flags.
inline constexpr u8 LIMITS_MAX    = 0x01;
inline constexpr u8 LIMITS_SHARED = 0x02;
inline constexpr u8 LIMITS_64     = 0x04;

// Opcodes a constant expression may hold.
enum Op : u8 {
    OP_UNREACHABLE = 0x00,
    OP_END         = 0x0b,
    OP_CALL        = 0x10,
    OP_DROP        = 0x1a,
    OP_GLOBAL_GET  = 0x23,
    OP_I32_CONST   = 0x41,
    OP_I64_CONST   = 0x42,
    OP_F32_CONST   = 0x43,
    OP_F64_CONST   = 0x44,
    OP_I32_ADD     = 0x6a,
    OP_I32_SUB     = 0x6b,
    OP_I32_MUL     = 0x6c,
    OP_I64_ADD     = 0x7c,
    OP_I64_SUB     = 0x7d,
    OP_I64_MUL     = 0x7e,
    OP_REF_NULL    = 0xd0,
    OP_REF_FUNC    = 0xd2,
};

// `linking` subsections.
enum Sub : u8 {
    SUB_SEGMENT_INFO = 5,
    SUB_INIT_FUNCS   = 6,
    SUB_COMDAT_INFO  = 7,
    SUB_SYMBOL_TABLE = 8,
};

enum SymKind : u8 {
    SYM_FUNCTION = 0,
    SYM_DATA     = 1,
    SYM_GLOBAL   = 2,
    SYM_SECTION  = 3,
    SYM_TAG      = 4,
    SYM_TABLE    = 5,
};

enum SymFlag : u32 {
    SYM_WEAK          = 0x01,
    SYM_LOCAL         = 0x02,
    SYM_HIDDEN        = 0x04,
    SYM_UNDEFINED     = 0x10,
    SYM_EXPORTED      = 0x20,
    SYM_EXPLICIT_NAME = 0x40,
    SYM_NO_STRIP      = 0x80,
    SYM_TLS           = 0x100,
    SYM_ABSOLUTE      = 0x200,
};

inline constexpr u32 SYM_BINDING_MASK = 0x03;

// Segment flags, from WASM_SEGMENT_INFO.
enum SegFlag : u32 {
    SEG_STRINGS = 0x01,
    SEG_TLS     = 0x02,
    SEG_RETAIN  = 0x04,
};

enum ComdatKind : u8 {
    COMDAT_DATA     = 0,
    COMDAT_FUNCTION = 1,
    COMDAT_SECTION  = 5,
};

enum RelocType : u8 {
    R_FUNCTION_INDEX_LEB     = 0,
    R_TABLE_INDEX_SLEB       = 1,
    R_TABLE_INDEX_I32        = 2,
    R_MEMORY_ADDR_LEB        = 3,
    R_MEMORY_ADDR_SLEB       = 4,
    R_MEMORY_ADDR_I32        = 5,
    R_TYPE_INDEX_LEB         = 6,
    R_GLOBAL_INDEX_LEB       = 7,
    R_FUNCTION_OFFSET_I32    = 8,
    R_SECTION_OFFSET_I32     = 9,
    R_TAG_INDEX_LEB          = 10,
    R_MEMORY_ADDR_REL_SLEB   = 11,
    R_TABLE_INDEX_REL_SLEB   = 12,
    R_GLOBAL_INDEX_I32       = 13,
    R_MEMORY_ADDR_LEB64      = 14,
    R_MEMORY_ADDR_SLEB64     = 15,
    R_MEMORY_ADDR_I64        = 16,
    R_MEMORY_ADDR_REL_SLEB64 = 17,
    R_TABLE_INDEX_SLEB64     = 18,
    R_TABLE_INDEX_I64        = 19,
    R_TABLE_NUMBER_LEB       = 20,
    R_MEMORY_ADDR_TLS_SLEB   = 21,
    R_FUNCTION_OFFSET_I64    = 22,
    R_MEMORY_ADDR_LOCREL_I32 = 23,
    R_TABLE_INDEX_REL_SLEB64 = 24,
    R_MEMORY_ADDR_TLS_SLEB64 = 25,
    R_FUNCTION_INDEX_I32     = 26,
    R_COUNT                  = 27,
};

// "R_WASM_..." as llvm-objdump prints it.
Str reloc_name(u8 type);

// Why a known type is refused; empty when ld reads it.
Str reloc_refusal(u8 type);

bool reloc_has_addend(u8 type);

// Bytes the patched field occupies.
u32 reloc_width(u8 type);

// The symbol kind the index must name; SYM_NONE for a type index.
inline constexpr u8 SYM_NONE = 0xff;
u8 reloc_symbol_kind(u8 type);

// "TYPE", "CODE", ... for a standard section.
Str section_name(u8 id);

} // namespace wasm
