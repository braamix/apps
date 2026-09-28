// C++ names as llvm::demangle() shows them, by LLVM's own Itanium demangler.
#pragma once

#include "kernel/str.h"
#include "kernel/string.h"

// The demangled name, or the name itself where it is not a mangled one.
void demangle(Str name, String &out);
