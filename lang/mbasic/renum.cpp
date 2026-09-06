// RENUM -- not upstream's, from the later Microsoft releases.
//
// The 6502 original could not have had it: a program was a linked list of
// absolute addresses. Here prog is a sorted Vec with no links, and a line
// number REFERENCE is plain ASCII in the crunched text -- CRUNCH enters digits
// without matching (crunch.cpp) and LINGET re-parses them at run time. So this
// rewrites digit runs and nothing else.
//
//   RENUM [new][,[old][,inc]]        defaults 10, first line, 10
#include "mbasic.h"

namespace {

// The tokens a line number can follow. GOTO and GOSUB take a comma-separated
// list, which is what serves ON X GOTO a,b,c.
bool takes_list(u8 c)
{
    return c == GOTOTK || c == GOSUTK;
}

bool takes_one(u8 c)
{
    return c == THENTK || c == RUNTK;
}

// Emit n as decimal digits.
bool put_num(Vec<u8> &dst, u16 n)
{
    u8 d[5];
    usize k = 0;
    do {
        d[k++] = u8('0' + n % 10);
        n /= 10;
    } while (n);
    while (k)
        if (!dst.push(d[--k]))
            return false;
    return true;
}

} // namespace

void Interp::renum()
{
    // LINGET answers 0 both for "no digits" and for a literal 0, so peek.
    u16 first = 10, inc = 10, old = 0;

    if (isdigit(chrgot())) {
        linget();
        CHK;
        first = linnum;
    }
    if (chrgot() == ',') {
        chrget();
        if (isdigit(chrgot())) {
            linget();
            CHK;
            old = linnum;
        }
        if (chrgot() == ',') {
            chrget();
            if (!isdigit(chrgot()))
                ERR(ERRSN);
            linget();
            CHK;
            inc = linnum;
        }
    }
    if (!terminator(chrgot()))
        ERR(ERRSN);
    if (inc == 0)
        ERR(ERRFC);

    bool found  = false;
    usize start = fndlin(old, found); // the first line >= old

    // RENUM cannot reorder the program, and cannot run past MAXLIN.
    if (start < prog.size()) {
        if (start > 0 && first <= prog[start - 1].num)
            ERR(ERRFC);
        if (u32(first) + u32(prog.size() - start - 1) * u32(inc) > MAXLIN)
            ERR(ERRFC);
    }

    Vec<u16> mapped;
    if (!reason(mapped.reserve(prog.size())))
        return;
    for (usize i = 0; i < prog.size(); i++)
        if (!reason(mapped.push(i < start ? prog[i].num : u16(first + (i - start) * inc))))
            return;

    // Rewritten texts are built beside the old ones: a reference resolves
    // through fndlin over prog, which must still hold the old numbers.
    Vec<Vec<u8>> texts;
    if (!reason(texts.reserve(prog.size())))
        return;

    for (usize i = 0; i < prog.size(); i++) {
        const Vec<u8> &src = prog[i].text;
        Vec<u8> dst;
        usize k = 0;

        while (k < src.size()) {
            u8 c = src[k];

            if (c == '"') { // a literal, closing quote included
                if (!reason(dst.push(src[k++])))
                    return;
                while (k < src.size())
                    if (!reason(dst.push(src[k])))
                        return;
                    else if (src[k++] == '"')
                        break;
                continue;
            }
            if (c == REMTK) { // the tail is verbatim, ':' and '"' included
                while (k < src.size())
                    if (!reason(dst.push(src[k++])))
                        return;
                break;
            }
            if (c == DATATK) { // verbatim to an unquoted ':', as DATAN scans it
                bool quoted = false;
                if (!reason(dst.push(src[k++])))
                    return;
                while (k < src.size() && (quoted || src[k] != ':')) {
                    if (src[k] == '"')
                        quoted = !quoted;
                    if (!reason(dst.push(src[k++])))
                        return;
                }
                continue;
            }

            bool list = takes_list(c);
            bool one  = takes_one(c);
            if (c == GOTK) { // GO TO: the tokenizer cannot match across the space
                if (!reason(dst.push(src[k++])))
                    return;
                while (k < src.size() && src[k] == ' ')
                    if (!reason(dst.push(src[k++])))
                        return;
                if (k == src.size() || src[k] != TOTK)
                    continue;
                one = true;
            } else if (!list && !one) {
                if (!reason(dst.push(src[k++])))
                    return;
                continue;
            }

            if (!reason(dst.push(src[k++]))) // the token itself
                return;

            for (;;) {
                while (k < src.size() && src[k] == ' ')
                    if (!reason(dst.push(src[k++])))
                        return;
                if (k == src.size() || !isdigit(src[k]))
                    break;

                usize at = k;
                u32 ref  = 0;
                while (k < src.size() && isdigit(src[k])) {
                    if (ref <= MAXLIN) // clamped, not wrapped: it stays a miss
                        ref = ref * 10 + u32(src[k] - '0');
                    k++;
                }

                bool hit = false;
                usize j  = ref <= MAXLIN ? fndlin(u16(ref), hit) : 0;
                if (hit) {
                    if (!reason(put_num(dst, mapped[j])))
                        return;
                } else {
                    // Microsoft renumbers the rest anyway and only reports it.
                    while (at < k)
                        if (!reason(dst.push(src[at++])))
                            return;
                    outstr("Undefined line ");
                    linprt(ref);
                    outstr(" in ");
                    linprt(mapped[i]);
                    crdo();
                    CHK;
                }

                if (!list)
                    break;
                while (k < src.size() && src[k] == ' ')
                    if (!reason(dst.push(src[k++])))
                        return;
                if (k == src.size() || src[k] != ',')
                    break;
                if (!reason(dst.push(src[k++])))
                    return;
            }
        }
        if (!reason(texts.push(static_cast<Vec<u8> &&>(dst))))
            return;
    }

    for (usize i = 0; i < prog.size(); i++) {
        prog[i].num  = mapped[i];
        prog[i].text = static_cast<Vec<u8> &&>(texts[i]);
    }

    // Editing the program clears the variables, and every saved TextPos names
    // a line whose text has just moved under it.
    runc();
    CHK;
    halt_ = Halt::Ready; // like LIST: back to the prompt, never onward
}

void Interp::stmt_renum()
{
    renum();
}
