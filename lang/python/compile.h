// The compiler: a parse tree to a code object.
#pragma once

#include "code.h"
#include "parse.h"

// The module's code object, or Nil with the error pending. Pin it before the
// next allocation. Eval mode wants a tree of one expression statement, and
// the code returns what it is worth.
Value py_compile(const Ast &ast, Str filename, CompileMode mode = CompileMode::Exec);

// Parse, compile and list, for --dis. False leaves the error pending.
bool py_dis(Str source, Str filename, String &out);
