// LIST -- the exact inverse of CRUNCH.
//
// m6502.asm:1973-2062; 03-tokenizer-editor.md §4.
#include "mbasic.h"

// Argument parsing (m6502.asm:1975-1990). LINGET reads the start bound; if a
// '-' follows, LINGET runs again and overwrites it with the end bound, and at
// LSTEND a zero end becomes 65535. With no digits at all LINGET returns zero,
// which is what makes the bare forms work:
//
//   LIST        0 .. 65535     everything
//   LIST 100    100 .. 100     one line
//   LIST 100-   100 .. 65535
//   LIST -200   0 .. 200
void Interp::list()
{
    linget();
    CHK;
    u16 from = linnum;
    u16 to   = linnum;

    if (chrgot() == MINUTK) {
        chrget();
        linget();
        CHK;
        to = linnum;
    }
    if (to == 0)
        to = 65535;

    for (usize i = 0; i < prog.size(); i++) {
        const Line &l = prog[i];
        if (l.num < from)
            continue;
        if (l.num > to)
            break;

        // ISCNTC once per line, so a long listing can be interrupted.
        iscntc();
        CHK;

        linprt(l.num);
        outdo(' ');

        for (usize k = 0; k + 1 < l.text.size(); k++) {
            u8 c = l.text[k];
            if (c < 0x80) {
                outdo(c);
                continue;
            }
            // Upstream expanded a token by rescanning RESLST from the
            // beginning, skipping ordinal-1 whole entries by looking for bytes
            // with bit 7 set, and stripping that marker off the last character
            // it printed. An indexed table needs neither.
            usize w = usize(c - ENDTK);
            outstr(w < RESLST_COUNT ? RESLST[w] : Str("?"));
        }
        crdo();
        CHK;
    }

    // LIST discards NEWSTT's return address and exits via JMP READY, so it
    // always goes back to the prompt rather than continuing a program.
    halt_ = Halt::Ready;
}

void Interp::stmt_list()
{
    list();
}
