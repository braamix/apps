// The string functions.
//
// m6502.asm's STRING FUNCTIONS section; 08-strings-gc.md.
//
// Upstream's whole subsystem existed to serve one invariant, stated at
// m6502.asm:648-650:
//
//   IT IS THE NATURE OF GARBAGE COLLECTION THAT DISALLOWS HAVING TWO STRING
//   DESCRIPTORS POINT TO THE SAME AREA IN STRING SPACE.
//
// The collector found the highest not-yet-moved string, slid it to the top of
// free space and rewrote the ONE descriptor it came from; two descriptors
// sharing a body would have left one dangling. Everything followed from that:
// the six-step protocol (work out the length, GETSPA -- which may collect, so
// only descriptor pointers survive it -- build the descriptor in DSCTMP, copy,
// FRETMP the arguments, PUTNEW), the copy-if-volatile rule, and the three
// temporaries.
//
// A String owns its bytes, so none of that is needed and none of it is here.
// Two things it decided are kept because they are language-visible: a result
// longer than 255 characters is ?STRING TOO LONG, and there are still only
// three live temporaries, so a deep enough string expression is ?FORMULA TOO
// COMPLEX (frmevl.cpp).
#include "kernel/fmt.h"
#include "kernel/text.h"
#include "mbasic.h"

namespace {

constexpr usize STRMAX = 255;

// Characters, not bytes: 255 is a count of runes here, LEN answers runes, and
// LEFT$/RIGHT$/MID$ cut on rune boundaries so none of them can halve one.
// utf8_decode answers 0 on a truncated tail, so every loop tests it.
usize runes(Str s)
{
    usize n = 0;
    for (usize i = 0; i < s.size();) {
        char32_t c;
        usize k = utf8_decode(s, i, c);
        i += k ? k : 1;
        n++;
    }
    return n;
}

// The byte offset of rune `n`, or s.size() past the end.
usize rune_at(Str s, usize n)
{
    usize i = 0;
    for (; n && i < s.size(); n--) {
        char32_t c;
        usize k = utf8_decode(s, i, c);
        i += k ? k : 1;
    }
    return i;
}

} // namespace

// CAT (m6502.asm:4522-4548), reached from FRMEVL only when the operator is +
// and VALTYP is 255. It pushes the current descriptor and calls EVAL, not
// FRMEVL, so string + behaves as a maximally tight left-to-right operator.
void Interp::cat()
{
    Val left = static_cast<Val &&>(fac);
    fac      = Val{};

    if (++ntemp > NUMTMP)
        ERR(ERRST);
    eval();
    ntemp--;
    CHK;
    chkstr();
    CHK;

    if (runes(left.s.str()) + runes(fac.s.str()) > STRMAX)
        ERR(ERRLS);

    String r;
    if (!reason(r.assign(left.s.str())) || !reason(r.append(fac.s.str())))
        return;
    fac.s      = static_cast<String &&>(r);
    fac.valtyp = VSTR;
}

// LEN1 frees the argument, forces VALTYP back to numeric, and floats the
// length.
void Interp::fn_len()
{
    chkstr();
    CHK;
    f64 n = f64(runes(fac.s.str()));
    fac.s.clear();
    fac.valtyp = VNUM;
    fac.n      = n;
}

// STR$ (m6502.asm:4242-4248): FOUTC with Y=0, so the text began at LOFBUF on
// page zero and was therefore copied. The leading space or '-' is FOUT's and
// is part of the answer.
void Interp::fn_str()
{
    chknum();
    CHK;
    char t[32];
    Str s      = fout(fac.n, t, sizeof t);
    fac.valtyp = VSTR;
    reason(fac.s.assign(s));
}

// VAL (m6502.asm:4763-4789). Upstream stored a zero at body+length to give FIN
// a terminator and restored the byte afterwards -- harmless on a 6502, a
// genuine out-of-bounds write for any bounded string type, and not re-entrant
// (13-porting-notes.md §3.9). Fixed: FIN runs over a bounded copy held in the
// direct-line buffer, so nothing outside the string is touched.
void Interp::fn_val()
{
    chkstr();
    CHK;
    String s = static_cast<String &&>(fac.s);
    fac      = Val{};

    Vec<u8> save  = static_cast<Vec<u8> &&>(dirbuf);
    TextPos savep = txtptr;
    for (usize i = 0; i < s.size(); i++)
        if (!reason(dirbuf.push(u8(s[i]))))
            return;
    if (!reason(dirbuf.push(0)))
        return;

    txtptr     = TextPos{ DIRECT, 0 };
    f64 v      = fin();
    txtptr     = savep;
    dirbuf     = static_cast<Vec<u8> &&>(save);
    fac.valtyp = VNUM;
    fac.n      = v;
}

