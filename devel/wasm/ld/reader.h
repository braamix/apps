// Reading objects and archives. Every read is bounds-checked; a failure
// leaves one message naming the file, the section and the offset.
#pragma once

#include "cursor.h"
#include "input.h"
#include "out.h"

// Parses `o.file` into `o`, whose `name` is already set. False on failure,
// with the message in `err`.
bool read_object(Object &o, Out &err);

bool is_archive(Bytes file);

// Lists the members of an archive, skipping its symbol and name tables.
bool read_archive(Str name, Bytes file, Vec<Member> &members, Out &err);
