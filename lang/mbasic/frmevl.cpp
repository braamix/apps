// FRMEVL and EVAL: formula evaluation.
//
// m6502.asm's FORMULA EVALUATION CODE section; 06-expressions.md.
//
// Upstream was a precedence-climbing evaluator that used the 6502 hardware
// stack for its operand and operator stack, building a 10-byte deferred frame
// each time it descended into a higher-precedence subexpression. Those frames
// had no tag byte and were never scanned for -- only unwound in order -- which
// is why they are ordinary locals of a recursive function here, and why
// STKINI has nothing to say about them.
//
// What does survive is the depth bound: every nesting level called GETSTK with
// A=1, and NUMLEV = 23 is the guaranteed depth. Exceeding it is ?OUT OF
// MEMORY, not a crash.
#include "math/math.h"
#include "mbasic.h"

// ============================================================== type checks
//
// m6502.asm:3169-3180. The convention is carry in = "I require a string".
// VALTYP is 0 for numeric and 255 for string, and is tested with BIT so the
// answer arrives in the N flag without disturbing A.
void Interp::chknum()
{
    if (!pending() && fac.is_str())
        error(ERRTM);
}

void Interp::chkstr()
{
    if (!pending() && !fac.is_str())
        error(ERRTM);
}

void Interp::chkval(bool want_str)
{
    want_str ? chkstr() : chknum();
}

void Interp::frmnum()
{
    frmevl();
    CHK;
    chknum();
}

// ============================================================== FRMEVL

void Interp::frmevl()
{
    if (!getstk(1))
        return;
    Depth d(*this);
    frmevl_prec(0);
}

// The dummy precedence 0 is the sentinel that means "the whole expression is
// finished" -- QOPGO tests for it.
void Interp::frmevl_prec(u8 minprec)
{
    eval();
    CHK;

    for (;;) {
        TextPos save = txtptr;

        // LOPREL (m6502.asm:3210-3223) gathers a run of relational characters
        // into a bit mask: 1 for '>', 2 for '=', 4 for '<'. Compound operators
        // are just OR-ed bits, so <= is 6 and >= is 3 -- and because any
        // combination is accepted, "=<" and "><" are legal spellings. A
        // repeated bit is rejected, so "<<" is a syntax error.
        u8 mask = 0;
        u8 c    = chrgot();
        while (c >= GREATK && c <= LESSTK) {
            u8 bit = (c == GREATK) ? 1 : (c == EQULTK) ? 2 : 4;
            if (mask & bit)
                ERR(ERRSN);
            mask |= bit;
            c = chrget();
        }
        CHK;

        u8 opi;
        if (mask)
            opi = OP_REL;
        else if (c >= PLUSTK && c <= ORTK)
            opi = u8(c - PLUSTK);
        else {
            txtptr = save;
            return;
        }

        // ENDREL's `ADC VALTYP` is simultaneously "is this a +?" and "is the
        // current value a string?", and only when both hold does it go to CAT.
        // String + calls EVAL and not FRMEVL, so it binds tighter than
        // everything and has no precedence interaction at all.
        if (opi == 0 && fac.is_str()) {
            chrget();
            cat();
            CHK;
            continue;
        }

        u8 prec = OPTAB[opi].prec;
        if (prec <= minprec) { // QPREC: old >= new, so apply the stacked one
            txtptr = save;
            return;
        }

        if (!mask)
            chrget();
        opmask = mask;

        // ARG is the left operand and the FAC the right, which is why FSUB is
        // ARG-FAC and FDIV is ARG/FAC.
        Val arg      = static_cast<Val &&>(fac);
        bool holding = arg.is_str();
        if (holding && ++ntemp > NUMTMP)
            ERR(ERRST); // ?FORMULA TOO COMPLEX: upstream had three temporaries

        fac = Val{};
        if (!getstk(1))
            return;
        {
            Depth d(*this);
            frmevl_prec(prec);
        }
        if (holding)
            ntemp--;
        CHK;

        domask = mask;
        (this->*OPTAB[opi].fn)(arg);
        CHK;
    }
}

