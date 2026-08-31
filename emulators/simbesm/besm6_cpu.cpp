/*
 * BESM-6 CPU simulator.
 *
 * Copyright (c) 1997-2009, Leonid Broukhis
 * Copyright (c) 2009, Serge Vakulenko
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

 * For more information about BESM-6 computer, visit sites:
 *  - http://www.computer-museum.ru/english/besm6.htm
 *  - http://mailcom.com/besm6/
 *  - http://groups.google.com/group/besm6
 *
 * Release notes for BESM-6/SIMH
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  1) All addresses and data values are displayed in octal.
 *  2) Memory size is 128 kwords.
 *  3) Interrupt system is to be synchronized with wallclock time.
 *  4) Execution times are in 1/10 of microsecond.
 *  5) Magnetic drums are implemented as a single "DRUM" device.
 *  6) Magnetic disks are implemented.
 *  7) Magnetic tape, punch tape and punch cards are not implemented.
 *  8) Displays are implemented.
 *  9) Printer АЦПУ-128 is not implemented.
 * 10) Instruction mnemonics, register names and stop messages
 *     are in Russian using UTF-8 encoding. It is assumed, that
 *     user locale is UTF-8.
 * 11) A lot of comments in Russian (UTF-8).
 */
#include "besm6_defs.h"

t_value memory[MEMSIZE];
uint32 PC, RK, Aex, M[NREGS], RAU, RUU;
t_value ACC, RMR, GRP, MGRP;
uint32 PRP, MPRP;
uint32 READY, READY2;       /* ready flags of various devices */
int32 tmr_poll = CLK_DELAY; /* pgm timer poll */
uint32 trace_counter;

/* Wired (non-registered) bits of interrupt registers (GRP and PRP)
 * cannot be cleared by writing to the GRP and must be cleared by clearing
 * the registers generating the corresponding interrupts.
 */
#define GRP_WIRED_BITS                                                                    \
    (GRP_DRUM1_FREE | GRP_DRUM2_FREE | GRP_CHAN3_DONE | GRP_CHAN4_DONE | GRP_CHAN5_DONE | \
     GRP_CHAN6_DONE | GRP_CHAN3_FREE | GRP_CHAN4_FREE | GRP_CHAN5_FREE | GRP_CHAN6_FREE | \
     GRP_CHAN7_FREE)

#define PRP_WIRED_BITS                                                                    \
    (PRP_VU1_END | PRP_VU2_END | PRP_PCARD1_PUNCH | PRP_PCARD2_PUNCH | PRP_PTAPE1_PUNCH | \
     PRP_PTAPE2_PUNCH)

int corr_stack;
int autotime;
int besm6_latin; /* persistent Latin (MADLEN) mnemonic mode */

t_stat cpu_reset(DEVICE *dptr);

/*
 * CPU data structures
 *
 * cpu_dev      CPU device descriptor
 * cpu_unit     CPU unit descriptor
 * cpu_reg      CPU register list
 * cpu_mod      CPU modifiers list
 */

UNIT cpu_unit = { 0 };

DEVICE cpu_dev = { .name = "CPU", .units = &cpu_unit, .numunits = 1, .reset = &cpu_reset };

/*
 * REG: A pseudo-device containing Latin synonyms of all CPU registers.
 */
UNIT reg_unit = { 0 };

DEVICE reg_dev = { .name = "REG", .units = &reg_unit, .numunits = 1 };

/*
 * SCP data structures and interface routines
 *
 * sim_devices          array of pointers to simulated devices
 * sim_stop_messages    array of pointers to stop messages
 */

DEVICE
*sim_devices[] = { &cpu_dev,   &reg_dev,   &drum_dev,  md_dev,     md_dev + 1,
                   md_dev + 2, md_dev + 3, md_dev + 4, md_dev + 5, md_dev + 6,
                   md_dev + 7, &mmu_dev,   &clock_dev, &tty_dev, /* terminals: teletypes,
                                                                    Videotons, "Consuls" */
                   0 };

const char *sim_stop_messages[SCPE_BASE] = {
    "Неизвестная ошибка",                 /* Unknown error */
    "Останов",                            /* STOP */
    "Выход за пределы памяти",            /* Run out end of memory */
    "Запрещенная команда",                /* Invalid instruction */
    "Контроль команды",                   /* A data-tagged word fetched */
    "Команда в чужом листе",              /* Paging error during fetch */
    "Число в чужом листе",                /* Paging error during load/store */
    "Контроль числа МОЗУ",                /* RAM parity error */
    "Контроль числа БРЗ",                 /* Write cache parity error */
    "Переполнение АУ",                    /* Arith. overflow */
    "Деление на нуль",                    /* Division by zero or denorm */
    "Двойное внутреннее прерывание",      /* SIMH: Double internal interrupt */
    "Чтение неформатированного барабана", /* Reading unformatted drum */
    "Чтение неформатированного диска",    /* Reading unformatted disk */
    "Останов по КРА",                     /* Hardware breakpoint */
    "Останов по считыванию",              /* Load watchpoint */
    "Останов по записи",                  /* Store watchpoint */
    "Не реализовано",                     /* Unimplemented I/O or special reg. access */
};

/*
 * Reset routine
 */
t_stat cpu_reset(DEVICE *dptr)
{
    int i;
    ACC = 0;
    RMR = 0;
    RAU = 0;
    RUU = RUU_EXTRACODE | RUU_AVOST_DISABLE;
    for (i = 0; i < NREGS; ++i)
        M[i] = 0;

    /* Punchcard readers not yet implemented thus not ready */
    /* READY2 |= 042000000; */

    /* Register 17: БлП, БлЗ, ПоП, ПоК, БлПр */
    M[PSW] =
        PSW_MMAP_DISABLE | PSW_PROT_DISABLE | PSW_INTR_HALT | PSW_CHECK_HALT | PSW_INTR_DISABLE;

    /* Register 23: БлП, БлЗ, РежЭ, БлПр */
    M[SPSW] = SPSW_MMAP_DISABLE | SPSW_PROT_DISABLE | SPSW_EXTRACODE | SPSW_INTR_DISABLE;

    GRP = MGRP = 0;
    // Disabled due to a conflict with loading
    // PC = 1;                 /* "reset cpu; go" should start from 1  */

    besm6_trace_reset(); /* full register dump on first trace */

    return SCPE_OK;
}

/*
 * The "рег" (reg) instruction
 */
