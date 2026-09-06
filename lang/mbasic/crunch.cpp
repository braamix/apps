// Text fetching, tokenizing, and the program editor.
//
// m6502.asm's RAM CODE and CRUNCH sections; 03-tokenizer-editor.md.
#include "mbasic.h"

// ============================================================== CHRGET
//
// Upstream's CHRGET lived in RAM and modified itself: TXTPTR was defined as
// CHRGOT+1, the address field of the LDA that fetches, so advancing the text
// pointer meant incrementing an instruction. That saved an index register and
// a zero-page indirect setup on the hottest routine in the interpreter, and a
// plain position is equivalent (13-porting-notes.md §1).
//
// What is not equivalent is the contract, which callers several levels away
// consume: A is the character, carry says "it is a digit", and Z says "it is a
// statement terminator", meaning ':' or NUL. Those two are is_digit() and
// terminator() here, computed by the caller rather than left in flags.
//
// Spaces are skipped transparently and without limit -- which is why the
// tokenizer has to handle them itself.

// The byte at a position, or 0 at the end of a line. A line's text is
// NUL-terminated as upstream's was, so every scan still ends on a zero.
u8 Interp::peek_at(TextPos p) const
{
    const Vec<u8> &t = p.direct() ? dirbuf : prog[usize(p.line)].text;
    return p.off < t.size() ? t[p.off] : 0;
}

// CHRGOT: re-fetch at the current position without moving.
u8 Interp::chrgot()
{
    if (pending()) // poisoned: 0 is a terminator, so every scan loop ends
        return 0;
    if (!txtptr.direct() && usize(txtptr.line) >= prog.size())
        return 0;
    u8 c;
    while ((c = peek_at(txtptr)) == ' ')
        txtptr.off++;
    return c;
}

// CHRGET: pre-increment, then fetch.
u8 Interp::chrget()
{
    if (pending())
        return 0;
    txtptr.off++;
    return chrgot();
}

// SYNCHR (m6502.asm:3246). Require and consume a specific character; the
// SYNCHK macro was LDAI q / JSR SYNCHR. Note it compares against the byte at
// TXTPTR directly rather than through CHRGOT, then CHRGETs past it.
void Interp::synchr(u8 want)
{
    if (chrgot() != want)
        ERR(ERRSN);
    chrget();
}

// ============================================================== CRUNCH
//
// m6502.asm:1785-1865. Runs once per input line. Reserved words are replaced
// by single bytes with the MSB on -- "THIS SAVES SPACE AND TIME BY ALLOWING
// FOR TABLE DISPATCH DURING EXECUTION" -- and everything else is copied.
//
// A token's value is 128 + the word's ordinal in RESLST. Upstream produced it
// arithmetically, keying on a difference of exactly 128 between the input byte
// and the table byte, which is how it recognised the bit-7-flagged last
// character of an entry; a Str table matches whole words instead.
//
// DORES is the flag that turns crunching off inside DATA, re-enabled by the
// next ':'.
namespace {

// A reserved word matches at `at` if its characters are there. Matching is
// first-fit in table order and does no space skipping of its own, which is
// what makes IF 2 > F OR T=5 THEN mis-tokenize -- the danger the source warns
// about at m6502.asm:1208-1216, and a property of the language, not a defect.
usize match_res(Str src, usize at)
{
    for (usize i = 0; i < RESLST_COUNT; i++) {
        Str w = RESLST[i];
        if (at + w.size() > src.size())
            continue;
        bool eq = true;
        for (usize k = 0; k < w.size(); k++)
            if (src[at + k] != w[k]) {
                eq = false;
                break;
            }
        if (eq)
            return i;
    }
    return RESLST_COUNT;
}

} // namespace

void Interp::crunch(Str src, Vec<u8> &dst)
{
    dst.clear();
    bool dores = true; // crunching allowed
    usize i    = 0;

    while (i < src.size()) {
        u8 c = u8(src[i]);
        if (c == 0)
            break;

        // A space is stored verbatim and never reaches the matcher. This was
        // a deliberate change on 1978-02-11 -- "DISALLOWED SPACES IN RESERVED
        // WORDS. PUT IN SPECIAL CHECK FOR GO TO" -- and it is why GO exists as
        // a token of its own.
        if (c == ' ') {
            if (!reason(dst.push(c)))
                return;
            i++;
            continue;
        }

        // A quote: copy verbatim to the closing quote or the end of the line.
        if (c == '"') {
            if (!reason(dst.push(c)))
                return;
            i++;
            while (i < src.size() && src[i] != '"') {
                if (!reason(dst.push(u8(src[i]))))
                    return;
                i++;
            }
            if (i < src.size()) {
                if (!reason(dst.push('"')))
                    return;
                i++;
            }
            continue;
        }

        if (!dores) { // inside DATA: store verbatim until the next ':'
            if (!reason(dst.push(c)))
                return;
            if (c == ':')
                dores = true;
            i++;
            continue;
        }

        // '?' is PRINT. Digits, ':' and ';' are entered straightaway, without
        // attempting a match.
        if (c == '?') {
            if (!reason(dst.push(PRINTK)))
                return;
            i++;
            continue;
        }
        if (c >= '0' && c < '<') {
            if (!reason(dst.push(c)))
                return;
            if (c == ':')
                dores = true;
            i++;
            continue;
        }

        usize w = match_res(src, i);
        if (w == RESLST_COUNT) {
            if (!reason(dst.push(c)))
                return;
            i++;
            continue;
        }

        u8 tok = u8(ENDTK + w);
        if (!reason(dst.push(tok)))
            return;
        i += RESLST[w].size();

        if (tok == DATATK)
            dores = false; // no crunching until the next ':'
        if (tok == REMTK) {
            // REM swallows the rest of the line, ':' and '"' included.
            while (i < src.size() && src[i] != 0) {
                if (!reason(dst.push(u8(src[i]))))
                    return;
                i++;
            }
            break;
        }
    }
    reason(dst.push(0));
}

