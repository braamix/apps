/*
 * besm6_sys.c: BESM-6 simulator interface
 *
 * Copyright (c) 2009, Serge Vakulenko
 * Copyright (c) 2009, Leonid Broukhis
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * SERGE VAKULENKO OR LEONID BROUKHIS BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
 * OR OTHER DEALINGS IN THE SOFTWARE.

 * Except as contained in this notice, the name of Leonid Broukhis or
 * Serge Vakulenko shall not be used in advertising or otherwise to promote
 * the sale, use or other dealings in this Software without prior written
 * authorization from Leonid Broukhis and Serge Vakulenko.
 *
 * This file implements two essential functions:
 *
 * sim_load()   - loading and dumping memory and CPU state
 *                in a way, specific for BESM-6 architecture
 * fprint_sym() - print a machine instruction using
 *                opcode mnemonic or in a digital format
 */
#include "besm6_defs.h"

const char *opname_short_bemsh[64] = {
    "зп",  "зпм",  "рег",  "счм",  "сл",  "вч",  "вчоб", "вчаб", "сч",  "и",   "нтж",
    "слц", "знак", "или",  "дел",  "умн", "сбр", "рзб",  "чед",  "нед", "слп", "вчп",
    "сд",  "рж",   "счрж", "счмр", "э32", "увв", "слпа", "вчпа", "сда", "ржа", "уи",
    "уим", "счи",  "счим", "уии",  "сли", "э46", "э47",  "э50",  "э51", "э52", "э53",
    "э54", "э55",  "э56",  "э57",  "э60", "э61", "э62",  "э63",  "э64", "э65", "э66",
    "э67", "э70",  "э71",  "э72",  "э73", "э74", "э75",  "э76",  "э77",
};

static const char *opname_long_bemsh[16] = {
    "э20", "э21", "мода", "мод",  "уиа", "слиа", "по",  "пе",
    "пб",  "пв",  "выпр", "стоп", "пио", "пино", "э36", "цикл",
};

const char *opname_short_madlen[64] = {
    "atx", "stx", "mod", "xts", "a+x", "a-x", "x-a", "amx", "xta", "aax", "aex", "arx", "avx",
    "aox", "a/x", "a*x", "apx", "aux", "acx", "anx", "e+x", "e-x", "asx", "xtr", "rte", "yta",
    "*32", "ext", "e+n", "e-n", "asn", "ntr", "ati", "sti", "ita", "its", "mtj", "j+m", "*46",
    "*47", "*50", "*51", "*52", "*53", "*54", "*55", "*56", "*57", "*60", "*61", "*62", "*63",
    "*64", "*65", "*66", "*67", "*70", "*71", "*72", "*73", "*74", "*75", "*76", "*77",
};

static const char *opname_long_madlen[16] = {
    "*20", "*21", "utc", "wtc",  "vtm", "utm", "uza", "u1a",
    "uj",  "vjm", "ij",  "stop", "vzm", "v1m", "*36", "vlm",
};

/*
 * The mnemonic for an instruction code.
 * The code must be in the range 000..077 or 0200..0370.
 */
const char *besm6_opname(int opcode)
{
    if (besm6_latin || (sim_switches & SWMASK('L'))) {
        /* Latin mnemonics. */
        if (opcode & 0200)
            return opname_long_madlen[(opcode >> 3) & 017];
        return opname_short_madlen[opcode];
    }
    if (opcode & 0200)
        return opname_long_bemsh[(opcode >> 3) & 017];
    return opname_short_bemsh[opcode];
}

/*
 * The instruction code for a mnemonic (UTF-8).
 */
int besm6_opcode(char *instr)
{
    int i;

    for (i = 0; i < 64; ++i)
        if (strcmp(opname_short_bemsh[i], instr) == 0 || strcmp(opname_short_madlen[i], instr) == 0)
            return i;
    for (i = 0; i < 16; ++i)
        if (strcmp(opname_long_bemsh[i], instr) == 0 || strcmp(opname_long_madlen[i], instr) == 0)
            return (i << 3) | 0200;
    return -1;
}

