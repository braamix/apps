// --dump: what `llvm-objdump -t -r` prints for an object or an archive,
// byte for byte, so that it can be held against it.
#pragma once

#include "input.h"
#include "out.h"

void dump_object(const Object &o, Out &out);

// An object or every member of an archive. False on a read failure, with the
// message in `err`; what was dumped before it stays in `out`.
bool dump_file(Str name, Bytes file, Out &out, Out &err);