// ============================================================== LINGET
//
// m6502.asm:2508-2536. Reads a line number into LINNUM. With no digits at all
// it returns immediately with LINNUM = 0, which both LIST and GOTO rely on.
// The overflow guard is applied before each multiply, so the largest accepted
// line number is 63999 -- and exceeding it is a SYNTAX error, not an overflow.
void Interp::linget()
{
    linnum = 0;
    for (u8 c = chrgot(); c >= '0' && c <= '9'; c = chrget()) {
        if (linnum >= 6400)
            ERR(ERRSN);
        linnum = u16(linnum * 10 + (c - '0'));
    }
}

// ============================================================== FNDLIN
//
// m6502.asm:1879-1904. One routine serving both "find this line" and "find
// where this line should go": on a miss it answers the index of the first line
// greater than n, which is the insertion point.
usize Interp::fndlin(u16 n, bool &found) const
{
    usize lo = 0, hi = prog.size();
    while (lo < hi) {
        usize mid = lo + (hi - lo) / 2;
        if (prog[mid].num < n)
            lo = mid + 1;
        else
            hi = mid;
    }
    found = lo < prog.size() && prog[lo].num == n;
    return lo;
}

// ============================================================== the editor
//
// MAIN, m6502.asm:1556-1571. A line that starts with a digit is a program
// line; anything else is executed in place.
//
// Note that typing any program line clears all variables: RUNC calls CLEARC.
// Upstream had no choice -- inserting a line moved VARTAB and everything above
// it, so no variable could have survived -- and it is a language-visible rule
// that the port keeps deliberately (13-porting-notes.md §2.8).
void Interp::main_line()
{
    Str line = in_line.str();

    usize i = 0;
    while (i < line.size() && line[i] == ' ')
        i++;
    // An empty line goes back to MAIN, not to READY: no OK is printed. The TAX
    // after CHRGET is there because CHRGET's Z flag conflates ':' with NUL, and
    // re-deriving Z from A alone separates them.
    if (i == line.size())
        SUSPEND(suspend_line("", 0, Resume::Main));

    curlin = DIRECT_LINE; // mark direct mode now, before knowing which it is

    if (line[i] < '0' || line[i] > '9') { // a direct statement
        crunch(line, dirbuf);
        CHK;
        txtptr = TextPos{ DIRECT, AT_LINE_START };
        gone();
        return;
    }

    // A program line. LINGET over the raw text, since it is not crunched yet.
    // It reads through CHRGET, which skips spaces without limit -- so the
    // spaces between and after the digits are gone before CRUNCH ever sees
    // them, and "1 0 PRINT" is line 10.
    u32 n = 0;
    for (; i < line.size(); i++) {
        if (line[i] == ' ')
            continue;
        if (line[i] < '0' || line[i] > '9')
            break;
        n = n * 10 + u32(line[i] - '0');
        if (n > MAXLIN)
            ERR(ERRSN);
    }
    while (i < line.size() && line[i] == ' ')
        i++;

    Vec<u8> text;
    crunch(line.substr(i), text);
    CHK;

    bool found = false;
    usize at   = fndlin(u16(n), found);
    if (found)
        prog.erase(at);

    // Everything but the terminating NUL: a bare line number deletes.
    if (text.size() > 1) {
        Line l;
        l.num  = u16(n);
        l.text = static_cast<Vec<u8> &&>(text);
        if (!reason(prog.insert(at, static_cast<Line &&>(l))))
            return;
    }
    runc();
    CHK;
    // FINI does JMP MAIN, not JMP READY: no OK is printed after a program line
    // is typed, only after a direct statement.
    suspend_line("", 0, Resume::Main);
}

// ============================================================== SCRTCH
//
// m6502.asm:1909-1922. NEW: empty the program, then fall into RUNC.
void Interp::scrtch()
{
    prog.clear();
    runc();
}

// STXTPT (m6502.asm:1964-1971): point TXTPTR at TXTTAB-1, so that the
// statement fetcher reads a terminator there and crosses into the first line
// by its ordinary path.
void Interp::stxtpt()
{
    txtptr = TextPos{ 0, AT_LINE_START };
    curlin = DIRECT_LINE;
}
