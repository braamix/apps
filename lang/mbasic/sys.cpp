// POKE, PEEK, WAIT, POS, USR and SYS -- the statements that lost their
// machine.
//
// m6502.asm's PEEK, POKE, AND FNWAIT section; 05-statements.md §7.
//
// All three of POKE, PEEK and WAIT got their address through GETADR, which
// rejects a negative value or one at or above 65536 with ?ILLEGAL QUANTITY.
// That check is unchanged, and so is the 64 KiB it checks against: they
// address a scratch space of their own rather than the interpreter's storage,
// which is what a browser tab has instead of a bus.
//
// USR and SYS have nothing to call. Upstream's USR was a FUNDSP entry pointing
// at USRPOK, a page-zero JMP FCERR whose target the user overwrote with POKE,
// so it raised ?ILLEGAL QUANTITY until it was aimed somewhere -- which is
// exactly what it does here, permanently.
#include "mbasic.h"

// POKE (m6502.asm:4821-4831).
void Interp::stmt_poke()
{
    u16 addr = getadr();
    CHK;
    if (chrgot() != ',')
        ERR(ERRSN);
    chrget();
    u8 v = getbyt();
    CHK;
    poke_space[addr] = v;
}

// PEEK (m6502.asm:4811-4820). Upstream saved and restored POKER around the
// fetch -- the 1977-12-01 fix, because POKER aliases LINNUM and without it
// POKE X,PEEK(Y) clobbered its own destination address.
void Interp::fn_peek()
{
    u16 addr = adr_of(); // ISFUN's PARCHK has already evaluated the argument
    CHK;
    fac.valtyp = VNUM;
    fac.n      = f64(poke_space[addr]);
}

// WAIT (m6502.asm:4832-4844): an address, an AND mask, and an optional EOR
// mask defaulting to zero, then spin until ((*addr) EOR eor) AND and != 0.
//
// Upstream had no timeout and no ^C check, so a WAIT on a condition that never
// occurred hung the machine (13-porting-notes.md §2.8). Nothing here can
// change the scratch space under us, so the condition is tested once: a wait
// that would not have ended immediately would not have ended at all.
void Interp::stmt_wait()
{
    u16 addr = getadr();
    CHK;
    if (chrgot() != ',')
        ERR(ERRSN);
    chrget();
    u8 andmsk = getbyt();
    CHK;
    u8 eormsk = 0;
    if (chrgot() == ',') {
        chrget();
        eormsk = getbyt();
        CHK;
    }
    if (((poke_space[addr] ^ eormsk) & andmsk) == 0)
        ERR(ERRFC);
}

// POS (m6502.asm:4130-4132): TRMPOS as an unsigned byte. The argument is
// parsed and discarded.
void Interp::fn_pos()
{
    fac.s.clear();
    fac.valtyp = VNUM;
    fac.n      = f64(trmpos & 0xFF);
}

void Interp::fn_usr()
{
    ERR(ERRFC);
}

void Interp::stmt_sys()
{
    ERR(ERRFC);
}