// ============================================================== EVAL
//
// m6502.asm:3323-3372. One term, in upstream's recognition order.
void Interp::eval()
{
    for (;;) {
        u8 c = chrgot();

        if ((c >= '0' && c <= '9') || c == '.') {
            fac.n      = fin();
            fac.valtyp = VNUM;
            return;
        }
        if (isletc(c)) {
            isvar();
            return;
        }
        if (c == MINUTK) {
            // GONPRC pushes a unary operator as an ordinary OPTAB entry, so
            // unary minus needs no special case beyond having a precedence.
            chrget();
            if (!getstk(1))
                return;
            {
                Depth d(*this);
                frmevl_prec(OPTAB[OP_NEG].prec);
            }
            CHK;
            Val ignored;
            op_neg(ignored);
            return;
        }
        if (c == PLUSTK) { // unary plus is a no-op
            chrget();
            continue;
        }
        if (c == NOTTK) {
            chrget();
            if (!getstk(1))
                return;
            {
                Depth d(*this);
                frmevl_prec(OPTAB[OP_NOT].prec);
            }
            CHK;
            Val ignored;
            op_not(ignored);
            return;
        }
        if (c == '"') {
            strtxt();
            return;
        }
        if (c == FNTK) {
            fndoer();
            return;
        }
        if (c >= ONEFUN) {
            isfun();
            return;
        }
        parchk();
        return;
    }
}

// STRTXT / STRLIT (m6502.asm:4271-4305). A run of characters up to the closing
// quote or the end of the line becomes the value. Upstream had to decide here
// whether to copy the bytes into string space -- the copy-if-volatile rule,
// which depended on which page the text was on and was observable through FRE.
// A String owns its bytes, so there is nothing to decide.
void Interp::strtxt()
{
    fac.s.clear();
    fac.valtyp = VSTR;

    const Vec<u8> &t = txtptr.direct() ? dirbuf : prog[usize(txtptr.line)].text;
    u32 i            = txtptr.off + 1;
    while (i < t.size() && t[i] != 0 && t[i] != '"') {
        if (!reason(fac.s.push(char(t[i]))))
            return;
        i++;
    }
    txtptr.off = (i < t.size() && t[i] == '"') ? i : i - 1;
    chrget();
}

// PARCHK (m6502.asm:3372-3380).
void Interp::parchk()
{
    if (chrgot() != '(')
        ERR(ERRSN);
    chrget();
    frmevl();
    CHK;
    if (chrgot() != ')')
        ERR(ERRSN);
    chrget();
}

// ISVAR (m6502.asm:3399-3465). This is the point at which upstream established
// "a string in the FAC is a pointer to a descriptor": ISVRET stored the
// variable's ADDRESS into FACMO. Here the value is copied instead.
void Interp::isvar()
{
    VarRef v = ptrget(false); // reading does not create -- see ptrget.cpp
    CHK;
    if (v.valtyp == VSTR) {
        fac.valtyp = VSTR;
        reason(fac.s.assign(var_get_str(v)));
        return;
    }
    fac.valtyp = VNUM;
    fac.n      = var_get_num(v);
}