/*
 * Output to the console and to the log file (BESM6_DEBUG).
 * When the first character of the format is an underscore, nothing goes to
 * the console.
 */
static void log_out(const char *fmt, va_list args, int newline)
{
    va_list console;

    if (*fmt == '_')
        ++fmt;
    else {
        va_copy(console, args);
        sink_vprintf(sim_con, fmt, console);
        if (newline)
            sink_puts(sim_con, "\r\n");
        va_end(console);
    }
    if (sim_deb) {
        sink_vprintf(sim_deb, fmt, args);
        if (newline)
            sink_puts(sim_deb, "\n");
    }
}

/*
 * Appends a newline.
 */
void besm6_log(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    log_out(fmt, args, 1);
    va_end(args);
}

/*
 * Appends no newline.
 */
void besm6_log_cont(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    log_out(fmt, args, 0);
    va_end(args);
}

/*
 * The same; historically the debug channel.
 */
void besm6_debug(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    log_out(fmt, args, 1);
    va_end(args);
}

/*
 * Convert a floating-point number to the BESM-6 format.
 *
 * The IEEE 754 (double) representation:
 *      64   63———53 52————–1
 *      sign exponent mantissa
 * The top (53rd) bit of the mantissa is not stored and is always 1.
 *
 * The BESM-6 representation:
 *      48——–42 41   40————————————————–1
 *      exponent sign mantissa in two's complement
 */
t_value ieee_to_besm6(double d)
{
    t_value word;
    int exponent;
    int sign;

    sign = d < 0;
    if (sign)
        d = -d;
    d = frexp(d, &exponent);
    /* 0.5 <= d < 1.0 */
    d    = ldexp(d, 40);
    word = (t_value)d;
    if (d - word >= 0.5)
        word += 1; /* Rounding. */
    if (exponent < -64)
        return 0LL; /* A number close to zero */
    if (exponent > 63) {
        return sign ? 0xFEFFFFFFFFFFLL : /* The largest number */
                   0xFF0000000000LL;     /* The smallest number */
    }
    if (sign)
        word = 0x20000000000LL - word; /* Sign. */
    word |= ((t_value)(exponent + 64)) << 41;
    return word;
}

double besm6_to_ieee(t_value word)
{
    double mantissa;
    int exponent;

    /* Strip the tag */
    word &= BITS48;

    /* Shift so that the sign of the mantissa lands on the sign of the integer;
     * `mantissa' is then the original mantissa multiplied by 2**63.
     */
    mantissa = (double)(((t_int64)word) << (64 - 48 + 7));

    exponent = word >> 41;

    /* The exponent is biased by 64, and the mantissa needs correcting */
    return ldexp(mantissa, exponent - 64 - 63);
}

/*
 * Skip whitespace.
 */
const char *skip_spaces(const char *p)
{
    for (;;) {
        if (*p == (char)0xEF && p[1] == (char)0xBB && p[2] == (char)0xBF) {
            /* Skip zero width no-break space. */
            p += 3;
            continue;
        }
        if (*p == ' ' || *p == '\t' || *p == '\r') {
            ++p;
            continue;
        }
        return p;
    }
}

/*
 * Fetch Unicode symbol from UTF-8 string.
 * Advance string pointer.
 */
int utf8_to_unicode(const char **p)
{
    int c1, c2, c3;

    c1 = (unsigned char)*(*p)++;
    if (!(c1 & 0x80))
        return c1;
    c2 = (unsigned char)*(*p)++;
    if (!(c1 & 0x20))
        return (c1 & 0x1f) << 6 | (c2 & 0x3f);
    c3 = (unsigned char)*(*p)++;
    return (c1 & 0x0f) << 12 | (c2 & 0x3f) << 6 | (c3 & 0x3f);
}

