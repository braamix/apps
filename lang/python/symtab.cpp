// The scope pass. Two walks: collect, then resolve.
//
// Annotations are not visited, because they are not compiled either; the
// README records that as a difference from CPython.
#include "symtab.h"

#include "err.h"
#include "intern.h"
#include "kernel/fmt.h"

namespace {

// ------------------------------------------------------------------ collect

struct Builder {
    const Ast *ast;
    Symtab *st;
    u32 cur     = 0;
    bool failed = false;

    Scope &scope() { return st->scopes[cur]; }

    bool fail(Str message, u32 n)
    {
        if (!failed) {
            failed         = true;
            const Token &t = ast->lex.tokens[ast->at(n).tok];
            err_set_at("SyntaxError", message, t.line, t.col);
        }
        return false;
    }

    bool oom()
    {
        if (!failed) {
            failed = true;
            err_set("MemoryError", "out of memory");
        }
        return false;
    }

    Sym *note(StrObj *name, u8 flags, u32 node = 0)
    {
        if (!name)
            return oom(), nullptr;
        Vec<Sym> &syms = scope().syms;
        Sym *found     = nullptr;
        for (usize i = 0; i < syms.size(); i++)
            if (syms[i].name == name) {
                syms[i].flags |= flags;
                found = &syms[i];
                break;
            }
        if (!found) {
            Sym s;
            s.name  = name;
            s.flags = flags;
            if (!syms.push(s))
                return oom(), nullptr;
            found = &syms.back();
        }
        if (node && !found->line) {
            const Token &t = ast->lex.tokens[ast->at(node).tok];
            found->line    = t.line;
            found->col     = t.col;
        }
        return found;
    }

    bool note_at(u32 n, u8 flags) { return note(str_intern(ast->text(n)), flags, n) != nullptr; }

    // A new scope under the current one, which becomes current.
    u32 open(ScopeKind kind, u32 node)
    {
        Scope s;
        s.kind   = kind;
        s.parent = cur;
        s.node   = node;
        if (!st->scopes.push(static_cast<Scope &&>(s)))
            return oom(), 0;
        u32 made          = u32(st->scopes.size() - 1);
        st->at_node[node] = made;
        cur               = made;
        return made;
    }

    bool param(StrObj *name)
    {
        Sym *s = note(name, SF_PARAM | SF_ASSIGN);
        if (!s)
            return false;
        if (s->param)
            return fail("duplicate argument in function definition", scope().node);
        s->param = ++scope().nparams;
        return true;
    }

    void kids(u32 n, u32 from, u32 count, void (Builder::*f)(u32))
    {
        const Node &node = ast->at(n);
        for (u32 k = 0; k < count && !failed; k++)
            (this->*f)(ast->kids[node.kid0 + from + k]);
    }

