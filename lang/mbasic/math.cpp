// The arithmetic functions.
//
// m6502.asm:4845-6669 -- the whole floating-point package -- reduced to what
// FUNDSP actually dispatches to; 10-math-functions.md.
//
// Upstream computed these itself, because it had to: LOG folded the argument
// into [0.5,1) by overwriting the exponent and then evaluated an odd minimax
// polynomial in (F-sqrt.5)/(F+sqrt.5); EXP used e^x = 2^(x log2 e) with a
// degree-7 polynomial on the fractional part; SIN reduced in TURNS rather than
// radians, dividing by 2pi and folding the quadrant so the polynomial
// approximated sin(2*pi*w) directly, which is why SINCON's leading coefficient
// is 2pi; and TAN depended on POLYX having left SIN's reduced argument in
// TEMPF1, re-entering the tail of SIN with a quadrant flag pushed by hand.
//
// Arithmetic is IEEE double here, so all of that is braam::math. What is kept
// is the behaviour around it: INT is a floor, RND is a deterministic sequence
// with the three argument cases, and the domain errors are upstream's.
#include "math/math.h"

#include "mbasic.h"

namespace {

// SIGN / FCSIGN (m6502.asm:5562-5569).
f64 sgn_of(f64 x)
{
    return x < 0 ? -1.0 : (x > 0 ? 1.0 : 0.0);
}

} // namespace

void Interp::fn_sgn()
{
    chknum();
    CHK;
    fac.n = sgn_of(fac.n);
}

// INT / QINT (m6502.asm:5641-5692). "QUICK GREATEST INTEGER FUNCTION" -- and a
// true floor, not a truncation: negatives were two's-complemented before the
// shift and the shift was arithmetic, so INT(-2.5) is -3.
void Interp::fn_int()
{
    chknum();
    CHK;
    fac.n = floor(fac.n);
}

// ABS was a single instruction, LSR FACSGN.
void Interp::fn_abs()
{
    chknum();
    CHK;
    if (fac.n < 0)
        fac.n = -fac.n;
}

// SQR was literally x ^ 0.5: MOVAF, load FHALF, fall into FPWRT.
void Interp::fn_sqr()
{
    chknum();
    CHK;
    if (fac.n < 0)
        ERR(ERRFC);
    fac.n = sqrt(fac.n);
}

// LOG (m6502.asm:5165-5260): zero or negative is ?FC.
void Interp::fn_log()
{
    chknum();
    CHK;
    if (fac.n <= 0)
        ERR(ERRFC);
    fac.n = log(fac.n);
}

void Interp::fn_exp()
{
    chknum();
    CHK;
    fac.n = exp(fac.n);
    ovcheck();
}

void Interp::fn_cos()
{
    chknum();
    CHK;
    fac.n = cos(fac.n);
}

void Interp::fn_sin()
{
    chknum();
    CHK;
    fac.n = sin(fac.n);
}

void Interp::fn_tan()
{
    chknum();
    CHK;
    fac.n = tan(fac.n);
}

void Interp::fn_atn()
{
    chknum();
    CHK;
    fac.n = atan(fac.n);
}

// RND (m6502.asm:6329-6397). The scheme, described at 6329-6342: multiply the
// previous value by a constant, add another, swap the high and low mantissa
// bytes -- so the low-order bits, which the multiply disturbs most, become the
// high-order bits of the result -- force the exponent so the result is below
// one, normalise, and store back.
//
//   RND(0)     returns the previous value again
//   RND(x<0)   reseeds the sequence from the argument itself
//   RND(x>0)   advances the sequence
//
// Upstream's RMULZC and RADDZC were declared with four bytes and read as five
// under ADDPRC=1, so the constants were not the ones intended and the sequence
// was not the one designed (13-porting-notes.md §3.3); the intended values are
// used here. It stays deterministic from a fixed seed, as upstream was: that
// is language-visible, and it is what makes a golden transcript possible.
void Interp::fn_rnd()
{
    chknum();
    CHK;

    f64 arg = fac.n;
    if (arg < 0)
        rndx = -arg;
    else if (arg > 0)
        rndx = rndx * 11879546.0 + 3.927677738602142e-08;

    // The byte swap, as a fold of the low-order bits over the high-order ones.
    rndx = rndx - floor(rndx);
    if (arg != 0) {
        f64 s = rndx * 65536.0;
        rndx  = (s - floor(s) + floor(s) / 65536.0) * 0.5;
        rndx  = rndx - floor(rndx);
    }
    fac.n = rndx;
}

// ============================================================== conversions
// ayint(), posint(), getbyt() and getadr() live in frmevl.cpp, beside the
// operators that need them.