char *besm6_parse_octal(const char *cptr, int *offset)
{
    char *eptr;

    *offset = strtol(cptr, &eptr, 8);
    if (eptr == cptr)
        return 0;
    return eptr;
}

static const char *get_alnum(const char *iptr, char *optr)
{
    while ((*iptr >= 'a' && *iptr <= 'z') || (*iptr >= 'A' && *iptr <= 'Z') ||
           (*iptr >= '0' && *iptr <= '9') || (*iptr & 0x80)) {
        *optr++ = *iptr++;
    }
    *optr = 0;
    return iptr;
}

/*
 * Parse single instruction (half word).
 * Allow mnemonics or octal code.
 */
const char *parse_instruction(const char *cptr, uint32 *val)
{
    int opcode, reg, addr, negate;
    char gbuf[CBUFSIZE];

    cptr = skip_spaces(cptr); /* absorb spaces */
    if (*cptr >= '0' && *cptr <= '7') {
        /* Octal form. */
        cptr = besm6_parse_octal(cptr, &reg); /* get register */
        if (!cptr || reg > 15) {
            /*printf ("Bad register\n");*/
            return 0;
        }
        cptr = skip_spaces(cptr); /* absorb spaces */
        if (*cptr == '2' || *cptr == '3') {
            /* Long instruction. */
            cptr = besm6_parse_octal(cptr, &opcode);
            if (!cptr || opcode < 020 || opcode > 037) {
                /*printf ("Bad long opcode\n");*/
                return 0;
            }
            opcode <<= 3;
        } else {
            /* Short instruction. */
            cptr = besm6_parse_octal(cptr, &opcode);
            if (!cptr || opcode > 0177) {
                /*printf ("Bad short opcode\n");*/
                return 0;
            }
        }
        cptr = besm6_parse_octal(cptr, &addr); /* get address */
        if (!cptr || addr > BITS(15) || (opcode <= 0177 && addr > BITS(12))) {
            /*printf ("Bad address\n");*/
            return 0;
        }
    } else {
        /* Mnemonic form of the instruction. */
        cptr   = get_alnum(cptr, gbuf); /* get opcode */
        opcode = besm6_opcode(gbuf);
        if (opcode < 0) {
            /*printf ("Bad opname: %s\n", gbuf);*/
            return 0;
        }
        negate = 0;
        cptr   = skip_spaces(cptr); /* absorb spaces */
        if (*cptr == '-') {         /* negative offset */
            negate = 1;
            cptr   = skip_spaces(cptr + 1); /* absorb spaces */
        }
        addr = 0;
        if (*cptr >= '0' && *cptr <= '7') {
            /* Octal address. */
            cptr = besm6_parse_octal(cptr, &addr);
            if (!cptr || addr > BITS(15)) {
                /*printf ("Bad address: %o\n", addr);*/
                return 0;
            }
            if (negate)
                addr = (-addr) & BITS(15);
            if (opcode <= 077 && addr > BITS(12)) {
                if (addr < 070000) {
                    /*printf ("Bad short address: %o\n", addr);*/
                    return 0;
                }
                opcode |= 0100;
                addr &= BITS(12);
            }
        }
        reg  = 0;
        cptr = skip_spaces(cptr); /* absorb spaces */
        if (*cptr == '(') {
            /* Index register in parentheses. */
            cptr = besm6_parse_octal(cptr + 1, &reg);
            if (!cptr || reg > 15) {
                /*printf ("Bad register: %o\n", reg);*/
                return 0;
            }
            cptr = skip_spaces(cptr); /* absorb spaces */
            if (*cptr != ')') {
                /*printf ("No closing brace\n");*/
                return 0;
            }
            ++cptr;
        }
    }
    *val = reg << 20 | opcode << 12 | addr;
    return cptr;
}

/*
 * Instruction parse: two commands per word.
 */
