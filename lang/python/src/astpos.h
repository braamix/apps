// Where each node is in the source, as CPython's ast reports it.
//
// The parser records only the token a node is at. A position is the span of
// tokens the node was written as: that token widened over everything under
// it, then over the brackets that close inside it. A few kinds begin at a
// keyword the parser did not point at; those are named one by one.
#pragma once

#include "parse.h"

struct Pos {
    u32 line, col;   // 1-based line, 0-based byte column
    u32 eline, ecol; // one past the last byte
};

// Fills `ast.spans`. False leaves a MemoryError pending.
bool ast_spans(Ast &ast);

// The position of `node`, from the spans. Meaningless before ast_spans.
Pos ast_pos(const Ast &ast, u32 node);