// ISFUN (m6502.asm:3468-3516). Tokens up to LASNUM take one parenthesised
// argument; the three past it take more and are dispatched differently.
void Interp::isfun()
{
    u8 tok = chrgot();
    chrget();

    if (tok <= LASNUM) {
        parchk();
        CHK;
        (this->*FUNDSP[tok - ONEFUN])();
        return;
    }

    // LEFT$, RIGHT$ and MID$. Upstream juggled the descriptor pointer and the
    // function number on the 6502 stack while GETBYT read the numeric
    // argument, and PREAM was the shared prologue that unpicked it.
    if (chrgot() != '(')
        ERR(ERRSN);
    chrget();
    frmevl();
    CHK;
    chkstr();
    CHK;
    fnstr = static_cast<String &&>(fac.s);

    if (chrgot() != ',')
        ERR(ERRSN);
    chrget();
    fnn1 = getbyt();
    CHK;
    fnhas2 = false;
    if (chrgot() == ',') {
        chrget();
        fnn2   = getbyt();
        fnhas2 = true;
        CHK;
    }
    if (chrgot() != ')')
        ERR(ERRSN);
    chrget();
    (this->*FUNDSP[tok - ONEFUN])();
}

// ============================================================== conversions

// AYINT (m6502.asm:3801-3810). Upstream compared against N32768, a constant
// declared with four bytes but read as five under ADDPRC=1, so the comparison
// value was -32768.00048828125 and exactly -32768 was rejected with ?FC --
// which broke A% = -32768, NOT 32767 and -32768 AND -1
// (13-porting-notes.md §3.2). Fixed: the range is the whole of i16.
i16 Interp::ayint()
{
    chknum();
    CHKV(0);
    f64 x = floor(fac.n); // QINT is a true floor, not a truncation
    if (x < -32768 || x > 32767)
        ERRV(0, ERRFC);
    return i16(x);
}

// POSINT additionally rejects negatives, and is the entry array subscripts
// use -- which is why A(-1) gives ?FC and not ?BS.
i16 Interp::posint()
{
    frmnum();
    CHKV(0);
    if (fac.n < 0)
        ERRV(0, ERRFC);
    return ayint();
}

// GETBYT / CONINT (m6502.asm:4749-4755): FRMNUM, then POSINT, then require the
// high byte to be zero, so the value must be 0..255.
u8 Interp::getbyt()
{
    i16 n = posint();
    CHKV(0);
    if (n > 255)
        ERRV(0, ERRFC);
    return u8(n);
}

// GETADR (m6502.asm:4802-4809): negative or >= 65536 is ?FC. POKE and WAIT
// parse their address; PEEK's has already been evaluated by PARCHK, so the two
// halves are separate here where upstream had one routine reached two ways.
u16 Interp::adr_of()
{
    chknum();
    CHKV(0);
    f64 x = floor(fac.n);
    if (x < 0 || x > 65535)
        ERRV(0, ERRFC);
    return u16(x);
}

u16 Interp::getadr()
{
    frmnum();
    CHKV(0);
    return adr_of();
}

// ============================================================== operators
//
// Upstream's exponent was one byte in excess-128, so the range was 2^-127 to
// 2^127 and a result past it raised ?OV from MULDIV or RNDSHF. There is no
// infinity in that format and no NaN, so every operation that can leave the
// range is checked; underflow, by contrast, silently became zero and still
// does.
namespace {

constexpr f64 FMAX = 1.7014118e38; // 2^127, upstream's largest exponent

} // namespace

void Interp::ovcheck()
{
    if (fac.n > FMAX || fac.n < -FMAX || !(fac.n == fac.n))
        error(ERROV);
}

void Interp::op_add(Val &arg)
{
    chknum();
    CHK;
    if (arg.is_str())
        ERR(ERRTM);
    fac.n = arg.n + fac.n;
    ovcheck();
}

void Interp::op_sub(Val &arg)
{
    chknum();
    CHK;
    if (arg.is_str())
        ERR(ERRTM);
    fac.n = arg.n - fac.n; // FSUB is ARG - FAC: memory minus the accumulator
    ovcheck();
}

void Interp::op_mul(Val &arg)
{
    chknum();
    CHK;
    if (arg.is_str())
        ERR(ERRTM);
    fac.n = arg.n * fac.n;
    ovcheck();
}