t_stat parse_instruction_word(const char *cptr, t_value *val)
{
    uint32 left, right;

    *val = 0;
    cptr = parse_instruction(cptr, &left);
    if (!cptr)
        return SCPE_ARG;
    right = 0;
    cptr  = skip_spaces(cptr);
    if (*cptr == ',') {
        cptr = parse_instruction(cptr + 1, &right);
        if (!cptr)
            return SCPE_ARG;
    }
    cptr = skip_spaces(cptr); /* absorb spaces */
    if (*cptr != 0 && *cptr != ';' && *cptr != '\n' && *cptr != '\r') {
        /*printf ("Extra symbols at eoln: %s\n", cptr);*/
        return SCPE_2MARG;
    }
    *val = (t_value)left << 24 | right;
    return SCPE_OK;
}

/*
 * Print a machine instruction with its mnemonic.
 */
void besm6_fprint_cmd(Sink *of, uint32 cmd)
{
    int reg, opcode, addr;

    reg = (cmd >> 20) & 017;
    if (cmd & BBIT(20)) {
        opcode = (cmd >> 12) & 0370;
        addr   = cmd & BITS(15);
    } else {
        opcode = (cmd >> 12) & 077;
        addr   = cmd & 07777;
        if (cmd & BBIT(19))
            addr |= 070000;
    }
    sink_printf(of, "%s", besm6_opname(opcode));
    if (addr) {
        sink_printf(of, " ");
        if (addr >= 077700)
            sink_printf(of, "-%o", (addr ^ 077777) + 1);
        else
            sink_printf(of, "%o", addr);
    }
    if (reg) {
        if (!addr)
            sink_printf(of, " ");
        sink_printf(of, "(%o)", reg);
    }
}

/*
 * Print a machine instruction in octal.
 */
void besm6_fprint_insn(Sink *of, uint32 insn)
{
    if (insn & BBIT(20))
        sink_printf(of, "%02o %02o %05o ", insn >> 20, (insn >> 15) & 037, insn & BITS(15));
    else
        sink_printf(of, "%02o %03o %04o ", insn >> 20, (insn >> 12) & 0177, insn & 07777);
}

/*
 * Symbolic decode
 *
 * Inputs:
 *      *of     = output stream
 *      addr    = current PC
 *      *val    = pointer to data
 *      *uptr   = pointer to unit
 *      sw      = switches
 * Outputs:
 *      return  = status code
 */
t_stat fprint_sym(Sink *of, uint32 addr, t_value *val, UNIT *uptr, int32 sw)
{
    t_value cmd;

    if (uptr && (uptr != &cpu_unit)) /* must be CPU */
        return SCPE_ARG;

    cmd = val[0];

    if (sw & SWMASK('M')) { /* symbolic decode? */
        besm6_fprint_cmd(of, (uint32)(cmd >> 24));
        sink_printf(of, ",\n\t");
        besm6_fprint_cmd(of, cmd & BITS(24));
    } else if (sw & SWMASK('I')) {
        besm6_fprint_insn(of, (cmd >> 24) & BITS(24));
        besm6_fprint_insn(of, cmd & BITS(24));
    } else if (sw & SWMASK('F')) {
        sink_printf(of, "%#.2g", besm6_to_ieee(cmd));
    } else if (sw & SWMASK('B')) {
        sink_printf(of, "%03o %03o %03o %03o %03o %03o", (int)(cmd >> 40) & 0377,
                    (int)(cmd >> 32) & 0377, (int)(cmd >> 24) & 0377, (int)(cmd >> 16) & 0377,
                    (int)(cmd >> 8) & 0377, (int)cmd & 0377);
    } else if (sw & SWMASK('X')) {
        sink_printf(of, "%013llx", cmd);
    } else
        sink_printf(of, "%04o %04o %04o %04o", (int)(cmd >> 36) & 07777, (int)(cmd >> 24) & 07777,
                    (int)(cmd >> 12) & 07777, (int)cmd & 07777);
    return SCPE_OK;
}

