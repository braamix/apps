// `_symtable`: the scope pass, as objects symtable.py can read.
//
// symtab.cpp already works all of this out for the compiler -- which name is
// a local, a cell, a free variable or a global, and which scope each one
// belongs to. This module walks that result once and builds one table per
// scope, with the flag words CPython's own symtable module hands out, so
// `symtable.py` ships byte for byte on top of it.
//
// The flag bits and the scope numbers are CPython's
// Include/internal/pycore_symtable.h: a symbol's word is its DEF_* bits with
// the scope in bits 12 and up.
#include "bigint.h"
#include "builtin.h"
#include "compile.h"
#include "func.h"
#include "gc.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "method.h"
#include "module.h"
#include "obj.h"
#include "ops.h"
#include "parse.h"
#include "symtab.h"
#include "type.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// CPython's DEF_* and its five scopes.
enum : u32 {
    DEF_GLOBAL     = 1,
    DEF_LOCAL      = 2,
    DEF_PARAM      = 2 << 1,
    DEF_NONLOCAL   = 2 << 2,
    USE            = 2 << 3,
    DEF_FREE_CLASS = 2 << 5,
    DEF_IMPORT     = 2 << 6,
    DEF_ANNOT      = 2 << 7,
    DEF_COMP_ITER  = 2 << 8,
    DEF_TYPE_PARAM = 2 << 9,
    DEF_COMP_CELL  = 2 << 10,
    DEF_BOUND      = DEF_LOCAL | DEF_PARAM | DEF_IMPORT,

    SCOPE_OFF  = 12,
    SCOPE_MASK = DEF_GLOBAL | DEF_LOCAL | DEF_PARAM | DEF_NONLOCAL,

    LOCAL           = 1,
    GLOBAL_EXPLICIT = 2,
    GLOBAL_IMPLICIT = 3,
    FREE            = 4,
    CELL            = 5,
};

// _Py_block_ty, in its own order.
enum : u32 {
    TYPE_FUNCTION        = 0,
    TYPE_CLASS           = 1,
    TYPE_MODULE          = 2,
    TYPE_ANNOTATION      = 3,
    TYPE_TYPE_ALIAS      = 4,
    TYPE_TYPE_PARAMETERS = 5,
    TYPE_TYPE_VARIABLE   = 6,
};

// One scope. The whole tree is built at once, so a table holds its children
// rather than a way back to the Symtab, which is gone by then.
struct TableObj : Obj {
    Value name;     // StrObj
    Value symbols;  // DictObj: name -> the flag word
    Value children; // ListObj of TableObj
    u32 id;
    u32 type;
    u32 lineno;
    bool nested;
};

extern const Type table_type;

TableObj *table_of(Value v)
{
    return static_cast<TableObj *>(v.obj());
}

void table_trace(Obj *o)
{
    TableObj *t = static_cast<TableObj *>(o);
    gc_mark(t->name);
    gc_mark(t->symbols);
    gc_mark(t->children);
}

R table_repr(Value v, String &out)
{
    TableObj *t = table_of(v);
    Buf<96> b;
    b.put("<symtable entry ").put(is_str(t->name) ? str_of(t->name)->str() : Str("?"));
    b.put('(').put(u64(t->id)).put("), line ").put(u64(t->lineno)).put('>');
    return out.append(b.str()) ? R::Ok : oom();
}

