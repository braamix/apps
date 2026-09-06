// INLIN: the line discipline, in userland.
//
// Lifted from games/adventure/edit.{h,cpp}, which lifted it from braam-core's
// src/cmd/sh/edit.{h,cpp}. The keys are the shell's, so they are the keys the
// user already knows.
//
// Upstream's own INLIN (m6502.asm:1673-1741) took backarrow as the character
// delete and '@' as the line delete, and on the Apple delegated the whole edit
// to the monitor's GETLN. Braam's cooked mode echoes but knows no erase key,
// so a correctable line means claiming the keyboard -- and delegating to the
// system's own editor is what the Apple build did too.
//
// One change from adventure's copy: the prompt is drawn by the interpreter,
// through OUTDO, so that TRMPOS is maintained and a redirected run sees it.
// The editor therefore anchors where the cursor already is rather than asking
// for a row of its own.
#pragma once

#include "kernel/string.h"
#include "kernel/vec.h"
#include "proc/io.h"

enum class LineEnd : u8 {
    Enter,     // committed with Return
    Interrupt, // abandoned with ^C
    Eof,       // ^D on an empty line
};

struct InLine {
    String text;
    LineEnd how = LineEnd::Enter;
};

struct LineEditor {
    // The oldest entries are dropped past this.
    static constexpr usize HISTORY_MAX = 32;

    // Edits until Return or ^C, and ends the row. The prompt is already on the
    // screen: the interpreter wrote it through OUTDO.
    //
    // The keyboard must be claimed already.
    Task<Result<InLine>> read_line();

private:
    Task<Result<void>> redraw();
    Task<Result<void>> anchor();
    bool set_text(Str utf8);
    bool set_text(const Vec<char32_t> &from);
    bool set_pending();
    bool text_of(const Vec<char32_t> &from, String &out) const;
    bool remember(Str s);
    usize word_start() const;

    Vec<char32_t> buf_;     // the line, one codepoint per cell
    Vec<char32_t> pending_; // the line being typed, parked by an Up
    Vec<String> history_;   // oldest first
    usize cur_     = 0;     // cursor index into buf_
    usize hist_    = 0;     // history_.size() means "the line being typed"
    usize painted_ = 0;     // cells the last redraw covered, so the tail erases
    u32 x0_ = 0, y0_ = 0;   // where buf_[0] draws
    u32 cols_    = 80;      // the last geometry the kernel reported
    u32 rows_    = 24;
    bool echoed_ = false; // the console already printed this ^C
};
