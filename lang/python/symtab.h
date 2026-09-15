// The scope pass: which name is a local, a cell, a free variable or a global.
//
// One walk collects what every scope binds and uses, a second decides each
// name's binding and hands out the slot numbers the code object indexes by.
// Names are interned, so a symbol is found by comparing pointers.
#pragma once

#include "parse.h"

enum class Bind : u8 {
    Name,   // LoadName/StoreName: a module or a class body
    Local,  // LoadFast/StoreFast
    Cell,   // a local some nested scope captures, so it lives in a cell
    Free,   // captured from an enclosing scope, through the closure
    Global, // LoadGlobal/StoreGlobal
};

enum class ScopeKind : u8 { Module, Function, Lambda, Class, Comprehension };

enum : u8 {
    SF_PARAM    = 1 << 0,
    SF_ASSIGN   = 1 << 1,
    SF_USE      = 1 << 2,
    SF_GLOBAL   = 1 << 3, // declared `global`
    SF_NONLOCAL = 1 << 4, // declared `nonlocal`
};

struct Sym {
    StrObj *name = nullptr;
    u8 flags     = 0;
    Bind bind    = Bind::Name;
    u32 param    = 0; // 1-based position among the parameters, 0 for a local
    u32 slot     = 0; // into the scope's varnames, cellvars or freevars
    u32 line     = 0; // where it was first seen, for a complaint about it
    u32 col      = 0;
};

struct Scope {
    ScopeKind kind = ScopeKind::Module;
    u32 parent     = 0;
    u32 node       = 0;
    Vec<Sym> syms;
    Vec<u32> varnames; // indices into syms, in code-object order
    Vec<u32> cellvars;
    Vec<u32> freevars;
    u32 argcount   = 0; // positional, posonly included
    u32 posonly    = 0;
    u32 kwonly     = 0;
    u32 nparams    = 0; // *args and **kwargs included
    bool varargs   = false;
    bool varkw     = false;
    bool generator = false;
};

struct Symtab {
    Vec<Scope> scopes;
    Vec<u32> at_node; // node index -> scope index; 0 where a node opens none

    // False leaves a SyntaxError pending, with the line and column.
    bool build(const Ast &ast);

    const Sym *find(u32 scope, const StrObj *name) const;
};
