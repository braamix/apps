// `_ast`: the node classes, and the tree compile() answers for PyCF_ONLY_AST.
#pragma once

#include "obj.h"
#include "parse.h"

// The compile() flags ast.py and codeop name.
enum : i32 {
    PYCF_DONT_IMPLY_DEDENT      = 0x0200,
    PYCF_ONLY_AST               = 0x0400,
    PYCF_ALLOW_INCOMPLETE_INPUT = 0x4000,
    PYCF_TYPE_COMMENTS          = 0x1000,
    PYCF_ALLOW_TOP_LEVEL_AWAIT  = 0x2000,
    PYCF_OPTIMIZED_AST          = 0x8400,
};

// The tree of `ast`, for source already parsed. Nil leaves an error pending.
// `interactive` and `eval` pick Interactive and Expression over Module.
Value ast_tree(Ast &ast, bool interactive, bool eval);
