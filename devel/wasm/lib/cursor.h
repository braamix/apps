// Bounds-checked reads over a section's bytes.
#pragma once

#include "kernel/span.h"
#include "kernel/str.h"
#include "kernel/types.h"

// A cursor over one section's bytes. Failure is sticky: the first one is
// kept, and every later read returns zero, so a record is checked once at
// its end rather than after every field.
struct Cursor {
    explicit Cursor(Bytes b, usize base = 0) : b_(b), base_(base) {}

    usize at() const { return at_; }

    usize left() const { return b_.size() - at_; }

    bool ok() const { return !failed_; }

    bool done() const { return at_ >= b_.size(); }

    Str why() const { return why_; }

    // Where the failure was, counted from the start of the section.
    usize where() const { return base_ + where_; }

    void fail(Str why, usize at);

    void fail(Str why) { fail(why, at_); }

    u8 byte();
    u32 u32le();
    u32 uleb();
    i32 sleb();
    i64 sleb64();
    u64 uleb64();
    Bytes take(usize n);
    Str name();

    // A vector's length, which cannot exceed the bytes left.
    u32 count();

private:
    Bytes b_;
    usize base_;
    usize at_    = 0;
    usize where_ = 0;
    Str why_;
    bool failed_ = false;
};