    void stmt(u32 i);
    void expr(u32 i);
    void target(u32 i);
    void comprehension(u32 i);
    void args_outer(u32 i);
    void args_inner(u32 i);
    void function(u32 i);
    void classdef(u32 i);
};

// The seven runs of an Arguments node, by the counts it carries.
struct ArgSpan {
    u32 posonly, args, vararg, kwonly, kwdefaults, kwarg, defaults;
    u32 at_posonly, at_args, at_vararg, at_kwonly, at_kwdefaults, at_kwarg, at_defaults;
};

ArgSpan arg_span(const Node &n)
{
    ArgSpan s;
    s.posonly    = n.a;
    s.args       = n.b;
    s.vararg     = (n.flags & ARG_VARARG) ? 1 : 0;
    s.kwonly     = n.c;
    s.kwdefaults = n.c;
    s.kwarg      = (n.flags & ARG_KWARG) ? 1 : 0;
    s.defaults   = n.d;

    s.at_posonly    = 0;
    s.at_args       = s.at_posonly + s.posonly;
    s.at_vararg     = s.at_args + s.args;
    s.at_kwonly     = s.at_vararg + s.vararg;
    s.at_kwdefaults = s.at_kwonly + s.kwonly;
    s.at_kwarg      = s.at_kwdefaults + s.kwdefaults;
    s.at_defaults   = s.at_kwarg + s.kwarg;
    return s;
}

void Builder::args_outer(u32 i)
{
    if (!i)
        return;
    ArgSpan s = arg_span(ast->at(i));
    kids(i, s.at_defaults, s.defaults, &Builder::expr);
    kids(i, s.at_kwdefaults, s.kwdefaults, &Builder::expr);
}

void Builder::args_inner(u32 i)
{
    if (!i)
        return;
    const Node &n = ast->at(i);
    ArgSpan s     = arg_span(n);

    scope().argcount = s.posonly + s.args;
    scope().posonly  = s.posonly;
    scope().kwonly   = s.kwonly;
    scope().varargs  = s.vararg != 0;
    scope().varkw    = s.kwarg != 0;

    // The order a call binds them in, which is the order of varnames:
    // positional, keyword-only, *args, **kwargs.
    const u32 at[4]    = { s.at_posonly, s.at_kwonly, s.at_vararg, s.at_kwarg };
    const u32 count[4] = { s.posonly + s.args, s.kwonly, s.vararg, s.kwarg };
    for (u32 g = 0; g < 4; g++)
        for (u32 k = 0; k < count[g] && !failed; k++) {
            u32 a = ast->kids[n.kid0 + at[g] + k];
            if (!param(str_intern(ast->text(a))))
                return;
        }
}

void Builder::function(u32 i)
{
    const Node &n = ast->at(i);
    // Decorators and defaults belong to the scope the definition sits in.
    kids(i, n.b, n.c, &Builder::expr);
    args_outer(n.a);
    if (n.kind != Nd::Lambda && !note_at(i, SF_ASSIGN))
        return;

    u32 saved = cur;
    if (!open(n.kind == Nd::Lambda ? ScopeKind::Lambda : ScopeKind::Function, i))
        return;
    args_inner(n.a);
    if (n.kind == Nd::Lambda)
        expr(n.b);
    else
        kids(i, 0, n.b, &Builder::stmt);
    cur = saved;
}

void Builder::classdef(u32 i)
{
    const Node &n = ast->at(i);
    kids(i, 0, n.a, &Builder::expr);               // bases
    kids(i, n.a, n.b, &Builder::expr);             // keywords
    kids(i, n.a + n.b + n.c, n.d, &Builder::expr); // decorators
    if (!note_at(i, SF_ASSIGN))
        return;

    u32 saved = cur;
    if (!open(ScopeKind::Class, i))
        return;
    kids(i, n.a + n.b, n.c, &Builder::stmt);
    cur = saved;
}

void Builder::comprehension(u32 i)
{
    const Node &n = ast->at(i);
    // The outermost iterable is evaluated where the comprehension is written.
    expr(ast->at(ast->kids[n.kid0]).b);

    u32 saved = cur;
    if (!open(ScopeKind::Comprehension, i))
        return;
    scope().argcount  = 1;
    scope().generator = n.kind == Nd::GeneratorExp;
    if (!param(str_intern(".0"))) {
        cur = saved;
        return;
    }
    for (u32 k = 0; k < n.nkid && !failed; k++) {
        u32 c         = ast->kids[n.kid0 + k];
        const Node &g = ast->at(c);
        if (g.flags & 1) {
            fail("async comprehensions are not compiled yet", c);
            break;
        }
        if (k)
            expr(g.b);
        target(g.a);
        for (u32 j = 0; j < g.nkid && !failed; j++)
            expr(ast->kids[g.kid0 + j]);
    }
    expr(n.a);
    if (n.kind == Nd::DictComp)
        expr(n.b);
    cur = saved;
}

void Builder::target(u32 i)
{
    if (!i || failed)
        return;
    const Node &n = ast->at(i);
    switch (n.kind) {
    case Nd::Name:
        note_at(i, SF_ASSIGN);
        return;
    case Nd::Attribute:
        expr(n.a);
        return;
    case Nd::Subscript:
        expr(n.a);
        expr(n.b);
        return;
    case Nd::Starred:
        target(n.a);
        return;
    case Nd::Tuple:
    case Nd::List:
        kids(i, 0, n.nkid, &Builder::target);
        return;
    default:
        expr(i);
        return;
    }
}

void Builder::expr(u32 i)
{
    if (!i || failed)
        return;
    const Node &n = ast->at(i);
    switch (n.kind) {
    case Nd::Name:
        note_at(i, SF_USE);
        return;
    case Nd::Constant:
        return;
    case Nd::FString:
        fail("f-strings are not compiled yet", i);
        return;
    case Nd::Await:
        fail("async is not compiled yet", i);
        return;

    case Nd::Lambda:
        function(i);
        return;
    case Nd::ListComp:
    case Nd::SetComp:
    case Nd::DictComp:
    case Nd::GeneratorExp:
        comprehension(i);
        return;

    case Nd::Yield:
    case Nd::YieldFrom:
        if (scope().kind == ScopeKind::Module || scope().kind == ScopeKind::Class) {
            fail("'yield' outside function", i);
            return;
        }
        scope().generator = true;
        expr(n.a);
        return;

    case Nd::NamedExpr:
        expr(n.b);
        target(n.a);
        return;
    case Nd::BinOp:
        expr(n.a);
        expr(n.b);
        return;
    case Nd::UnaryOp:
    case Nd::Starred:
        expr(n.a);
        return;
    case Nd::IfExp:
        expr(n.a);
        expr(n.b);
        expr(n.c);
        return;
    case Nd::Compare:
        expr(n.a);
        kids(i, 0, n.nkid, &Builder::expr);
        return;
    case Nd::CmpOp:
    case Nd::Keyword:
        expr(n.a);
        return;
    case Nd::Call:
        expr(n.a);
        kids(i, 0, n.nkid, &Builder::expr);
        return;
    case Nd::Attribute:
        expr(n.a);
        return;
    case Nd::Subscript:
    case Nd::Slice:
        expr(n.a);
        expr(n.b);
        expr(n.c);
        return;
    case Nd::BoolOp:
    case Nd::Dict:
    case Nd::Set:
    case Nd::List:
    case Nd::Tuple:
        kids(i, 0, n.nkid, &Builder::expr);
        return;
    default:
        return;
    }
}

void Builder::stmt(u32 i)
{
    if (!i || failed)
        return;
    const Node &n = ast->at(i);
    switch (n.kind) {
    case Nd::Module:
        kids(i, 0, n.nkid, &Builder::stmt);
        return;
    case Nd::FunctionDef:
        function(i);
        return;
    case Nd::ClassDef:
        classdef(i);
        return;
    case Nd::AsyncFunctionDef:
    case Nd::AsyncFor:
    case Nd::AsyncWith:
        fail("async is not compiled yet", i);
        return;

    case Nd::Return:
        if (scope().kind != ScopeKind::Function && scope().kind != ScopeKind::Lambda) {
            fail("'return' outside function", i);
            return;
        }
        expr(n.a);
        return;
    case Nd::Expr:
        expr(n.a);
        return;
    case Nd::Delete:
        for (u32 k = 0; k < n.nkid && !failed; k++) {
            u32 t = ast->kids[n.kid0 + k];
            if (ast->at(t).kind == Nd::Name)
                note_at(t, SF_ASSIGN);
            else
                target(t);
        }
        return;
    case Nd::Assign:
        expr(n.a);
        kids(i, 0, n.nkid, &Builder::target);
        return;
    case Nd::AugAssign:
        expr(n.b);
        expr(n.a); // an augmented target is read as well as written
        target(n.a);
        return;
    case Nd::AnnAssign:
        if (n.c) {
            expr(n.c);
            target(n.a);
        } else if (ast->at(n.a).kind != Nd::Name) {
            target(n.a);
        }
        return;
    case Nd::For:
        expr(n.b);
        target(n.a);
        kids(i, 0, n.c, &Builder::stmt);
        kids(i, n.c, n.d, &Builder::stmt);
        return;
    case Nd::While:
    case Nd::If:
        expr(n.a);
        kids(i, 0, n.b, &Builder::stmt);
        kids(i, n.b, n.c, &Builder::stmt);
        return;
    case Nd::With:
        kids(i, 0, n.a, &Builder::stmt); // WithItem, handled below
        kids(i, n.a, n.b, &Builder::stmt);
        return;
    case Nd::WithItem:
        expr(n.a);
        target(n.b);
        return;
    case Nd::Raise:
        expr(n.a);
        expr(n.b);
        return;
    case Nd::Try:
        kids(i, 0, n.a, &Builder::stmt);
        kids(i, n.a, n.b, &Builder::stmt);
        kids(i, n.a + n.b, n.c, &Builder::stmt);
        kids(i, n.a + n.b + n.c, n.d, &Builder::stmt);
        return;
    case Nd::ExceptHandler:
        expr(n.a);
        if (n.flags & 1)
            note_at(i, SF_ASSIGN);
        kids(i, 0, n.b, &Builder::stmt);
        return;
    case Nd::Assert:
        expr(n.a);
        expr(n.b);
        return;

    case Nd::Import:
    case Nd::ImportFrom:
        for (u32 k = 0; k < n.nkid && !failed; k++) {
            const Node &a = ast->at(ast->kids[n.kid0 + k]);
            if (a.flags & 1) // `from x import *`
                continue;
            StrObj *bound = a.a ? str_intern(ast->lex.text_of(ast->lex.tokens[a.a - 1]))
                                : str_intern(ast->lex.text_of(ast->lex.tokens[a.tok]));
            if (!note(bound, SF_ASSIGN))
                return;
        }
        return;

    case Nd::Global:
    case Nd::Nonlocal: {
        bool global = n.kind == Nd::Global;
        if (!global && scope().kind == ScopeKind::Module) {
            fail("nonlocal declaration not allowed at module level", i);
            return;
        }
        for (u32 k = 0; k < n.nkid && !failed; k++) {
            u32 t  = ast->kids[n.kid0 + k];
            Sym *s = note(str_intern(ast->text(t)), global ? SF_GLOBAL : SF_NONLOCAL, t);
            if (!s)
                return;
            if ((s->flags & SF_GLOBAL) && (s->flags & SF_NONLOCAL)) {
                fail("name is nonlocal and global", t);
                return;
            }
            if (s->flags & SF_PARAM) {
                fail("name is parameter and global", t);
                return;
            }
        }
        return;
    }

    default:
        return;
    }
}

// ------------------------------------------------------------------ resolve

using Names = Vec<StrObj *>;

bool holds(const Names &v, const StrObj *n)
{
    for (usize i = 0; i < v.size(); i++)
        if (v[i] == n)
            return true;
    return false;
}

Sym *find_mut(Scope &s, const StrObj *name)
{
    for (usize i = 0; i < s.syms.size(); i++)
        if (s.syms[i].name == name)
            return &s.syms[i];
    return nullptr;
}

// A free name in a child: the scope that owns it keeps it in a cell, and every
// scope in between carries it free so the closure reaches through.
bool link_free(Symtab &st, u32 from, StrObj *name)
{
    for (u32 a = from;; a = st.scopes[a].parent) {
        Sym *y = find_mut(st.scopes[a], name);
        if (y && (y->bind == Bind::Local || y->bind == Bind::Cell)) {
            y->bind = Bind::Cell;
            return true;
        }
        if (a == 0)
            return true; // the module binds no cells
        if (y) {
            y->bind = Bind::Free;
        } else {
            Sym s;
            s.name = name;
            s.bind = Bind::Free;
            if (!st.scopes[a].syms.push(s))
                return false;
        }
    }
}

bool decide(Symtab &st, u32 si, const Names &bound)
{
    Scope &s   = st.scopes[si];
    bool block = s.kind == ScopeKind::Module || s.kind == ScopeKind::Class;

    for (usize i = 0; i < s.syms.size(); i++) {
        Sym &y = s.syms[i];
        if (y.flags & SF_GLOBAL) {
            y.bind = s.kind == ScopeKind::Module ? Bind::Name : Bind::Global;
        } else if (y.flags & SF_NONLOCAL) {
            if (!holds(bound, y.name)) {
                Buf<160> b;
                b.put("no binding for nonlocal '").put(y.name->str()).put("' found");
                return err_set_at("SyntaxError", b.str(), y.line, y.col), false;
            }
            y.bind = Bind::Free;
        } else if (y.flags & (SF_ASSIGN | SF_PARAM)) {
            y.bind = block ? Bind::Name : Bind::Local;
        } else if (!block && holds(bound, y.name)) {
            y.bind = Bind::Free;
        } else if (s.kind == ScopeKind::Class && holds(bound, y.name)) {
            y.bind = Bind::Free;
        } else {
            y.bind = block ? Bind::Name : Bind::Global;
        }
    }
    return true;
}

// Parameters first, in the order a call binds them; then the plain locals.
bool slots(Scope &s)
{
    if (!s.varnames.resize(s.nparams))
        return false;
    for (usize i = 0; i < s.syms.size(); i++)
        if (s.syms[i].param)
            s.varnames[s.syms[i].param - 1] = u32(i);
    for (usize i = 0; i < s.syms.size(); i++)
        if (!s.syms[i].param && s.syms[i].bind == Bind::Local && !s.varnames.push(u32(i)))
            return false;
    for (usize i = 0; i < s.syms.size(); i++)
        if (s.syms[i].bind == Bind::Cell && !s.cellvars.push(u32(i)))
            return false;
    for (usize i = 0; i < s.syms.size(); i++)
        if (s.syms[i].bind == Bind::Free && !s.freevars.push(u32(i)))
            return false;

    for (usize k = 0; k < s.varnames.size(); k++)
        if (s.syms[s.varnames[k]].bind == Bind::Local)
            s.syms[s.varnames[k]].slot = u32(k);
    for (usize k = 0; k < s.cellvars.size(); k++)
        s.syms[s.cellvars[k]].slot = u32(k);
    for (usize k = 0; k < s.freevars.size(); k++)
        s.syms[s.freevars[k]].slot = u32(k);
    return true;
}

bool analyze(Symtab &st, u32 si, const Names &bound)
{
    if (!decide(st, si, bound))
        return false;

    for (u32 c = si + 1; c < st.scopes.size(); c++) {
        if (st.scopes[c].parent != si)
            continue;

        // What the child may capture: this scope's own bindings on top of
        // what reaches it from further out, less anything declared global. A
        // class body binds nothing a nested scope can see.
        Names next;
        for (usize i = 0; i < bound.size(); i++) {
            Scope &s = st.scopes[si];
            Sym *y   = find_mut(s, bound[i]);
            if (y && (y->flags & SF_GLOBAL))
                continue;
            if (!next.push(bound[i]))
                return err_set("MemoryError", "out of memory"), false;
        }
        if (st.scopes[si].kind != ScopeKind::Class)
            for (usize i = 0; i < st.scopes[si].syms.size(); i++) {
                Sym &y = st.scopes[si].syms[i];
                if (y.bind != Bind::Local && y.bind != Bind::Cell && y.bind != Bind::Free)
                    continue;
                if (holds(next, y.name))
                    continue;
                if (!next.push(y.name))
                    return err_set("MemoryError", "out of memory"), false;
            }

        if (!analyze(st, c, next))
            return false;

        for (usize i = 0; i < st.scopes[c].syms.size(); i++) {
            Sym &y = st.scopes[c].syms[i];
            if (y.bind == Bind::Free && !link_free(st, si, y.name))
                return err_set("MemoryError", "out of memory"), false;
        }
    }

    return slots(st.scopes[si]);
}

} // namespace

const Sym *Symtab::find(u32 scope, const StrObj *name) const
{
    const Scope &s = scopes[scope];
    for (usize i = 0; i < s.syms.size(); i++)
        if (s.syms[i].name == name)
            return &s.syms[i];
    return nullptr;
}

bool Symtab::build(const Ast &ast)
{
    if (!at_node.resize(ast.nodes.size()))
        return err_set("MemoryError", "out of memory"), false;

    Scope module;
    module.node = ast.root;
    if (!scopes.push(static_cast<Scope &&>(module)))
        return err_set("MemoryError", "out of memory"), false;
    at_node[ast.root] = 0;

    Builder b{ &ast, this };
    b.stmt(ast.root);
    if (b.failed)
        return false;

    Names empty;
    return analyze(*this, 0, empty);
}
