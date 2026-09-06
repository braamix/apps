// The statements.
//
// m6502.asm:2064-2653; 05-statements.md.
//
// Every handler was entered with A = the character following its token and
// CHRGET's flag contract set, and returned by RTS to NEWSTT. A handler that
// pushed a semi-permanent frame or abandoned the current position had to
// discard NEWSTT's return address and jump back to it, because FNDFOR's scan
// geometry depended on that address sitting immediately below the topmost
// frame. The scan is over a Vec now, so every handler here returns normally.
#include "math/math.h"
#include "mbasic.h"

// ============================================================== scanning
//
// DATAN and REMN (m6502.asm:2433-2462) both scan forward for a terminator
// using the CHARAC/ENDCHR pair and swap the two at every quote (EXCHQT), which
// is what makes a quoted colon invisible to DATA scanning. DATAN stops on ':'
// or NUL; REMN only on NUL.
void Interp::scan_to(bool stop_colon)
{
    bool quoted = false;
    for (;;) {
        u8 c = peek_at(txtptr);
        if (c == 0)
            return;
        if (c == '"')
            quoted = !quoted;
        else if (stop_colon && !quoted && c == ':')
            return;
        txtptr.off++;
    }
}

void Interp::datan()
{
    scan_to(true);
}

void Interp::remn()
{
    scan_to(false);
}

void Interp::stmt_data()
{
    datan();
}

void Interp::stmt_rem()
{
    remn();
}

// ============================================================== LET
//
// m6502.asm:2539-2653. The store is chosen by the VARIABLE's type, not the
// expression's, which is why the type is stacked around FRMEVL.
void Interp::stmt_let()
{
    forpnt = ptrget();
    CHK;
    u8 vt    = valtyp;
    bool ifl = intflg;

    synchr(EQULTK);
    CHK;
    frmevl();
    CHK;

    chkval(vt == VSTR);
    CHK;
    if (vt == VSTR) {
        // COPSTR. Upstream had to decide whether to duplicate the body:
        // GETSPT copied only when the data lived in dynamic string space AND
        // the descriptor belonged to a real variable, because of the
        // collector's single-owner invariant. A String owns its bytes.
        reason(var_put_str(forpnt, fac.s.str()));
        ntemp = 0;
    } else if (ifl) {
        i16 n = ayint(); // QINTGR: upstream stored two big-endian bytes
        CHK;
        var_put_num(forpnt, f64(n));
    } else {
        var_put_num(forpnt, fac.n);
    }
}

// ============================================================== control flow

// GOTO (m6502.asm:2391-2412). Upstream set TXTPTR = LOWTR - 1 so that NEWSTT
// would read a NUL there and take its ordinary "follow the link, load CURLIN,
// step over the header" path -- the jump reuses the line-crossing code rather
// than duplicating it. AT_LINE_START is that position.
void Interp::stmt_goto()
{
    linget();
    CHK;
    bool found = false;
    usize at   = fndlin(linnum, found);
    if (!found)
        ERR(ERRUS); // ?UNDEF'D STATEMENT
    txtptr = TextPos{ i32(at), AT_LINE_START };
}

// GOSUB (m6502.asm:2381-2389). The saved TXTPTR points BEFORE the target line
// number, since CHRGOT has not run yet at the point of the push -- which is
// why RETURN falls straight into DATA/DATAN afterwards: it must skip the rest
// of the GOSUB statement, and for ON X GOSUB a,b,c that is the whole remaining
// line-number list.
void Interp::stmt_gosub()
{
    if (!getstk(3))
        return;
    Frame f;
    f.kind   = Frame::Gosub;
    f.curlin = curlin;
    f.txtptr = txtptr;
    if (!reason(frames.push(f)))
        return;
    stmt_goto();
}

