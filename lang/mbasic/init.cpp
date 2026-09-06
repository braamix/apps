// INIT: the banner, and what upstream's two questions used to set.
//
// m6502.asm:6670-6948; 11-io-init.md §3.
//
// Most of upstream's INIT was about a machine that is not here: patching the
// page-zero JMP vectors, copying CHRGET into RAM, publishing ADRAYI and
// ADRGAY for machine-language callers, and probing memory by writing 85 and
// 170 and reading them back until an address failed. What survives is what its
// two questions set, without the questions.
#include "mbasic.h"

namespace {

// The budget FRE reports against and ?OM fires on. Upstream probed the machine
// for it and then asked; there is no machine to probe, so it is fixed.
constexpr u32 MEMSIZ_DEFAULT = 65535;

} // namespace

// Upstream asked MEMORY SIZE and TERMINAL WIDTH here. Neither has an answer
// worth a question now: the memory is the kernel's, and the width is what
// tty_of reports.
Reason Interp::start(u32 cols, bool interactive)
{
    (void)interactive;
    memsiz = MEMSIZ_DEFAULT;
    // LINWID was a RAM byte and not a constant. Zero means no automatic wrap,
    // which is what a pipe wants: wrapping a redirected transcript at some
    // terminal's width would be a hard thing to explain. The comma zones still
    // need a width, since a PRINT with commas has to line up somewhere, and 80
    // is the assumption where there is no terminal to ask.
    linwid   = cols;
    u32 wide = cols ? cols : LINLEN;
    ncmwid   = wide - wide % CLMWID;
    rndx     = 0.8116351573262364; // RNDX, the initial random number

    if (script) { // no banner, no Ok: the file's output is the program's
        scrtch();
        suspend_line("", 0, Resume::Main);
        halt_ = Halt::None;
        return want_;
    }

    banner();
    scrtch();
    ready();
    halt_ = Halt::None;
    return want_;
}

// m6502.asm:6873-6907 and 6909-6948. Upstream printed the free-memory line
// ahead of the sign-on; the memory here is the kernel's, not the machine's, so
// the number said nothing and is dropped.
void Interp::banner()
{
    crdo();
    outstr("Braam BASIC v1.1\r\n");
    outstr("Copyright 1978 Microsoft\r\n");
}