static t_stat cmd_002()
{
#if 0
    besm6_debug ("*** reg %03o", Aex & 0377);
#endif
    switch (Aex & 0377) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
        /* Write to the БРЗ */
        mmu_setcache(Aex & 7, ACC);
        break;
    case 020:
    case 021:
    case 022:
    case 023:
    case 024:
    case 025:
    case 026:
    case 027:
        /* Write to the page registers */
        mmu_setrp(Aex & 7, ACC);
        break;
    case 030:
    case 031:
    case 032:
    case 033:
        /* Write to the protection registers */
        mmu_setprotection(Aex & 3, ACC);
        break;
    case 036:
        /* Write to the mask of the main interrupt register */
        MGRP = ACC;
        break;
    case 037:
        /* Clearing the main interrupt register: */
        /* it is impossible to clear wired (stateless) bits this way */
        GRP &= ACC | GRP_WIRED_BITS;
        break;
    case 64:
    case 65:
    case 66:
    case 67:
    case 68:
    case 69:
    case 70:
    case 71:
    case 72:
    case 73:
    case 74:
    case 75:
    case 76:
    case 77:
    case 78:
    case 79:
    case 80:
    case 81:
    case 82:
    case 83:
    case 84:
    case 85:
    case 86:
    case 87:
    case 88:
    case 89:
    case 90:
    case 91:
    case 92:
    case 93:
    case 94:
    case 95:
        /* 0100 - 0137:
         * Bit 1 controls the БРО halt-mode block.
         * Bits 2 and 3 say whether the check bits
         * (ПКП and ПКЛ) are generated. */
        if (Aex & 1)
            RUU |= RUU_AVOST_DISABLE;
        else
            RUU &= ~RUU_AVOST_DISABLE;
        if (Aex & 2)
            RUU |= RUU_PARITY_RIGHT;
        else
            RUU &= ~RUU_PARITY_RIGHT;
        if (Aex & 4)
            RUU |= RUU_PARITY_LEFT;
        else
            RUU &= ~RUU_PARITY_LEFT;
        break;
    case 0200:
    case 0201:
    case 0202:
    case 0203:
    case 0204:
    case 0205:
    case 0206:
    case 0207:
        /* Read the БРЗ */
        ACC = mmu_getcache(Aex & 7);
        break;
    case 0237:
        /* Read the main interrupt register */
        ACC = GRP;
        break;
    default:
        if ((Aex & 0340) == 0140) {
            /* TODO: watchdog reset mechanism */
            return STOP_UNIMPLEMENTED;
        }
        /* Unused addresses */
        besm6_debug("*** %05o%s: REG %o - invalid special register address", PC,
                    (RUU & RUU_RIGHT_INSTR) ? "R" : "L", Aex);
        break;
    }
    return SCPE_OK;
}

/*
 * The "увв" (ext) instruction
 */
static t_stat cmd_033()
{
    static uint32 tableau;
#if 1
    if (Aex & ~04177)
        besm6_debug("*** @%05o, ext %05o, ACC[24:1]=%08o", PC, Aex, (uint32)ACC & BITS(24));
#endif
    switch (Aex & 04177) {
    case 0:
        /*
         * Releasing the drum printer solenoids. No effect on simulation.
         */
        break;
    case 1:
    case 2:
        /* Control a magnetic drum transfer */
        CPU_TRY(drum(Aex - 1, (uint32)ACC));
        break;
    case 3:
    case 4:
        /* Hand over the control word for a magnetic
         * disk transfer */
        disk_io(Aex - 3, (uint32)ACC);
        break;
    case 5:
    case 6:
        /* control a magnetic tape transfer: there is no tape */
        break;
    case 7:
        return STOP_UNIMPLEMENTED;
        break;
    case 010:
    case 011:
        /* control the paper tape readers: there is no ФС */
        break;
    case 012:
    case 013:
        /* TODO: control the paper tape readers from the hardwired program */
        return STOP_UNIMPLEMENTED;
        break;
    case 014:
    case 015:
        /* Control the АЦПУ printer: there is none */
        break;
    case 023:
    case 024:
        /* Control a magnetic disk transfer */
        CPU_TRY(disk_ctl(Aex - 023, (uint32)ACC));
        break;
    case 030:
        /* Clear the ПРП */
        /*              besm6_debug(">>> clear PRP");*/
        PRP &= ACC | PRP_WIRED_BITS;
        break;
    case 031:
        /* Simulate ГРП interrupt signals */
        /*besm6_debug ("*** %05o%s: simulated interrupts GRP %016llo",
          PC, (RUU & RUU_RIGHT_INSTR) ? "R" : "L", ACC << 24);*/
        GRP |= (ACC & BITS(24)) << 24;
        break;
    case 032:
    case 033:
        /* TODO: simulate signals from the КМБ to the КВУ */
        return STOP_UNIMPLEMENTED;
        break;
    case 034:
        /* Write to the МПРП */
        /*              besm6_debug(">>> write to MPRP");*/
        MPRP = ACC & 077777777;
        // besm6_debug("MPRP = %016llo", MPRP);
        break;
    case 035:
        /* TODO: control the transfer-simulation mode for drums
         * and tapes, and simulate a transfer */
        return STOP_UNIMPLEMENTED;
        break;
    case 040:
    case 041:
    case 042:
    case 043:
    case 044:
    case 045:
    case 046:
    case 047:
    case 050:
    case 051:
    case 052:
    case 053:
    case 054:
    case 055:
    case 056:
    case 057:
        /* Control the АЦПУ hammers: there is no printer */
        break;
    case 070:
        /* ES printer output */
        besm6_debug(">>> ES print: %016llo", ACC);
        break;
    case 0140:
        /* Write to the telegraph channel register */
        tty_send((uint32)ACC & BITS(24));
        break;
    case 0141:
        /* formatting magnetic tape: no tape */
        break;
    case 0142:
        /* TODO: simulate ПРП interrupt signals */
        return STOP_UNIMPLEMENTED;
        break;
    case 0143:
        /* sending a syllable to the muxed serial interface */
        mux_send((uint32)ACC & BITS(16));
        break;
    case 0150:
    case 0151:
        /* commands to the punched card readers: no reader */
        break;
    case 0153:
        /* clear the terminal interface hardware */
        mux_clear(); /* ACC is ignored */
        memory[077023] = SET_PARITY(1LL << 47, PARITY_NUMBER);
        memory[076774] = SET_PARITY(00101010101010101LL, PARITY_NUMBER);
        MPRP |= 040;
        break;
    case 0154:
    case 0155:
        /* Punchcard output, motor and culling control: no puncher */
        break;
    case 0160:
    case 0161:
    case 0162:
    case 0163:
    case 0164:
    case 0165:
    case 0166:
    case 0167:
        /* Punchcard output, punching solenoids: no puncher */
        break;
    case 0170:
    case 0171:
        /* punch a row on paper tape: there is no punch */
        break;
    case 0172:
    case 0173:
        besm6_debug(">>> Potential plotter output: %03o", (uint32)ACC & BITS(8));
        break;
    case 0174:
    case 0175:
        /* Send a code to the operator's console */
        consul_print(Aex & 1, (uint32)ACC & BITS(8));
        break;
    case 0147:
        /* Writing to the power supply control register
         * does not have any observable effect; repurposed
         */
        // break;
    case 0177:
        /* control the display panel of the ГПВЦ СО АН СССР */
        if (tableau != ((uint32)ACC & BITS(24))) {
            tableau = (uint32)ACC & BITS(24);
            // besm6_debug(">>> PANEL: %08o", tableau);
        }
        break;
    case 04001:
    case 04002:
        /* TODO: read a syllable in transfer-simulation mode */
        return STOP_UNIMPLEMENTED;
        break;
    case 04003:
    case 04004:
        /* Query the magnetic disk controller status */
        ACC = disk_state(Aex - 04003);
        break;
    case 04006:
        /* TODO: read a row from the paper tape reader
         * in the hardwired program */
        return STOP_UNIMPLEMENTED;
        break;
    case 04007:
        /* TODO: poll the sync pulse of a non-zero row
         * in the hardwired paper tape reader program */
        return STOP_UNIMPLEMENTED;
        break;
    case 04014:
    case 04015:
        /* read a row from the paper tape reader: there is no ФС */
        ACC = 0;
        break;
    case 04016:
    case 04017:
        /* TODO: read a row from the paper
         * tape reader */
        return STOP_UNIMPLEMENTED;
        break;
    case 04020:
    case 04021:
    case 04022:
    case 04023:
        /* TODO: read a syllable in external-transfer
         * simulation mode */
        return STOP_UNIMPLEMENTED;
        break;
    case 04030:
        /* Read the high half of the ПРП */
        ACC = PRP & 077770000;
        break;
    case 04031:
        /* Poll the ready signals (the АЦПУ printer and the rest) */
        /*              besm6_debug("Reading READY");*/
        ACC = READY;
        break;
    case 04034:
        /* Read the low half of the ПРП */
        ACC = (PRP & 07777) | 037;
        break;
    case 04035:
        /* Poll the ОШМi trigger: whether an external transfer had errors. */
        ACC = drum_errors() | disk_errors();
        break;
    case 04070:
        /* ES printer status: */
        besm6_debug("<<< ES printer read");
        ACC = 01000;
        break;
    case 04100:
        /* Poll the telegraph channels */
        ACC = tty_query();
        break;
    case 04102:
        /* Poll the punched card and paper tape ready signals */
        /*              besm6_debug("Reading punchcard/punchtape READY @%05o", PC);*/
        ACC = READY2;
        break;
    case 04103:
    case 04104:
    case 04105:
    case 04106:
        /* Poll the tape transport status: there is no tape */
        ACC = 0;
        break;
    case 04107:
        /* poll the tape write-check circuitry */
        ACC = 0;
        break;
    case 04115:
        /* An unknown access.  DISPAK issues this instruction
         * in groups of eight every few seconds. */
        ACC = 0; // 0240;
        break;
    case 04143:
        /* reading from the muxed serial interface */
        ACC = mux_read();
        break;
    case 04150:
    case 04154:
        /* read a row from the card reader: there is no ВУ */
        ACC = 0;
        break;
    case 04160:
    case 04161:
    case 04162:
    case 04163:
    case 04164:
    case 04165:
    case 04166:
    case 04167:
        /* Punchcard output, reading a punched line back: no puncher */
        ACC = 0;
        break;
    case 04170:
    case 04171:
    case 04172:
    case 04173:
        /* TODO: read the check code of a
         * paper tape row */
        return STOP_UNIMPLEMENTED;
        break;
    case 04174:
    case 04175:
        /* Read a code from the operator's console */
        ACC = consul_read(Aex & 1);
        break;
    case 04177:
        /* read the display panel of the ГПВЦ СО АН СССР */
        ACC = tableau;
        break;
    default: {
        unsigned val = Aex & 04177;
        if (0100 <= val && val <= 0137) {
            /* Control the tape transports and clear the bits of the
             * zone-seek-complete registers: there is no tape. */
        } else if (04140 <= val && val <= 04157) {
            /* TODO: read a punched card row */
            return STOP_UNIMPLEMENTED;
        } else {
            /* Unused addresses */
            /*              if (sim_deb && cpu_dev.dctrl)*/
            besm6_debug("*** %05o%s: EXT %o - invalid I/O address", PC,
                        (RUU & RUU_RIGHT_INSTR) ? "R" : "L", Aex);
            ACC = 0;
        }
    } break;
    }
    return SCPE_OK;
}