// RETURN (m6502.asm:2417-2436). Upstream called FNDFOR with a deliberately
// unmatchable pointer in order to skip past every FOR frame; the 1978-07-27
// note calls the bug that lived here "A SERIOUS BUG IN ALL VERSIONS" -- a loop
// variable whose pointer low byte was $FF compared equal, so FNDFOR stopped on
// a FOR frame and a spurious ?RG resulted. A typed frame kind has no byte to
// spell wrong.
//
// Discarding those FOR frames is the language rule that a GOSUB which leaves
// loops open still returns correctly (m6502.asm:425-441).
void Interp::stmt_return()
{
    if (!terminator(chrgot()))
        return; // let NEWSTT raise ?SN
    usize i = frames.size();
    while (i > 0 && frames[i - 1].kind == Frame::For)
        i--;
    if (i == 0)
        ERR(ERRRG);

    Frame f = frames[i - 1];
    frames.erase(i - 1, frames.size() - (i - 1));
    curlin = f.curlin;
    txtptr = f.txtptr;
    datan(); // skip the rest of the GOSUB statement
}

// IF (m6502.asm:2465-2477).
void Interp::stmt_if()
{
    frmevl();
    CHK;
    // Truth is FACEXP != 0: only the exponent byte was examined, so any
    // non-zero value is true -- and since a zero float always has exponent 0,
    // the test is exact.
    bool cond = fac.n != 0;

    u8 c = chrgot();
    if (c == GOTOTK) {
        // Deliberately NOT consumed, so that IF X THEN GOTO 100 reaches GONE3
        // and dispatches GOTO normally.
    } else {
        synchr(THENTK);
        CHK;
    }

    if (!cond) {
        // A false IF skips the ENTIRE rest of the line, not just to the next
        // ':': it shares the REM path, which stops on NUL only. So in
        // IF X THEN A=1 : B=2, B=2 is part of the consequent.
        remn();
        return;
    }

    c = chrgot();
    if (c >= '0' && c <= '9') { // a bare line number is a GOTO
        stmt_goto();
        return;
    }
    gone3();
}

// ON ... GOTO / GOSUB (m6502.asm:2480-2495). An index of 0, or one larger than
// the list, simply returns -- execution falls through to the next statement
// with no error. GETBYT yields 0..255, so a negative index has already raised
// ?FC in POSINT.
void Interp::stmt_ongoto()
{
    u8 n = getbyt();
    CHK;
    u8 tok = chrgot();
    if (tok != GOSUTK && tok != GOTOTK)
        ERR(ERRSN);
    chrget();

    for (u32 k = 1; k != n; k++) {
        linget(); // parse and discard one line number
        CHK;
        if (chrgot() != ',')
            return; // off the end of the list: fall through
        chrget();
    }
    if (n == 0) { // consume the list, then fall through
        for (;;) {
            linget();
            CHK;
            if (chrgot() != ',')
                return;
            chrget();
        }
    }
    if (tok == GOSUTK)
        stmt_gosub();
    else
        stmt_goto();
}

// ============================================================== FOR and NEXT

// FOR (m6502.asm:2081-2128). Frame layout in 02-data-structures.md §4.1.
void Interp::stmt_for()
{
    // SUBFLG = 128 is what forbids FOR A(1)= and FOR A%=: a FOR frame holds a
    // reference to the loop variable, and upstream's was a raw pointer that
    // creating any other variable would have invalidated.
    subflg = 128;
    stmt_let(); // the initial assignment, which leaves the variable in FORPNT
    CHK;

    Frame f;
    f.kind   = Frame::For;
    f.forpnt = forpnt;
    f.curlin = curlin;

    // The saved text pointer is not the loop body: it is TXTPTR + DATAN, the
    // ':' or NUL that ends the FOR statement itself. Taken here, before TO and
    // STEP are parsed, exactly as upstream took it.
    TextPos here = txtptr;
    datan();
    f.txtptr = txtptr;
    txtptr   = here;

    synchr(TOTK);
    CHK;
    frmnum();
    CHK;
    f.limit = fac.n;

    f.step = 1;
    if (chrgot() == STEPTK) {
        chrget();
        frmnum();
        CHK;
        f.step = fac.n;
    }

    // If a frame for the same variable already exists, it and everything above
    // it are discarded -- which is what stops a program that jumps out of a
    // loop and re-enters it from consuming a frame each time. The rationale is
    // spelled out at m6502.asm:411-425.
    usize at = fndfor(f.forpnt);
    if (at != frames.size())
        frames.erase(at, frames.size() - at);

    if (!getstk(9))
        return;
    reason(frames.push(f));
}

