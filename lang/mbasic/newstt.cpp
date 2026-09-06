// The statement fetcher, the dispatcher, the error handler, and the driver's
// half of the suspension protocol.
//
// m6502.asm's NEW STATEMENT FETCHER and ERROR HANDLER sections;
// 04-interpreter-loop.md.
#include "mbasic.h"

// ============================================================== step
//
// The one entry point, and the one place the answer-shaped state is consumed.
// Upstream had a program counter for `resume_`; a read that unwinds the native
// stack has to make it data.
Reason Interp::step()
{
    Resume back = resume_;
    resume_     = Resume::Newstt;
    switch (back) {
    case Resume::Main:
        main_resume();
        break;
    case Resume::Input:
        input_resume();
        break;
    case Resume::Get:
        get_resume();
        break;
    case Resume::File:
        file_resume();
        break;
    case Resume::Newstt:
        break;
    }

    for (;;) {
        if (!pending())
            newstt();

        Halt h = halt_;
        halt_  = Halt::None;
        switch (h) {
        case Halt::Suspend:
            return want_;
        case Halt::Quit:
            return Reason::Done;
        case Halt::Error:
            error_print();
            if (script) {
                status = 1; // sticky
                crdo();     // no Ok to end the line for us
            }
            break;
        case Halt::Break:
            break_print();
            if (script) {
                status = 130;
                crdo();
            }
            break;
        case Halt::None:
        case Halt::Ready:
            break;
        }
        // A script ends when its implicit RUN does, or at a ^C; before that,
        // round again for the next line of the file.
        if (script && (running || h == Halt::Break))
            return Reason::Done;
        ready();
        halt_ = Halt::None;
        return want_;
    }
}

// ============================================================== NEWSTT
//
// m6502.asm:2131-2192. The source states the contract at 2132-2137:
//
//   BACK HERE FOR NEW STATEMENT. CHARACTER POINTED TO BY TXTPTR IS ":" OR
//   END-OF-LINE. THE ADDRESS OF THIS LOC IS LEFT ON THE STACK WHEN A STATEMENT
//   IS EXECUTED SO THAT IT CAN MERELY DO A RTS WHEN IT IS DONE.
//
// The return address is gone with the 6502 stack, and with it the rule that a
// statement creating a semi-permanent frame had to discard it first: that
// invariant existed only so FNDFOR's scan geometry worked, and the scan is
// over a Vec now. FOR, NEXT, LIST, STOP/END and RUN n all return normally.
//
// The burst is new. A signal is delivered where a process parks and this loop
// parks nowhere, so without it `10 GOTO 10` is a compute loop no ^C can reach
// -- and a driver that awaited per statement would grow the native stack until
// it trapped. The count is per statement, which is where ISCNTC already was.
// Every variable the loop uses is a field, so re-entry is simply calling it.
void Interp::newstt()
{
    for (;;) {
        iscntc();
        CHK;

        // Per statement, and only in program mode: this is the anchor for
        // "restart this statement", which is what makes CONT and INPUT's
        // ?REDO FROM START work. Note the test is on TXTPTR, not on CURLIN --
        // the interpreter decides "am I running a typed-in line?" two
        // different ways in two different places and a port must keep both.
        if (!txtptr.direct()) {
            oldtxt.txt   = txtptr;
            oldtxt.lin   = curlin;
            oldtxt.valid = true;
        }

        u8 c = (txtptr.off == AT_LINE_START) ? 0 : peek_at(txtptr);
        if (c != 0) {
            // MORSTS: a byte that is neither NUL nor ':' where a terminator
            // was expected is a syntax error. Because NEWSTT re-checks after
            // every handler returns, a statement that reads its arguments and
            // simply returns without consuming the rest of its text raises one
            // automatically -- which NEW, CLEAR, CONT and RETURN all use
            // deliberately to mean "only act if the syntax was perfect".
            if (c != ':')
                ERR(ERRSN);
            gone();
            CHK;
        } else {
            // End of line: cross into the next one. AT_LINE_START means we
            // are already before the line named, which is where GOTO and
            // STXTPT leave us.
            usize next = usize(txtptr.line) + (txtptr.off == AT_LINE_START ? 0 : 1);
            if (txtptr.direct() || next >= prog.size()) {
                halt_ = Halt::Ready; // ENDCON with carry clear: no BREAK
                return;
            }
            curlin      = prog[next].num;
            txtptr.line = i32(next);
            txtptr.off  = AT_LINE_START;
            gone();
            CHK;
        }

        if (--burst_ == 0) {
            burst_ = STMT_BURST;
            want_  = Reason::Yield;
            halt_  = Halt::Suspend;
            return;
        }
    }
}