void check_initial_setup()
{
    const int MGRP_COPY = 01455; /* OS version specific? */
    const int TAKEN     = 0442;  /* fixed? */
    const int YEAR      = 0221;  /* fixed */

    /* bit 47 of the ЗАНЯТА cell enables operator commands at all */
    const t_value SETUP_REQS_ENABLED = 1LL << 46;

    /* bit 7 of the ЗАНЯТА cell enables any command */
    const t_value ALL_REQS_ENABLED = 1 << 6;

    if (!vt_is_idle()) {
        /* Avoid sending setup requests while the OS
         * is still printing boot-up messages.
         */
        return;
    }
    if ((memory[TAKEN] & SETUP_REQS_ENABLED) == 0 || /* not ready for setup */
        (memory[TAKEN] & ALL_REQS_ENABLED) != 0 ||   /* all done */
        (MGRP & GRP_PANEL_REQ) == 0) {               /* not at the moment */
        return;
    }

    /* Issue the operator commands СМЕ and ВРЕ,
     * and patch the date directly in memory.
     */
    /* The shift number is in bits 22-24 of the МГРП: set it if it is not set */
    if (((memory[MGRP_COPY] >> 21) & 3) == 0) {
        /* command СМЕ: ТР6 = 010, ТР4 = 1, bits 22-24 of ТР5 are the shift number */
        pult[0][6] = 010;
        pult[0][4] = 1;
        pult[0][5] = 1 << 21;
        GRP |= GRP_PANEL_REQ;
        // trace_counter = 2000;
        // besm6_debug("Setting operator shift number");
    } else {
        SimTime d;
        int mon;
        t_value date;

        /* The ГОД cell is updated here directly */
        sim_get_time(&d);
        mon  = d.mon + 1;
        date = (t_value)(d.mday / 10) << 33 | (t_value)(d.mday % 10) << 29 | (mon / 10) << 28 |
               (mon % 10) << 24 | (d.year % 10) << 20 | ((d.year / 10) % 10) << 16 |
               (memory[YEAR] & 7);
        memory[YEAR] = SET_PARITY(date, PARITY_NUMBER);
        /* command ВРЕ: ТР6 = 016, ТР5 bits 9-14 are the hour, bits 1-8 the minute */
        pult[0][6] = 016;
        pult[0][4] = 0;
        pult[0][5] = (d.hour / 10) << 12 | (d.hour % 10) << 8 | (d.min / 10) << 4 | (d.min % 10);
        GRP |= GRP_PANEL_REQ;
    }
}

static unsigned short extmem[32768];
static unsigned short last;

void write_032(int addr, t_value val)
{
    int v = val & 077777777;
    // if (v || addr) besm6_debug("W32 %08o -> %05o", v, addr);
    switch (addr) {
    case 0:
        if (v & 2) {
            PRP &= ~040;
        } else if (v == 0) {
            static int cnt;
            if (++cnt % 10000 == 0) {
                besm6_debug("10K writes of 0 to 0");
            }
        }
        break;
    case 077777:
        if (v & 0400) {
            // interrupt - will result in setting E6 in PRP after some time
            PRP |= 040;
        }
        break;
    default:
        last = extmem[addr] = v;
    }
}

t_value read_032(int addr)
{
    // besm6_debug("R32 %05o", addr);
    switch (addr) {
    case 0:
        return 0400; /* ready */
    case 2:
        return last;
    default:
        return extmem[addr];
    }
}

/*
 * Execute one instruction, placed on address PC:RUU_RIGHT_INSTR.
 * Returns a stop code when the instruction trapped, and zero otherwise;
 * upstream longjmp'd to cpu_halt instead.
 */
