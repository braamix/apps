#include "wasm.h"

namespace wasm {

namespace {

const Str RELOC_NAMES[R_COUNT] = {
    "R_WASM_FUNCTION_INDEX_LEB",     "R_WASM_TABLE_INDEX_SLEB",
    "R_WASM_TABLE_INDEX_I32",        "R_WASM_MEMORY_ADDR_LEB",
    "R_WASM_MEMORY_ADDR_SLEB",       "R_WASM_MEMORY_ADDR_I32",
    "R_WASM_TYPE_INDEX_LEB",         "R_WASM_GLOBAL_INDEX_LEB",
    "R_WASM_FUNCTION_OFFSET_I32",    "R_WASM_SECTION_OFFSET_I32",
    "R_WASM_TAG_INDEX_LEB",          "R_WASM_MEMORY_ADDR_REL_SLEB",
    "R_WASM_TABLE_INDEX_REL_SLEB",   "R_WASM_GLOBAL_INDEX_I32",
    "R_WASM_MEMORY_ADDR_LEB64",      "R_WASM_MEMORY_ADDR_SLEB64",
    "R_WASM_MEMORY_ADDR_I64",        "R_WASM_MEMORY_ADDR_REL_SLEB64",
    "R_WASM_TABLE_INDEX_SLEB64",     "R_WASM_TABLE_INDEX_I64",
    "R_WASM_TABLE_NUMBER_LEB",       "R_WASM_MEMORY_ADDR_TLS_SLEB",
    "R_WASM_FUNCTION_OFFSET_I64",    "R_WASM_MEMORY_ADDR_LOCREL_I32",
    "R_WASM_TABLE_INDEX_REL_SLEB64", "R_WASM_MEMORY_ADDR_TLS_SLEB64",
    "R_WASM_FUNCTION_INDEX_I32",
};

} // namespace

Str reloc_name(u8 type)
{
    return type < R_COUNT ? RELOC_NAMES[type] : Str();
}

Str reloc_refusal(u8 type)
{
    switch (type) {
    case R_MEMORY_ADDR_REL_SLEB:
    case R_TABLE_INDEX_REL_SLEB:
    case R_MEMORY_ADDR_LOCREL_I32:
        return "position-independent code (-fPIC) is not linked here"_s;
    case R_MEMORY_ADDR_TLS_SLEB:
        return "thread-local storage is not linked here"_s;
    case R_MEMORY_ADDR_LEB64:
    case R_MEMORY_ADDR_SLEB64:
    case R_MEMORY_ADDR_I64:
    case R_MEMORY_ADDR_REL_SLEB64:
    case R_TABLE_INDEX_SLEB64:
    case R_TABLE_INDEX_I64:
    case R_FUNCTION_OFFSET_I64:
    case R_TABLE_INDEX_REL_SLEB64:
    case R_MEMORY_ADDR_TLS_SLEB64:
        return "wasm64 is not linked here"_s;
    case R_TAG_INDEX_LEB:
        return "exception tags are not linked here; Braam has no exceptions"_s;
    default:
        return Str();
    }
}

bool reloc_has_addend(u8 type)
{
    switch (type) {
    case R_MEMORY_ADDR_LEB:
    case R_MEMORY_ADDR_SLEB:
    case R_MEMORY_ADDR_I32:
    case R_FUNCTION_OFFSET_I32:
    case R_SECTION_OFFSET_I32:
    case R_MEMORY_ADDR_REL_SLEB:
    case R_MEMORY_ADDR_LEB64:
    case R_MEMORY_ADDR_SLEB64:
    case R_MEMORY_ADDR_I64:
    case R_MEMORY_ADDR_REL_SLEB64:
    case R_MEMORY_ADDR_TLS_SLEB:
    case R_FUNCTION_OFFSET_I64:
    case R_MEMORY_ADDR_LOCREL_I32:
    case R_MEMORY_ADDR_TLS_SLEB64:
        return true;
    default:
        return false;
    }
}

u32 reloc_width(u8 type)
{
    switch (type) {
    case R_TABLE_INDEX_I32:
    case R_MEMORY_ADDR_I32:
    case R_FUNCTION_OFFSET_I32:
    case R_SECTION_OFFSET_I32:
    case R_GLOBAL_INDEX_I32:
    case R_MEMORY_ADDR_LOCREL_I32:
    case R_FUNCTION_INDEX_I32:
        return 4;
    case R_MEMORY_ADDR_LEB64:
    case R_MEMORY_ADDR_SLEB64:
    case R_MEMORY_ADDR_REL_SLEB64:
    case R_TABLE_INDEX_SLEB64:
    case R_TABLE_INDEX_REL_SLEB64:
    case R_MEMORY_ADDR_TLS_SLEB64:
        return 10;
    case R_MEMORY_ADDR_I64:
    case R_TABLE_INDEX_I64:
    case R_FUNCTION_OFFSET_I64:
        return 8;
    default:
        return 5;
    }
}

u8 reloc_symbol_kind(u8 type)
{
    switch (type) {
    case R_TYPE_INDEX_LEB:
        return SYM_NONE;
    case R_FUNCTION_INDEX_LEB:
    case R_FUNCTION_INDEX_I32:
    case R_TABLE_INDEX_SLEB:
    case R_TABLE_INDEX_I32:
    case R_TABLE_INDEX_REL_SLEB:
    case R_TABLE_INDEX_SLEB64:
    case R_TABLE_INDEX_I64:
    case R_TABLE_INDEX_REL_SLEB64:
    case R_FUNCTION_OFFSET_I32:
    case R_FUNCTION_OFFSET_I64:
        return SYM_FUNCTION;
    case R_GLOBAL_INDEX_LEB:
    case R_GLOBAL_INDEX_I32:
        return SYM_GLOBAL;
    case R_SECTION_OFFSET_I32:
        return SYM_SECTION;
    case R_TAG_INDEX_LEB:
        return SYM_TAG;
    case R_TABLE_NUMBER_LEB:
        return SYM_TABLE;
    default:
        return SYM_DATA;
    }
}

Str section_name(u8 id)
{
    switch (id) {
    case SEC_TYPE:
        return "TYPE"_s;
    case SEC_IMPORT:
        return "IMPORT"_s;
    case SEC_FUNCTION:
        return "FUNCTION"_s;
    case SEC_TABLE:
        return "TABLE"_s;
    case SEC_MEMORY:
        return "MEMORY"_s;
    case SEC_GLOBAL:
        return "GLOBAL"_s;
    case SEC_EXPORT:
        return "EXPORT"_s;
    case SEC_START:
        return "START"_s;
    case SEC_ELEM:
        return "ELEM"_s;
    case SEC_CODE:
        return "CODE"_s;
    case SEC_DATA:
        return "DATA"_s;
    case SEC_DATACOUNT:
        return "DATACOUNT"_s;
    case SEC_TAG:
        return "TAG"_s;
    default:
        return Str();
    }
}

} // namespace wasm