// GONE: CHRGET the first character after the statement name, then dispatch.
void Interp::gone()
{
    chrget();
    gone3();
}

void Interp::gone3()
{
    u8 c = chrgot();
    if (terminator(c)) // ISCRTS: nothing on this statement
        return;
    gone2(c);
}

// GONE2 (m6502.asm:2172-2181).
void Interp::gone2(u8 c)
{
    // Any byte below ENDTK -- a letter, a digit, anything that is not a
    // reserved word -- falls through to LET. So A=1 and LET A=1 take the same
    // path, and a stray character produces a LET-shaped syntax error rather
    // than a distinct one. The character is not consumed: LET's PTRGET wants
    // to see it.
    if (c < ENDTK) {
        stmt_let();
        return;
    }

    if (c > SCRATK) {
        // SNERRX. GO is a reserved word with a token but no STMDSP entry, so
        // it lands here; this arm checks for it, consumes the following TO and
        // enters GOTO. It exists purely because the tokenizer cannot match
        // across the space (03-tokenizer-editor.md §2.3).
        if (c != GOTK)
            ERR(ERRSN);
        chrget();
        synchr(TOTK);
        CHK;
        stmt_goto();
        return;
    }

    chrget(); // upstream's JMP CHRGET: handlers see the character after the token
    (this->*STMDSP[c - ENDTK])();
}

// ============================================================== ISCNTC
//
// m6502.asm:2205-2226, called once per statement from NEWSTT and once per line
// from LIST. Upstream's Apple version was broken: ISCCAP called INCHR, which
// does ANDI 127, and then compared the resulting 3 against the unstripped 131,
// so the comparison always failed, END's CLC ran and STOPC's BNE returned. The
// ^C was consumed from the keyboard but never interrupted (13-porting-notes.md
// §3.1). Fixed here, and structurally -- there is no comparison left to get
// wrong. The driver sets brkflg from sig_take(SIG_INT).
void Interp::iscntc()
{
    if (!brkflg)
        return;
    brkflg = false;
    stpend(true); // upstream's fall-through into STOP with carry set
}

// ============================================================== errors
//
// The flag goes up LAST, after everything that writes to `out`, because outdo()
// is poisoned on it (err.h). The message is formatted by error_print() from
// step(), once the unwind has finished -- upstream's ERRCRD/TYPERR/ERRFIN.
void Interp::error(ErrCode code)
{
    if (pending()) // the first error wins, as upstream's did
        return;
    cntwfl   = 0; // LSR CNTWFL: force output back on
    chanl    = 0; // EXTIO: close the current channel
    errcode_ = code;
    halt_    = Halt::Error;
}

void Interp::error_print()
{
    crdo();
    outdo('?');
    outstr(ERRTAB[errcode_]);
    outstr(" error");

    // STKINI is what makes an error unrecoverable: it discards every FOR and
    // GOSUB frame and zeroes the byte that lets CONT work. Variables and the
    // program survive; all control-flow context does not.
    stkini();

    if (curlin != DIRECT_LINE) {
        outstr(" in ");
        linprt(curlin);
    }
}