t_stat cpu_one_inst()
{
    int reg, opcode, addr, nextpc, next_mod;
    t_value word, op;

    /*
     * Instruction execution time in 100 ns ticks; not really used
     * as the amortized 1 MIPS instruction rate is assumed.
     * The assignments of MEAN_TIME(x,y) to the delay variable
     * are kept as a reference.
     */
    uint32 delay __attribute__((unused)); /* MEAN_TIME() assignments below are documentation */
    corr_stack = 0;
    CPU_TRY(mmu_fetch(PC, &word));
    if (RUU & RUU_RIGHT_INSTR)
        RK = (uint32)word; /* get right instruction */
    else
        RK = (uint32)(word >> 24); /* get left instruction */

    RK &= BITS(24);
    reg = RK >> 20;
    if (RK & BBIT(20)) {
        addr   = RK & BITS(15);
        opcode = (RK >> 12) & 0370;
    } else {
        addr = RK & BITS(12);
        if (RK & BBIT(19))
            addr |= 070000;
        opcode = (RK >> 12) & 077;
    }
    if (trace_counter && PC != 04440) {
        const char *besm6_opname(int opcode);
        --trace_counter;
        besm6_log_cont("_%05o: %s\t%o", PC, besm6_opname(opcode), addr);
        if (reg)
            besm6_log_cont("_(M%o)", reg);
        besm6_log("_");
    }
    if (sim_deb && cpu_dev.dctrl) {
        /* Trace this instruction: address, octal fields and mnemonics. */
        besm6_trace_instruction();
    }
    nextpc = ADDR(PC + 1);
    if (RUU & RUU_RIGHT_INSTR) {
        PC += 1; /* increment PC */
        RUU &= ~RUU_RIGHT_INSTR;
    } else {
        mmu_prefetch(nextpc | (IS_SUPERVISOR(RUU) ? BBIT(16) : 0), 0);
        RUU |= RUU_RIGHT_INSTR;
    }

    if (RUU & RUU_MOD_RK) {
        addr = ADDR(addr + M[MOD]);
    }
    next_mod = 0;
    delay    = 0;

    switch (opcode) {
    case 000: /* зп, atx */
        Aex = ADDR(addr + M[reg]);
        CPU_TRY(mmu_store(Aex, ACC));
        if (!addr && reg == 017)
            M[017] = ADDR(M[017] + 1);
        delay = MEAN_TIME(3, 3);
        break;
    case 001: /* зпм, stx */
        Aex = ADDR(addr + M[reg]);
        CPU_TRY(mmu_store(Aex, ACC));
        M[017]     = ADDR(M[017] - 1);
        corr_stack = 1;
        CPU_TRY(mmu_load(M[017], &ACC));
        RAU   = SET_LOGICAL(RAU);
        delay = MEAN_TIME(6, 6);
        break;
    case 002: /* рег, mod */
        Aex = ADDR(addr + M[reg]);
        if (!IS_SUPERVISOR(RUU))
            return STOP_BADCMD;
        CPU_TRY(cmd_002());
        /* The АУ mode is logical when the operation was a read */
        if (Aex & 0200)
            RAU = SET_LOGICAL(RAU);
        delay = MEAN_TIME(3, 3);
        break;
    case 003: /* счм, xts */
        CPU_TRY(mmu_store(M[017], ACC));
        M[017]     = ADDR(M[017] + 1);
        corr_stack = -1;
        Aex        = ADDR(addr + M[reg]);
        CPU_TRY(mmu_load(Aex, &ACC));
        RAU   = SET_LOGICAL(RAU);
        delay = MEAN_TIME(6, 6);
        break;
    case 004: /* сл, a+x */
        if (!addr && reg == 017) {
            M[017]     = ADDR(M[017] - 1);
            corr_stack = 1;
        }
        Aex = ADDR(addr + M[reg]);
        CPU_TRY(mmu_load(Aex, &op));
        CPU_TRY(besm6_add(op, 0, 0));
        RAU   = SET_ADDITIVE(RAU);
        delay = MEAN_TIME(3, 11);
        break;
    case 005: /* вч, a-x */
        if (!addr && reg == 017) {
            M[017]     = ADDR(M[017] - 1);
            corr_stack = 1;
        }
        Aex = ADDR(addr + M[reg]);
        CPU_TRY(mmu_load(Aex, &op));
        CPU_TRY(besm6_add(op, 0, 1));
        RAU   = SET_ADDITIVE(RAU);
        delay = MEAN_TIME(3, 11);
        break;
    case 006: /* вчоб, x-a */
        if (!addr && reg == 017) {
            M[017]     = ADDR(M[017] - 1);
            corr_stack = 1;
        }
        Aex = ADDR(addr + M[reg]);
        CPU_TRY(mmu_load(Aex, &op));
        CPU_TRY(besm6_add(op, 1, 0));
        RAU   = SET_ADDITIVE(RAU);
        delay = MEAN_TIME(3, 11);
        break;
    case 007: /* вчаб, amx */
        if (!addr && reg == 017) {
            M[017]     = ADDR(M[017] - 1);
            corr_stack = 1;
        }
        Aex = ADDR(addr + M[reg]);
        CPU_TRY(mmu_load(Aex, &op));
        CPU_TRY(besm6_add(op, 1, 1));
        RAU   = SET_ADDITIVE(RAU);
        delay = MEAN_TIME(3, 11);
        break;
    case 010: /* сч, xta */
        if (!addr && reg == 017) {
            M[017]     = ADDR(M[017] - 1);
            corr_stack = 1;
        }
        Aex = ADDR(addr + M[reg]);
        CPU_TRY(mmu_load(Aex, &ACC));
        RAU   = SET_LOGICAL(RAU);
        delay = MEAN_TIME(3, 3);
        break;
    case 011: /* и, aax */
        if (!addr && reg == 017) {
            M[017]     = ADDR(M[017] - 1);
            corr_stack = 1;
        }
        Aex = ADDR(addr + M[reg]);
        CPU_TRY(mmu_load(Aex, &op));
        ACC &= op;
        RMR   = 0;
        RAU   = SET_LOGICAL(RAU);
        delay = MEAN_TIME(3, 4);
        break;
    case 012: /* нтж, aex */
        if (!addr && reg == 017) {
            M[017]     = ADDR(M[017] - 1);
            corr_stack = 1;
        }
        Aex = ADDR(addr + M[reg]);
        RMR = ACC;
        CPU_TRY(mmu_load(Aex, &op));
        ACC ^= op;
        RAU   = SET_LOGICAL(RAU);
        delay = MEAN_TIME(3, 3);
        break;
    case 013: /* слц, arx */
        if (!addr && reg == 017) {
            M[017]     = ADDR(M[017] - 1);
            corr_stack = 1;
        }
        Aex = ADDR(addr + M[reg]);
        CPU_TRY(mmu_load(Aex, &op));
        ACC += op;
        if (ACC & BIT49)
            ACC = (ACC + 1) & BITS48;
        RMR   = 0;
        RAU   = SET_MULTIPLICATIVE(RAU);
        delay = MEAN_TIME(3, 6);
        break;
    case 014: /* знак, avx */
        if (!addr && reg == 017) {
            M[017]     = ADDR(M[017] - 1);
            corr_stack = 1;
        }
        Aex = ADDR(addr + M[reg]);
        CPU_TRY(mmu_load(Aex, &op));
        CPU_TRY(besm6_change_sign(op >> 40 & 1));
        RAU   = SET_ADDITIVE(RAU);
        delay = MEAN_TIME(3, 5);
        break;
    case 015: /* или, aox */
        if (!addr && reg == 017) {
            M[017]     = ADDR(M[017] - 1);
            corr_stack = 1;
        }
        Aex = ADDR(addr + M[reg]);
        CPU_TRY(mmu_load(Aex, &op));
        ACC |= op;
        RMR   = 0;
        RAU   = SET_LOGICAL(RAU);
        delay = MEAN_TIME(3, 4);
        break;
    case 016: /* дел, a/x */
        if (!addr && reg == 017) {
            M[017]     = ADDR(M[017] - 1);
            corr_stack = 1;
        }
        Aex = ADDR(addr + M[reg]);
        CPU_TRY(mmu_load(Aex, &op));
        CPU_TRY(besm6_divide(op));
        RAU   = SET_MULTIPLICATIVE(RAU);
        delay = MEAN_TIME(3, 50);
        break;
    case 017: /* умн, a*x */
        if (!addr && reg == 017) {
            M[017]     = ADDR(M[017] - 1);
            corr_stack = 1;
        }
        Aex = ADDR(addr + M[reg]);
        CPU_TRY(mmu_load(Aex, &op));
        CPU_TRY(besm6_multiply(op));
        RAU   = SET_MULTIPLICATIVE(RAU);
        delay = MEAN_TIME(3, 18);
        break;
    case 020: /* сбр, apx */
        if (!addr && reg == 017) {
            M[017]     = ADDR(M[017] - 1);
            corr_stack = 1;
        }
        Aex = ADDR(addr + M[reg]);
        CPU_TRY(mmu_load(Aex, &op));
        ACC   = besm6_pack(ACC, op);
        RMR   = 0;
        RAU   = SET_LOGICAL(RAU);
        delay = MEAN_TIME(3, 53);
        break;
    case 021: /* рзб, aux */
        if (!addr && reg == 017) {
            M[017]     = ADDR(M[017] - 1);
            corr_stack = 1;
        }
        Aex = ADDR(addr + M[reg]);
        CPU_TRY(mmu_load(Aex, &op));
        ACC   = besm6_unpack(ACC, op);
        RMR   = 0;
        RAU   = SET_LOGICAL(RAU);
        delay = MEAN_TIME(3, 53);
        break;
    case 022: /* чед, acx */
        if (!addr && reg == 017) {
            M[017]     = ADDR(M[017] - 1);
            corr_stack = 1;
        }
        Aex = ADDR(addr + M[reg]);
        CPU_TRY(mmu_load(Aex, &op));
        ACC = besm6_count_ones(ACC) + op;
        if (ACC & BIT49)
            ACC = (ACC + 1) & BITS48;
        RAU   = SET_LOGICAL(RAU);
        delay = MEAN_TIME(3, 56);
        break;
    case 023: /* нед, anx */
        if (!addr && reg == 017) {
            M[017]     = ADDR(M[017] - 1);
            corr_stack = 1;
        }
        Aex = ADDR(addr + M[reg]);
        if (ACC) {
            int n = besm6_highest_bit(ACC);

            /* The "remainder" of the accumulator, excluding the bit
             * whose number was found, goes into the РМР,
             * starting from the top bit of the РМР. */
            besm6_shift(48 - n);

            /* Cyclic addition of the number to the word at Аисп. */
            CPU_TRY(mmu_load(Aex, &op));
            ACC = n + op;
            if (ACC & BIT49)
                ACC = (ACC + 1) & BITS48;
        } else {
            RMR = 0;
            CPU_TRY(mmu_load(Aex, &ACC));
        }
        RAU   = SET_LOGICAL(RAU);
        delay = MEAN_TIME(3, 32);
        break;
    case 024: /* слп, e+x */
        if (!addr && reg == 017) {
            M[017]     = ADDR(M[017] - 1);
            corr_stack = 1;
        }
        Aex = ADDR(addr + M[reg]);
        CPU_TRY(mmu_load(Aex, &op));
        CPU_TRY(besm6_add_exponent((op >> 41) - 64));
        RAU   = SET_MULTIPLICATIVE(RAU);
        delay = MEAN_TIME(3, 5);
        break;
    case 025: /* вчп, e-x */
        if (!addr && reg == 017) {
            M[017]     = ADDR(M[017] - 1);
            corr_stack = 1;
        }
        Aex = ADDR(addr + M[reg]);
        CPU_TRY(mmu_load(Aex, &op));
        CPU_TRY(besm6_add_exponent(64 - (op >> 41)));
        RAU   = SET_MULTIPLICATIVE(RAU);
        delay = MEAN_TIME(3, 5);
        break;
    case 026: { /* сд, asx */
        int n;
        if (!addr && reg == 017) {
            M[017]     = ADDR(M[017] - 1);
            corr_stack = 1;
        }
        Aex = ADDR(addr + M[reg]);
        CPU_TRY(mmu_load(Aex, &op));
        n = (op >> 41) - 64;
        besm6_shift(n);
        RAU   = SET_LOGICAL(RAU);
        delay = MEAN_TIME(3, 4 + abs(n));
        break;
    }
    case 027: /* рж, xtr */
        if (!addr && reg == 017) {
            M[017]     = ADDR(M[017] - 1);
            corr_stack = 1;
        }
        Aex = ADDR(addr + M[reg]);
        CPU_TRY(mmu_load(Aex, &op));
        RAU   = (op >> 41) & 077;
        delay = MEAN_TIME(3, 3);
        break;
    case 030: /* счрж, rte */
        Aex   = ADDR(addr + M[reg]);
        ACC   = (t_value)(RAU & Aex & 0177) << 41;
        RAU   = SET_LOGICAL(RAU);
        delay = MEAN_TIME(3, 3);
        break;
    case 031: /* счмр, yta */
        Aex = ADDR(addr + M[reg]);
        if (IS_LOGICAL(RAU)) {
            ACC = RMR;
        } else {
            t_value x = RMR;
            ACC       = (ACC & ~BITS41) | (RMR & BITS40);
            besm6_add_exponent((Aex & 0177) - 64);
            RMR = x;
        }
        delay = MEAN_TIME(3, 5);
        break;
    case 032: /* э32, ext */
        if (RK & BBIT(19))
            write_032(ADDR(addr - 070000 + M[reg]), ACC);
        else {
            t_value res;
            res = read_032(ADDR(addr + M[reg]));
            ACC = (res << 24) | (ACC & 077777777);
        }
        break;
    case 033: /* увв, ext */
        Aex = ADDR(addr + M[reg]);
        if (!IS_SUPERVISOR(RUU))
            return STOP_BADCMD;
        CPU_TRY(cmd_033());
        /* The АУ mode is logical when the operation was a read */
        if (Aex & 04000)
            RAU = SET_LOGICAL(RAU);
        delay = MEAN_TIME(3, 8);
        break;
    case 034: /* слпа, e+n */
        Aex = ADDR(addr + M[reg]);
        besm6_add_exponent((Aex & 0177) - 64);
        RAU   = SET_MULTIPLICATIVE(RAU);
        delay = MEAN_TIME(3, 5);
        break;
    case 035: /* вчпа, e-n */
        Aex = ADDR(addr + M[reg]);
        besm6_add_exponent(64 - (Aex & 0177));
        RAU   = SET_MULTIPLICATIVE(RAU);
        delay = MEAN_TIME(3, 5);
        break;
    case 036: { /* сда, asn */
        int n;
        Aex = ADDR(addr + M[reg]);
        n   = (Aex & 0177) - 64;
        besm6_shift(n);
        RAU   = SET_LOGICAL(RAU);
        delay = MEAN_TIME(3, 4 + abs(n));
        break;
    }
    case 037: /* ржа, ntr */
        Aex   = ADDR(addr + M[reg]);
        RAU   = Aex & 077;
        delay = MEAN_TIME(3, 3);
        break;
    case 040: /* уи, ati */
        Aex = ADDR(addr + M[reg]);
        if (IS_SUPERVISOR(RUU)) {
            int reg = Aex & 037;
            M[reg]  = ADDR(ACC);
            /* breakpoint/watchpoint regs will match physical
             * or virtual addresses depending on the current
             * mapping mode.
             */
            if ((M[PSW] & PSW_MMAP_DISABLE) && (reg == IBP || reg == DWP))
                M[reg] |= BBIT(16);

        } else
            M[Aex & 017] = ADDR(ACC);
        M[0]  = 0;
        delay = MEAN_TIME(14, 3);
        break;
    case 041: { /* уим, sti */
        unsigned rg, ad;

        Aex = ADDR(addr + M[reg]);
        rg  = Aex & (IS_SUPERVISOR(RUU) ? 037 : 017);
        ad  = ADDR(ACC);
        if (rg != 017) {
            M[017]     = ADDR(M[017] - 1);
            corr_stack = 1;
        }
        CPU_TRY(mmu_load(rg != 017 ? M[017] : ad, &ACC));
        M[rg] = ad;
        if ((M[PSW] & PSW_MMAP_DISABLE) && (rg == IBP || rg == DWP))
            M[rg] |= BBIT(16);
        M[0]  = 0;
        RAU   = SET_LOGICAL(RAU);
        delay = MEAN_TIME(14, 3);
        break;
    }
    case 042: /* счи, ita */
        delay = MEAN_TIME(6, 3);
    load_modifier:
        Aex = ADDR(addr + M[reg]);
        ACC = ADDR(M[Aex & (IS_SUPERVISOR(RUU) ? 037 : 017)]);
        RAU = SET_LOGICAL(RAU);
        break;
    case 043: /* счим, its */
        CPU_TRY(mmu_store(M[017], ACC));
        M[017] = ADDR(M[017] + 1);
        delay  = MEAN_TIME(9, 6);
        goto load_modifier;
    case 044: /* уии, mtj */
        Aex = addr;
        if (IS_SUPERVISOR(RUU)) {
        transfer_modifier:
            M[Aex & 037] = M[reg];
            if ((M[PSW] & PSW_MMAP_DISABLE) && ((Aex & 037) == IBP || (Aex & 037) == DWP))
                M[Aex & 037] |= BBIT(16);

        } else
            M[Aex & 017] = M[reg];
        M[0]  = 0;
        delay = 6;
        break;
    case 045: /* сли, j+m */
        Aex = addr;
        if ((Aex & 020) && IS_SUPERVISOR(RUU))
            goto transfer_modifier;
        M[Aex & 017] = ADDR(M[Aex & 017] + M[reg]);
        M[0]         = 0;
        delay        = 6;
        break;
    case 046: /* э46, x46 */
        Aex = addr;
        if (!IS_SUPERVISOR(RUU))
            return STOP_BADCMD;
        M[Aex & 017] = ADDR(Aex);
        M[0]         = 0;
        delay        = 6;
        break;
    case 047: /* э47, x47 */
        Aex = addr;
        if (!IS_SUPERVISOR(RUU))
            return STOP_BADCMD;
        M[Aex & 017] = ADDR(M[Aex & 017] + Aex);
        M[0]         = 0;
        delay        = 6;
        break;
    case 050:
    case 051:
    case 052:
    case 053:
    case 054:
    case 055:
    case 056:
    case 057:
    case 060:
    case 061:
    case 062:
    case 063:
    case 064:
    case 065:
    case 066:
    case 067:
    case 070:
    case 071:
    case 072:
    case 073:
    case 074:
    case 075:
    case 076:
    case 077:  /* э50...э77 */
    case 0200: /* э20 */
    case 0210: /* э21 */
    stop_as_extracode:
        Aex = ADDR(addr + M[reg]);
        /*besm6_okno ("экстракод");*/
        /* The return address from the extracode. */
        M[ERET] = nextpc;
        /* The saved УУ modes. */
        M[SPSW] = (M[PSW] & (PSW_INTR_DISABLE | PSW_MMAP_DISABLE | PSW_PROT_DISABLE)) |
                  IS_SUPERVISOR(RUU);
        /* The current УУ modes. */
        M[PSW] = PSW_INTR_DISABLE | PSW_MMAP_DISABLE | PSW_PROT_DISABLE | /*?*/ PSW_INTR_HALT;
        M[14]  = Aex;
        RUU    = SET_SUPERVISOR(RUU, SPSW_EXTRACODE);

        if (opcode <= 077)
            PC = 0500 + opcode; /* э50-э77 */
        else
            PC = 0540 + (opcode >> 3); /* э20, э21 */
        RUU &= ~RUU_RIGHT_INSTR;
        delay = 7;
        break;
    case 0220: /* мода, utc */
        Aex      = ADDR(addr + M[reg]);
        next_mod = Aex;
        delay    = 4;
        break;
    case 0230: /* мод, wtc */
        if (!addr && reg == 017) {
            M[017]     = ADDR(M[017] - 1);
            corr_stack = 1;
        }
        Aex = ADDR(addr + M[reg]);
        CPU_TRY(mmu_load(Aex, &op));
        next_mod = ADDR(op);
        delay    = MEAN_TIME(13, 3);
        break;
    case 0240: /* уиа, vtm */
        Aex    = addr;
        M[reg] = addr;
        M[0]   = 0;
        if (IS_SUPERVISOR(RUU) && reg == 0) {
            M[PSW] &= ~(PSW_INTR_DISABLE | PSW_MMAP_DISABLE | PSW_PROT_DISABLE);
            M[PSW] |= addr & (PSW_INTR_DISABLE | PSW_MMAP_DISABLE | PSW_PROT_DISABLE);
        }
        delay = 4;
        break;
    case 0250: /* слиа, utm */
        Aex    = ADDR(addr + M[reg]);
        M[reg] = Aex;
        M[0]   = 0;
        if (IS_SUPERVISOR(RUU) && reg == 0) {
            M[PSW] &= ~(PSW_INTR_DISABLE | PSW_MMAP_DISABLE | PSW_PROT_DISABLE);
            M[PSW] |= addr & (PSW_INTR_DISABLE | PSW_MMAP_DISABLE | PSW_PROT_DISABLE);
        }
        delay = 4;
        break;
    case 0260: /* по, uza */
        Aex   = ADDR(addr + M[reg]);
        RMR   = ACC;
        delay = MEAN_TIME(12, 3);
        if (IS_ADDITIVE(RAU)) {
            if (ACC & BIT41)
                break;
        } else if (IS_MULTIPLICATIVE(RAU)) {
            if (!(ACC & BIT48))
                break;
        } else if (IS_LOGICAL(RAU)) {
            if (ACC)
                break;
        } else
            break;
        if (Aex == 05176)
            besm6_log("_uza phyzobm");
        PC = Aex;
        RUU &= ~RUU_RIGHT_INSTR;
        delay += 3;
        break;
    case 0270: /* пе, u1a */
        Aex   = ADDR(addr + M[reg]);
        RMR   = ACC;
        delay = MEAN_TIME(12, 3);
        if (IS_ADDITIVE(RAU)) {
            if (!(ACC & BIT41))
                break;
        } else if (IS_MULTIPLICATIVE(RAU)) {
            if (ACC & BIT48)
                break;
        } else if (IS_LOGICAL(RAU)) {
            if (!ACC)
                break;
        } else
            /* fall thru, i.e. branch */;
        if (Aex == 05176)
            besm6_log("_u1a phyzobm");
        PC = Aex;
        RUU &= ~RUU_RIGHT_INSTR;
        delay += 3;
        break;
    case 0300: /* пб, uj */
        Aex = ADDR(addr + M[reg]);
        if (Aex == 05176) {
            int zone = (ACC >> 24) & 01777;
            if (zone) {
                int pc = RUU & RUU_RIGHT_INSTR ? PC : PC - 1;
                besm6_log("_physobm @%05o, zone = %04o, WORD = %016llo", pc, zone - 4, memory[pc]);
            }
        }
        PC = Aex;
        RUU &= ~RUU_RIGHT_INSTR;
        /* uj through a saved link register (reg, 0) is a subroutine return. */
        if (reg != 0 && addr == 0 && sim_deb && cpu_dev.dctrl)
            besm6_trace_call_return();
        delay = 7;
        break;
    case 0310: /* пв, vjm */
        Aex    = addr;
        M[reg] = nextpc;
        M[0]   = 0;
        if (Aex == 05176) {
            int zone = (ACC >> 24) & 01777;
            if (zone) {
                int pc = RUU & RUU_RIGHT_INSTR ? PC : PC - 1;
                besm6_log("_physobm @%05o, zone = %04o, WORD = %016llo", pc, zone - 4, memory[pc]);
            }
        }
        PC = addr;
        RUU &= ~RUU_RIGHT_INSTR;
        /* пв/vjm is a subroutine call: name the function at the target. */
        if (sim_deb && cpu_dev.dctrl)
            besm6_trace_call_return();
        delay = 7;
        break;
    case 0320: /* выпр, iret */
        Aex = addr;
        if (!IS_SUPERVISOR(RUU)) {
            return STOP_BADCMD;
        }
        M[PSW] = (M[PSW] & PSW_WRITE_WATCH) |
                 (M[SPSW] & (SPSW_INTR_DISABLE | SPSW_MMAP_DISABLE | SPSW_PROT_DISABLE));
        PC     = M[(reg & 3) | 030];
        RUU &= ~RUU_RIGHT_INSTR;
        if (M[SPSW] & SPSW_RIGHT_INSTR)
            RUU |= RUU_RIGHT_INSTR;
        else
            RUU &= ~RUU_RIGHT_INSTR;
        RUU = SET_SUPERVISOR(RUU, M[SPSW] & (SPSW_EXTRACODE | SPSW_INTERRUPT));
        if (M[SPSW] & SPSW_MOD_RK)
            next_mod = M[MOD];
        /*besm6_okno ("Выход из прерывания");*/
        delay = 7;
        break;
    case 0330: /* стоп, stop */
        Aex   = ADDR(addr + M[reg]);
        delay = 7;
        if (!IS_SUPERVISOR(RUU)) {
            if (M[PSW] & PSW_CHECK_HALT)
                break;
            else {
                opcode = 063;
                goto stop_as_extracode;
            }
        }
        mmu_print_brz();
        return STOP_STOP;
        break;
    case 0340: /* пио, vzm */
    branch_zero:
        Aex   = addr;
        delay = 4;
        if (!M[reg]) {
            if (addr == 05176)
                besm6_log("_vzm phyzobm");
            PC = addr;
            RUU &= ~RUU_RIGHT_INSTR;
            delay += 3;
        }
        break;
    case 0350: /* пино, v1m */
        Aex   = addr;
        delay = 4;
        if (M[reg]) {
            if (addr == 05176)
                besm6_log("_v1m phyzobm");
            PC = addr;
            RUU &= ~RUU_RIGHT_INSTR;
            delay += 3;
        }
        break;
    case 0360: /* э36, *36 */
        goto branch_zero;
    case 0370: /* цикл, vlm */
        Aex   = addr;
        delay = 4;
        if (!M[reg])
            break;
        M[reg] = ADDR(M[reg] + 1);
        PC     = addr;
        RUU &= ~RUU_RIGHT_INSTR;
        delay += 3;
        break;
    default:
        /* Unknown instruction - cannot happen. */
        return STOP_STOP;
        break;
    }
    if (next_mod) {
        /* Modify the address of the next instruction. */
        M[MOD] = next_mod;
        RUU |= RUU_MOD_RK;
    } else
        RUU &= ~RUU_MOD_RK;

    /* Are we in DISPAK's "ЖДУ" (wait) loop? */
    if (RUU == 047 && PC == 04440 && RK == 067704440) {
        if (autotime)
            check_initial_setup();
        sim_interval -= 1;
    }
    return SCPE_OK;
}

