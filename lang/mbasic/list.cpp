// LIST -- the exact inverse of CRUNCH.
//
// m6502.asm:1973-2062; 03-tokenizer-editor.md §4.
#include "mbasic.h"

// DETOK. Upstream rescanned RESLST from the beginning to expand a token; an
// indexed table does not.
//
// It needs CRUNCH's regions, because a token and a UTF-8 byte are both >= 0x80:
// inside a literal, a DATA item or a REM tail the bytes are text, and expanding
// one there turns "café" into "caflenstep". Outside them CRUNCH stores no byte
// >= 0x80 at all, so there it is always a token. SAVE shares this (file.cpp).
bool Interp::detok(const Vec<u8> &src, String &dst)
{
    // Everything but the terminating NUL.
    usize n = src.size() ? src.size() - 1 : 0;
    usize i = 0;

    while (i < n) {
        u8 c = src[i];

        if (c == '"') { // a literal, closing quote included
            if (!reason(dst.push('"')))
                return false;
            i++;
            while (i < n) {
                if (!reason(dst.push(char(src[i]))))
                    return false;
                if (src[i++] == '"')
                    break;
            }
            continue;
        }
        if (c == REMTK) { // the tail is verbatim, ':' and '"' included
            if (!reason(dst.append(RESLST[REMTK - ENDTK])))
                return false;
            for (i++; i < n; i++)
                if (!reason(dst.push(char(src[i]))))
                    return false;
            break;
        }
        if (c == DATATK) { // verbatim to an unquoted ':', as CRUNCH stored it
            bool quoted = false;
            if (!reason(dst.append(RESLST[DATATK - ENDTK])))
                return false;
            for (i++; i < n && (quoted || src[i] != ':'); i++) {
                if (src[i] == '"')
                    quoted = !quoted;
                if (!reason(dst.push(char(src[i]))))
                    return false;
            }
            continue;
        }

        i++;
        if (c < 0x80) {
            if (!reason(dst.push(char(c))))
                return false;
            continue;
        }
        usize w = usize(c - ENDTK);
        if (!reason(dst.append(w < RESLST_COUNT ? RESLST[w] : Str("?"))))
            return false;
    }
    return true;
}

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

        String text;
        if (!detok(l.text, text))
            return;
        outstr(text.str());
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
