// The scope pass: which name is a local, a cell, a free variable or a global.
//
// One walk collects what every scope binds and uses, a second decides each
// name's binding and hands out the slot numbers the code object indexes by.
// Names are interned, so a symbol is found by comparing pointers.
#pragma once

#include "intern.h"
#include "parse.h"

enum class Bind : u8 {
    Name,          // LoadName/StoreName: a module or a class body
    Local,         // LoadFast/StoreFast
    Cell,          // a local some nested scope captures, so it lives in a cell
    Free,          // captured from an enclosing scope, through the closure
    Global,        // LoadGlobal/StoreGlobal
    GlobalOrClass, // a global, unless the class the scope sits in has it
    FreeOrClass,   // a free variable, unless the class has it
};

// An annotation scope is PEP 695's: a function-like scope for type
// parameters, their bounds and defaults, and an alias's value, which may see
// the namespace of a class it is written in.
enum class ScopeKind : u8 { Module, Function, Lambda, Class, Comprehension, Annotation };

// What an annotation scope is for; with the node, it names the scope.
enum : u8 { AN_PARAMS, AN_BOUND, AN_DEFAULT, AN_VALUE };

enum : u8 {
    SF_PARAM    = 1 << 0,
    SF_ASSIGN   = 1 << 1,
    SF_USE      = 1 << 2,
    SF_GLOBAL   = 1 << 3, // declared `global`
    SF_NONLOCAL = 1 << 4, // declared `nonlocal`
    SF_ITER     = 1 << 5, // a comprehension's iteration variable
    SF_TPARAM   = 1 << 6, // a type parameter
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
    u32 argcount    = 0; // positional, posonly included
    u32 posonly     = 0;
    u32 kwonly      = 0;
    u32 nparams     = 0; // *args and **kwargs included
    bool varargs    = false;
    bool varkw      = false;
    bool generator  = false;
    bool coroutine  = false;   // `async def`, or a comprehension that awaits
    u32 retval      = 0;       // the first `return` with a value, for async generators
    StrObj *priv    = nullptr; // the class a `__name` in here is private to
    bool sees_class = false;   // an annotation scope that looks in a class first
    bool classdict  = false;   // a class whose namespace such a scope looks in
    bool classcell  = false;   // a class whose methods use __class__ or super()
    Str info;                  // an annotation scope, as a complaint names it
};

struct Anno {
    u32 node;
    u8 role;
    u32 scope;
};

struct Symtab {
    Vec<Scope> scopes;
    Vec<u32> at_node; // node index -> scope index; 0 where a node opens none
    Vec<Anno> annos;  // the annotation scopes, by node and role

    // False leaves a SyntaxError pending, with the line and column.
    bool build(const Ast &ast);

    const Sym *find(u32 scope, const StrObj *name) const;

    // An annotation scope's index, or 0 for none.
    u32 anno(u32 node, u8 role) const;
};