/*
 * Interrupt operation 1: an internal interrupt.
 * Described in volume 9 of the BESM-6 technical description, page 119.
 */
void op_int_1(const char *msg)
{
    /*besm6_okno (msg);*/
    M[SPSW] =
        (M[PSW] & (PSW_INTR_DISABLE | PSW_MMAP_DISABLE | PSW_PROT_DISABLE)) | IS_SUPERVISOR(RUU);
    if (RUU & RUU_RIGHT_INSTR)
        M[SPSW] |= SPSW_RIGHT_INSTR;
    M[IRET] = PC;
    M[PSW] |= PSW_INTR_DISABLE | PSW_MMAP_DISABLE | PSW_PROT_DISABLE;
    if (RUU & RUU_MOD_RK) {
        M[SPSW] |= SPSW_MOD_RK;
        RUU &= ~RUU_MOD_RK;
    }
    PC = 0500;
    RUU &= ~RUU_RIGHT_INSTR;
    RUU = SET_SUPERVISOR(RUU, SPSW_INTERRUPT);
}

/*
 * Interrupt operation 2: an external interrupt.
 * Described in volume 9 of the BESM-6 technical description, page 129.
 */
void op_int_2()
{
    /*besm6_okno ("Внешнее прерывание");*/
    if (sim_deb && cpu_dev.dctrl)
        besm6_trace_exception("external interrupt");
    M[SPSW] =
        (M[PSW] & (PSW_INTR_DISABLE | PSW_MMAP_DISABLE | PSW_PROT_DISABLE)) | IS_SUPERVISOR(RUU);
    M[IRET] = PC;
    M[PSW] |= PSW_INTR_DISABLE | PSW_MMAP_DISABLE | PSW_PROT_DISABLE;
    if (RUU & RUU_MOD_RK) {
        M[SPSW] |= SPSW_MOD_RK;
        RUU &= ~RUU_MOD_RK;
    }
    PC = 0501;
    RUU &= ~RUU_RIGHT_INSTR;
    RUU = SET_SUPERVISOR(RUU, SPSW_INTERRUPT);
}

