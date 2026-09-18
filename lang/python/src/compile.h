// The compiler: a parse tree to a code object.
#pragma once

#include "code.h"
#include "parse.h"

// The module's code object, or Nil with the error pending. Pin it before the
// next allocation. Eval mode wants a tree of one expression statement, and
// the code returns what it is worth.
Value py_compile(const Ast &ast, Str filename, CompileMode mode = CompileMode::Exec);

// The optimization level: 0, 1 for -O (asserts go and __debug__ is False) or
// 2 for -OO (docstrings go too). compile_level_for is compile()'s optimize
// argument for the one call it covers; -1 hands back the default.
void compile_set_optimize(u32 level);
u32 compile_optimize();
void compile_level_for(i32 level);

// Parse, compile and list, for --dis. False leaves the error pending.
bool py_dis(Str source, Str filename, String &out);
