// The compiler: a parse tree to a code object.
#pragma once

#include "code.h"
#include "parse.h"

// The module's code object, or Nil with the error pending. Pin it before the
// next allocation.
Value py_compile(const Ast &ast, Str filename);

// Parse, compile and list, for --dis. False leaves the error pending.
bool py_dis(Str source, Str filename, String &out);