/*
 * What setjmp(cpu_halt)'s landing pad was.  A trap reported by cpu_one_inst()
 * arrives here: most codes become a guest interrupt and execution continues,
 * which is what returning zero says; anything else halts the machine and is
 * returned as the stop code.
 *
 * ПоП and ПоК halt the machine on any internal interrupt and on any
 * parity-check interrupt respectively.  After such a halt, execution resumes
 * at the instruction following the one that caused the interrupt -- as though
 * the "ТП" (transfer type) button had been pressed.  See page 119 of ТО9.
 */
static t_stat cpu_trap(t_stat r, int *iintr)
{
    M[017] += corr_stack;
    if (sim_deb && cpu_dev.dctrl) {
        const char *message = (r >= SCPE_BASE) ? sim_error_text(r) : sim_stop_messages[r];
        besm6_trace_exception(message);
    }
    switch (r) {
    default:
        return r;
    case STOP_BADCMD:
        if (M[PSW] & PSW_INTR_HALT) /* ПоП */
            return r;
        op_int_1(sim_stop_messages[r]);
        // SPSW_NEXT_RK is not important for this interrupt
        GRP |= GRP_ILL_INSN;
        break;
    case STOP_INSN_CHECK:
        if (M[PSW] & PSW_CHECK_HALT) /* ПоК */
            return r;
        op_int_1(sim_stop_messages[r]);
        // SPSW_NEXT_RK must be 0 for this interrupt; it is already
        GRP |= GRP_INSN_CHECK;
        break;
    case STOP_INSN_PROT:
        if (M[PSW] & PSW_INTR_HALT) /* ПоП */
            return r;
        if (RUU & RUU_RIGHT_INSTR) {
            ++PC;
        }
        RUU ^= RUU_RIGHT_INSTR;
        op_int_1(sim_stop_messages[r]);
        // SPSW_NEXT_RK must be 1 for this interrupt
        M[SPSW] |= SPSW_NEXT_RK;
        GRP |= GRP_INSN_PROT;
        break;
    case STOP_OPERAND_PROT:
#if 0
    /* DISPAK keeps the ПоП flag set.
     * Starting СЕРП reaches into someone else's page. */
        if (M[PSW] & PSW_INTR_HALT)             /* ПоП */
            return r;
#endif
        if (RUU & RUU_RIGHT_INSTR) {
            ++PC;
        }
        RUU ^= RUU_RIGHT_INSTR;
        op_int_1(sim_stop_messages[r]);
        M[SPSW] |= SPSW_NEXT_RK;
        // The offending virtual page is in bits 5-9
        GRP |= GRP_OPRND_PROT;
        GRP = GRP_SET_PAGE(GRP, iintr_data);
        break;
    case STOP_RAM_CHECK:
        if (M[PSW] & PSW_CHECK_HALT) /* ПоК */
            return r;
        op_int_1(sim_stop_messages[r]);
        // The offending interleaved block # is in bits 1-3.
        GRP |= GRP_CHECK | GRP_RAM_CHECK;
        GRP = GRP_SET_BLOCK(GRP, iintr_data);
        break;
    case STOP_CACHE_CHECK:
        if (M[PSW] & PSW_CHECK_HALT) /* ПоК */
            return r;
        op_int_1(sim_stop_messages[r]);
        // The offending BRZ # is in bits 1-3.
        GRP |= GRP_CHECK;
        GRP &= ~GRP_RAM_CHECK;
        GRP = GRP_SET_BLOCK(GRP, iintr_data);
        break;
    case STOP_INSN_ADDR_MATCH:
        if (M[PSW] & PSW_INTR_HALT) /* ПоП */
            return r;
        if (RUU & RUU_RIGHT_INSTR) {
            ++PC;
        }
        RUU ^= RUU_RIGHT_INSTR;
        op_int_1(sim_stop_messages[r]);
        M[SPSW] |= SPSW_NEXT_RK;
        GRP |= GRP_BREAKPOINT;
        break;
    case STOP_LOAD_ADDR_MATCH:
        if (M[PSW] & PSW_INTR_HALT) /* ПоП */
            return r;
        if (RUU & RUU_RIGHT_INSTR) {
            ++PC;
        }
        RUU ^= RUU_RIGHT_INSTR;
        op_int_1(sim_stop_messages[r]);
        M[SPSW] |= SPSW_NEXT_RK;
        GRP |= GRP_WATCHPT_R;
        break;
    case STOP_STORE_ADDR_MATCH:
        if (M[PSW] & PSW_INTR_HALT) /* ПоП */
            return r;
        if (RUU & RUU_RIGHT_INSTR) {
            ++PC;
        }
        RUU ^= RUU_RIGHT_INSTR;
        op_int_1(sim_stop_messages[r]);
        M[SPSW] |= SPSW_NEXT_RK;
        GRP |= GRP_WATCHPT_W;
        break;
    case STOP_OVFL:
        /* An АУ interrupt halts the machine when БРО=0
         * and either ПоП or ПоК is set.
         * Page 118 of ТО9. */
        if (!(RUU & RUU_AVOST_DISABLE) && /* ! БРО */
            ((M[PSW] & PSW_INTR_HALT) ||  /* ПоП */
             (M[PSW] & PSW_CHECK_HALT)))  /* ПоК */
            return r;
        op_int_1(sim_stop_messages[r]);
        GRP |= GRP_OVERFLOW | GRP_RAM_CHECK;
        break;
    case STOP_DIVZERO:
        if (!(RUU & RUU_AVOST_DISABLE) && /* ! БРО */
            ((M[PSW] & PSW_INTR_HALT) ||  /* ПоП */
             (M[PSW] & PSW_CHECK_HALT)))  /* ПоК */
            return r;
        op_int_1(sim_stop_messages[r]);
        GRP |= GRP_DIVZERO | GRP_RAM_CHECK;
        break;
    }
    ++*iintr;
    return SCPE_OK;
}

