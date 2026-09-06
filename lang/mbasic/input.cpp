// INPUT, INPUT#, READ and GET.
//
// m6502.asm's INPUT AND READ CODE section; 05-statements.md §3.
//
// All three statements share one loop, selected by INPFLG -- whose three
// values, 0, 64 and 152, were chosen so that one byte answers all three
// questions with single-bit branches (BEQ, BVS, BMI). The trick that makes the
// loop work is that TXTPTR is temporarily repointed at the DATA, so the same
// FIN and STRLT2 that parse program text parse input.
//
// This is the one place a suspension can happen MIDWAY through a statement:
// GETNTH's JSR QINLIN (m6502.asm:2994) is the "?? " continuation prompt, and
// it is reached with some of the variable list already filled. Upstream got
// away with holding the rest of its state in page zero and the 6502 program
// counter, because QINLIN never returned until it had a line. A read that
// unwinds to a driver has to name every field, which is what InputState is.
//
// Nothing is replayed on resume: the resume point is inp.varlist, a position
// in the VARIABLE LIST, so variables already assigned are behind it. That is
// distinct from ?REDO FROM START, which deliberately restarts the whole
// statement from OLDTXT.
#include "mbasic.h"

// ============================================================== helpers

// FIN over a bounded string rather than over program text. Upstream pointed
// TXTPTR straight at the data; the same thing here, through the direct-line
// buffer, saved and put back.
f64 Interp::fin_str(Str s, usize &used)
{
    Vec<u8> save  = static_cast<Vec<u8> &&>(dirbuf);
    TextPos savep = txtptr;

    dirbuf.clear();
    for (usize i = 0; i < s.size(); i++)
        if (!reason(dirbuf.push(u8(s[i]))))
            break;
    reason(dirbuf.push(0));

    txtptr = TextPos{ DIRECT, 0 };
    f64 v  = fin();
    used   = txtptr.off;

    txtptr = savep;
    dirbuf = static_cast<Vec<u8> &&>(save);
    return v;
}

// ============================================================== the sources

// The buffer the loop is consuming and where it has got to: BUF/INPPTR for
// INPUT and GET, the DATA statement's text for READ.
Str Interp::in_src() const
{
    return inp.inpflg == INPFLG_READ ? inp.dbuf.str() : inp.buf.str();
}

usize &Interp::in_pos()
{
    return inp.inpflg == INPFLG_READ ? inp.dpos : inp.inpptr;
}

// DATLOP (m6502.asm:3044-3072). Uses DATAN -- the same scanner DATA itself
// uses -- to skip statements; at end of line it follows the link, and a zero
// link raises ?OUT OF DATA. Each line's number is copied into DATLIN as it is
// crossed, so a badly formed value is reported against the offending DATA
// line and not against the READ.
void Interp::datlop()
{
    TextPos save = txtptr;
    txtptr       = datptr;

    for (;;) {
        u8 c = (txtptr.off == AT_LINE_START) ? 0 : peek_at(txtptr);
        if (c == 0) {
            usize next = usize(txtptr.line) + (txtptr.off == AT_LINE_START ? 0 : 1);
            if (txtptr.direct() || next >= prog.size()) {
                txtptr = save;
                ERR(ERROD);
            }
            txtptr.line = i32(next);
            txtptr.off  = AT_LINE_START;
            datlin      = prog[next].num;
        }
        chrget();

        if (chrgot() == DATATK) {
            chrget();
            TextPos from = txtptr;
            datan();
            const Vec<u8> &t = prog[usize(from.line)].text;
            inp.dbuf.clear();
            for (u32 i = from.off; i < txtptr.off && i < t.size(); i++)
                if (!reason(inp.dbuf.push(char(t[i]))))
                    break;
            inp.dpos = 0;
            datptr   = txtptr;
            txtptr   = save;
            return;
        }
        datan();
    }
}

// ============================================================== the loop

