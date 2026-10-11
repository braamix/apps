// An Asm into a relocatable object, laid out as llvm's WasmObjectWriter
// lays it out: the same sections in the same order, every size a padded
// LEB, and every relocatable field holding llvm's provisional value.
#pragma once

#include "asm.h"
#include "diag.h"
#include "kernel/vec.h"

namespace as {

// False with a message in `diag` for what llvm would refuse as a fatal
// error; `out` is then not to be written.
bool write_object(Asm &a, Vec<u8> &out, Diag &diag);

} // namespace as