void Interp::op_div(Val &arg)
{
    chknum();
    CHK;
    if (arg.is_str())
        ERR(ERRTM);
    if (fac.n == 0)
        ERR(ERRDV0); // detected at entry: there would be nowhere to put a result
    fac.n = arg.n / fac.n;
    ovcheck();
}

// FPWRT (m6502.asm:6102-6150), whose rules the source states at 6111-6119:
// y == 0 gives 1, so 0^0 is 1; x == 0 gives 0; a negative base needs an
// integer exponent or LOG raises ?FC, and the result is negated when the
// exponent is odd.
void Interp::op_pwr(Val &arg)
{
    chknum();
    CHK;
    if (arg.is_str())
        ERR(ERRTM);
    f64 x = arg.n, y = fac.n;

    if (y == 0) {
        fac.n = 1;
        return;
    }
    if (x == 0) {
        fac.n = 0;
        return;
    }
    if (x < 0) {
        if (floor(y) != y)
            ERR(ERRFC);
        f64 r = exp(y * log(-x));
        fac.n = (i64(y) & 1) ? -r : r;
        return;
    }
    fac.n = exp(y * log(x));
    ovcheck();
}

// ANDOP / OROP (m6502.asm:3518-3540). One routine implemented both by De
// Morgan: COUNT = 0 gave AND, COUNT = 255 complemented the inputs and the
// output. Both operands go through AYINT, so they must be in -32768..32767.
void Interp::op_and(Val &arg)
{
    i16 b = ayint();
    CHK;
    fac   = static_cast<Val &&>(arg);
    i16 a = ayint();
    CHK;
    fac.valtyp = VNUM;
    fac.n      = f64(i16(a & b));
}

void Interp::op_or(Val &arg)
{
    i16 b = ayint();
    CHK;
    fac   = static_cast<Val &&>(arg);
    i16 a = ayint();
    CHK;
    fac.valtyp = VNUM;
    fac.n      = f64(i16(a | b));
}

// NEGOP is a no-op on zero, so -0 cannot be produced.
void Interp::op_neg(Val &)
{
    chknum();
    CHK;
    if (fac.n != 0)
        fac.n = -fac.n;
}

// NOT x = ~x = -x-1 in 16-bit two's complement, so NOT 0 = -1, which is
// consistent with the comparison result.
void Interp::op_not(Val &)
{
    i16 a = ayint();
    CHK;
    fac.valtyp = VNUM;
    fac.n      = f64(i16(~a));
}

// DOREL (m6502.asm:3547-3596). FCOMP answers 1 if ARG < FAC, 0 if equal, -1 if
// ARG > FAC; DOCMP then maps {-1,0,1} to the bits {4,2,1} -- exactly '<', '='
// and '>' -- and ANDs with the mask the scan gathered.
//
// So BASIC's true is -1, and because it is produced by masking, A<=B and
// (A<B) OR (A=B) yield identical values.
void Interp::op_rel(Val &arg)
{
    int r;
    if (arg.is_str() || fac.is_str()) {
        if (arg.is_str() != fac.is_str())
            ERR(ERRTM);
        // STRCMP: lexicographic up to the shorter length, then the length
        // difference decides. No case folding.
        Str a = arg.s.str(), b = fac.s.str();
        usize n = a.size() < b.size() ? a.size() : b.size();
        r       = 0;
        for (usize i = 0; i < n && r == 0; i++)
            r = u8(a[i]) < u8(b[i]) ? -1 : (u8(a[i]) > u8(b[i]) ? 1 : 0);
        if (r == 0)
            r = a.size() < b.size() ? -1 : (a.size() > b.size() ? 1 : 0);
    } else {
        r = arg.n < fac.n ? -1 : (arg.n > fac.n ? 1 : 0);
    }

    u8 bits = u8(r < 0 ? 4 : (r > 0 ? 1 : 2));
    fac.s.clear();
    fac.valtyp = VNUM;
    fac.n      = (bits & domask) ? -1.0 : 0.0;
}
