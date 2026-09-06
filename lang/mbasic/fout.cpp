// FOUT and FIN: the decimal conversions.
//
// m6502.asm's FLOATING POINT INPUT/OUTPUT ROUTINE section; 09- and
// 10-math-functions.md §§1-2.
//
// The arithmetic is IEEE double here rather than upstream's 5-byte
// excess-128 format, but the FORMAT is language-visible and is reproduced
// exactly. Upstream generated digits by repeatedly adding the entries of
// FOUTBL -- big-endian two's-complement powers of ten with alternating signs,
// driven by a loop whose condition is "continue while C equals N" -- and
// scaled by repeated MUL10/DIV10 rather than by a table. None of that survives
// under double, but every rule about what comes out does:
//
//   - 9 significant digits (ADDPRC=1; it was 6 without);
//   - a leading ' ' when non-negative and '-' when negative, and PRINT adds a
//     trailing space, which is why numbers separate even under ';';
//   - fixed notation for 0.01 <= |x| < 1e9, exponential outside it;
//   - below 1 a leading '.', so 0.5 prints as .5 and never as 0.5;
//   - trailing zeros stripped, and the '.' with them if nothing follows;
//   - in E notation, one digit before the point, an always-present sign, and
//     exactly two exponent digits;
//   - zero prints as " 0".
//
// Maximum width is 1 sign + 9 digits + 1 point + E + sign + 2 digits = 15,
// which is exactly what FBUFFR held.
#include "kernel/fmt.h"
#include "math/ftoa.h"
#include "math/math.h"
#include "mbasic.h"

namespace {

constexpr u32 SIGDIG = 9; // ADDPRC=1

// Nine significant digits of |v|, and the power of ten the first one carries.
// fmt_f64's %e rounds once, which is what upstream's FADDH-then-QINT did.
struct Digits {
    char d[SIGDIG + 2];
    u32 n;
    i32 exp10;
};

bool decompose(f64 a, Digits &out)
{
    char t[64];
    Str e = fmt_f64(t, sizeof t, a, i32(SIGDIG) - 1, 'e');
    if (e.empty())
        return false;

    out.n     = 0;
    out.exp10 = 0;
    usize i   = 0;
    for (; i < e.size() && e[i] != 'e' && e[i] != 'E'; i++)
        if (e[i] >= '0' && e[i] <= '9' && out.n < SIGDIG)
            out.d[out.n++] = e[i];
    if (i >= e.size())
        return false;

    i++;
    bool neg = e[i] == '-';
    if (e[i] == '+' || e[i] == '-')
        i++;
    i32 x = 0;
    for (; i < e.size() && e[i] >= '0' && e[i] <= '9'; i++)
        x = x * 10 + (e[i] - '0');
    out.exp10 = neg ? -x : x;

    while (out.n > 1 && out.d[out.n - 1] == '0') // trailing zeros go
        out.n--;
    return true;
}

} // namespace

Str Interp::fout(f64 v, char *buf, usize cap)
{
    Buf<32> b;

    // FOUTC stores the sign character in FACSGN itself; both characters have
    // bit 7 clear, so the FAC is positive from here on.
    b.put(v < 0 ? '-' : ' ');

    f64 a = v < 0 ? -v : v;
    if (a == 0 || !(a == a)) { // no NaN exists in upstream's format
        b.put('0');
    } else if (a > 1.7e38) {
        // Upstream had no infinity: an exponent past 255 raised ?OV. Nothing
        // reaches here that has not already, so print the largest thing that
        // makes sense rather than a word BASIC has no name for.
        b.put("1E+38");
    } else {
        Digits g;
        if (!decompose(a, g))
            return Str();

        // The fixed/exponential decision, m6502.asm:5893-5907.
        if (g.exp10 >= -2 && g.exp10 <= i32(SIGDIG) - 1) {
            if (g.exp10 < 0) {
                b.put('.');
                for (i32 k = -1; k > g.exp10; k--)
                    b.put('0');
                for (u32 k = 0; k < g.n; k++)
                    b.put(g.d[k]);
            } else {
                u32 whole = u32(g.exp10) + 1;
                for (u32 k = 0; k < whole; k++)
                    b.put(k < g.n ? g.d[k] : '0');
                if (g.n > whole) {
                    b.put('.');
                    for (u32 k = whole; k < g.n; k++)
                        b.put(g.d[k]);
                }
            }
        } else {
            b.put(g.d[0]);
            if (g.n > 1) {
                b.put('.');
                for (u32 k = 1; k < g.n; k++)
                    b.put(g.d[k]);
            }
            b.put('E');
            i32 x = g.exp10;
            b.put(x < 0 ? '-' : '+');
            if (x < 0)
                x = -x;
            b.put(char('0' + (x / 10) % 10)); // exactly two digits
            b.put(char('0' + x % 10));
        }
    }

    Str s   = b.str();
    usize n = s.size() < cap ? s.size() : cap;
    for (usize i = 0; i < n; i++)
        buf[i] = s[i];
    return Str(buf, n);
}

// ============================================================== FIN
//
// m6502.asm:5694-5807. Entered with TXTPTR on the first character. Note that
// the digits are gathered through CHRGET, which skips spaces without limit --
// so "1 2" really does parse as twelve, in upstream and here.
//
// The E's sign has to accept the crunched PLUSTK and MINUTK as well as '+' and
// '-' (m6502.asm:5726-5733), because by the time FIN runs on program text the
// tokenizer has already replaced them.
//
// Upstream applied the decimal exponent by calling MUL10 or DIV10 in a loop --
// there is no table of powers of ten -- so 1E38 cost 38 multiplications and
// accumulated the rounding error of each. scan_f64 rounds once.
f64 Interp::fin()
{
    Buf<64> t;
    u8 c = chrgot();

    if (c == '-' || c == MINUTK) {
        t.put('-');
        c = chrget();
    } else if (c == '+' || c == PLUSTK) {
        c = chrget();
    }

    bool any = false;
    for (; c >= '0' && c <= '9'; c = chrget()) {
        t.put(char(c));
        any = true;
    }
    if (c == '.') {
        t.put('.');
        for (c = chrget(); c >= '0' && c <= '9'; c = chrget()) {
            t.put(char(c));
            any = true;
        }
    }
    if (!any)
        return 0; // FIN with no digits at all answers zero

    if (c == 'E') {
        c = chrget();
        Buf<8> e;
        if (c == '-' || c == MINUTK) {
            e.put('-');
            c = chrget();
        } else if (c == '+' || c == PLUSTK) {
            c = chrget();
        }
        bool edig = false;
        for (; c >= '0' && c <= '9'; c = chrget()) {
            e.put(char(c));
            edig = true;
        }
        if (edig) {
            t.put('E');
            t.put(e.str());
        }
    }

    usize used    = 0;
    Option<f64> v = scan_f64(t.str(), used);
    if (!v.has_value())
        return 0;
    // Upstream raised ?OV on a positive exponent that would reach 100 and
    // silently clamped a negative one, which underflowed to zero.
    fac.n = v.value();
    ovcheck();
    CHKV(0);
    return v.value();
}