// INLOOP (m6502.asm:2968-2999).
void Interp::inloop()
{
    for (;;) {
        inp.varlist = txtptr; // <-- the resume point, written before PTRGET
        inp.forpnt  = ptrget();
        CHK;
        u8 vt = valtyp;

        // The source is exhausted. READ finds the next DATA statement, and
        // comes back HERE rather than to the top: FORPNT is already resolved
        // and TXTPTR has moved past the variable's name. Upstream had the same
        // shape for the same reason -- DATLOP works on DATPTR and never
        // touches TXTPTR, so it resumes at DATBK, in the middle of the value.
        Str src;
        usize pos;
        for (;;) {
            src = in_src();
            pos = in_pos();
            while (pos < src.size() && src[pos] == ' ')
                pos++;
            if (pos < src.size())
                break;
            if (inp.inpflg == INPFLG_READ) {
                in_pos() = pos;
                datlop();
                CHK;
                continue;
            }
            // GET reads one character; INPUT prints another '?' and asks again.
            if (inp.inpflg == INPFLG_GET)
                SUSPEND(suspend_char(Resume::Get));
            if (inp.chan == 0)
                SUSPEND(suspend_line("?? ", 0, Resume::Input)); // GETNTH
            SUSPEND(suspend_line("", inp.chan, Resume::Input));
        }

        // String values: if the first character is a quote the delimiters
        // become quote/quote and the value starts after it; otherwise they are
        // ':' and ',' and the value starts where it is.
        if (vt == VSTR) {
            String v;
            if (src[pos] == '"') {
                pos++;
                while (pos < src.size() && src[pos] != '"')
                    if (!reason(v.push(src[pos++])))
                        return;
                if (pos < src.size())
                    pos++;
            } else {
                while (pos < src.size() && src[pos] != ',' && src[pos] != ':')
                    if (!reason(v.push(src[pos++])))
                        return;
            }
            reason(var_put_str(inp.forpnt, v.str()));
        } else {
            usize used = 0;
            f64 n      = fin_str(src.substr(pos), used);
            CHK;
            if (used == 0) {
                in_pos() = pos;
                trmnok();
                return;
            }
            pos += used;
            var_put_num(inp.forpnt, inp.forpnt.intflg ? f64(i16(n)) : n);
        }
        CHK;

        // STRDN2: the value must be followed by NUL or ','.
        while (pos < src.size() && src[pos] == ' ')
            pos++;
        if (pos < src.size() && src[pos] != ',' && src[pos] != ':') {
            in_pos() = pos;
            trmnok();
            return;
        }
        if (pos < src.size())
            pos++;
        in_pos() = pos;

        if (chrgot() != ',') { // the variable list is finished
            varend();
            return;
        }
        chrget();
    }
}

// TRMNOK (m6502.asm:2858-2878): a badly formed value.
void Interp::trmnok()
{
    switch (inp.inpflg) {
    case INPFLG_READ:
        // Report the syntax error against the offending DATA line, not the READ.
        curlin = datlin;
        error(ERRSN);
        return;
    case INPFLG_GET:
        curlin = DIRECT_LINE; // force it to look direct
        error(ERRSN);
        return;
    default:
        // The ENTIRE INPUT statement re-executes, including any variables it
        // has already assigned. That is upstream's documented behaviour, and
        // it needs no special case: NEWSTT re-dispatches INPUT from OLDTXT.
        outstr("?REDO FROM START\r\n");
        if (oldtxt.valid) {
            txtptr = oldtxt.txt;
            curlin = oldtxt.lin;
        }
        return;
    }
}

// VAREND (m6502.asm:3073-3086): the variable list is finished but the data is
// not.
void Interp::varend()
{
    Str src   = in_src();
    usize pos = in_pos();
    while (pos < src.size() && src[pos] == ' ')
        pos++;
    if (pos >= src.size())
        return;
    if (inp.inpflg == INPFLG_READ)
        return; // READ just leaves DATPTR where it is
    outstr("?EXTRA IGNORED\r\n");
}

// ============================================================== INPUT