R table_getattr(Value v, StrObj *name, Value &out)
{
    TableObj *t = table_of(v);
    Str n       = name->str();
    if (n == "id")
        out = Value::of_int(i32(t->id));
    else if (n == "name")
        out = t->name;
    else if (n == "type")
        out = Value::of_int(i32(t->type));
    else if (n == "lineno")
        out = Value::of_int(i32(t->lineno));
    else if (n == "nested")
        out = Value::of_int(t->nested ? 1 : 0);
    else if (n == "symbols")
        out = t->symbols;
    else if (n == "children")
        out = t->children;
    else
        return R::NotImpl;
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr Type table_type{ .name    = "symtable entry",
                           .trace   = table_trace,
                           .repr    = table_repr,
                           .getattr = table_getattr,
                           .final   = true };

// ------------------------------------------------------------ the walk

// What a name's binding is worth, in CPython's numbering. A module's and a
// class body's names are LOCAL where they are bound and GLOBAL_IMPLICIT
// where they are only read, which is what a LoadName does.
u32 scope_of(const Sym &y)
{
    switch (y.bind) {
    case Bind::Local:
        return LOCAL;
    case Bind::Cell:
        return CELL;
    case Bind::Free:
    case Bind::FreeOrClass:
        return FREE;
    case Bind::Global:
        return (y.flags & SF_GLOBAL) ? GLOBAL_EXPLICIT : GLOBAL_IMPLICIT;
    case Bind::GlobalOrClass:
        return GLOBAL_IMPLICIT;
    case Bind::Name:
    default:
        return (y.flags & (SF_PARAM | SF_ASSIGN)) ? LOCAL : GLOBAL_IMPLICIT;
    }
}

// An import binds the way an assignment does here, and nothing records which
// it was, so DEF_IMPORT is never set: Symbol.is_imported() answers False.
u32 flags_of(const Sym &y)
{
    u32 f = 0;
    if (y.flags & SF_GLOBAL)
        f |= DEF_GLOBAL;
    // A parameter is bound by being one; symtab.cpp marks it assigned as
    // well, and CPython does not, so DEF_PARAM wins.
    if ((y.flags & SF_ASSIGN) && !(y.flags & SF_PARAM))
        f |= DEF_LOCAL;
    if (y.flags & SF_PARAM)
        f |= DEF_PARAM;
    if (y.flags & SF_NONLOCAL)
        f |= DEF_NONLOCAL;
    if (y.flags & SF_USE)
        f |= USE;
    if (y.flags & SF_ANNOT)
        f |= DEF_ANNOT;
    if (y.flags & SF_ITER)
        f |= DEF_COMP_ITER;
    if (y.flags & SF_TPARAM)
        f |= DEF_TYPE_PARAM;
    return f | (scope_of(y) << SCOPE_OFF);
}

// The role an annotation scope was made for, which is what says whether it
// is an annotation, an alias's value or a type parameter's.
u8 role_of(const Symtab &st, u32 scope)
{
    for (usize i = 0; i < st.annos.size(); i++)
        if (st.annos[i].scope == scope)
            return st.annos[i].role;
    return AN_ANNOTATE;
}

u32 type_of_scope(const Symtab &st, u32 scope)
{
    switch (st.scopes[scope].kind) {
    case ScopeKind::Module:
        return TYPE_MODULE;
    case ScopeKind::Class:
        return TYPE_CLASS;
    case ScopeKind::Annotation:
        switch (role_of(st, scope)) {
        case AN_VALUE:
            return TYPE_TYPE_ALIAS;
        case AN_PARAMS:
            return TYPE_TYPE_PARAMETERS;
        case AN_BOUND:
        case AN_DEFAULT:
            return TYPE_TYPE_VARIABLE;
        default:
            return TYPE_ANNOTATION;
        }
    default:
        return TYPE_FUNCTION;
    }
}

// What the compiler calls a scope that has no identifier of its own. Its own
// copy: compile.cpp's is private to the compiler.
Str made_up_name(Nd kind)
{
    switch (kind) {
    case Nd::Lambda:
        return "<lambda>";
    case Nd::ListComp:
        return "<listcomp>";
    case Nd::SetComp:
        return "<setcomp>";
    case Nd::DictComp:
        return "<dictcomp>";
    case Nd::GeneratorExp:
        return "<genexpr>";
    default:
        return Str();
    }
}

// The name CPython gives a block: `top` for the module, the identifier for a
// `def` or a `class`, and the compiler's own name for a lambda or a
// comprehension.
Value name_of_scope(const Ast &ast, const Symtab &st, u32 scope)
{
    const Scope &s = st.scopes[scope];
    if (s.kind == ScopeKind::Module)
        return str_new("top");
    Str given = made_up_name(ast.at(s.node).kind);
    if (!given.empty())
        return str_new(given);
    if (s.kind == ScopeKind::Annotation && role_of(st, scope) == AN_ANNOTATE)
        return str_new("__annotate__");
    // A `type X = ...` node's own token is the keyword; its name is in `a`.
    u32 at = ast.at(s.node).kind == Nd::TypeAlias ? ast.at(s.node).a : s.node;
    return str_new(ast.text(at));
}

// Nested means "inside a function", which is what makes a name free rather
// than global. A class body between the two does not stop it.
bool nested_in_function(const Symtab &st, u32 scope)
{
    for (u32 at = st.scopes[scope].parent; at; at = st.scopes[at].parent)
        if (st.scopes[at].kind != ScopeKind::Class)
            return true;
    return false;
}

Value table_new(const Ast &ast, const Symtab &st, u32 scope)
{
    const Scope &s = st.scopes[scope];
    Root nm{ name_of_scope(ast, st, scope) };
    if (nm.v.is_nil())
        return Value();
    DictObj *syms = dict_new();
    if (!syms)
        return oom(), Value();
    Root rs{ obj_value(syms) };
    for (usize i = 0; i < s.syms.size(); i++) {
        const Sym &y = s.syms[i];
        if (dict_set(static_cast<DictObj *>(rs.v.obj()), obj_value(y.name),
                     Value::of_int(i32(flags_of(y)))) != R::Ok)
            return Value();
    }
    ListObj *kids = list_new();
    if (!kids)
        return Value();
    Root rk{ obj_value(kids) };
    TableObj *t = static_cast<TableObj *>(obj_alloc(&table_type, sizeof(TableObj)));
    if (!t)
        return oom(), Value();
    t->name     = nm.v;
    t->symbols  = rs.v;
    t->children = rk.v;
    // CPython's id is the table's address, so it is never zero; here it is
    // the scope's place in the tree, counted from one.
    t->id   = scope + 1;
    t->type = type_of_scope(st, scope);
    // The module's block has no line, as CPython's has none.
    t->lineno = s.kind == ScopeKind::Module ? 0 : ast.lex.tokens[ast.at(s.node).tok].line;
    t->nested = nested_in_function(st, scope);
    return obj_value(t);
}

// Every scope as a table, then each one's children linked in. Two passes,
// because a child is made before its parent's list is known to be safe.
Value tables_build(const Ast &ast, const Symtab &st)
{
    ListObj *all = list_new();
    if (!all)
        return Value();
    Root ra{ obj_value(all) };
    for (u32 i = 0; i < st.scopes.size(); i++) {
        Root t{ table_new(ast, st, i) };
        if (t.v.is_nil() || !list_push(list_of(ra.v), t.v))
            return Value();
    }
    for (u32 i = 1; i < st.scopes.size(); i++) {
        Value parent = list_of(ra.v)->items[st.scopes[i].parent];
        if (!list_push(list_of(table_of(parent)->children), list_of(ra.v)->items[i]))
            return oom(), Value();
    }
    return list_of(ra.v)->items[0];
}

R s_symtable(const CallArgs &a, Value &out)
{
    static const Str NAMES[] = { "code", "filename", "compile_type", "module" };
    Value got[4];
    if (!func_take(a, "symtable", NAMES, 3, got))
        return R::Err;
    Str source;
    if (is_str(got[0]))
        source = str_of(got[0])->str();
    else if (!bytes_like(got[0], source))
        return err_set("TypeError", "symtable() argument 1 must be str or bytes");
    Ast ast;
    if (!ast.parse(source, is_str(got[0])))
        return R::Err;
    Symtab st;
    if (!st.build(ast))
        return R::Err;
    out = tables_build(ast, st);
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr ModDef DEFS[] = { { "symtable", s_symtable } };

struct Constant {
    Str name;
    i64 v;
};

constexpr Constant CONSTANTS[] = {
    { "USE", USE },
    { "DEF_GLOBAL", DEF_GLOBAL },
    { "DEF_NONLOCAL", DEF_NONLOCAL },
    { "DEF_LOCAL", DEF_LOCAL },
    { "DEF_PARAM", DEF_PARAM },
    { "DEF_TYPE_PARAM", DEF_TYPE_PARAM },
    { "DEF_FREE_CLASS", DEF_FREE_CLASS },
    { "DEF_IMPORT", DEF_IMPORT },
    { "DEF_BOUND", DEF_BOUND },
    { "DEF_ANNOT", DEF_ANNOT },
    { "DEF_COMP_ITER", DEF_COMP_ITER },
    { "DEF_COMP_CELL", DEF_COMP_CELL },
    { "SCOPE_OFF", SCOPE_OFF },
    { "SCOPE_MASK", SCOPE_MASK },
    { "FREE", FREE },
    { "LOCAL", LOCAL },
    { "GLOBAL_IMPLICIT", GLOBAL_IMPLICIT },
    { "GLOBAL_EXPLICIT", GLOBAL_EXPLICIT },
    { "CELL", CELL },
    { "TYPE_FUNCTION", TYPE_FUNCTION },
    { "TYPE_CLASS", TYPE_CLASS },
    { "TYPE_MODULE", TYPE_MODULE },
    { "TYPE_ANNOTATION", TYPE_ANNOTATION },
    { "TYPE_TYPE_ALIAS", TYPE_TYPE_ALIAS },
    { "TYPE_TYPE_PARAMETERS", TYPE_TYPE_PARAMETERS },
    { "TYPE_TYPE_VARIABLE", TYPE_TYPE_VARIABLE },
};

} // namespace

bool symtable_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    if (!mod_defs(d, DEFS) || !mod_type(d, &table_type))
        return false;
    for (const Constant &c : CONSTANTS)
        if (!mod_int(d, c.name, c.v))
            return false;
    return true;
}
