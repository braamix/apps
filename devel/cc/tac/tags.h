enum {
    TAG_EOL             = 0,          // end of list / NULL
    TAG_TAC_MAGIC       = 0x54414335, // 'TAC5' - stream header, bumped on format change
    TAG_TAC_CONST       = 0x636e7374, // 'cnst' - for struct Tac_Const
    TAG_TAC_INSTR       = 0x696e7372, // 'insr' - for struct Tac_Instruction
    TAG_TAC_MEMBER      = 0x746d6272, // 'tmbr' - for struct Tac_Member
    TAG_TAC_PARAM       = 0x7470726d, // 'tprm' - for struct Tac_Param
    TAG_TAC_STATIC_INIT = 0x73696e69, // 'sini' - for struct Tac_StaticInit
    TAG_TAC_STATIC_LOC  = 0x73746c63, // 'stlc' - for struct Tac_StaticLocal
    TAG_TAC_TOPLEVEL    = 0x74706c76, // 'tplv' - for struct Tac_TopLevel
    TAG_TAC_TYPE        = 0x74747970, // 'ttyp' - for struct Tac_Type
    TAG_TAC_VAL         = 0x7476616c, // 'tval' - for struct Tac_Val
};

// High-bit flag OR'd into a TAG_TAC_INSTR header word when the instruction is a
// volatile memory access. Old streams have this bit clear, so reading them yields
// is_volatile = false (backward compatible). Defined as a macro (not an enum
// constant) because 0x80000000 is not representable as a plain int.
#define TAG_INSTR_VOLATILE ((size_t)0x80000000)
