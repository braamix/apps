// Archives: the GNU and BSD ar formats. Every Str and Bytes is a view into
// the archive's own bytes.
#pragma once

#include "kernel/span.h"
#include "kernel/str.h"
#include "kernel/vec.h"
#include "out.h"

// An archive member: its name, and its bytes inside the archive's.
struct Member {
    Str name;
    Bytes data;
    u32 file_off;
};

bool is_archive(Bytes file);

// Lists the members of an archive, skipping its symbol and name tables.
bool read_archive(Str name, Bytes file, Vec<Member> &members, Out &err);