/*
 * Read one line of the input file.
 * The line formats:
 * п 76543                     - start address
 * в 12345                     - load address
 * ч -123.45e+6                - a floating-point number
 * с 0123 4567 0123 4567       - an octal word
 * к 00 22 00000 00 010 0000   - instructions
 */
t_stat besm6_read_line(Blob *input, int *type, t_value *val)
{
    char buf[512];
    const char *p;
    int i, c;
again:
    if (!blob_gets(input, buf, sizeof(buf))) {
        *type = 0;
        return SCPE_OK;
    }
    p = skip_spaces(buf);
    if (*p == '\n' || *p == ';')
        goto again;
    c = utf8_to_unicode(&p);
    if (c == CYRILLIC_SMALL_LETTER_VE || c == CYRILLIC_CAPITAL_LETTER_VE || c == 'b' || c == 'B') {
        /* Where the data goes. */
        *type = ':';
        *val  = strtol(p, 0, 8);
        return SCPE_OK;
    }
    if (c == CYRILLIC_SMALL_LETTER_PE || c == CYRILLIC_CAPITAL_LETTER_PE || c == 'p' || c == 'P') {
        /* Start address. */
        *type = '@';
        *val  = strtol(p, 0, 8);
        return SCPE_OK;
    }
    if (c == CYRILLIC_SMALL_LETTER_CHE || c == CYRILLIC_CAPITAL_LETTER_CHE || c == 'f' ||
        c == 'F') {
        /* A floating-point number. */
        *type = '=';
        *val  = ieee_to_besm6(sim_strtod(p, 0));
        return SCPE_OK;
    }
    if (c == CYRILLIC_SMALL_LETTER_ES || c == CYRILLIC_CAPITAL_LETTER_ES || c == 'c' || c == 'C') {
        /* An octal word. */
        *type = '=';
        *val  = 0;
        for (i = 0; i < 16; ++i) {
            p = skip_spaces(p);
            if (*p < '0' || *p > '7') {
                if (i == 0) {
                    /* the word is too short */
                    goto bad;
                }
                break;
            }
            *val = *val << 3 | (*p++ - '0');
        }
        return SCPE_OK;
    }
    if (c == CYRILLIC_SMALL_LETTER_KA || c == CYRILLIC_CAPITAL_LETTER_KA || c == 'k' || c == 'K') {
        /* An instruction. */
        *type = '*';
        if (parse_instruction_word(p, val) != SCPE_OK)
            goto bad;
        return SCPE_OK;
    }
    /* A bad line in the input file */
bad:
    besm6_log("Invalid input line: %s", buf);
    return SCPE_FMT;
}

/*
 * Read one 6-byte big-endian word from a binary a.out image.
 * Returns (t_value)-1 on end of file.
 */
static t_value freadw(Blob *f)
{
    t_value w = 0;
    int i, c;

    for (i = 0; i < 6; ++i) {
        c = blob_getc(f);
        if (c < 0)
            return (t_value)-1;
        w = (w << 8) | (c & 0xff);
    }
    return w;
}

/*
 * Base load address of a fully linked a.out image (BADDR = HDRSZ / W).
 * Words 0..7 are reserved, so the const segment starts at word 010.
 */
#define AOUT_BADDR 8

/*
 * a.out magic numbers: the string "BESM" plus a variant code in the low bits.
 * FMAGIC - standard impure executable; NMAGIC - read-only (pure) text segment.
 */
#define AOUT_FMAGIC 0x4245534d0107LL /* "BESM" + 0407 */
#define AOUT_NMAGIC 0x4245534d0108LL /* "BESM" + 0410 */
#define AOUT_RELFLG 1                /* fully linked, no relocation */

/*
 * Symbol-table entry fields (see v7besm cross/besm6/b.out.h).
 * N_TEXT symbols are functions - the only ones we keep for tracing.
 */