void Interp::stmt_input()
{
    cntwfl = 0; // "BE TALKATIVE"

    if (chrgot() == '"') { // INPUT "PROMPT";A
        strtxt();
        CHK;
        String p = static_cast<String &&>(fac.s);
        fac      = Val{};
        if (chrgot() != ';')
            ERR(ERRSN);
        chrget();
        strprt(p.str());
    }
    // Upstream's ERRDIR: INPUT is illegal in direct mode.
    if (curlin == DIRECT_LINE)
        ERR(ERRID);

    inp.inpflg = INPFLG_INPUT;
    inp.chan   = 0;
    inp.buf.clear();
    inp.inpptr  = 0;
    inp.varlist = txtptr;
    SUSPEND(suspend_line("? ", 0, Resume::Input));
}

// INPUT# (EXTIO). Two fields differ from INPUT: the channel, and the empty
// prompt -- QINLIN skips the '?' when CHANNL is not zero.
void Interp::stmt_inputn()
{
    u8 n = getbyt();
    CHK;
    if (!chan_find(n))
        ERR(ERRFD);
    if (chrgot() != ',' && chrgot() != ';')
        ERR(ERRSN);
    chrget();

    inp.inpflg = INPFLG_INPUT;
    inp.chan   = n;
    inp.buf.clear();
    inp.inpptr  = 0;
    inp.varlist = txtptr;
    SUSPEND(suspend_line("", n, Resume::Input));
}

void Interp::input_resume()
{
    switch (in_end) {
    case InEnd::Interrupt:
        brkflg = true; // ^C at the prompt
        return;
    case InEnd::Eof:
        halt_ = Halt::Quit;
        return;
    case InEnd::Error:
        error(ERRFD);
        return;
    case InEnd::Line:
        break;
    }

    // A blank line typed to INPUT is a silent, continuable STOP
    // (m6502.asm:2939-2940): STPEND with carry clear, so no BREAK is printed
    // but the program stops and CONT resumes it.
    if (in_line.empty() && inp.chan == 0) {
        stpend(false);
        return;
    }

    inp.buf.clear();
    if (!reason(inp.buf.assign(in_line.str())))
        return;
    inp.inpptr = 0;
    txtptr     = inp.varlist;
    inloop();
}

// ============================================================== READ

void Interp::stmt_read()
{
    inp.inpflg = INPFLG_READ;
    inp.chan   = 0;
    inloop();
}

// ============================================================== GET
//
// m6502.asm:2879-2900. Illegal in direct mode. Fetches exactly one character
// into BUF, and in the value loop skips the quote logic entirely and uses zero
// terminators, so exactly one character is consumed.
void Interp::stmt_get()
{
    if (curlin == DIRECT_LINE)
        ERR(ERRID);
    inp.inpflg = INPFLG_GET;
    inp.chan   = 0;
    inp.buf.clear();
    inp.inpptr  = 0;
    inp.varlist = txtptr;
    SUSPEND(suspend_char(Resume::Get));
}

void Interp::get_resume()
{
    if (in_end == InEnd::Interrupt) {
        brkflg = true;
        return;
    }
    if (in_end == InEnd::Eof) {
        halt_ = Halt::Quit;
        return;
    }
    inp.buf.clear();
    if (in_char)
        reason(inp.buf.push(char(in_char)));
    inp.inpptr = 0;
    txtptr     = inp.varlist;

    // GET assigns whatever arrived -- the null string for a key with no
    // character -- and consumes exactly one variable, so it does not loop.
    inp.forpnt = ptrget();
    CHK;
    if (valtyp == VSTR) {
        reason(var_put_str(inp.forpnt, inp.buf.str()));
    } else {
        usize used = 0;
        f64 n      = inp.buf.empty() ? 0 : fin_str(inp.buf.str(), used);
        CHK;
        var_put_num(inp.forpnt, inp.forpnt.intflg ? f64(i16(n)) : n);
    }
    inp.inpflg = INPFLG_INPUT;
}
