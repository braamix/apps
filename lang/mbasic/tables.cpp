// The four parallel tables, and the error text.
//
// m6502.asm:993-1365. RESLST, STMDSP, FUNDSP and OPTAB are in one file because
// the connection between them is purely positional and the hazard is that they
// must be edited together:
//
//   - a token's value is 128 + its ordinal in RESLST;
//   - the statement dispatcher indexes STMDSP by token - ENDTK and range-checks
//     against SCRATK - ENDTK + 1;
//   - the function dispatcher indexes FUNDSP by token - ONEFUN, with LASNUM
//     separating the one-argument functions from the rest;
//   - FRMEVL computes OPTAB's index as 3*(token - PLUSTK) upstream, so the
//     seven binary operators must stay contiguous and in RESLST's order, and
//     the relationals must be adjacent and ordered >, =, <.
//
// Adding a word under one conditional shifts every later token in all four at
// once. This build is EXTIO=1 DISKO=1 GETCMD=1 INTPRC=1 ADDPRC=1 LNGERR=1,
// NULCMD=0 TIME=0, and no VERIFY, DDT or CLR -- those are Commodore- and
// simulator-only.
#include "mbasic.h"

// Matching is first-fit in table order, so a word that is a prefix of another
// must come second: INPUT# before INPUT, PRINT# before PRINT. And, from
// m6502.asm:1208-1216:
//
//   NOTE DANGER OF ONE RESERVED WORD BEING A PART OF ANOTHER:
//   IE . . IF 2 GREATER THAN F OR T=5 THEN... WILL NOT WORK!!!
//   SINCE "FOR" WILL BE CRUNCHED!!
//
// TAB( and SPC( carry their open paren, because the DCI macro could not take
// one as an argument -- ";MACRO DOESNT LIKE ('S IN ARGUMENTS."
const Str RESLST[] = {
    // statements, 0x80..0xA1
    "END",
    "FOR",
    "NEXT",
    "DATA",
    "INPUT#",
    "INPUT",
    "DIM",
    "READ",
    "LET",
    "GOTO",
    "RUN",
    "IF",
    "RESTORE",
    "GOSUB",
    "RETURN",
    "REM",
    "STOP",
    "ON",
    "WAIT",
    "LOAD",
    "SAVE",
    "DEF",
    "POKE",
    "PRINT#",
    "PRINT",
    "CONT",
    "LIST",
    "CLEAR",
    "CMD",
    "SYS",
    "OPEN",
    "CLOSE",
    "GET",
    "NEW",
    // keywords, 0xA2..0xA8
    "TAB(",
    "TO",
    "FN",
    "SPC(",
    "THEN",
    "NOT",
    "STEP",
    // operators, 0xA9..0xB2
    "+",
    "-",
    "*",
    "/",
    "^",
    "AND",
    "OR",
    ">",
    "=",
    "<",
    // functions, 0xB3..0xC9
    "SGN",
    "INT",
    "ABS",
    "USR",
    "FRE",
    "POS",
    "SQR",
    "RND",
    "LOG",
    "EXP",
    "COS",
    "SIN",
    "TAN",
    "ATN",
    "PEEK",
    "LEN",
    "STR$",
    "VAL",
    "ASC",
    "CHR$",
    "LEFT$",
    "RIGHT$",
    "MID$",
    // and GO, which has no STMDSP entry: SNERRX checks for it, consumes the
    // following TO and enters GOTO. It exists only because the tokenizer
    // cannot match across the space in "GO TO" (03-tokenizer-editor.md §2.3).
    "GO",
};

static_assert(sizeof RESLST / sizeof RESLST[0] == RESLST_COUNT, "RESLST and GOTK disagree");

const StmtFn STMDSP[STMT_COUNT] = {
    &Interp::stmt_end,     &Interp::stmt_for,    &Interp::stmt_next,   &Interp::stmt_data,
    &Interp::stmt_inputn,  &Interp::stmt_input,  &Interp::stmt_dim,    &Interp::stmt_read,
    &Interp::stmt_let,     &Interp::stmt_goto,   &Interp::stmt_run,    &Interp::stmt_if,
    &Interp::stmt_restore, &Interp::stmt_gosub,  &Interp::stmt_return, &Interp::stmt_rem,
    &Interp::stmt_stop,    &Interp::stmt_ongoto, &Interp::stmt_wait,   &Interp::stmt_load,
    &Interp::stmt_save,    &Interp::stmt_def,    &Interp::stmt_poke,   &Interp::stmt_printn,
    &Interp::stmt_print,   &Interp::stmt_cont,   &Interp::stmt_list,   &Interp::stmt_clear,
    &Interp::stmt_cmd,     &Interp::stmt_sys,    &Interp::stmt_open,   &Interp::stmt_close,
    &Interp::stmt_get,     &Interp::stmt_new,
};

const FuncFn FUNDSP[FUNC_COUNT] = {
    &Interp::fn_sgn,  &Interp::fn_int,   &Interp::fn_abs, &Interp::fn_usr, &Interp::fn_fre,
    &Interp::fn_pos,  &Interp::fn_sqr,   &Interp::fn_rnd, &Interp::fn_log, &Interp::fn_exp,
    &Interp::fn_cos,  &Interp::fn_sin,   &Interp::fn_tan, &Interp::fn_atn, &Interp::fn_peek,
    &Interp::fn_len,  &Interp::fn_str,   &Interp::fn_val, &Interp::fn_asc, &Interp::fn_chr,
    &Interp::fn_left, &Interp::fn_right, &Interp::fn_mid,
};

// m6502.asm:1084-1103. The precedences are arbitrary except in their ordering
// -- the source says so at 335-339 -- so NOT binding tighter than AND but
// looser than a comparison, and unary minus tighter than *, are the design
// decisions written down here. Entries 0..6 are reached arithmetically from
// the token; 7, 8 and 9 only by name.
const OpEntry OPTAB[OPTAB_COUNT] = {
    { 121, &Interp::op_add }, // +
    { 121, &Interp::op_sub }, // -
    { 123, &Interp::op_mul }, // *
    { 123, &Interp::op_div }, // /
    { 127, &Interp::op_pwr }, // ^
    { 80, &Interp::op_and },  // AND
    { 70, &Interp::op_or },   // OR
    { 125, &Interp::op_neg }, // NEGTAB: unary minus
    { 90, &Interp::op_not },  // NOTTAB
    { 100, &Interp::op_rel }, // PTDORL: any relational
};

// ERRTAB under IFN LNGERR (m6502.asm:1289-1348). Upstream's offsets into the
// blob are meaningless here: 04-interpreter-loop.md §4.3 warns that the
// numeric value of every code differs between LNGERR settings, so a
// reimplementation must key on the symbols. It does; these are in ErrCode
// order.
const Str ERRTAB[ERR_COUNT] = {
    "NEXT WITHOUT FOR",
    "SYNTAX",
    "RETURN WITHOUT GOSUB",
    "OUT OF DATA",
    "ILLEGAL QUANTITY",
    "OVERFLOW",
    "OUT OF MEMORY",
    "UNDEF'D STATEMENT",
    "BAD SUBSCRIPT",
    "REDIM'D ARRAY",
    "DIVISION BY ZERO",
    "ILLEGAL DIRECT",
    "TYPE MISMATCH",
    "STRING TOO LONG",
    "FILE DATA",
    "FORMULA TOO COMPLEX",
    "CAN'T CONTINUE",
    "UNDEF'D FUNCTION",
};