#define AOUT_N_EXT  040 /* external (global) bit */
#define AOUT_N_TYPE 037 /* mask for the type field */
#define AOUT_N_TEXT 03  /* text (code) segment */

/*
 * Extract the a.out symbol table into the tracer's symbol table.
 * The file position is expected to sit at the symbol table (right after
 * the data segment).  Reads at most a_syms bytes; each entry is a byte
 * stream: 1-byte name length (0 terminates), 1-byte type, 3-byte
 * big-endian word address, then the raw name.  Keeps only functions.
 */
static void besm6_load_symbols(Blob *input, int nbytes)
{
    char name[256];
    int n_len, n_type, i, c;
    uint32 n_value;

    besm6_sym_clear();
    while (nbytes > 0) {
        n_len = blob_getc(input);
        if (n_len <= 0) /* terminator or EOF */
            break;
        n_type  = blob_getc(input);
        n_value = 0;
        for (i = 0; i < 3; ++i) {
            c = blob_getc(input);
            if (c < 0)
                return;
            n_value = (n_value << 8) | (c & 0xff);
        }
        for (i = 0; i < n_len; ++i) {
            c = blob_getc(input);
            if (c < 0)
                return;
            name[i] = c;
        }
        name[n_len] = 0;
        nbytes -= n_len + 5;
        if ((n_type & AOUT_N_TYPE) == AOUT_N_TEXT)
            besm6_sym_add(n_value, name);
    }
    besm6_sym_sort();
}

/*
 * Load a binary a.out image: header, then the const/text/data segments.
 * The entry point (a_entry) becomes the start address.
 */
static t_stat besm6_load_aout(Blob *input)
{
    t_value a_magic, a_const, a_text, a_data, a_bss, a_syms, a_entry, a_flag;
    t_value word;
    int addr, i, n;

    a_magic = freadw(input);
    a_const = freadw(input);
    a_text  = freadw(input);
    a_data  = freadw(input);
    a_bss   = freadw(input);
    a_syms  = freadw(input);
    a_entry = freadw(input);
    a_flag  = freadw(input);
    if (a_flag == (t_value)-1) {
        besm6_log("Truncated a.out header");
        return SCPE_FMT;
    }
    (void)a_bss;

    /* Only fully linked images (RELFLG set) can be loaded and run directly. */
    if (!(a_flag & AOUT_RELFLG)) {
        besm6_log("Cannot load relocatable binary");
        return SCPE_FMT;
    }

    addr = AOUT_BADDR;
    /* const segment - read-only data */
    n = (int)(a_const / 6);
    for (i = 0; i < n; ++i) {
        word = freadw(input);
        if (word == (t_value)-1 || addr > MEMSIZE)
            return SCPE_FMT;
        /*
         * The const segment is data, and is tagged as such, EXCEPT for the
         * fixed vector block a kernel lays down inside it: 0500/0501 are the
         * internal- and external-interrupt vectors, and 0550-0577 are the
         * extracode vectors for э50-э77.  Those words are executed, so they
         * must carry the instruction tag -- mmu_fetch() raises "контроль
         * команды" (instruction check) on a data-tagged word.
         */
        if (addr >= 0500 && addr <= 0577)
            memory[addr++] = SET_PARITY(word, PARITY_INSN);
        else
            memory[addr++] = SET_PARITY(word, PARITY_NUMBER);
    }
    /* text segment - machine code */
    n = (int)(a_text / 6);
    for (i = 0; i < n; ++i) {
        word = freadw(input);
        if (word == (t_value)-1 || addr > MEMSIZE)
            return SCPE_FMT;
        memory[addr++] = SET_PARITY(word, PARITY_INSN);
    }
    /* Pure text: page-align the data segment to a 1024-word boundary. */
    if (a_magic == AOUT_NMAGIC)
        addr = (addr + 1023) & ~1023;
    /* data segment - initialized variables */
    n = (int)(a_data / 6);
    for (i = 0; i < n; ++i) {
        word = freadw(input);
        if (word == (t_value)-1 || addr > MEMSIZE)
            return SCPE_FMT;
        memory[addr++] = SET_PARITY(word, PARITY_NUMBER);
    }
    /* symbol table - function names for the call/return trace */
    besm6_load_symbols(input, (int)a_syms);
    PC = (uint32)a_entry;
    return SCPE_OK;
}

