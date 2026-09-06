// INIT: the two questions, the free-byte line and the banner.
//
// m6502.asm:6670-6948; 11-io-init.md §3.
//
// Most of upstream's INIT was about a machine that is not here: patching the
// page-zero JMP vectors, copying CHRGET into RAM, publishing ADRAYI and
// ADRGAY for machine-language callers, and probing memory by writing 85 and
// 170 and reading them back until an address failed. What survives is the two
// questions and what they set.
#include "kernel/fmt.h"
#include "kernel/text.h"
#include "mbasic.h"

namespace {

// The default budget FRE reports against and ?OM fires on. Upstream probed
// for it; here it is a number the user may still lower, which is the part of
// MEMORY SIZE that was ever about BASIC rather than about the hardware.
constexpr u32 MEMSIZ_DEFAULT = 65535;

Option<u32> whole_number(Str s)
{
    u32 n    = 0;
    bool any = false;
    for (usize i = 0; i < s.size(); i++) {
        if (s[i] == ' ')
            continue;
        if (s[i] < '0' || s[i] > '9')
            return {};
        n   = n * 10 + u32(s[i] - '0');
        any = true;
        if (n > 0xFFFFFF)
            return {};
    }
    return any ? Option<u32>(n) : Option<u32>();
}

} // namespace

Reason Interp::start(u32 cols, bool interactive)
{
    memsiz = MEMSIZ_DEFAULT;
    // LINWID was a RAM byte and not a constant, because it is settable. Zero
    // means no automatic wrap, which is what a pipe wants: upstream had no
    // pipes, and wrapping a redirected transcript at a terminal's width would
    // be a hard thing to explain. The comma zones still come from a width,
    // since a PRINT with commas has to line up somewhere.
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

    if (!interactive) {
        banner();
        scrtch();
        ready();
        halt_ = Halt::None;
        return want_;
    }

    outstr("Memory size");
    suspend_line("? ", 0, Resume::MemSize);
    halt_ = Halt::None;
    return want_;
}

// m6502.asm:6759-6819. An empty answer keeps the default; anything that is not
// a number is refused by asking again, as upstream's LINGET-and-require-a-
// terminator did.
void Interp::memsize_resume()
{
    if (in_end != InEnd::Line) {
        halt_ = Halt::Quit;
        return;
    }
    Str s = in_line.str();

    // Answering "A" prints the authors' names and restarts INIT
    // (m6502.asm:6700-6702, 6913-6919). The 12 is a form feed, as written.
    if (s.size() == 1 && (s[0] == 'A' || s[0] == 'a')) {
        outstr("\r\n");
        outdo(12);
        outstr("Written by Weiland & Gates\r\n");
        outstr("Memory size");
        SUSPEND(suspend_line("? ", 0, Resume::MemSize));
    }

    if (!s.empty()) {
        Option<u32> n = whole_number(s);
        if (!n.has_value()) {
            outstr("Memory size");
            SUSPEND(suspend_line("? ", 0, Resume::MemSize));
        }
        memsiz = n.value();
    }

    outstr("Terminal width");
    SUSPEND(suspend_line("? ", 0, Resume::TtyWidth));
}

// m6502.asm:6820-6843. A value at or above 256, or below 16, is refused.
//
// Upstream had two different formulas for NCMWID -- the compile-time
// NCMPOS = ((LINLEN/CLMWID)-1)*CLMWID and the run-time
// NCMWID = LINWID - (LINWID mod CLMWID) -- so answering 40 gave different
// comma-zone behaviour from accepting the default 40 (13-porting-notes.md
// §3.7). One formula here, applied in both places.
void Interp::ttywidth_resume()
{
    if (in_end != InEnd::Line) {
        halt_ = Halt::Quit;
        return;
    }
    Str s = in_line.str();

    if (!s.empty()) {
        Option<u32> n = whole_number(s);
        if (!n.has_value() || n.value() >= 256 || n.value() < 16) {
            outstr("Terminal width");
            SUSPEND(suspend_line("? ", 0, Resume::TtyWidth));
        }
        linwid = n.value();
        ncmwid = linwid - linwid % CLMWID;
    }

    banner();
    scrtch();
    // READY, which step() reaches by way of the Ready halt.
    halt_ = Halt::Ready;
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
