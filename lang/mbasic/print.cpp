// PRINT, the column machinery, and the output sink.
//
// m6502.asm's PRINT CODE section; 05-statements.md §2 and 11-io-init.md §1.
//
// Output is never a suspension here. Upstream's OUTDO called the machine's
// monitor; this one appends to a buffer the driver drains at every stop, which
// is what keeps PRINT, LIST, STROUT, FOUT and the whole error printer plain
// synchronous code. The buffer cannot grow without bound because the statement
// burst drains it every 512 statements.
#include "kernel/fmt.h"
#include "mbasic.h"

// ============================================================== OUTDO
//
// m6502.asm:2818-2848, the single character-output choke point. Four
// responsibilities: output suppression, TRMPOS maintenance, automatic wrap at
// LINWID, and the per-target call.
void Interp::outdo(u8 c)
{
    if (pending()) // poisoned (err.h): drop it
        return;
    if (cntwfl & 0x80) // the user typed ^O
        return;

    if (chanl) { // EXTIO: a non-terminal channel keeps no column
        Chan *ch = chan_find(chanl);
        if (ch)
            reason(ch->obuf.push(char(c)));
        return;
    }

    if (c >= ' ') {
        // Automatic line wrap, BEFORE the character. Upstream compared against
        // LINWID, a RAM byte, not a constant: it is settable at startup.
        if (linwid && trmpos == linwid)
            crdo();
        trmpos++;
    }
    reason(out.push(char(c)));
}

// STROUT (m6502.asm:2785-2790) took a NUL-terminated pointer, ran it through
// STRLIT to build a descriptor, and fell into STRPRT -- so every message the
// interpreter prints goes through the same CR handling as a BASIC string.
void Interp::outstr(Str s)
{
    strprt(s);
}

// CRDO (m6502.asm:2711-2746). Upstream set TRMPOS = 13 before emitting
// anything, so that the wrap check in OUTDO could not fire while the newline
// itself was being written; setting it to zero afterwards is the same thing
// said plainly. CRFIN's null padding is NULCMD only, which this build is not.
void Interp::crdo()
{
    if (pending())
        return;
    if (chanl) {
        Chan *ch = chan_find(chanl);
        if (ch) {
            reason(ch->obuf.push('\r'));
            reason(ch->obuf.push('\n'));
        }
        return;
    }
    reason(out.push('\r'));
    reason(out.push('\n'));
    crfin();
}

void Interp::crfin()
{
    trmpos = 0;
}

// STRPRT (m6502.asm:2785-2801). A character equal to 13 inside the string
// triggers CRFIN, so embedded carriage returns reset TRMPOS.
void Interp::strprt(Str s)
{
    for (usize i = 0; i < s.size(); i++) {
        u8 c = u8(s[i]);
        outdo(c);
        if (c == '\r')
            crfin();
    }
}

// LINPRT: a line number, or the free-byte count, printed unsigned. Upstream
// preset carry so FLOATC treated the 16-bit value as unsigned, which is how
// the same code path prints 0..65535 where it would otherwise be signed.
void Interp::linprt(u32 n)
{
    Buf<16> b;
    b.put(n);
    outstr(b.str());
}

// ============================================================== PRINT
//
// The item loop, m6502.asm:2669-2710. The whole ";"/","-suppresses-the-newline
// rule is the two-instruction difference between the PRINT and PRINTC entry
// points: a separator jumps to code that re-enters at PRINTC, which does not
// emit the newline, so a trailing separator leaves the cursor where it is.
void Interp::print()
{
    bool newline = true;

    for (;;) {
        u8 c = chrgot();
        if (terminator(c))
            break;
        newline = true;

        if (c == TABTK || c == SPCTK) {
            // TABER (m6502.asm:2764-2780). The closing paren is checked
            // explicitly -- it is not part of the token.
            bool tab = (c == TABTK);
            chrget();
            u8 n = getbyt();
            CHK;
            synchr(')');
            CHK;
            u32 want = tab ? u32(n) : trmpos + u32(n);
            // TAB to a column already passed does nothing rather than wrapping.
            while (trmpos < want)
                outdo(' ');
            newline = false;
            continue;
        }
        if (c == ',') {
            chrget();
            // COMPRT (m6502.asm:2748-2762). Past the last comma field, just
            // move to the next line.
            if (trmpos >= ncmwid)
                crdo();
            else
                do
                    outdo(' ');
                while (trmpos % CLMWID);
            newline = false;
            continue;
        }
        if (c == ';') {
            chrget();
            newline = false;
            continue;
        }

        frmevl();
        CHK;
        if (fac.is_str()) {
            strprt(fac.s.str());
            ntemp = 0;
        } else {
            char t[32];
            Str s = fout(fac.n, t, sizeof t);
            // Before printing a number, TRMPOS + length is compared against
            // LINWID and a newline issued first if it would not fit.
            if (linwid && trmpos + s.size() > linwid)
                crdo();
            strprt(s);
            outdo(' '); // OUTSPC: every number gets a trailing space
        }
        CHK;
    }

    if (newline)
        crdo();
}

// PRINT# (m6502.asm:2656-2666). CMD reads a channel number, opens the output
// channel, stores it in CHANNL and falls into PRINT. Throughout the printer a
// non-zero CHANNL suppresses TRMPOS maintenance and automatic newlines.
void Interp::printn()
{
    u8 n = getbyt();
    CHK;
    if (!chan_find(n))
        ERR(ERRFD);
    if (chrgot() == ',' || chrgot() == ';')
        chrget();
    chanl = n;
    print();
    chanl = 0;
}

void Interp::cmd()
{
    printn();
}

void Interp::stmt_print()
{
    print();
}

void Interp::stmt_printn()
{
    printn();
}

void Interp::stmt_cmd()
{
    cmd();
}