// ASC (m6502.asm:4741-4746): a null string gives ?FC. The codepoint, not the
// first byte, so ASC and CHR$ are inverses over the whole range.
void Interp::fn_asc()
{
    chkstr();
    CHK;
    if (fac.s.empty())
        ERR(ERRFC);
    char32_t r;
    f64 n = f64(utf8_decode(fac.s.str(), 0, r) ? u32(r) : 0xFFFD);
    fac.s.clear();
    fac.valtyp = VNUM;
    fac.n      = n;
}

// CHR$ (m6502.asm:4632-4642). Upstream was CONINT, so 0..255 and one byte; a
// codepoint here, encoded, which is the only way to write a non-ASCII
// character from a program and the one thing that keeps CHR$ from building an
// invalid sequence.
void Interp::fn_chr()
{
    chknum();
    CHK;
    if (fac.n < 0 || fac.n > 0x10FFFF)
        ERR(ERRFC);
    char b[4];
    usize n    = utf8_encode(char32_t(u32(fac.n)), b);
    fac.valtyp = VSTR;
    fac.s.clear();
    reason(fac.s.assign(Str(b, n)));
}

// LEFT$ (m6502.asm:4648-4671): n >= len uses len and offset 0.
void Interp::fn_left()
{
    Str s      = fnstr.str();
    fac.valtyp = VSTR;
    reason(fac.s.assign(s.substr(0, rune_at(s, fnn1))));
}

// RIGHT$ (m6502.asm:4672-4676): computes offset = len - n, then shares LEFT$'s
// tail.
void Interp::fn_right()
{
    Str s      = fnstr.str();
    usize len  = runes(s);
    usize n    = fnn1 < len ? fnn1 : len;
    usize at   = rune_at(s, len - n);
    fac.valtyp = VSTR;
    reason(fac.s.assign(s.substr(at, s.size() - at)));
}

// MID$ (m6502.asm:4684-4704): the length defaults to 255, position 0 gives
// ?FC, and a position past the end yields the null string.
void Interp::fn_mid()
{
    Str s = fnstr.str();
    if (fnn1 == 0)
        ERR(ERRFC);
    usize want = fnhas2 ? fnn2 : STRMAX;
    usize at   = rune_at(s, fnn1 - 1);
    fac.valtyp = VSTR;
    if (at >= s.size()) {
        fac.s.clear();
        return;
    }
    usize end = rune_at(s, fnn1 - 1 + want);
    reason(fac.s.assign(s.substr(at, end - at)));
}

// FRE (m6502.asm:4113-4122). Upstream freed its argument if it was a string,
// forced a full garbage collection -- calling FRE(0) purely for that side
// effect was the idiomatic use -- and returned FRETOP - STREND floated as a
// SIGNED 16-bit value, so more than 32767 free bytes reported as negative.
//
// There is no collector to force. The count is against a fixed budget,
// less what the program, the variables and the arrays actually hold, and it is
// still signed, because a program comparing it against 32767 should still
// behave.
void Interp::fn_fre()
{
    fac.s.clear();
    fac.valtyp = VNUM;

    u32 used = 0;
    for (usize i = 0; i < prog.size(); i++)
        used += u32(prog[i].text.size()) + 4;
    for (usize i = 0; i < vars.size(); i++)
        used += 7 + u32(vars[i].str.size());
    for (usize i = 0; i < arrays.size(); i++) {
        const Array &a = arrays[i];
        used += 5 + 2 * u32(a.extent.size());
        used += 5 * u32(a.num.size()) + 2 * u32(a.ints.size());
        for (usize k = 0; k < a.str.size(); k++)
            used += 3 + u32(a.str[k].size());
    }

    i32 left = i32(memsiz) - i32(used);
    fac.n    = f64(i16(left));
}