/*
 * Main instruction fetch/decode loop
 */
/*
 * Run instructions until there is something for the driver to do, and say
 * which: a transfer to perform, the burst being up, or a stop code.  Upstream
 * ran to a stop and did everything itself; nothing below here may block
 * (machine.h).
 */
t_stat cpu_burst(void)
{
    static int iintr;
    static int started;
    static t_stat deferred_trap;
    int32 left = BURST_INSTRUCTIONS;
    t_stat r;

    if (!started) {
        started = 1;
        PC      = PC & BITS(15); /* mask PC */
        mmu_setup();             /* copy RP to TLB */
    }

    /* A trap taken by an instruction that also posted a transfer, held until
     * the driver had performed it -- which is the order upstream had, doing
     * the transfer inside the instruction.  No instruction does both: the one
     * that starts an exchange has nothing left to fault on. */
    if (deferred_trap) {
        r             = deferred_trap;
        deferred_trap = 0;
        goto trapped;
    }

    for (;;) {
        if (sim_interval <= 0) { /* check clock queue */
            r = sim_process_event();
            if (r) {
                return r;
            }
        }

        if (PC > BITS(15) && IS_SUPERVISOR(RUU)) {
            /*
             * Runaway instruction execution in supervisor mode
             * warrants attention.
             */
            return STOP_RUNOUT; /* stop simulation */
        }

        if (PRP & MPRP) {
            /* There are interrupts pending in the peripheral
             * interrupt register */
            GRP |= GRP_SLAVE;
        }

        if (!iintr && !(RUU & RUU_RIGHT_INSTR) && !(M[PSW] & PSW_INTR_DISABLE) && (GRP & MGRP)) {
            /* external interrupt */
            op_int_2();
        }
        r = cpu_one_inst(); /* one instr */
        if (io_request.unit) {
            deferred_trap = r;
            return REASON_IO;
        }
        if (r) {
        trapped:
            /* The instruction trapped.  Either it becomes a guest interrupt
             * and the loop goes on, or the machine stops. */
            r = cpu_trap(r, &iintr);
            if (r)
                return r;
            if (iintr > 1)
                return STOP_DOUBLE_INTR;
            continue;
        }
        if (sim_deb && cpu_dev.dctrl)
            besm6_trace_registers(); /* show changed registers */
        iintr = 0;

        sim_interval -= 1; /* count down instructions */
        if (--left <= 0)
            return REASON_YIELD;
    }
}