// FNDFOR (m6502.asm:1371-1392). Upstream walked upward from the stack pointer
// looking for FORTK bytes and stopped at the first byte that was not one; the
// sentinel region above STKEND existed only so that scan would terminate, and
// frames.empty() is that sentinel now.
//
// A null forpnt is the wildcard a bare NEXT uses -- upstream's FORPNT+1 == 0,
// which adopted the topmost frame's variable and then matched it.
usize Interp::fndfor(VarRef v) const
{
    for (usize i = frames.size(); i-- > 0;) {
        if (frames[i].kind != Frame::For)
            return frames.size(); // not a FOR frame: stop
        if (v.null() || frames[i].forpnt == v)
            return i;
    }
    return frames.size();
}

// NEXT (m6502.asm:3110-3163).
void Interp::stmt_next()
{
    for (;;) {
        VarRef v;
        if (!terminator(chrgot()) && chrgot() != ',') {
            v = ptrget();
            CHK;
        }

        usize i = fndfor(v);
        if (i == frames.size())
            ERR(ERRNF); // ?NEXT WITHOUT FOR

        // HAVFOR's TXS: truncate to this frame at once, discarding anything
        // above it.
        frames.erase(i + 1, frames.size() - i - 1);
        Frame &f = frames[i];

        f64 x = var_get_num(f.forpnt) + f.step;
        var_put_num(f.forpnt, x);

        // The termination test is sign(variable - limit) - sign(step) == 0,
        // and it is written that way and not as x > limit for a reason:
        // EQUALITY NEVER ENDS THE LOOP, and a step of zero loops for ever,
        // because sign(step) is 0 and the difference only reaches 0 when the
        // variable exactly equals the limit.
        f64 d  = x - f.limit;
        int sd = d < 0 ? -1 : (d > 0 ? 1 : 0);
        int ss = f.step < 0 ? -1 : (f.step > 0 ? 1 : 0);
        if (sd - ss != 0) {
            curlin = f.curlin;
            txtptr = f.txtptr;
            return;
        }

        frames.pop(); // LOOPDN
        if (chrgot() != ',')
            return;
        chrget(); // NEXT I,J re-enters at GETFOR
    }
}

// ============================================================== program state

void Interp::stmt_restore()
{
    datptr = TextPos{ 0, AT_LINE_START };
    datlin = 0;
}

// STOP and END (m6502.asm:2227-2248). Carry doubles as the "print BREAK" flag,
// and STOPC's BNE returns without acting if a terminator does not follow --
// so NEWSTT raises ?SN and no BREAK is printed.
void Interp::stmt_stop()
{
    if (terminator(chrgot()))
        stpend(true);
}

void Interp::stmt_end()
{
    if (terminator(chrgot()))
        stpend(false);
}

// CONT (m6502.asm:2255-2266).
void Interp::stmt_cont()
{
    if (!terminator(chrgot()))
        return; // let NEWSTT raise ?SN
    if (!oldtxt.valid)
        ERR(ERRCN); // ?CAN'T CONTINUE
    txtptr = oldtxt.txt;
    curlin = oldtxt.lin;
}

// RUN (m6502.asm:2366-2368). Without an argument, RUNC; with a line number,
// CLEARC and then GOTO.
void Interp::stmt_run()
{
    if (terminator(chrgot())) {
        runc();
        return;
    }
    clearc();
    stmt_goto();
}

// CLEAR (m6502.asm:1927): a bare return if no terminator follows, so that
// NEWSTT raises ?SN. This "return and let NEWSTT complain" idiom is how
// several statements say "only act if the syntax was perfect".
void Interp::stmt_clear()
{
    if (terminator(chrgot()))
        clearc();
}

void Interp::stmt_new()
{
    if (terminator(chrgot()))
        scrtch();
}
