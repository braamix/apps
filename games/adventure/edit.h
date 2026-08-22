// The line discipline, in userland: history, cursor movement and kill-word over
// Echo, which paints a run per colour and says where that left off.
//
// Lifted from braam-core's src/cmd/sh/edit.{h,cpp} and cut to one prompt run.
// The keys are the shell's, so they are the keys the player already knows.
// Upstream read with getchar(); Braam's cooked mode echoes but knows no erase
// key, so a correctable line means claiming the keyboard.
#pragma once

#include "kernel/string.h"
#include "kernel/vec.h"
#include "proc/io.h"

enum class LineEnd : u8 {
    Enter,     // committed with Return
    Interrupt, // abandoned with ^C
};

struct Line {
    String text;
    LineEnd how = LineEnd::Enter;
};

struct LineEditor {
    // The oldest entries are dropped past this.
    static constexpr usize HISTORY_MAX = 32;

    // Draws the prompt, edits until Return or ^C, and ends the row — what the
    // game writes next starts on a line of its own.
    //
    // The keyboard must be claimed already.
    Task<Result<Line>> read_line(Str prompt);

private:
    Task<Result<void>> redraw();
    Task<Result<void>> anchor(Str prompt);
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
    u32 cols_ = 80;         // the last geometry the kernel reported
    u32 rows_ = 24;
};