/*
 * Load memory from file.
 * Automatically detects a binary a.out image and loads it; otherwise
 * falls back to the textual .b6 memory-image format.
 */
t_stat besm6_load(Blob *input)
{
    int addr, type;
    t_value word;
    t_stat err;
    unsigned char magic[6];

    /* Peek at the first word to detect a binary a.out image. */
    if (blob_read(input, magic, 6) == 6 && magic[0] == 'B' && magic[1] == 'E' && magic[2] == 'S' &&
        magic[3] == 'M' && magic[4] == 0x01 && (magic[5] == 0x07 || magic[5] == 0x08)) {
        blob_rewind(input);
        return besm6_load_aout(input);
    }
    blob_rewind(input);

    /* Textual .b6 image carries no symbols: drop any from a prior a.out. */
    besm6_sym_clear();
    addr = 1;
    PC   = 1;
    for (;;) {
        err = besm6_read_line(input, &type, &word);
        if (err)
            return err;
        switch (type) {
        case 0: /* EOF */
            return SCPE_OK;
        case ':': /* address */
            addr = (int)word;
            break;
        case '=': /* word */
            if (addr < 010)
                pult[0][addr] = SET_PARITY(word, PARITY_NUMBER);
            else
                memory[addr] = SET_PARITY(word, PARITY_NUMBER);
            ++addr;
            break;
        case '*': /* instruction */
            if (addr < 010)
                pult[0][addr] = SET_PARITY(word, PARITY_INSN);
            else
                memory[addr] = SET_PARITY(word, PARITY_INSN);
            ++addr;
            break;
        case '@': /* start address */
            PC = (uint32)word;
            break;
        }
        if (addr > MEMSIZE)
            return SCPE_FMT;
    }
    return SCPE_OK;
}

/*
 * Dump memory to file.
 */
t_stat besm6_dump(Sink *of, const char *fnam)
{
    int addr, last_addr = -1;
    t_value word;

    sink_printf(of, "; %s\n", fnam);
    for (addr = 1; addr < MEMSIZE; ++addr) {
        if (addr < 010)
            word = pult[0][addr];
        else
            word = memory[addr];
        if (word == 0)
            continue;
        if (addr != last_addr + 1) {
            sink_printf(of, "\nв %05o\n", addr);
        }
        last_addr = addr;
        if (IS_INSN(word)) {
            sink_printf(of, "к ");
            besm6_fprint_cmd(of, (uint32)(word >> 24));
            sink_printf(of, ", ");
            besm6_fprint_cmd(of, word & BITS(24));
            sink_printf(of, "\t\t; %05o - ", addr);
            sink_printf(of, "%04o %04o %04o %04o\n", (int)(word >> 36) & 07777,
                        (int)(word >> 24) & 07777, (int)(word >> 12) & 07777, (int)word & 07777);
        } else {
            sink_printf(of, "с %04o %04o %04o %04o", (int)(word >> 36) & 07777,
                        (int)(word >> 24) & 07777, (int)(word >> 12) & 07777, (int)word & 07777);
            sink_printf(of, "\t\t; %05o\n", addr);
        }
    }
    return SCPE_OK;
}

/*
 * Loader.  Upstream folded the dumper in behind a `dump_flag' and passed it the
 * *input* file to write to, which nothing ever exercised; besm6_dump() is its
 * own entry point now and takes a Sink like everything else that formats.
 */
t_stat sim_load(Blob *fi)
{
    return besm6_load(fi);
}
