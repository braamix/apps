// Reading objects and archives. Every read is bounds-checked; a failure
// leaves one message naming the file, the section and the offset.
#pragma once

#include "input.h"
#include "out.h"

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

// Parses `o.file` into `o`, whose `name` is already set. False on failure,
// with the message in `err`.
bool read_object(Object &o, Out &err);

bool is_archive(Bytes file);

// Lists the members of an archive, skipping its symbol and name tables.
bool read_archive(Str name, Bytes file, Vec<Member> &members, Out &err);