// ERRFIN entered with BRKTXT instead of ERR (m6502.asm:2242-2247), which is
// why BREAK IN 10 and ?SYNTAX ERROR IN 10 come out of one routine. Note it
// does NOT call STKINI: that is precisely why CONT works after a STOP or a ^C
// but not after an error.
void Interp::break_print()
{
    outstr("\r\nBreak");
    if (curlin != DIRECT_LINE) {
        outstr(" in ");
        linprt(curlin);
    }
}

// STOP, END and ^C all land here, each saving the pointer to its own
// terminating character so that CONT resumes after the STOP.
void Interp::stpend(bool print_break)
{
    if (curlin != DIRECT_LINE) {
        oldtxt.txt   = txtptr;
        oldtxt.lin   = curlin;
        oldtxt.valid = true;
    }
    cntwfl = 0;
    halt_  = print_break ? Halt::Break : Halt::Ready;
}

// READY (m6502.asm:2194-2197), then MAIN. A script prints no prompt.
void Interp::ready()
{
    cntwfl = 0;
    if (!script)
        outstr("\r\nOk\r\n");
    curlin = DIRECT_LINE;
    suspend_line("", 0, Resume::Main);
}

void Interp::main_resume()
{
    switch (in_end) {
    case InEnd::Eof:
        // The file ran out: run what it stored, unless it ran itself.
        if (script && !ran_ && !running && !prog.empty()) {
            running = true;
            runc();
            return;
        }
        halt_ = Halt::Quit;
        return;
    case InEnd::Error:
        halt_ = Halt::Quit;
        return;
    case InEnd::Interrupt:
        // ^C at the prompt abandons the line and asks again; upstream's INLIN
        // had no way to say this, because it could not fail.
        outstr("\r\n");
        suspend_line("", 0, Resume::Main);
        return;
    case InEnd::Line:
        break;
    }
    main_line();
}

// ============================================================== bookkeeping

// STKINI (m6502.asm:1949-1962). Upstream reset S -- discarding every FOR,
// GOSUB and deferred-expression frame at once -- and then did four more
// things. The stack reset is gone: the native stack unwinds itself one CHK at
// a time, and the expression frames were frmevl() locals and went with it.
void Interp::stkini()
{
    frames.clear();
    ntemp        = 0;
    oldtxt.valid = false; // OLDTXT+1 = 0, so CONT refuses
    subflg       = 0;
}

// CLEARC (m6502.asm:1934-1941). Upstream discarded all variables and arrays by
// pointing ARYTAB and STREND back at VARTAB -- nothing was erased, it was just
// no longer reachable.
void Interp::clearc()
{
    vars.clear();
    arrays.clear();
    datptr = TextPos{ 0, AT_LINE_START }; // RESTOR
    datlin = 0;
    stkini();
}

void Interp::runc()
{
    stxtpt();
    clearc();
}

// GETSTK (m6502.asm:1472-1480). Upstream's one budget -- the 256-byte page --
// covered three unlike things: expression nesting, the frame count, and
// FBUFFR's safety below the stack. The third goes away with the page; the
// other two separate. NUMLEV = 23 is the guaranteed nesting depth, raised from
// 19 on 1978-02-25, and exceeding it is ?OM and not a crash.
bool Interp::getstk(u8 n)
{
    bool ok = (n == 1) ? numlev < NUMLEV : frames.size() < STKLIM;
    if (!ok)
        error(ERROM);
    return ok;
}

// ============================================================== suspension

void Interp::suspend_line(Str prompt, u8 chan, Resume back)
{
    outstr(prompt); // upstream wrote the '?' before it read
    req.chan   = chan;
    req.main   = back == Resume::Main; // a script feeds these from the file
    req.prompt = prompt;
    want_      = Reason::NeedLine;
    resume_    = back;
    halt_      = Halt::Suspend;
}

void Interp::suspend_char(Resume back)
{
    req.chan = 0;
    want_    = Reason::NeedChar;
    resume_  = back;
    halt_    = Halt::Suspend;
}

void Interp::suspend_file(Resume back)
{
    want_   = Reason::NeedFile;
    resume_ = back;
    halt_   = Halt::Suspend;
}