/*
 * A 250 Hz clock as per the original documentation,
 * and matching the available software binaries.
 * Some installations used 50 Hz with a modified OS
 * for a better user time/system time ratio.
 */
t_stat fast_clk(UNIT *self)
{
    static unsigned counter;
    static unsigned tty_counter;

    ++counter;
    ++tty_counter;

    GRP |= GRP_TIMER;

    if ((counter & 3) == 0) {
        /*
         * The OS used the (undocumented, later addition)
         * slow clock interrupt to initiate servicing
         * terminal I/O. Its frequency was reportedly about 50-60 Hz;
         * 16 ms is a good enough approximation.
         */
        GRP |= GRP_SLOW_CLK;
    }

    /* Baudot TTYs are synchronised to the main timer rather than the
     * serial line clock. Their baud rate is 50.
     */
    if (tty_counter == CLK_TPS / 50) {
        tt_print();
        tt_receive();
        tty_counter = 0;
    }

    tmr_poll = sim_rtcn_calb(CLK_TPS);                  /* calibrate clock */
    return sim_activate_after(self, 1000000 / CLK_TPS); /* reactivate unit */
}

UNIT clocks[] = {
    { .action = fast_clk, .wait = CLK_DELAY }, /* Bit 40 of the GRP, 250 Hz */
};

t_stat clk_reset(DEVICE *dev)
{
    sim_register_clock_unit(&clocks[0]);

    /* The auto-start circuit is triggered by the unimplemented "МР" button */

    /* Upstream guarded this with !sim_is_running, to tell RESET from IORESET;
     * reset_all() is called once, at startup, and there is no IORESET. */
    tmr_poll = sim_rtcn_init(clocks[0].wait); /* init timer */
    sim_activate(&clocks[0], tmr_poll);       /* activate unit */
    return SCPE_OK;
}

DEVICE clock_dev = { .name = "CLK", .units = clocks, .numunits = 1, .reset = &clk_reset };
